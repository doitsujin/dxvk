#include "dxvk_latency_reflex.h"

namespace dxvk {

  DxvkReflexLatencyTrackerNv::DxvkReflexLatencyTrackerNv(
    const Rc<Presenter>&            presenter)
  : m_presenter(presenter) {

  }

  DxvkReflexLatencyTrackerNv::~DxvkReflexLatencyTrackerNv() {

  }


  bool DxvkReflexLatencyTrackerNv::needsAutoMarkers() {
    // In markerless mode we want to avoid submitting
    // any markers at all and ignore the context
    return false;
  }


  void DxvkReflexLatencyTrackerNv::notifyCpuPresentBegin(
          uint64_t                  frameId) {
    std::lock_guard lock(m_mutex);

    if (m_lastPresentAppFrameId) {
      uint64_t expectedFrameId = lookupFrameId(m_lastPresentAppFrameId);

      if (frameId != expectedFrameId) {
        // This is a normal occurence after a swapchain recreation, or if
        // tracking got reset for any reason. Remap the current app frame
        // to the current internal frame, and map any app frames with a
        // higher frame ID to subsequent frame IDs in order to fix the
        // mapping; we should catch up within a few frames.
        Logger::warn(str::format("Reflex: Expected internal frame ID ",
          expectedFrameId, " for ", m_lastPresentAppFrameId, ", got ", frameId));

        uint64_t nextAppFrameId = m_lastPresentAppFrameId;
        uint64_t nextDxvkFrameId = frameId;

        auto entry = m_appToDxvkFrameIds.find(nextAppFrameId);

        while (entry != m_appToDxvkFrameIds.end()) {
          nextAppFrameId = entry->first;

          mapFrameId(nextAppFrameId, nextDxvkFrameId++);

          entry = m_appToDxvkFrameIds.upper_bound(nextAppFrameId);
        }

        m_nextAllocFrameId = nextDxvkFrameId;
        m_nextValidFrameId = nextDxvkFrameId + 1u;
      }

      m_lowLatencyNoMarkers = false;
    } else if (m_lowLatencyMode) {
      // Game seemingly doesn't use markers?
      if (!m_lowLatencyNoMarkers) {
        Logger::warn("Reflex: No latency markers provided");
        m_lowLatencyNoMarkers = true;
        reset();
      }

      // Update sleep duration since we haven't had the chance yet
      auto& frame = getFrameData(frameId);
      frame.sleepDuration = m_lastSleepDuration;

      m_lastSleepDuration = duration(0u);
    }

    m_lastPresentAppFrameId = 0u;
  }


  void DxvkReflexLatencyTrackerNv::notifyCpuPresentEnd(
          uint64_t                  frameId) {
    std::lock_guard lock(m_mutex);
    m_lastPresentQueued = frameId;
  }


  void DxvkReflexLatencyTrackerNv::notifyCsRenderBegin(
          uint64_t                  frameId) {
    bool setMarker = false;

    { std::lock_guard lock(m_mutex);

      auto& frame = getFrameData(frameId);
      setMarker = frame.appFrameId && frameId >= m_nextValidFrameId;
    }

    if (setMarker)
      m_presenter->setLatencyMarkerNv(frameId, VK_LATENCY_MARKER_RENDERSUBMIT_START_NV);
  }


  void DxvkReflexLatencyTrackerNv::notifyCsRenderEnd(
          uint64_t                  frameId) {
    bool setMarker = false;

    { std::lock_guard lock(m_mutex);

      auto& frame = getFrameData(frameId);
      setMarker = frame.appFrameId && frameId >= m_nextValidFrameId;
    }

    if (setMarker)
      m_presenter->setLatencyMarkerNv(frameId, VK_LATENCY_MARKER_RENDERSUBMIT_END_NV);
  }


  void DxvkReflexLatencyTrackerNv::notifyQueueSubmit(
          uint64_t                  frameId) {
    std::lock_guard lock(m_mutex);
    auto& frame = getFrameData(frameId);

    if (frame.queueSubmit == time_point())
      frame.queueSubmit = dxvk::high_resolution_clock::now();
  }


  void DxvkReflexLatencyTrackerNv::notifyQueuePresentBegin(
          uint64_t                  frameId) {
    bool setMarker = false;

    { std::lock_guard lock(m_mutex);

      auto& frame = getFrameData(frameId);
      setMarker = frame.appFrameId && frameId >= m_nextValidFrameId;
    }

    if (setMarker)
      m_presenter->setLatencyMarkerNv(frameId, VK_LATENCY_MARKER_PRESENT_START_NV);
  }


  void DxvkReflexLatencyTrackerNv::notifyQueuePresentEnd(
          uint64_t                  frameId,
          VkResult                  status) {
    bool setMarker = false;

    { std::lock_guard lock(m_mutex);

      auto& frame = getFrameData(frameId);
      setMarker = frame.appFrameId && frameId >= m_nextValidFrameId;
    }

    time_point cpuTime = time_point();

    if (setMarker)
      cpuTime = m_presenter->setLatencyMarkerNv(frameId, VK_LATENCY_MARKER_PRESENT_END_NV);

    std::lock_guard lock(m_mutex);

    if (setMarker) {
      auto& frame = getFrameData(frameId);
      frame.presentStatus = status;
      frame.queuePresent = cpuTime;
    }

    // Ignore errors or we might never wake up a waiting thread
    m_lastPresentComplete = frameId;
    m_cond.notify_all();
  }


  void DxvkReflexLatencyTrackerNv::notifyGpuExecutionBegin(
          uint64_t                  frameId) {
    std::lock_guard lock(m_mutex);
    auto now = dxvk::high_resolution_clock::now();

    auto& frame = getFrameData(frameId);
    frame.gpuIdleEnd = now;

    if (frame.gpuExecStart == time_point())
      frame.gpuExecStart = now;

    if (frame.gpuIdleStart != time_point())
      frame.gpuIdleTime += frame.gpuIdleEnd - frame.gpuIdleStart;
  }


  void DxvkReflexLatencyTrackerNv::notifyGpuExecutionEnd(
          uint64_t                  frameId) {
    std::lock_guard lock(m_mutex);
    auto now = dxvk::high_resolution_clock::now();

    auto& frame = getFrameData(frameId);
    frame.gpuExecEnd = now;
    frame.gpuIdleStart = now;
  }


  void DxvkReflexLatencyTrackerNv::notifyGpuPresentEnd(
          uint64_t                  frameId) {
    std::lock_guard lock(m_mutex);

    auto& frame = getFrameData(frameId);
    frame.frameEnd = dxvk::high_resolution_clock::now();

    m_lastCompletedFrameId = frameId;
  }


  void DxvkReflexLatencyTrackerNv::sleepAndBeginFrame(
          uint64_t                  frameId,
          double                    maxFrameRate) {
    std::lock_guard lock(m_mutex);
    m_lastNoMarkerFrameId = frameId;

    if (m_lowLatencyMode) {
      auto& frame = getFrameData(frameId);
      frame.frameStart = dxvk::high_resolution_clock::now();
    }
  }


  void DxvkReflexLatencyTrackerNv::discardTimings() {
    std::lock_guard lock(m_mutex);
    reset();
  }


  DxvkLatencyStats DxvkReflexLatencyTrackerNv::getStatistics(
          uint64_t                  frameId) {
    std::lock_guard lock(m_mutex);

    if (!m_lastCompletedFrameId)
      return DxvkLatencyStats();

    auto& frame = getFrameData(m_lastCompletedFrameId);

    if (frame.frameEnd == time_point())
      return DxvkLatencyStats();

    time_point frameStart = frame.cpuSimBegin;

    if (frame.cpuInputSample != time_point())
      frameStart = frame.cpuInputSample;

    if (frameStart == time_point())
      frameStart = frame.frameStart;

    if (frameStart == time_point())
      return DxvkLatencyStats();

    DxvkLatencyStats stats = { };
    stats.frameLatency = std::chrono::duration_cast<std::chrono::microseconds>(frame.frameEnd - frameStart);
    stats.sleepDuration = std::chrono::duration_cast<std::chrono::microseconds>(frame.sleepDuration);
    return stats;
  }


  void DxvkReflexLatencyTrackerNv::setLatencySleepMode(
          bool                      enableLowLatency,
          bool                      enableBoost,
          uint64_t                  minIntervalUs) {
    if (m_lowLatencyMode != enableLowLatency)
      Logger::info(str::format("Reflex: Low latency mode ", enableLowLatency ? "enabled" : "disabled"));

    VkLatencySleepModeInfoNV modeInfo = { VK_STRUCTURE_TYPE_LATENCY_SLEEP_MODE_INFO_NV };
    modeInfo.lowLatencyMode = enableLowLatency;
    modeInfo.lowLatencyBoost = enableBoost;
    modeInfo.minimumIntervalUs = minIntervalUs;

    m_presenter->setLatencySleepModeNv(modeInfo);

    m_lowLatencyMode = enableLowLatency;
  }


  void DxvkReflexLatencyTrackerNv::setLatencyMarker(
          uint64_t                  appFrameId,
          VkLatencyMarkerNV         marker) {
    std::unique_lock lock(m_mutex);

    // Find frame ID. If this is the first marker in a new frame,
    // try to map it to a new internal frame ID.
    uint64_t frameId = lookupFrameId(appFrameId);

    if (!frameId && (marker == VK_LATENCY_MARKER_SIMULATION_START_NV
                  || marker == VK_LATENCY_MARKER_INPUT_SAMPLE_NV))
      frameId = allocateFrameId(appFrameId);

    // This can hapen if we reset tracking state and receive
    // a stray present or render submit marker. Ignore these
    // so that the next presents can recalibrate properly.
    if (!frameId)
      return;

    // We use present markers to correlate app frame IDs
    // with internal frame IDs, so always write this back.
    if (marker == VK_LATENCY_MARKER_PRESENT_START_NV)
      m_lastPresentAppFrameId = appFrameId;

    // Don't submit markers for invalid frames since
    // that could potentially confuse the algorithm
    if (frameId < m_nextValidFrameId)
      return;

    // Need to unlock here so we don't deadlock with the presenter
    auto cpuTime = dxvk::high_resolution_clock::now();

    if (marker == VK_LATENCY_MARKER_INPUT_SAMPLE_NV
     || marker == VK_LATENCY_MARKER_SIMULATION_START_NV
     || marker == VK_LATENCY_MARKER_SIMULATION_END_NV) {
      lock.unlock();

      cpuTime = m_presenter->setLatencyMarkerNv(frameId, marker);

      lock.lock();
    }

    // Store CPU timestamp to correlate times
    auto& frame = getFrameData(frameId);

    switch (marker) {
      case VK_LATENCY_MARKER_INPUT_SAMPLE_NV:
        frame.cpuInputSample = cpuTime;
        break;

      case VK_LATENCY_MARKER_SIMULATION_START_NV:
        frame.cpuSimBegin = cpuTime;

        if (m_lastSleepDuration != duration(0u))
          frame.sleepDuration = std::exchange(m_lastSleepDuration, duration(0u));
        break;

      case VK_LATENCY_MARKER_SIMULATION_END_NV:
        frame.cpuSimEnd = cpuTime;
        break;

      case VK_LATENCY_MARKER_RENDERSUBMIT_START_NV:
        frame.cpuRenderBegin = cpuTime;
        break;

      case VK_LATENCY_MARKER_RENDERSUBMIT_END_NV:
        frame.cpuRenderEnd = cpuTime;
        break;

      case VK_LATENCY_MARKER_PRESENT_START_NV:
        frame.cpuPresentBegin = cpuTime;
        break;

      case VK_LATENCY_MARKER_PRESENT_END_NV:
        frame.cpuPresentEnd = cpuTime;
        break;

      default:
        Logger::warn(str::format("Reflex: Unknown marker ", marker));
    }
  }


  void DxvkReflexLatencyTrackerNv::latencySleep() {
    { std::unique_lock lock(m_mutex);
      // If the app doesn't use markers, wait for the previous present
      // call to complete so that we don't confuse the algorithm by
      // sleeping at random times relative to actual graphics work.
      if (m_lowLatencyNoMarkers) {
        m_cond.wait(lock, [this] {
          return m_lastPresentComplete >= m_lastPresentQueued;
        });
      }
    }

    // Actually sleep and write back sleep duration for the next frame
    auto sleepDuration = m_presenter->latencySleepNv();

    std::lock_guard lock(m_mutex);
    m_lastSleepAppFrameId = m_lastBeginAppFrameId;
    m_lastSleepDuration = sleepDuration;

    if (m_lowLatencyNoMarkers && m_lastNoMarkerFrameId > m_lastPresentQueued) {
      // In markerless mode, assume that this gets called before any
      // work is done for the next frame and update the frame start
      // time accordingly.
      auto& frame = getFrameData(m_lastNoMarkerFrameId);
      frame.frameStart = dxvk::high_resolution_clock::now();
    }
  }


  uint32_t DxvkReflexLatencyTrackerNv::getFrameReports(
          uint32_t                  maxCount,
          DxvkReflexFrameReport*    reports) {
    small_vector<VkLatencyTimingsFrameReportNV, 64> nvReports(maxCount);

    for (uint32_t i = 0; i < maxCount; i++)
      nvReports[i] = { VK_STRUCTURE_TYPE_LATENCY_TIMINGS_FRAME_REPORT_NV };

    // Adjust some statistics so that we actually return the
    // correct timestamps for the application-defined markers
    uint32_t count = m_presenter->getLatencyTimingsNv(maxCount, nvReports.data());

    // Only lock after calling into the presenter to avoid deadlocks
    std::lock_guard lock(m_mutex);

    for (uint32_t i = 0; i < count; i++) {
      auto& report = nvReports[i];
      const auto& currFrame = m_frames[report.presentID % FrameCount];

      if (report.presentID != currFrame.frameId || report.presentID < m_nextValidFrameId)
        return 0;

      report.presentID = currFrame.appFrameId;

      // These represent when the CS thread starts processing the frame
      report.driverStartTimeUs = report.renderSubmitStartTimeUs;
      report.driverEndTimeUs = report.renderSubmitEndTimeUs;

      // Return when the app set these markers rather than the time when
      // we forward them to the driver
      report.renderSubmitStartTimeUs = mapFrameTimestampToReportUs(currFrame, report, currFrame.cpuRenderBegin);
      report.renderSubmitEndTimeUs = mapFrameTimestampToReportUs(currFrame, report, currFrame.cpuRenderEnd);
      report.presentStartTimeUs = mapFrameTimestampToReportUs(currFrame, report, currFrame.cpuPresentBegin);
      report.presentEndTimeUs = mapFrameTimestampToReportUs(currFrame, report, currFrame.cpuPresentEnd);

      // Documentation for the OS timers seems nonsensical, but it seems to
      // be the time from the the first submission to the end of the frame
      report.osRenderQueueStartTimeUs = mapFrameTimestampToReportUs(currFrame, report, currFrame.queueSubmit);
      report.osRenderQueueEndTimeUs = report.gpuRenderEndTimeUs;

      // Apparently gpuRenderEndTime is when presentation completes rather
      // than rendering, so we need to compute the active render time using
      // our own timestamps
      auto gpuActiveTime = currFrame.gpuExecEnd - currFrame.gpuExecStart - currFrame.gpuIdleTime;

      reports[i].report = report;
      reports[i].gpuActiveTimeUs = std::max<uint64_t>(0u,
        std::chrono::duration_cast<std::chrono::microseconds>(gpuActiveTime).count());
    }

    return count;
  }


  uint64_t DxvkReflexLatencyTrackerNv::frameIdFromAppFrameId(
          uint64_t                  appFrameId) {
    std::lock_guard lock(m_mutex);
    return lookupFrameId(appFrameId);
  }


  DxvkReflexLatencyFrameData& DxvkReflexLatencyTrackerNv::getFrameData(
          uint64_t                  dxvkFrameId) {
    auto& frameData = m_frames[dxvkFrameId % FrameCount];

    if (frameData.frameId != dxvkFrameId) {
      m_appToDxvkFrameIds.erase(frameData.appFrameId);

      frameData = DxvkReflexLatencyFrameData();
      frameData.frameId = dxvkFrameId;
    }

    return frameData;
  }


  uint64_t DxvkReflexLatencyTrackerNv::lookupFrameId(
          uint64_t                  appFrameId) {
    auto entry = m_appToDxvkFrameIds.find(appFrameId);

    if (entry == m_appToDxvkFrameIds.end())
      return 0u;

    return entry->second;
  }


  uint64_t DxvkReflexLatencyTrackerNv::allocateFrameId(
          uint64_t                  appFrameId) {
    if (appFrameId <= m_lastBeginAppFrameId) {
      Logger::warn(str::format("Reflex: Frame ID ", appFrameId, " not monotonic, last was ", m_lastBeginAppFrameId));
      reset();
    }

    uint64_t frameId = m_nextAllocFrameId++;
    mapFrameId(appFrameId, frameId);

    m_lastBeginAppFrameId = appFrameId;
    return frameId;
  }


  void DxvkReflexLatencyTrackerNv::mapFrameId(
          uint64_t                  appFrameId,
          uint64_t                  dxvkFrameId) {
    while (m_appToDxvkFrameIds.size() > FrameCount)
      m_appToDxvkFrameIds.erase(m_appToDxvkFrameIds.begin());

    m_appToDxvkFrameIds.insert_or_assign(appFrameId, dxvkFrameId);
    getFrameData(dxvkFrameId).appFrameId = appFrameId;
  }


  void DxvkReflexLatencyTrackerNv::reset() {
    m_nextValidFrameId = uint64_t(-1);

    m_lastSleepDuration = duration(0u);

    m_lastBeginAppFrameId = 0u;
    m_lastPresentAppFrameId = 0u;

    for (size_t i = 0; i < FrameCount; i++)
      m_frames[i].appFrameId = 0u;

    m_appToDxvkFrameIds.clear();
  }


  uint64_t DxvkReflexLatencyTrackerNv::mapFrameTimestampToReportUs(
    const DxvkReflexLatencyFrameData&     frame,
    const VkLatencyTimingsFrameReportNV&  report,
          time_point                      timestamp) {
    if (frame.cpuSimBegin == time_point() || !report.simStartTimeUs)
      return 0u;

    int64_t diffUs = std::chrono::duration_cast<std::chrono::microseconds>(timestamp - frame.cpuSimBegin).count();
    return report.simStartTimeUs + diffUs;
  }

}
