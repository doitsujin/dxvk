#include <cmath>

#include "dxvk_latency_builtin.h"

#include "../util/log/log.h"

#include "../util/util_fps_limiter.h"
#include "../util/util_string.h"

namespace dxvk {

  DxvkBuiltInLatencyTracker::DxvkBuiltInLatencyTracker(
          Rc<Presenter>             presenter,
          int32_t                   toleranceUs,
          bool                      useNvLowLatency2)
  : m_presenter(std::move(presenter)),
    m_tolerance(std::chrono::duration_cast<duration>(
      std::chrono::microseconds(std::max(toleranceUs, 0)))),
    m_useNvLowLatency2(useNvLowLatency2) {
    Logger::info(str::format("Latency control enabled, using ",
      useNvLowLatency2 ? "VK_NV_low_latency2" : "built-in algorithm"));

    auto limit = FpsLimiter::getEnvironmentOverride();

    if (limit)
      m_envFpsLimit = *limit;
  }


  DxvkBuiltInLatencyTracker::~DxvkBuiltInLatencyTracker() {

  }


  bool DxvkBuiltInLatencyTracker::needsAutoMarkers() {
    return true;
  }


  void DxvkBuiltInLatencyTracker::notifyCpuPresentBegin(
          uint64_t                  frameId) {
    std::unique_lock lock(m_mutex);
    auto frame = findFrame(frameId);

    if (frame)
      frame->cpuPresentBegin = dxvk::high_resolution_clock::now();
  }


  void DxvkBuiltInLatencyTracker::notifyCpuPresentEnd(
          uint64_t                  frameId) {
    std::unique_lock lock(m_mutex);
    auto frame = findFrame(frameId);

    if (frame)
      frame->cpuPresentEnd = dxvk::high_resolution_clock::now();
  }


  void DxvkBuiltInLatencyTracker::notifyCsRenderBegin(
          uint64_t                  frameId) {
    if (forwardLatencyMarkerNv(frameId)) {
      m_presenter->setLatencyMarkerNv(frameId, VK_LATENCY_MARKER_SIMULATION_END_NV);
      m_presenter->setLatencyMarkerNv(frameId, VK_LATENCY_MARKER_RENDERSUBMIT_START_NV);
    }
  }


  void DxvkBuiltInLatencyTracker::notifyCsRenderEnd(
          uint64_t                  frameId) {
    if (forwardLatencyMarkerNv(frameId))
      m_presenter->setLatencyMarkerNv(frameId, VK_LATENCY_MARKER_RENDERSUBMIT_END_NV);
  }


  void DxvkBuiltInLatencyTracker::notifyQueueSubmit(
          uint64_t                  frameId) {
    std::unique_lock lock(m_mutex);
    auto frame = findFrame(frameId);

    if (frame && frame->queueSubmit == time_point())
      frame->queueSubmit = dxvk::high_resolution_clock::now();
  }


  void DxvkBuiltInLatencyTracker::notifyQueuePresentBegin(
          uint64_t                  frameId) {
    if (forwardLatencyMarkerNv(frameId))
      m_presenter->setLatencyMarkerNv(frameId, VK_LATENCY_MARKER_PRESENT_START_NV);
  }


  void DxvkBuiltInLatencyTracker::notifyQueuePresentEnd(
          uint64_t                  frameId,
          VkResult                  status) {
    { std::unique_lock lock(m_mutex);
      auto frame = findFrame(frameId);

      if (frame) {
        frame->presentStatus = status;
        frame->queuePresent = dxvk::high_resolution_clock::now();
      }

      m_cond.notify_one();
    }

    if (forwardLatencyMarkerNv(frameId))
      m_presenter->setLatencyMarkerNv(frameId, VK_LATENCY_MARKER_PRESENT_END_NV);
  }


  void DxvkBuiltInLatencyTracker::notifyGpuExecutionBegin(
          uint64_t                  frameId) {
    std::unique_lock lock(m_mutex);
    auto frame = findFrame(frameId);

    if (frame) {
      auto now = dxvk::high_resolution_clock::now();

      if (frame->gpuExecStart == time_point())
        frame->gpuExecStart = now;

      if (frame->gpuIdleStart != time_point()) {
        frame->gpuIdleTime += now - frame->gpuIdleStart;
        frame->gpuIdleEnd = now;
      }
    }
  }


  void DxvkBuiltInLatencyTracker::notifyGpuExecutionEnd(
          uint64_t                  frameId) {
    std::unique_lock lock(m_mutex);
    auto frame = findFrame(frameId);

    if (frame) {
      auto now = dxvk::high_resolution_clock::now();

      frame->gpuExecEnd = now;
      frame->gpuIdleStart = now;
    }
  }


  void DxvkBuiltInLatencyTracker::notifyGpuPresentEnd(
          uint64_t                  frameId) {
    std::unique_lock lock(m_mutex);
    auto frame = findFrame(frameId);

    if (frame)
      frame->frameEnd = dxvk::high_resolution_clock::now();

    m_cond.notify_one();
  }


  void DxvkBuiltInLatencyTracker::sleepAndBeginFrame(
          uint64_t                  frameId,
          double                    maxFrameRate) {
    auto duration = m_useNvLowLatency2
      ? sleepNv(frameId, maxFrameRate)
      : sleepBuiltin(frameId, maxFrameRate);

    { std::unique_lock lock(m_mutex);

      auto next = initFrame(frameId);
      next->frameStart = dxvk::high_resolution_clock::now();
      next->sleepDuration = duration;
    }

    if (m_useNvLowLatency2) {
      m_presenter->setLatencyMarkerNv(frameId, VK_LATENCY_MARKER_SIMULATION_START_NV);
      m_presenter->setLatencyMarkerNv(frameId, VK_LATENCY_MARKER_INPUT_SAMPLE_NV);
    }
  }


  void DxvkBuiltInLatencyTracker::discardTimings() {
    std::unique_lock lock(m_mutex);
    m_validRangeBegin = m_validRangeEnd + 1u;
  }


  DxvkLatencyStats DxvkBuiltInLatencyTracker::getStatistics(
          uint64_t                  frameId) {
    std::unique_lock lock(m_mutex);

    DxvkLatencyStats stats = { };

    while (frameId && frameId >= m_validRangeBegin) {
      auto f = findFrame(frameId--);

      if (f && f->frameEnd != time_point()) {
        stats.frameLatency = std::chrono::duration_cast<std::chrono::microseconds>(f->frameEnd - f->frameStart);
        stats.sleepDuration = std::chrono::duration_cast<std::chrono::microseconds>(f->sleepDuration);
        break;
      }
    }

    return stats;
  }


  DxvkBuiltInLatencyTracker::duration DxvkBuiltInLatencyTracker::sleepNv(
          uint64_t                  frameId,
          double                    maxFrameRate) {
    // Set up low latency mode for subsequent frames. The presenter
    // will figure out whether to reapply latency state or not.
    VkLatencySleepModeInfoNV latencyMode = { VK_STRUCTURE_TYPE_LATENCY_SLEEP_MODE_INFO_NV };
    latencyMode.lowLatencyMode = VK_TRUE;
    latencyMode.lowLatencyBoost = VK_TRUE;
    latencyMode.minimumIntervalUs = 0;

    if (m_envFpsLimit > 0.0)
      maxFrameRate = m_envFpsLimit;

    if (maxFrameRate > 0.0)
      latencyMode.minimumIntervalUs = uint64_t(1'000'000.0 / maxFrameRate);

    m_presenter->setLatencySleepModeNv(latencyMode);

    // Wait for previous present call to complete in order to
    // avoid potential issues with oscillating frame times
    bool presentSuccessful = false;

    { std::unique_lock lock(m_mutex);
      auto curr = findFrame(frameId - 1u);

      if (curr && curr->cpuPresentEnd != time_point()) {
        m_cond.wait(lock, [curr] {
          return curr->presentStatus != VK_NOT_READY;
        });

        presentSuccessful = curr->presentStatus >= 0;
      }
    }

    if (!presentSuccessful)
      return duration(0u);

    return m_presenter->latencySleepNv();
  }


  DxvkBuiltInLatencyTracker::duration DxvkBuiltInLatencyTracker::sleepBuiltin(
          uint64_t                  frameId,
          double                    maxFrameRate) {
    // Wait for all relevant timings to become available. This should
    // generally not stall for very long if a maximum frame latency of
    // 1 is enforced correctly by the swap chain.
    std::unique_lock lock(m_mutex);

    for (uint32_t i = 2; i <= FrameCount; i++) {
      auto f = findFrame(frameId - i);

      if (!f || f->cpuPresentEnd == time_point())
        return duration(0u);

      m_cond.wait(lock, [f] {
        return f->frameEnd != time_point();
      });
    }

    // Wait for the current frame's present call to be processed. Our
    // algorithm will otherwise get confused if Present stalls or if
    // any CPU work from previous frames delays GPU execution of the
    // current frame.
    auto curr = findFrame(frameId - 1u);

    if (curr && curr->cpuPresentEnd != time_point()) {
      m_cond.wait(lock, [curr] {
        return curr->presentStatus != VK_NOT_READY;
      });
    }

    // Frame entry of the last frame that fully completed
    auto prev = findFrame(frameId - 2u);

    // The way we want to align subsequent frames depends on whether
    // we are limited by GPU performance or display refresh.
    //
    // In either case, we estimate the amount of CPU time the game requires
    // before any GPU work can start to be the delay between frame start and
    // first submission, plus any GPU idle time during the frame. This is not
    // accurate if there are forced GPU sync points, but we can't work around
    // that in a meaningful way.
    constexpr size_t EntryCount = FrameCount - 1u;

    std::array<duration, EntryCount> cpuTimes = { };
    std::array<duration, EntryCount> gpuTimes = { };

    for (uint32_t i = 0; i < EntryCount; i++) {
      auto f = findFrame(frameId - (i + 2u));

      cpuTimes[i] = (f->queueSubmit - f->frameStart) + f->gpuIdleTime;
      gpuTimes[i] = (f->gpuExecEnd - f->gpuExecStart) - f->gpuIdleTime;
    }

    duration nextCpuTime = estimateTime(cpuTimes.data(), cpuTimes.size());
    duration nextGpuTime = estimateTime(gpuTimes.data(), gpuTimes.size());

    // Compute the initial deadline based on GPU execution times
    time_point gpuDeadline = prev->gpuExecEnd + 2u * nextGpuTime;

    // If we're rendering faster than refresh, use present_wait timings from
    // previous frames as a starting point and compute an average in order to
    // account for potentially erratic present_wait delays.
    duration frameInterval = computeFrameInterval(maxFrameRate);

    if (frameInterval.count()) {
      duration nextPresentFromPrev = duration(0u);

      for (uint32_t i = 2; i <= FrameCount; i++) {
        auto f = findFrame(frameId - i);

        time_point deadline = f->frameEnd + i * frameInterval - m_tolerance;
        nextPresentFromPrev += deadline - prev->frameEnd;
      }

      time_point wsiDeadline = prev->frameEnd + (nextPresentFromPrev / int32_t(FrameCount - 1u));
      gpuDeadline = std::max(gpuDeadline, wsiDeadline);
    }

    // Line up the next frame in such a way that the first GPU submission
    // happens just before the current frame's final submission completes
    time_point gpuStartTime = gpuDeadline - nextGpuTime;
    time_point cpuStartTime = gpuStartTime - nextCpuTime - m_tolerance;

    time_point now = dxvk::high_resolution_clock::now();

    // Release lock before actually sleeping, or
    // it will affect the time measurements.
    lock.unlock();

    Sleep::sleepUntil(now, cpuStartTime);
    return std::max(duration(0u), cpuStartTime - now);
  }


  DxvkLatencyFrameData* DxvkBuiltInLatencyTracker::initFrame(
          uint64_t                  frameId) {
    if (m_validRangeEnd + 1u != frameId)
      m_validRangeBegin = frameId;

    if (m_validRangeBegin + FrameCount <= frameId)
      m_validRangeBegin = frameId + 1u - FrameCount;

    m_validRangeEnd = frameId;

    auto& frame = m_frames[frameId % FrameCount];
    frame = DxvkLatencyFrameData();
    frame.frameId = frameId;
    return &frame;
  }


  DxvkLatencyFrameData* DxvkBuiltInLatencyTracker::findFrame(
          uint64_t                  frameId) {
    return frameId >= m_validRangeBegin && frameId <= m_validRangeEnd
      ? &m_frames[frameId % FrameCount]
      : nullptr;
  }


  bool DxvkBuiltInLatencyTracker::forwardLatencyMarkerNv(
          uint64_t                  frameId) {
    if (!m_useNvLowLatency2)
      return false;

    std::unique_lock lock(m_mutex);
    return findFrame(frameId) != nullptr;
  }


  DxvkBuiltInLatencyTracker::duration DxvkBuiltInLatencyTracker::computeFrameInterval(
          double                    maxFrameRate) {
    if (m_envFpsLimit > 0.0)
      maxFrameRate = m_envFpsLimit;

    return computeIntervalFromRate(maxFrameRate);
  }


  DxvkBuiltInLatencyTracker::duration DxvkBuiltInLatencyTracker::computeIntervalFromRate(
          double                    frameRate) {
    if (frameRate <= 0.0 || !std::isnormal(frameRate))
      return duration(0u);

    uint64_t ns = uint64_t(1'000'000'000.0 / frameRate);
    return std::chrono::duration_cast<duration>(std::chrono::nanoseconds(ns));
  }


  DxvkBuiltInLatencyTracker::duration DxvkBuiltInLatencyTracker::estimateTime(
    const duration*                 frames,
          size_t                    frameCount) {
    // For each frame, find the median of its neighbours, then
    // use the maximum of those medians as our estimate.
    duration result = duration(0u);

    for (size_t i = 0u; i < frameCount - 2u; i++) {
      duration a = frames[i];
      duration b = frames[i + 1];
      duration c = frames[i + 2];

      duration min = std::min(std::min(a, b), c);
      duration max = std::max(std::max(a, b), c);

      result = std::max(result, a + b + c - min - max);
    }

    return result;
  }
}
