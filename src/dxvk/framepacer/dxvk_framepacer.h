#pragma once

#include "dxvk_framepacer_mode.h"
#include "dxvk_latency_markers.h"
#include "../dxvk_latency.h"
#include "../../util/util_time.h"
#include <dxgi.h>


namespace dxvk {

  struct DxvkOptions;

  /* \brief Frame pacer interface managing the CPU - GPU synchronization.
   *
   * GPUs render frames asynchronously to the game's and dxvk's CPU-side work
   * in order to improve fps-throughput. Aligning the cpu work to chosen time-
   * points allows to tune certain characteristics of the video presentation,
   * like smoothness and latency.
   */

  class FramePacer : public DxvkLatencyTracker {
    using microseconds = std::chrono::microseconds;
  public:

    FramePacer( const DxvkOptions& options );
    ~FramePacer();

    void sleepAndBeginFrame(
            uint64_t                  frameId,
            double                    maxFrameRate) override {
      // wait for finished rendering of a previous frame, typically the one before last
      m_mode->waitRenderFinished(frameId);
      // potentially wait some more if the cpu gets too much ahead
      m_mode->startFrame(frameId);
      m_latencyMarkersStorage.registerFrameStart(frameId);
      m_gpuStarts[ frameId % m_gpuStarts.size() ].store(0);
    }

    void notifyGpuPresentEnd( uint64_t frameId ) override {
      // the frame has been displayed to the screen
      m_latencyMarkersStorage.registerFrameEnd(frameId);
      m_mode->endFrame(frameId);
    }

    void notifyCsRenderBegin( uint64_t frameId ) override {
      auto now = high_resolution_clock::now();
      LatencyMarkers* m = m_latencyMarkersStorage.getMarkers(frameId);
      m->csStart = std::chrono::duration_cast<microseconds>(now - m->start).count();
    }

    void notifyCsRenderEnd( uint64_t frameId ) override {
      auto now = high_resolution_clock::now();
      LatencyMarkers* m = m_latencyMarkersStorage.getMarkers(frameId);
      m->csFinished = std::chrono::duration_cast<microseconds>(now - m->start).count();
      m_mode->signalCsFinished( frameId );
    }

    void notifySubmit() override {
      LatencyMarkers* m = m_latencyMarkersStorage.getMarkers(m_lastSubmitFrameId+1);
      m->gpuSubmit.push_back(high_resolution_clock::now());
    }

    void notifyPresent( uint64_t frameId ) override {
      // dx to vk translation is finished
      if (frameId != 0) {
        auto now = high_resolution_clock::now();
        m_lastSubmitFrameId = frameId;
        LatencyMarkers* m = m_latencyMarkersStorage.getMarkers(frameId);
        LatencyMarkers* next = m_latencyMarkersStorage.getMarkers(frameId+1);
        m->gpuSubmit.push_back(now);
        m->cpuFinished = std::chrono::duration_cast<microseconds>(now - m->start).count();
        next->gpuSubmit.clear();

        m_latencyMarkersStorage.m_timeline.cpuFinished.store(frameId);
      }
    }

    void notifyQueueSubmit( uint64_t frameId ) override {
      assert( frameId == m_lastQueueSubmitFrameId + 1 );
      auto now = high_resolution_clock::now();
      LatencyMarkers* m = m_latencyMarkersStorage.getMarkers(frameId);
      m->gpuQueueSubmit.push_back(now);
      queueSubmitCheckGpuStart(frameId, m, now);
    }

    void notifyQueuePresentBegin( uint64_t frameId ) override {
      if (frameId != 0) {
        auto now = high_resolution_clock::now();
        m_lastQueueSubmitFrameId = frameId;
        LatencyMarkers* m = m_latencyMarkersStorage.getMarkers(frameId);
        LatencyMarkers* next = m_latencyMarkersStorage.getMarkers(frameId+1);
        m->gpuQueueSubmit.push_back(now);
        next->gpuQueueSubmit.clear();
        queueSubmitCheckGpuStart(frameId, m, now);
      }
    }

    void notifyGpuExecutionBegin( uint64_t frameId ) override {
      assert( frameId == m_lastFinishedFrameId+1 );
      LatencyMarkers* m = m_latencyMarkersStorage.getMarkers(m_lastFinishedFrameId+1);
      gpuExecutionCheckGpuStart(frameId, m, high_resolution_clock::now());
    }

    void notifyGpuExecutionEnd( uint64_t frameId ) override {
      auto now = high_resolution_clock::now();
      LatencyMarkers* m = m_latencyMarkersStorage.getMarkers(m_lastFinishedFrameId+1);
      m->gpuReady.push_back(now);
    }

    virtual void notifyGpuPresentBegin( uint64_t frameId ) override {
      // we get frameId == 0 for repeated presents (SyncInterval)
      if (frameId != 0) {
        m_lastFinishedFrameId = frameId;
        auto now = high_resolution_clock::now();

        LatencyMarkers* m = m_latencyMarkersStorage.getMarkers(frameId);
        LatencyMarkers* next = m_latencyMarkersStorage.getMarkers(frameId+1);
        m->gpuReady.push_back(now);
        m->gpuFinished = std::chrono::duration_cast<microseconds>(now - m->start).count();
        next->gpuReady.clear();
        next->gpuReady.push_back(now);

        gpuExecutionCheckGpuStart(frameId, m, now);

        m_latencyMarkersStorage.m_timeline.gpuFinished.store(frameId);
        m_mode->finishRender(frameId);
        m_mode->signalRenderFinished(frameId);
      }
    }

    FramePacerMode::Mode getMode() const {
      return m_mode->m_mode;
    }

    void setTargetFrameRate( double frameRate ) {
      m_mode->setTargetFrameRate(frameRate);
    }

    bool needsAutoMarkers() override {
      return true;
    }

    LatencyMarkersStorage m_latencyMarkersStorage;


    // not implemented methods


    void notifyCpuPresentBegin( uint64_t frameId) override { }
    void notifyCpuPresentEnd( uint64_t frameId ) override { }
    void notifyQueuePresentEnd( uint64_t frameId, VkResult status) override { }
    void discardTimings() override { }
    DxvkLatencyStats getStatistics( uint64_t frameId ) override
      { return DxvkLatencyStats(); }

  private:

    void signalGpuStart( uint64_t frameId, LatencyMarkers* m, const high_resolution_clock::time_point& t ) {
      m->gpuStart = std::chrono::duration_cast<microseconds>(t - m->start).count();
      m_latencyMarkersStorage.m_timeline.gpuStart.store(frameId);
      m_mode->signalGpuStart(frameId);
    }

    void queueSubmitCheckGpuStart( uint64_t frameId, LatencyMarkers* m, const high_resolution_clock::time_point& t ) {
      auto& gpuStart = m_gpuStarts[ frameId % m_gpuStarts.size() ];
      uint16_t val = gpuStart.fetch_or(queueSubmitBit);
      if (val == gpuReadyBit)
        signalGpuStart( frameId, m, t );
    }

    void gpuExecutionCheckGpuStart( uint64_t frameId, LatencyMarkers* m, const high_resolution_clock::time_point& t ) {
      auto& gpuStart = m_gpuStarts[ frameId % m_gpuStarts.size() ];
      uint16_t val = gpuStart.fetch_or(gpuReadyBit);
      if (val == queueSubmitBit)
        signalGpuStart( frameId, m, t );
    }

    std::unique_ptr<FramePacerMode> m_mode;

    uint64_t m_lastSubmitFrameId      = { DXGI_MAX_SWAP_CHAIN_BUFFERS };
    uint64_t m_lastQueueSubmitFrameId = { DXGI_MAX_SWAP_CHAIN_BUFFERS };
    uint64_t m_lastFinishedFrameId    = { DXGI_MAX_SWAP_CHAIN_BUFFERS };

    std::array< std::atomic< uint16_t >, 16 > m_gpuStarts = { };
    static constexpr uint16_t queueSubmitBit = 1;
    static constexpr uint16_t gpuReadyBit    = 2;

  };

}
