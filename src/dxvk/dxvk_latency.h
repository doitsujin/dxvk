#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>

#include "../util/thread.h"

#include "../util/config/config.h"

#include "../util/rc/util_rc_ptr.h"

#include "../util/util_likely.h"
#include "../util/util_sleep.h"
#include "../util/util_time.h"

#include "../vulkan/vulkan_loader.h"

namespace dxvk {

  class Presenter;

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
    time_point  csRenderBegin   = time_point();
    time_point  csRenderEnd     = time_point();
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
   * \brief Additional frame report info
   */
  struct DxvkReflexFrameReport {
    VkLatencyTimingsFrameReportNV report;
    uint64_t gpuActiveTimeUs;
  };


  /**
   * \brief Latency tracker
   *
   * Accumulates time stamps of certain parts of a frame.
   */
  class DxvkLatencyTracker {
    using time_point = typename DxvkLatencyFrameData::time_point;
    using duration = typename DxvkLatencyFrameData::duration;

    // Keep data for a large number of frames around to support
    // retrieving statistics from the driver properly.
    constexpr static size_t FrameCount = 256u;
  public:

    DxvkLatencyTracker(
      const Rc<DxvkDevice>&           device,
      const Rc<Presenter>&            presenter,
            Tristate                  lowLatencyMode);

    ~DxvkLatencyTracker();

    /**
     * \brief Increments ref count
     */
    void incRef() {
      m_refCount.fetch_add(1);
    }

    /**
     * \brief Decrements ref count
     *
     * Destroys the object when there are no users left.
     */
    void decRef() {
      if (m_refCount.fetch_sub(1) == 1u)
        delete this;
    }

    /**
     * \brief Checks whether automatic markers are needed
     *
     * Relevant for forwarding the latency tracker to the context.
     * \returns \c true if automatic markers are necessary.
     */
    bool needsAutoMarkers();

    /**
     * \brief Called when presentation begins on the CPU timeline
     *
     * Must happen before acquiring an image from the presenter.
     * \param [in] frameId Current frame ID
     */
    void notifyCpuPresentBegin(
            uint64_t                  frameId);

    /**
     * \brief Called when the CS thread reaches a given frame
     *
     * Should be recorded into the CS thread after completing
     * the previous frame on the application's CPU timeline.
     * \param [in] frameId Current frame ID
     */
    void notifyCsRenderBegin(
            uint64_t                  frameId);

    /**
     * \brief Called when the CS thread completes a frame
     *
     * Should be recorded into the CS thread after recording
     * presentation commands for that frame.
     * \param [in] frameId Current frame ID
     */
    void notifyCsRenderEnd(
            uint64_t                  frameId);

    /**
     * \brief Called when presentation ends on the CPU timeline
     *
     * Must happen after acquiring an image for presentation, but
     * before synchronizing with previous frames or performing
     * latency sleep. The intention is to measure acquire delays.
     * \param [in] frameId Current frame ID
     */
    void notifyCpuPresentEnd(
            uint64_t                  frameId);

    /**
     * \brief Called when a command list is submitted to the GPU
     *
     * \param [in] frameId Associated frame ID
     */
    void notifyQueueSubmit(
            uint64_t                  frameId);

    /**
     * \brief Called when a frame is queued for presentation
     *
     * \param [in] frameId Associated frame ID
     */
    void notifyQueuePresentBegin(
            uint64_t                  frameId);

    /**
     * \brief Called after a frame has been queued for presentation
     *
     * \param [in] frameId Associated frame ID
     * \param [in] status Result of the present operation
     */
    void notifyQueuePresentEnd(
            uint64_t                  frameId,
            VkResult                  status);

    /**
     * \brief Called when a submission begins execution on the GPU
     *
     * Any previous submissions will have completed by this time. This
     * can be used to measure GPU idle time throughout a frame.
     * \param [in] frameId Associated frame ID
     */
    void notifyGpuExecutionBegin(
            uint64_t                  frameId);

    /**
     * \brief Called when a submission completes execution on the GPU
     *
     * The previous submission will have completed by the time this
     * gets called. This may be used to measure GPU idle time.
     * \param [in] frameId Associated frame ID
     */
    void notifyGpuExecutionEnd(
            uint64_t                  frameId);

    /**
     * \brief Called when presentation of a given frame finishes on the GPU
     *
     * This is generally the last thing that happens within a frame.
     * \param [in] frameId Associated frame ID
     */
    void notifyGpuPresentEnd(
            uint64_t                  frameId);

    /**
     * \brief Performs latency sleep and begins next frame
     *
     * Uses latency data from previous frames to estimate when to wake
     * up the application thread in order to minimize input latency.
     * \param [in] frameId Frame ID of the upcoming frame
     */
    void sleepAndBeginFrame(
            uint64_t                  frameId);

    /**
     * \brief Discards all current timing data
     *
     * Should be called to reset latency tracking in case
     * presentation failed for any given frame.
     */
    void discardTimings();

    /**
     * \brief Queries statistics for the given frame
     *
     * Returns statistics for the frame closest to \c frameId for
     * which data is available. If no such frame exists, the stat
     * counters will return 0.
     * \param [in] frameId Frame to query
     */
    DxvkLatencyStats getStatistics(
            uint64_t                  frameId);

    /**
     * \brief Sets Reflex state
     *
     * \param [in] enableLowLatency Whether to enable latency control
     * \param [in] enableBoost Whether to enable boost
     * \param [in] minIntervalUs Minimum frame interval
     */
    void setLatencySleepMode(
            bool                      enableLowLatency,
            bool                      enableBoost,
            uint64_t                  minIntervalUs);

    /**
     * \brief Sets latency marker from application
     *
     * \param [in] appFrameId Application-provided frame ID
     * \param [in] marker Marker to set
     */
    void setLatencyMarker(
            uint64_t                  appFrameId,
            VkLatencyMarkerNV         marker);

    /**
     * \brief Performs latency sleep
     */
    void latencySleep();

    /**
     * \brief Retrieves frame reports
     *
     * \param [in] maxCount Maximum number of reports
     * \param [out] reports Frame reports
     * \returns Number of reports retrieved
     */
    uint32_t getFrameReports(
            uint32_t                  maxCount,
            DxvkReflexFrameReport*    reports);

    /**
     * \brief Looks up frame ID from application frame ID
     *
     * \param [in] appFrameId Application-provided frame ID
     * \returns Internal frame ID, or 0 if none was found
     */
    uint64_t frameIdFromAppFrameId(
            uint64_t                  appFrameId);

  private:

    std::atomic<uint64_t> m_refCount = { 0u };

    Rc<DxvkDevice>            m_device;
    Rc<Presenter>             m_presenter;

    dxvk::mutex               m_mutex;
    dxvk::condition_variable  m_cond;

    uint64_t                  m_lastBeginAppFrameId = 0u;
    uint64_t                  m_lastSleepAppFrameId = 0u;
    uint64_t                  m_lastPresentAppFrameId = 0u;

    uint64_t                  m_nextAllocFrameId = 1u;
    uint64_t                  m_nextValidFrameId = uint64_t(-1);

    uint64_t                  m_lastCompletedFrameId = 0u;

    uint64_t                  m_lastPresentQueued   = 0u;
    uint64_t                  m_lastPresentComplete = 0u;

    uint64_t                  m_lastNoMarkerFrameId = 0u;

    duration                  m_lastSleepDuration = duration(0u);

    VkLatencySleepModeInfoNV  m_lowLatencyMode = { VK_STRUCTURE_TYPE_LATENCY_SLEEP_MODE_INFO_NV };
    bool                      m_lowLatencyNoMarkers = false;

    std::array<DxvkLatencyFrameData, FrameCount> m_frames = { };

    std::map<uint64_t, uint64_t> m_appToDxvkFrameIds;

    Tristate                  m_setting = Tristate::Auto;

    DxvkLatencyFrameData& lookupFrameData(
            uint64_t                  dxvkFrameId);

    DxvkLatencyFrameData& getFrameData(
            uint64_t                  dxvkFrameId);

    uint64_t lookupFrameId(
            uint64_t                  appFrameId);

    uint64_t allocateFrameId(
            uint64_t                  appFrameId);

    void mapFrameId(
            uint64_t                  appFrameId,
            uint64_t                  dxvkFrameId);

    void reset();

    time_point forwardLatencyMarker(
            uint64_t                  dxvkFrameId,
            VkLatencyMarkerNV         marker);

    duration performSleep(
            std::unique_lock<dxvk::mutex>& lock,
            uint64_t                  dxvkFrameId);

    static uint64_t timeToUs(time_point timestamp);

    static duration estimateTime(
      const duration*                 frames,
            size_t                    frameCount);

  };

}
