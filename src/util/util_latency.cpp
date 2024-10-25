#include "util_env.h"
#include "util_latency.h"
#include "util_string.h"

#include "./log/log.h"

#include "./sync/sync_spinlock.h"

namespace dxvk {

  DxvkLatencyControl::DxvkLatencyControl() {
    std::string env = env::getEnvVar("DXVK_FRAME_RATE");

    if (!env.empty())
      m_frameRateLimit = std::stod(env);
  }


  DxvkLatencyControl::~DxvkLatencyControl() {

  }


  void DxvkLatencyControl::sleep(
        uint64_t                      frameId,
        double                        frameRate) {
    // Apply environment override as necessary
    if (m_frameRateLimit != 0.0) {
      frameRate = frameRate == 0.0f ? std::abs(m_frameRateLimit)
        : std::min(std::abs(frameRate), std::abs(m_frameRateLimit));
    }

    // Wait for the current frame's first submission to become available
    auto& currFrame = m_frames[(frameId - 0u) % m_frames.size()];
    auto& prevFrame = m_frames[(frameId - 1u) % m_frames.size()];

    sync::spin(-1u, [&currFrame, &prevFrame] {
      return bool(currFrame.markerMask.load(std::memory_order::memory_order_acquire) & (1u << uint32_t(DxvkLatencyMarker::GpuFrameStart)))
          && bool(prevFrame.markerMask.load(std::memory_order::memory_order_acquire) & (1u << uint32_t(DxvkLatencyMarker::GpuPresentEnd)));
    });

    // Estimate GPU execution time. Use the minimum from the past frames
    // to avoid creating a feedback loop with oscillating frame times.
    auto gpuFrameInterval = std::chrono::nanoseconds(~0u);

    for (uint32_t i = 1; i < m_frames.size(); i++) {
      auto& frame = m_frames[(frameId - i) % m_frames.size()];

      gpuFrameInterval = std::min(gpuFrameInterval,
        std::chrono::duration_cast<std::chrono::nanoseconds>(
          frame.timestamps[uint32_t(DxvkLatencyMarker::GpuFrameEnd)] -
          frame.timestamps[uint32_t(DxvkLatencyMarker::GpuFrameStart)]));
    }

    // If the minimum present interval is higher than the GPU execution time,
    // we need to delay the next frame even further to reduce frame latency
    auto presentInterval = std::chrono::nanoseconds(0);

    if (frameRate != 0.0)
      presentInterval = std::chrono::nanoseconds(int64_t(1000000000.0 / std::abs(frameRate)));

    // Estimate simulation time from end of present to first submission. Use
    // the maximum of the past few frames here to account for fluctuations.
    auto cpuSubmitDelay = std::chrono::nanoseconds(0);

    for (uint32_t i = 0; i < m_frames.size(); i++) {
      auto& frame = m_frames[(frameId - i) % m_frames.size()];
      cpuSubmitDelay = std::max(cpuSubmitDelay, std::chrono::duration_cast<std::chrono::nanoseconds>(
        frame.timestamps[uint32_t(DxvkLatencyMarker::CpuFirstSubmit)] -
        frame.timestamps[uint32_t(DxvkLatencyMarker::CpuFrameStart)]));
    }

    // Aim for roughly 2ms of delay between the first CPU submit within a
    // frame and the GPU starting to process that submission. This gives
    // us some headroom to not starve the GPU.
    auto tolerance = std::chrono::nanoseconds(1000000) + gpuFrameInterval / 16u;

    // Compute time when to start the next frame
    auto nextGpuStartDeadline = std::max(
      currFrame.timestamps[uint32_t(DxvkLatencyMarker::GpuFrameStart)] + gpuFrameInterval,
      prevFrame.timestamps[uint32_t(DxvkLatencyMarker::GpuPresentEnd)] + (presentInterval + presentInterval - gpuFrameInterval));
    auto nextCpuStartDeadline = nextGpuStartDeadline - (cpuSubmitDelay + tolerance);

    // Sleep if necessary, and return the amount of time spent sleeping
    auto now = high_resolution_clock::now();
    Sleep::sleepUntil(now, nextCpuStartDeadline);

    // Store sleep duration for HUD statistics
    std::lock_guard lock(m_statLock);
    m_statSleepDuration = std::max(std::chrono::nanoseconds(0),
      std::chrono::duration_cast<std::chrono::nanoseconds>(nextCpuStartDeadline - now));
  }

}
