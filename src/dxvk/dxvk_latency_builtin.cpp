#include <cmath>

#include "dxvk_latency_builtin.h"

#include "../util/log/log.h"

#include "../util/util_fps_limiter.h"
#include "../util/util_string.h"

namespace dxvk {

  DxvkBuiltInLatencyTracker::DxvkBuiltInLatencyTracker(
          int32_t                   toleranceUs)
  : m_tolerance(std::chrono::duration_cast<duration>(
      std::chrono::microseconds(std::max(toleranceUs, 0)))) {
    Logger::info("Latency control enabled, using built-in algorithm");
    auto limit = FpsLimiter::getEnvironmentOverride();

    if (limit)
      m_envFpsLimit = *limit;
  }


  DxvkBuiltInLatencyTracker::~DxvkBuiltInLatencyTracker() {

  }


  void DxvkBuiltInLatencyTracker::notifyCpuPresentBegin(
          uint64_t                  frameId) {
    // Not interesting here
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
    // Not interesting here
  }


  void DxvkBuiltInLatencyTracker::notifyCsRenderEnd(
          uint64_t                  frameId) {
    // Not interesting here
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
    std::unique_lock lock(m_mutex);
    auto frame = findFrame(frameId);

    if (frame)
      frame->queuePresent = dxvk::high_resolution_clock::now();
  }


  void DxvkBuiltInLatencyTracker::notifyQueuePresentEnd(
          uint64_t                  frameId,
          VkResult                  status) {
    std::unique_lock lock(m_mutex);
    auto frame = findFrame(frameId);

    if (frame)
      frame->presentStatus = status;

    m_cond.notify_one();
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

    m_cond.notify_one();
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
      frame->gpuPresent = dxvk::high_resolution_clock::now();

    m_cond.notify_one();
  }


  void DxvkBuiltInLatencyTracker::sleepAndBeginFrame(
          uint64_t                  frameId,
          double                    maxFrameRate) {
    auto duration = sleep(frameId, maxFrameRate);

    std::unique_lock lock(m_mutex);

    auto next = initFrame(frameId);
    next->frameStart = dxvk::high_resolution_clock::now();
    next->sleepDuration = duration;
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

      if (f && f->gpuPresent != time_point()) {
        stats.frameLatency = std::chrono::duration_cast<std::chrono::microseconds>(f->gpuPresent - f->frameStart);
        stats.sleepDuration = std::chrono::duration_cast<std::chrono::microseconds>(f->sleepDuration);
        break;
      }
    }

    return stats;
  }


  DxvkBuiltInLatencyTracker::duration DxvkBuiltInLatencyTracker::sleep(
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
        return f->gpuPresent != time_point();
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

        time_point deadline = f->gpuPresent + i * frameInterval - m_tolerance;
        nextPresentFromPrev += deadline - prev->gpuPresent;
      }

      time_point wsiDeadline = prev->gpuPresent + (nextPresentFromPrev / int32_t(FrameCount - 1u));
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
