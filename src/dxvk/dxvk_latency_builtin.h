#pragma once

#include <array>

#include "dxvk_latency.h"
#include "dxvk_presenter.h"

#include "../util/thread.h"

#include "../util/util_sleep.h"
#include "../util/util_time.h"

#include "../util/config/config.h"

#include "../util/sync/sync_spinlock.h"

namespace dxvk {

  /**
   * \brief Timings for a single tracked frame
   */
  struct DxvkLatencyFrameData {
    using time_point = dxvk::high_resolution_clock::time_point;
    using duration = dxvk::high_resolution_clock::duration;

    uint64_t    frameId         = 0u;
    time_point  frameStart      = time_point();
    time_point  frameEnd        = time_point();
    time_point  cpuPresentBegin = time_point();
    time_point  cpuPresentEnd   = time_point();
    time_point  queueSubmit     = time_point();
    time_point  queuePresent    = time_point();
    time_point  gpuExecStart    = time_point();
    time_point  gpuExecEnd      = time_point();
    time_point  gpuIdleStart    = time_point();
    time_point  gpuIdleEnd      = time_point();
    duration    gpuIdleTime     = duration(0u);
    duration    sleepDuration   = duration(0u);
    VkResult    presentStatus   = VK_NOT_READY;
  };


  /**
   * \brief Built-in latency tracker
   *
   * Implements a simple latency reduction algorithm
   * based on CPU timestamps received from the backend.
   */
  class DxvkBuiltInLatencyTracker : public DxvkLatencyTracker {
    using time_point = typename DxvkLatencyFrameData::time_point;
    using duration = typename DxvkLatencyFrameData::duration;

    constexpr static size_t FrameCount = 8u;
  public:

    DxvkBuiltInLatencyTracker(
            Rc<Presenter>             presenter,
            int32_t                   toleranceUs,
            bool                      useNvLowLatency2);

    ~DxvkBuiltInLatencyTracker();

    bool needsAutoMarkers();

    void notifyCpuPresentBegin(
            uint64_t                  frameId);

    void notifyCpuPresentEnd(
            uint64_t                  frameId);

    void notifyCsRenderBegin(
            uint64_t                  frameId);

    void notifyCsRenderEnd(
            uint64_t                  frameId);

    void notifyQueueSubmit(
            uint64_t                  frameId);

    void notifyQueuePresentBegin(
            uint64_t                  frameId);

    void notifyQueuePresentEnd(
            uint64_t                  frameId,
            VkResult                  status);

    void notifyGpuExecutionBegin(
            uint64_t                  frameId);

    void notifyGpuExecutionEnd(
            uint64_t                  frameId);

    void notifyGpuPresentEnd(
            uint64_t                  frameId);

    void sleepAndBeginFrame(
            uint64_t                  frameId,
            double                    maxFrameRate);

    void discardTimings();

    DxvkLatencyStats getStatistics(
            uint64_t                  frameId);

  private:

    Rc<Presenter>             m_presenter;

    dxvk::mutex               m_mutex;
    dxvk::condition_variable  m_cond;

    duration                  m_tolerance;

    double                    m_envFpsLimit = 0.0;
    bool                      m_useNvLowLatency2 = false;

    std::array<DxvkLatencyFrameData, FrameCount> m_frames = { };

    uint64_t m_validRangeBegin = 0u;
    uint64_t m_validRangeEnd = 0u;

    duration sleepNv(
            uint64_t                  frameId,
            double                    maxFrameRate);

    duration sleepBuiltin(
            uint64_t                  frameId,
            double                    maxFrameRate);

    DxvkLatencyFrameData* initFrame(
            uint64_t                  frameId);

    DxvkLatencyFrameData* findFrame(
            uint64_t                  frameId);

    duration computeFrameInterval(
            double                    maxFrameRate);

    static duration computeIntervalFromRate(
            double                    frameRate);

    static duration estimateTime(
      const duration*                 frames,
            size_t                    frameCount);

  };

}
