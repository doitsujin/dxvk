#pragma once

#include "dxvk_framepacer_mode.h"
#include "dxvk_presentation_latency.h"
#include "../dxvk_options.h"
#include "../../util/log/log.h"
#include "../../util/util_string.h"
#include <assert.h>

namespace dxvk {

  /*
   * This low-latency mode aims to reduce latency with minimal impact in fps.
   * Effective when operating in the GPU-limit. Efficient to be used in the CPU-limit as well.
   *
   * Greatly reduces input lag variations when switching between CPU- and GPU-limit, and
   * compared to the max-frame-latency approach, it has a much more stable input lag when
   * GPU running times change dramatically, which can happen for example when rotating within a scene.
   *
   * The current implementation rather generates fluctuations alternating frame-by-frame
   * depending on the game's and dxvk's CPU-time variations. This might be visible as a loss
   * in smoothness, which is an area this implementation can be further improved. Unsuitable
   * smoothing however might degrade input-lag feel, so it's not implemented for now, but
   * more advanced smoothing techniques will be investigated in the future.
   * In some situations however, this low-latency pacing actually improves smoothing though,
   * it will depend on the game.
   *
   * An interesting observation while playtesting was that not only the input lag was affected,
   * but the video generated did progress more cleanly in time as well with regards to
   * medium-term time consistency, in other words, the video playback speed remained more steady.
   *
   * Optimized for VRR and VK_PRESENT_MODE_IMMEDIATE_KHR. It also comes with its own fps-limiter
   * which is typically used to prevent the game's fps exceeding the monitor's refresh rate,
   * and which is tightly integrated into the pacing logic.
   *
   * Can be fine-tuned via the dxvk.lowLatencyOffset and dxvk.lowLatencyAllowCpuFramesOverlap
   * variables (or their respective environment variables)
   * Compared to maxFrameLatency = 3, render-latency reductions of up to 67% are achieved.
   */

  class LowLatencyMode : public FramePacerMode {
    using microseconds = std::chrono::microseconds;
    using time_point = high_resolution_clock::time_point;
  public:

    LowLatencyMode(Mode mode, LatencyMarkersStorage* storage, const DxvkOptions& options, int refreshRate = 0)
    : FramePacerMode(mode, storage),
      m_lowLatencyOffset(getLowLatencyOffset(options)),
      m_allowCpuFramesOverlap(options.lowLatencyAllowCpuFramesOverlap) {
      Logger::info( str::format("Using lowLatencyOffset: ", m_lowLatencyOffset) );
      Logger::info( str::format("Using lowLatencyAllowCpuFramesOverlap: ", m_allowCpuFramesOverlap) );

      if (refreshRate > 0) {
        m_vrrRefreshInterval = 1'000'000 / refreshRate;
        Logger::info( str::format("Using vrr refresh rate: ", refreshRate) );
      }

    }

    ~LowLatencyMode() {}

    bool getDesiredPresentMode( uint32_t& presentMode ) const override;

    void startFrame( uint64_t frameId ) override {

      using std::chrono::duration_cast;

      if (!m_allowCpuFramesOverlap)
        m_fenceCsFinished.wait( frameId-1 );

      m_fenceGpuStart.wait( frameId-1 );

      time_point now = high_resolution_clock::now();
      uint64_t finishedId = m_latencyMarkersStorage->getTimeline()->gpuFinished.load();
      if (finishedId <= DXGI_MAX_SWAP_CHAIN_BUFFERS+1ull)
        return;

      if (finishedId == frameId-1) {
        // we are the only in-flight frame, nothing to do other then
        // to sync to v-blank and apply fps-limiter if needed
        int32_t delay = 0;
        if (m_mode == LOW_LATENCY_VRR) {
          const SyncProps props = getSyncPrediction();
          delay = std::max(delay, getVrrDelay(frameId, props, now));
          delay += m_lowLatencyOffset;
        }

        m_lastStart = sleepFor( now, delay );
        return;
      }

      if (finishedId != frameId-2) {
        Logger::err( str::format("internal error during low-latency frame pacing: expected finished frameId=",
          frameId-2, ", got: ", finishedId) );
      }

      const LatencyMarkers* m = m_latencyMarkersStorage->getConstMarkers(frameId-1);

      // estimate the target gpu sync point for this frame
      // and calculate backwards when we want to start this frame

      const SyncProps props = getSyncPrediction();
      int32_t lastFrameStart = duration_cast<microseconds>( m->start - now ).count();
      int32_t gpuReadyPrediction = lastFrameStart
        + std::max( props.cpuUntilGpuStart, m->gpuStart )
        + props.optimizedGpuTime;

      int32_t targetGpuSync = gpuReadyPrediction + props.gpuSync;
      int32_t gpuDelay = targetGpuSync - props.cpuUntilGpuSync;

      int32_t cpuReadyPrediction = duration_cast<microseconds>(
        m->start + microseconds(props.csFinished) - now).count();
      int32_t cpuDelay = cpuReadyPrediction - props.csStart;

      int32_t delay = std::max(gpuDelay, cpuDelay);

      if (m_mode == LOW_LATENCY_VRR) {
        delay = std::max(delay, getVrrDelay(frameId, props, now));
      }

      delay += m_lowLatencyOffset;
      m_lastStart = sleepFor( now, delay );

    }


    void finishRender( uint64_t frameId ) override {

      using std::chrono::duration_cast;
      const LatencyMarkers* m = m_latencyMarkersStorage->getConstMarkers(frameId);

      int32_t numLoop = (int32_t)(m->gpuReady.size())-1;
      if (numLoop <= 1) {
        m_props[frameId % m_props.size()] = SyncProps();
        m_props[frameId % m_props.size()].isOutlier = true;
        m_propsFinished.store( frameId );
        return;
      }

      // estimates the optimal overlap for cpu/gpu work by optimizing gpu scheduling first
      // such that the gpu doesn't go into idle for this frame, and then aligning cpu submits
      // where gpuSubmit[i] <= gpuRun[i] for all i

      std::vector<int32_t>& gpuRun = m_tempGpuRun;
      gpuRun.clear();
      int32_t optimizedGpuTime = 0;
      gpuRun.push_back(optimizedGpuTime);

      for (int i=0; i<numLoop; ++i) {
        time_point _gpuRun = std::max( m->gpuReady[i], m->gpuQueueSubmit[i] );
        int32_t duration = duration_cast<microseconds>( m->gpuReady[i+1] - _gpuRun ).count();
        optimizedGpuTime += duration;
        gpuRun.push_back(optimizedGpuTime);
      }

      int32_t alignment = duration_cast<microseconds>( m->gpuSubmit[numLoop-1] - m->gpuSubmit[0] ).count()
        - gpuRun[numLoop-1];

      int32_t offset = 0;
      for (int i=numLoop-2; i>=0; --i) {
        int32_t curSubmit = duration_cast<microseconds>( m->gpuSubmit[i] - m->gpuSubmit[0] ).count();
        int32_t diff = curSubmit - gpuRun[i] - alignment;
        diff = std::max( 0, diff );
        offset += diff;
        alignment += diff;
      }


      SyncProps& props = m_props[frameId % m_props.size()];
      props.gpuSync = gpuRun[numLoop-1];
      props.cpuUntilGpuSync = offset + duration_cast<microseconds>( m->gpuSubmit[numLoop-1] - m->start ).count();
      props.cpuUntilGpuStart = props.cpuUntilGpuSync - props.gpuSync;
      props.optimizedGpuTime = optimizedGpuTime;
      props.csStart = m->csStart;
      props.csFinished = m->csFinished;
      props.isOutlier = isOutlier(frameId);

      m_propsFinished.store( frameId );

    }


    void endFrame( uint64_t frameId ) override {

      if (m_mode == LOW_LATENCY_VRR) {
        const LatencyMarkers* m = m_latencyMarkersStorage->getConstMarkers(frameId);
        m_presentationLatency.push( m->presentFinished - m->gpuFinished );
      }

    }



  private:

    struct SyncProps {
      int32_t optimizedGpuTime;   // gpu executing packed submits in one go
      int32_t gpuSync;            // gpuStart to this sync point, in microseconds
      int32_t cpuUntilGpuSync;
      int32_t cpuUntilGpuStart;
      int32_t csStart;
      int32_t csFinished;
      bool    isOutlier;
    };


    SyncProps getSyncPrediction() {
      // In the future we might use more samples to get a prediction.
      // Possibly this will be optional, as until now, basing it on
      // just the previous frame gave us the best mouse input feel.
      // Simple averaging or median filtering is surely not the way
      // to go, but more advanced methods will be investigated.
      // The best place to filter should be on the Present() timeline,
      // so not sure if we really will do any filtering here other
      // than outlier removal, which will dampen stuttering effects.
      SyncProps res = {};
      uint64_t id = m_propsFinished;
      if (id < DXGI_MAX_SWAP_CHAIN_BUFFERS+7)
        return res;

      for (size_t i=0; i<7; ++i) {
        const SyncProps& props = m_props[ (id-i) % m_props.size() ];
        if (!props.isOutlier) {
          id = id-i;
          break;
        }
      }

      return m_props[ id % m_props.size() ];
    };


    bool isOutlier( uint64_t frameId ) {
      constexpr size_t numLoop = 7;
      int32_t totalCpuTime = 0;
      for (size_t i=1; i<numLoop; ++i) {
        const LatencyMarkers* m = m_latencyMarkersStorage->getConstMarkers(frameId-i);
        totalCpuTime += m->cpuFinished;
      }

      int32_t avgCpuTime = totalCpuTime / (numLoop-1);
      const LatencyMarkers* m = m_latencyMarkersStorage->getConstMarkers(frameId);
      if (m->cpuFinished > 1.3*avgCpuTime || m->gpuSubmit.empty() || m->gpuReady.size() != (m->gpuSubmit.size()+1) )
        return true;

      return false;
    }


    int32_t getVrrDelay( uint64_t frameId, const SyncProps& props, const time_point& now ) {
      uint64_t frameFinishedId = m_latencyMarkersStorage->getTimeline()->frameFinished.load();
      int32_t lastVBlank = std::chrono::duration_cast<microseconds> (
        m_latencyMarkersStorage->getConstMarkers(frameFinishedId)->end - now ).count();
      int32_t presentLatency = m_presentationLatency.getMedian();

      int32_t targetVBlank = lastVBlank
        + (frameId - frameFinishedId) * m_vrrRefreshInterval
        - presentLatency;

      return targetVBlank - props.optimizedGpuTime - props.cpuUntilGpuStart;
    }


    Sleep::TimePoint sleepFor( const Sleep::TimePoint t, int32_t delay ) {

      // account for the fps limit and ensure we won't sleep too long, just in case
      int32_t frametime = std::chrono::duration_cast<microseconds>( t - m_lastStart ).count();
      int32_t frametimeDiff = std::max( 0, m_fpsLimitFrametime.load() - frametime );
      delay = std::max( delay, frametimeDiff );
      int32_t maxDelay = std::max( m_fpsLimitFrametime.load(), 20000 );
      delay = std::max( 0, std::min( delay, maxDelay ) );

      Sleep::TimePoint nextStart = t + microseconds(delay);
      Sleep::sleepUntil( t, nextStart );
      return nextStart;

    }


    int32_t getLowLatencyOffset( const DxvkOptions& options );
    bool getLowLatencyAllowCpuFramesOverlap( const DxvkOptions& options );

    const int32_t m_lowLatencyOffset;
    const bool    m_allowCpuFramesOverlap;

    Sleep::TimePoint m_lastStart = { high_resolution_clock::now() };
    int32_t m_vrrRefreshInterval = { 0 };
    PresentationLatency m_presentationLatency;

    std::array<SyncProps, 16> m_props;
    std::atomic<uint64_t> m_propsFinished = { 0 };

    std::vector<int32_t>  m_tempGpuRun;

  };

}
