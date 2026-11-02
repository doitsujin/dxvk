#include "dxvk_device.h"
#include "dxvk_latency_reflex.h"

namespace dxvk {

  DxvkReflexLatencyTrackerNv::DxvkReflexLatencyTrackerNv(
    const Rc<DxvkDevice>&           device,
    const Rc<Presenter>&            presenter,
          Tristate                  lowLatencyMode)
  : m_device(device), m_presenter(presenter), m_setting(lowLatencyMode) {

  }

  DxvkReflexLatencyTrackerNv::~DxvkReflexLatencyTrackerNv() {

  }


  bool DxvkReflexLatencyTrackerNv::needsAutoMarkers() {
    // Submit auto-markers if explicit low-latency mode is disabled.
    // In markerless mode we want to avoid submitting any markers at
    // all and ignore the context
    std::lock_guard lock(m_mutex);
    return !m_lowLatencyMode.lowLatencyMode;
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
    } else if (m_lowLatencyMode.lowLatencyMode) {
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

      if (frame.csRenderBegin == time_point())
        frame.csRenderBegin = dxvk::high_resolution_clock::now();
    }

    if (setMarker)
      forwardLatencyMarker(frameId, VK_LATENCY_MARKER_RENDERSUBMIT_START_NV);
  }


  void DxvkReflexLatencyTrackerNv::notifyCsRenderEnd(
          uint64_t                  frameId) {
    bool setMarker = false;

    { std::lock_guard lock(m_mutex);

      auto& frame = getFrameData(frameId);
      setMarker = frame.appFrameId && frameId >= m_nextValidFrameId;

      if (frame.csRenderEnd == time_point())
        frame.csRenderEnd = dxvk::high_resolution_clock::now();
    }

    if (setMarker)
      forwardLatencyMarker(frameId, VK_LATENCY_MARKER_RENDERSUBMIT_END_NV);
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
      forwardLatencyMarker(frameId, VK_LATENCY_MARKER_PRESENT_START_NV);
  }


  void DxvkReflexLatencyTrackerNv::notifyQueuePresentEnd(
          uint64_t                  frameId,
          VkResult                  status) {
    bool setMarker = false;

    { std::lock_guard lock(m_mutex);

      auto& frame = getFrameData(frameId);
      setMarker = frame.appFrameId && frameId >= m_nextValidFrameId;
    }

    time_point cpuTime = dxvk::high_resolution_clock::now();

    if (setMarker)
      cpuTime = forwardLatencyMarker(frameId, VK_LATENCY_MARKER_PRESENT_END_NV);

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
    std::unique_lock lock(m_mutex);

    // Only sleep if the app doesn't explicitly do Reflex and if
    // latency sleep is explicitly enabled via the configuration
    if (!m_lowLatencyMode.lowLatencyMode && m_setting == Tristate::True) {
      auto sleepDuration = performSleep(lock, frameId);

      m_lastSleepAppFrameId = m_lastBeginAppFrameId;
      m_lastSleepDuration = sleepDuration;
    }

    m_lastNoMarkerFrameId = frameId;

    if (m_lowLatencyMode.lowLatencyMode) {
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
    if (m_setting == Tristate::False)
      return;

    std::unique_lock lock(m_mutex);

    if (m_lowLatencyMode.lowLatencyMode != enableLowLatency)
      Logger::info(str::format("Reflex: Low latency mode ", enableLowLatency ? "enabled" : "disabled"));

    m_lowLatencyMode.lowLatencyMode = enableLowLatency;
    m_lowLatencyMode.lowLatencyBoost = enableBoost;
    m_lowLatencyMode.minimumIntervalUs = minIntervalUs;

    if (m_device->features().nvLowLatency2)
      m_presenter->setLatencySleepModeNv(m_lowLatencyMode);
  }


  void DxvkReflexLatencyTrackerNv::setLatencyMarker(
          uint64_t                  appFrameId,
          VkLatencyMarkerNV         marker) {
    if (m_setting == Tristate::False)
      return;

    std::unique_lock lock(m_mutex);

    // Find frame ID. If this is the first marker in a new frame,
    // try to map it to a new internal frame ID.
    uint64_t frameId = lookupFrameId(appFrameId);

    bool isStartMarker = marker == VK_LATENCY_MARKER_SIMULATION_START_NV
                      || marker == VK_LATENCY_MARKER_RENDERSUBMIT_START_NV
                      || marker == VK_LATENCY_MARKER_INPUT_SAMPLE_NV;

    if (!frameId && isStartMarker)
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
     || marker == VK_LATENCY_MARKER_SIMULATION_END_NV
     || marker == VK_LATENCY_MARKER_TRIGGER_FLASH_NV) {
      lock.unlock();

      cpuTime = forwardLatencyMarker(frameId, marker);

      lock.lock();
    }

    // Store CPU timestamp to correlate times
    auto& frame = getFrameData(frameId);

    if (isStartMarker && m_lastSleepDuration != duration(0u))
      frame.sleepDuration = std::exchange(m_lastSleepDuration, duration(0u));

    switch (marker) {
      case VK_LATENCY_MARKER_INPUT_SAMPLE_NV:
        frame.cpuInputSample = cpuTime;
        break;

      case VK_LATENCY_MARKER_SIMULATION_START_NV:
        frame.cpuSimBegin = cpuTime;
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

      case VK_LATENCY_MARKER_TRIGGER_FLASH_NV:
        // Nothing to do here
        break;

      default:
        Logger::warn(str::format("Reflex: Unknown marker ", marker));
    }
  }


  void DxvkReflexLatencyTrackerNv::latencySleep() {
    if (m_setting == Tristate::False)
      return;

    std::unique_lock lock(m_mutex);

    // If the app doesn't use markers, wait for the previous present
    // call to complete so that we don't confuse the algorithm by
    // sleeping at random times relative to actual graphics work.
    uint64_t frameId = lookupFrameId(m_lastBeginAppFrameId) + 1u;

    if (m_lowLatencyNoMarkers) {
      m_cond.wait(lock, [this] {
        return m_lastPresentComplete >= m_lastPresentQueued;
      });

      frameId = m_lastPresentComplete + 1u;
    }

    // Actually sleep and write back sleep duration for the next frame
    auto sleepDuration = performSleep(lock, frameId);

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
    // Only lock after calling into the presenter to avoid deadlocks
    std::lock_guard lock(m_mutex);

    for (uint32_t i = 0; i < maxCount; i++) {
      uint64_t frameId = m_lastCompletedFrameId - i;

      const auto& currFrame = m_frames[frameId % FrameCount];

      if (frameId != currFrame.frameId || frameId < m_nextValidFrameId)
        return i;

      auto& report = reports[i].report;
      report.presentID = currFrame.appFrameId;

      // Return when the app set these markers rather than the time when
      // we forward them to the driver
      report.inputSampleTimeUs = timeToUs(currFrame.cpuInputSample);
      report.simStartTimeUs = timeToUs(currFrame.cpuSimBegin);
      report.simEndTimeUs = timeToUs(currFrame.cpuSimEnd);
      report.renderSubmitStartTimeUs = timeToUs(currFrame.cpuRenderBegin);
      report.renderSubmitEndTimeUs = timeToUs(currFrame.cpuRenderEnd);
      report.presentStartTimeUs = timeToUs(currFrame.cpuPresentBegin);
      report.presentEndTimeUs = timeToUs(currFrame.cpuPresentEnd);

      // These represent when the CS thread starts processing the frame
      report.driverStartTimeUs = timeToUs(currFrame.csRenderBegin);
      report.driverEndTimeUs = timeToUs(currFrame.csRenderEnd);

      // Documentation for the OS timers seems nonsensical, but it seems to
      // be the time from the the first submission to the end of the frame
      report.osRenderQueueStartTimeUs = timeToUs(currFrame.queueSubmit);
      report.osRenderQueueEndTimeUs = timeToUs(currFrame.gpuExecEnd);

      // Apparently gpuRenderEndTime is when presentation completes rather
      // than rendering somehow....
      report.gpuRenderStartTimeUs = timeToUs(currFrame.gpuExecStart);
      report.gpuRenderEndTimeUs = timeToUs(currFrame.frameEnd);

      auto gpuActiveTime = currFrame.gpuExecEnd - currFrame.gpuExecStart - currFrame.gpuIdleTime;

      reports[i].gpuActiveTimeUs = std::max<uint64_t>(0u,
        std::chrono::duration_cast<std::chrono::microseconds>(gpuActiveTime).count());
    }

    return maxCount;
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


  DxvkReflexLatencyTrackerNv::time_point DxvkReflexLatencyTrackerNv::forwardLatencyMarker(
          uint64_t                  dxvkFrameId,
          VkLatencyMarkerNV         marker) {
    if (m_device->features().nvLowLatency2) {
      if (m_setting == Tristate::True || m_lowLatencyMode.lowLatencyMode)
        return m_presenter->setLatencyMarkerNv(dxvkFrameId, marker);
    }

    return dxvk::high_resolution_clock::now();
  }


  void DxvkReflexLatencyTrackerNv::performSleep(
          std::unique_lock<dxvk::mutex>& lock,
          uint64_t                  dxvkFrameId) {
    if (m_device->features().nvLowLatency2) {
      lock.unlock();
      m_presenter->latencySleepNv();
      lock.lock();
    }
  }


  uint64_t DxvkReflexLatencyTrackerNv::timeToUs(time_point timestamp) {
    return std::chrono::duration_cast<std::chrono::microseconds>(timestamp.time_since_epoch()).count();
  }

}
