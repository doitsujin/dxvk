#pragma once

#include <array>
#include <map>

#include "dxvk_latency.h"
#include "dxvk_presenter.h"

#include "../util/thread.h"

#include "../util/util_sleep.h"
#include "../util/util_time.h"

#include "../util/config/config.h"

#include "../util/sync/sync_spinlock.h"

namespace dxvk {

  /**
   * \brief Reflex frame info
   *
   * Stores frame ID mapping and all sorts of time stamps
   * that are used for latency sleep or frame reports.
   */
  using DxvkReflexLatencyFrameData = DxvkLatencyFrameData;


  /**
   * \brief Additional frame report info
   */
  struct DxvkReflexFrameReport {
    VkLatencyTimingsFrameReportNV report;
    uint64_t gpuActiveTimeUs;
  };


  /**
   * \brief Built-in latency tracker based on VK_NV_low_latency2
   *
   * Implements a simple latency reduction algorithm
   * based on CPU timestamps received from the backend.
   */
  class DxvkReflexLatencyTrackerNv : public DxvkLatencyTracker {
    using time_point = typename DxvkReflexLatencyFrameData::time_point;
    using duration = typename DxvkReflexLatencyFrameData::duration;

    // Keep data for a large number of frames around to support
    // retrieving statistics from the driver properly.
    constexpr static size_t FrameCount = 256u;
  public:

    DxvkReflexLatencyTrackerNv(
      const Rc<Presenter>&            presenter);

    ~DxvkReflexLatencyTrackerNv();

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

    bool                      m_lowLatencyMode      = false;
    bool                      m_lowLatencyNoMarkers = false;

    std::array<DxvkReflexLatencyFrameData, FrameCount> m_frames = { };

    std::map<uint64_t, uint64_t> m_appToDxvkFrameIds;

    DxvkReflexLatencyFrameData& getFrameData(
            uint64_t                  dxvkFrameId);

    uint64_t lookupFrameId(
            uint64_t                  appFrameId);

    uint64_t allocateFrameId(
            uint64_t                  appFrameId);

    void mapFrameId(
            uint64_t                  appFrameId,
            uint64_t                  dxvkFrameId);

    void reset();

    static uint64_t mapFrameTimestampToReportUs(
      const DxvkReflexLatencyFrameData&     frame,
      const VkLatencyTimingsFrameReportNV&  report,
            time_point                      timestamp);

  };

}
