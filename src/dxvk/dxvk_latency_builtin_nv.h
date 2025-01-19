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
   * \brief Internal timers for LL2 timing
   */
  struct DxvkLatencyFrameDataNv {
    using time_point = dxvk::high_resolution_clock::time_point;
    using duration = dxvk::high_resolution_clock::duration;

    uint64_t    frameId         = 0u;
    time_point  frameStart      = time_point();
    time_point  frameEnd        = time_point();
    duration    sleepDuration   = duration(0u);
    VkResult    presentResult   = VK_NOT_READY;
    VkBool32    presentPending  = VK_FALSE;
  };


  /**
   * \brief Built-in latency tracker based on VK_NV_low_latency2
   *
   * Implements a simple latency reduction algorithm
   * based on CPU timestamps received from the backend.
   */
  class DxvkBuiltInLatencyTrackerNv : public DxvkLatencyTracker {
    using time_point = typename DxvkLatencyFrameDataNv::time_point;
    using duration = typename DxvkLatencyFrameDataNv::duration;

    constexpr static size_t FrameCount = 8u;
  public:

    DxvkBuiltInLatencyTrackerNv(
      const Rc<Presenter>&            presenter);

    ~DxvkBuiltInLatencyTrackerNv();

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
    double                    m_envFpsLimit = 0.0;

    dxvk::mutex               m_mutex;
    dxvk::condition_variable  m_cond;

    uint64_t                  m_lastFrameId = 0u;
    uint64_t                  m_lastDiscard = 0u;

    bool                      m_lowLatencyEnabled = false;

    std::array<DxvkLatencyFrameDataNv, FrameCount> m_frames = { };

    DxvkLatencyFrameDataNv* initFrame(uint64_t frameId);

    DxvkLatencyFrameDataNv* getFrame(uint64_t frameId);

  };

}
