#include "dxvk_latency_builtin_nv.h"

namespace dxvk {

  DxvkBuiltInLatencyTrackerNv::DxvkBuiltInLatencyTrackerNv(
    const Rc<Presenter>&            presenter)
  : m_presenter(presenter) {
    Logger::info("Latency control enabled, using VK_NV_low_latency2");
    auto limit = FpsLimiter::getEnvironmentOverride();

    if (limit)
      m_envFpsLimit = *limit;
  }


  DxvkBuiltInLatencyTrackerNv::~DxvkBuiltInLatencyTrackerNv() {
    VkLatencySleepModeInfoNV latencyMode = { VK_STRUCTURE_TYPE_LATENCY_SLEEP_MODE_INFO_NV };
    latencyMode.lowLatencyMode = VK_FALSE;
    latencyMode.lowLatencyBoost = VK_FALSE;
    latencyMode.minimumIntervalUs = 0;

    m_presenter->setLatencySleepModeNv(latencyMode);
  }


  void DxvkBuiltInLatencyTrackerNv::notifyCpuPresentBegin(
          uint64_t                  frameId) {
    // Not interesting here
  }


  void DxvkBuiltInLatencyTrackerNv::notifyCpuPresentEnd(
          uint64_t                  frameId) {
    std::unique_lock lock(m_mutex);
    auto frame = getFrame(frameId);

    if (frame)
      frame->presentPending = VK_TRUE;
  }


  void DxvkBuiltInLatencyTrackerNv::notifyCsRenderBegin(
          uint64_t                  frameId) {
    m_presenter->setLatencyMarkerNv(frameId,
      VK_LATENCY_MARKER_SIMULATION_END_NV);
    m_presenter->setLatencyMarkerNv(frameId,
      VK_LATENCY_MARKER_RENDERSUBMIT_START_NV);
  }


  void DxvkBuiltInLatencyTrackerNv::notifyCsRenderEnd(
          uint64_t                  frameId) {
    m_presenter->setLatencyMarkerNv(frameId,
      VK_LATENCY_MARKER_RENDERSUBMIT_END_NV);
  }


  void DxvkBuiltInLatencyTrackerNv::notifyQueueSubmit(
          uint64_t                  frameId) {
    // Handled by driver
  }


  void DxvkBuiltInLatencyTrackerNv::notifyQueuePresentBegin(
          uint64_t                  frameId) {
    m_presenter->setLatencyMarkerNv(frameId,
      VK_LATENCY_MARKER_PRESENT_START_NV);
  }


  void DxvkBuiltInLatencyTrackerNv::notifyQueuePresentEnd(
          uint64_t                  frameId,
          VkResult                  status) {
    m_presenter->setLatencyMarkerNv(frameId,
      VK_LATENCY_MARKER_PRESENT_END_NV);

    std::unique_lock lock(m_mutex);
    auto frame = getFrame(frameId);

    if (frame)
      frame->presentResult = status;

    m_cond.notify_one();
  }


  void DxvkBuiltInLatencyTrackerNv::notifyGpuExecutionBegin(
          uint64_t                  frameId) {
    // Handled by driver
  }


  void DxvkBuiltInLatencyTrackerNv::notifyGpuExecutionEnd(
          uint64_t                  frameId) {
    // Handled by driver
  }


  void DxvkBuiltInLatencyTrackerNv::notifyGpuPresentEnd(
          uint64_t                  frameId) {
    std::unique_lock lock(m_mutex);
    auto frame = getFrame(frameId);

    if (frame)
      frame->frameEnd = dxvk::high_resolution_clock::now();
  }


  void DxvkBuiltInLatencyTrackerNv::sleepAndBeginFrame(
          uint64_t                  frameId,
          double                    maxFrameRate) {
    bool presentSuccessful = false;

    duration sleepDuration(0u);

    { std::unique_lock lock(m_mutex);

      // Don't try to sleep if we haven't set up
      // low latency mode for the swapchain yet
      if (m_lowLatencyEnabled) {
        auto curr = getFrame(frameId - 1u);

        if (curr && curr->presentPending) {
          m_cond.wait(lock, [curr] {
            return curr->presentResult != VK_NOT_READY;
          });

          presentSuccessful = curr->presentResult >= 0;
        }
      }
    }

    if (presentSuccessful) {
      auto t0 = dxvk::high_resolution_clock::now();
      m_presenter->latencySleepNv(frameId - 1u);

      sleepDuration += dxvk::high_resolution_clock::now() - t0;
    }

    { std::unique_lock lock(m_mutex);
      // Set up low latency mode for subsequent frames
      VkLatencySleepModeInfoNV latencyMode = { VK_STRUCTURE_TYPE_LATENCY_SLEEP_MODE_INFO_NV };
      latencyMode.lowLatencyMode = VK_TRUE;
      latencyMode.lowLatencyBoost = VK_TRUE;
      latencyMode.minimumIntervalUs = 0;

      if (m_envFpsLimit > 0.0)
        maxFrameRate = m_envFpsLimit;

      if (maxFrameRate > 0.0)
        latencyMode.minimumIntervalUs = uint64_t(1'000'000.0 / maxFrameRate);

      m_presenter->setLatencySleepModeNv(latencyMode);
      m_presenter->setLatencyMarkerNv(frameId,
        VK_LATENCY_MARKER_INPUT_SAMPLE_NV);
      m_presenter->setLatencyMarkerNv(frameId,
        VK_LATENCY_MARKER_SIMULATION_START_NV);

      auto next = initFrame(frameId);
      next->frameStart = dxvk::high_resolution_clock::now();
      next->sleepDuration = sleepDuration;

      m_lowLatencyEnabled = true;
    }
  }


  void DxvkBuiltInLatencyTrackerNv::discardTimings() {
    std::unique_lock lock(m_mutex);
    m_lastDiscard = m_lastFrameId;
  }


  DxvkLatencyStats DxvkBuiltInLatencyTrackerNv::getStatistics(
          uint64_t                  frameId) {
    std::unique_lock lock(m_mutex);

    auto frame = getFrame(frameId);

    while (frame && frame->frameEnd == time_point())
      frame = getFrame(--frameId);

    if (!frame)
      return DxvkLatencyStats();

    DxvkLatencyStats stats = { };
    stats.frameLatency = std::chrono::duration_cast<std::chrono::microseconds>(frame->frameEnd - frame->frameStart);
    stats.sleepDuration = std::chrono::duration_cast<std::chrono::microseconds>(frame->sleepDuration);
    return stats;
  }


  DxvkLatencyFrameDataNv* DxvkBuiltInLatencyTrackerNv::initFrame(uint64_t frameId) {
    auto& frame = m_frames[frameId % FrameCount];

    frame = DxvkLatencyFrameDataNv();
    frame.frameId = frameId;

    m_lastFrameId = frameId;
    return &m_frames[frameId % FrameCount];
  }


  DxvkLatencyFrameDataNv* DxvkBuiltInLatencyTrackerNv::getFrame(uint64_t frameId) {
    auto& frame = m_frames[frameId % FrameCount];

    if (frameId <= m_lastDiscard || frame.frameId != frameId)
      return nullptr;

    return &frame;
  }

}
