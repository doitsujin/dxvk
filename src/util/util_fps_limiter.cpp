#include <thread>

#include "thread.h"
#include "util_env.h"
#include "util_fps_limiter.h"
#include "util_sleep.h"
#include "util_string.h"

#include "./log/log.h"

using namespace std::chrono_literals;

namespace dxvk {
  
  FpsLimiter::FpsLimiter() {
    auto override = getEnvironmentOverride();

    if (override) {
      setTargetFrameRate(*override, 0);
      m_envOverride = true;
    }
  }


  FpsLimiter::~FpsLimiter() {

  }


  void FpsLimiter::setTargetFrameRate(double frameRate, uint32_t maxLatency) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!m_envOverride) {
      TimerDuration interval = frameRate != 0.0
        ? TimerDuration(int64_t(double(TimerDuration::period::den) / frameRate))
        : TimerDuration::zero();

      if (m_targetInterval != interval) {
        m_targetInterval = interval;

        m_heuristicFrameTime = TimePoint();
        m_heuristicFrameCount = 0;
        m_heuristicEnable = false;

        m_maxLatency = maxLatency;
      }
    }
  }


  void FpsLimiter::delay() {
    std::unique_lock<dxvk::mutex> lock(m_mutex);
    auto interval = m_targetInterval;
    auto latency = m_maxLatency;

    if (interval == TimerDuration::zero()) {
      m_nextFrame = TimePoint();
      return;
    }

    auto t1 = dxvk::high_resolution_clock::now();

    if (interval < TimerDuration::zero()) {
      interval = -interval;

      if (!testRefreshHeuristic(interval, t1, latency))
        return;
    }

    // Subsequent code must not access any class members
    // that can be written by setTargetFrameRate
    lock.unlock();

    if (t1 < m_nextFrame)
      Sleep::sleepUntil(t1, m_nextFrame);

    m_nextFrame = (t1 < m_nextFrame + interval)
      ? m_nextFrame + interval
      : t1 + interval;
  }


  bool FpsLimiter::testRefreshHeuristic(TimerDuration interval, TimePoint now, uint32_t maxLatency) {
    if (m_heuristicEnable)
      return true;

    constexpr static uint32_t MinWindowSize = 8;
    constexpr static uint32_t MaxWindowSize = 128;

    if (m_heuristicFrameCount >= MinWindowSize) {
      TimerDuration windowTotalTime = now - m_heuristicFrameTime;
      TimerDuration windowExpectedTime = m_heuristicFrameCount * interval;

      uint32_t minFrameCount = m_heuristicFrameCount - 1;
      uint32_t maxFrameCount = m_heuristicFrameCount + maxLatency;

      // Enable frame rate limiter if frames have been delivered faster than
      // the desired refresh rate even accounting for swap chain buffering.
      if ((maxFrameCount * windowTotalTime) < (m_heuristicFrameCount * windowExpectedTime)) {
        double got = (double(m_heuristicFrameCount) * double(TimerDuration::period::den))
                   / (double(windowTotalTime.count()) * double(TimerDuration::period::num));
        double refresh = double(TimerDuration::period::den) / (double(TimerDuration::period::num) * double(interval.count()));

        Logger::info(str::format("Detected frame rate (~", uint32_t(got), ") higher than selected refresh rate of ~",
          uint32_t(refresh), " Hz.\n", "Engaging frame rate limiter."));

        m_heuristicEnable = true;
        return true;
      }

      // Reset heuristics if frames have been delivered slower than the refresh rate.
      if (((minFrameCount * windowTotalTime) > (m_heuristicFrameCount * windowExpectedTime))
       || (m_heuristicFrameCount >= MaxWindowSize)) {
        m_heuristicFrameCount = 1;
        m_heuristicFrameTime = now;
        return false;
      }
    }

    if (!m_heuristicFrameCount)
      m_heuristicFrameTime = now;

    m_heuristicFrameCount += 1;
    return false;
  }


  std::optional<double> FpsLimiter::getEnvironmentOverride() {
    std::string env = env::getEnvVar("DXVK_FRAME_RATE");

    if (!env.empty()) {
      try {
        return std::stod(env);
      } catch (const std::invalid_argument&) {
        // no op
      }
    }

    return std::nullopt;
  }

}
