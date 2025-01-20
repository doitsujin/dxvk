#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "../util/util_likely.h"
#include "../util/util_time.h"

#include "../util/rc/util_rc_ptr.h"

#include "../vulkan/vulkan_loader.h"

namespace dxvk {

  /**
   * \brief Latency tracker statistics
   */
  struct DxvkLatencyStats {
    std::chrono::microseconds frameLatency;
    std::chrono::microseconds sleepDuration;
  };


  /**
   * \brief Timings for a single tracked frame
   */
  struct DxvkLatencyFrameData {
    using time_point = dxvk::high_resolution_clock::time_point;
    using duration = dxvk::high_resolution_clock::duration;

    uint64_t    frameId         = 0u;
    uint64_t    appFrameId      = 0u;
    time_point  frameStart      = time_point();
    time_point  frameEnd        = time_point();
    time_point  cpuInputSample  = time_point();
    time_point  cpuSimBegin     = time_point();
    time_point  cpuSimEnd       = time_point();
    time_point  cpuRenderBegin  = time_point();
    time_point  cpuRenderEnd    = time_point();
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
   * \brief Latency tracker
   *
   * Accumulates time stamps of certain parts of a frame.
   */
  class DxvkLatencyTracker {

  public:

    virtual ~DxvkLatencyTracker() { }

    /**
     * \brief Increments ref count
     */
    void incRef() {
      m_refCount.fetch_add(1, std::memory_order_acquire);
    }

    /**
     * \brief Decrements ref count
     *
     * Destroys the object when there are no users left.
     */
    void decRef() {
      if (m_refCount.fetch_sub(1, std::memory_order_release) == 1u)
        delete this;
    }

    /**
     * \brief Checks whether automatic markers are needed
     *
     * Relevant for forwarding the latency tracker to the context.
     * \returns \c true if automatic markers are necessary.
     */
    virtual bool needsAutoMarkers() = 0;

    /**
     * \brief Called when presentation begins on the CPU timeline
     *
     * Must happen before acquiring an image from the presenter.
     * \param [in] frameId Current frame ID
     */
    virtual void notifyCpuPresentBegin(
            uint64_t                  frameId) = 0;

    /**
     * \brief Called when the CS thread reaches a given frame
     *
     * Should be recorded into the CS thread after completing
     * the previous frame on the application's CPU timeline.
     * \param [in] frameId Current frame ID
     */
    virtual void notifyCsRenderBegin(
            uint64_t                  frameId) = 0;

    /**
     * \brief Called when the CS thread completes a frame
     *
     * Should be recorded into the CS thread after recording
     * presentation commands for that frame.
     * \param [in] frameId Current frame ID
     */
    virtual void notifyCsRenderEnd(
            uint64_t                  frameId) = 0;

    /**
     * \brief Called when presentation ends on the CPU timeline
     *
     * Must happen after acquiring an image for presentation, but
     * before synchronizing with previous frames or performing
     * latency sleep. The intention is to measure acquire delays.
     * \param [in] frameId Current frame ID
     */
    virtual void notifyCpuPresentEnd(
            uint64_t                  frameId) = 0;

    /**
     * \brief Called when a command list is submitted to the GPU
     *
     * \param [in] frameId Associated frame ID
     */
    virtual void notifyQueueSubmit(
            uint64_t                  frameId) = 0;

    /**
     * \brief Called when a frame is queued for presentation
     *
     * \param [in] frameId Associated frame ID
     */
    virtual void notifyQueuePresentBegin(
            uint64_t                  frameId) = 0;

    /**
     * \brief Called after a frame has been queued for presentation
     *
     * \param [in] frameId Associated frame ID
     * \param [in] status Result of the present operation
     */
    virtual void notifyQueuePresentEnd(
            uint64_t                  frameId,
            VkResult                  status) = 0;

    /**
     * \brief Called when a submission begins execution on the GPU
     *
     * Any previous submissions will have completed by this time. This
     * can be used to measure GPU idle time throughout a frame.
     * \param [in] frameId Associated frame ID
     */
    virtual void notifyGpuExecutionBegin(
            uint64_t                  frameId) = 0;

    /**
     * \brief Called when a submission completes execution on the GPU
     *
     * The previous submission will have completed by the time this
     * gets called. This may be used to measure GPU idle time.
     * \param [in] frameId Associated frame ID
     */
    virtual void notifyGpuExecutionEnd(
            uint64_t                  frameId) = 0;

    /**
     * \brief Called when presentation of a given frame finishes on the GPU
     *
     * This is generally the last thing that happens within a frame.
     * \param [in] frameId Associated frame ID
     */
    virtual void notifyGpuPresentEnd(
            uint64_t                  frameId) = 0;

    /**
     * \brief Performs latency sleep and begins next frame
     *
     * Uses latency data from previous frames to estimate when to wake
     * up the application thread in order to minimize input latency.
     * \param [in] frameId Frame ID of the upcoming frame
     * \param [in] maxFrameRate Maximum frame rate or refresh rate
     */
    virtual void sleepAndBeginFrame(
            uint64_t                  frameId,
            double                    maxFrameRate) = 0;

    /**
     * \brief Discards all current timing data
     *
     * Should be called to reset latency tracking in case
     * presentation failed for any given frame.
     */
    virtual void discardTimings() = 0;

    /**
     * \brief Queries statistics for the given frame
     *
     * Returns statistics for the frame closest to \c frameId for
     * which data is available. If no such frame exists, the stat
     * counters will return 0.
     * \param [in] frameId Frame to query
     */
    virtual DxvkLatencyStats getStatistics(
            uint64_t                  frameId) = 0;

  private:

    std::atomic<uint64_t> m_refCount = { 0u };

  };

}
