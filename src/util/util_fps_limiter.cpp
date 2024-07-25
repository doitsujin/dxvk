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
    std::string env = env::getEnvVar("DXVK_FRAME_RATE");

    if (!env.empty()) {
      try {
        setTargetFrameRate(std::stod(env));
        m_envOverride = true;
      } catch (const std::invalid_argument&) {
        // no-op
      }
    }
  }


  FpsLimiter::~FpsLimiter() {

  }


  void FpsLimiter::setTargetFrameRate(double frameRate) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!m_envOverride) {
      TimerDuration interval = frameRate != 0.0
        ? TimerDuration(int64_t(double(TimerDuration::period::den) / frameRate))
        : TimerDuration::zero();

      if (m_targetInterval != interval) {
        m_targetInterval = interval;

        m_heuristicFrameCount = 0;
        m_heuristicEnable = false;
      }
    }
  }


  void FpsLimiter::delay() {
    std::unique_lock<dxvk::mutex> lock(m_mutex);
    auto interval = m_targetInterval;

    if (interval == TimerDuration::zero()) {
      m_nextFrame = TimePoint();
      return;
    }

    auto t1 = dxvk::high_resolution_clock::now();

    if (interval < TimerDuration::zero()) {
      interval = -interval;

      if (!testRefreshHeuristic(interval, t1))
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


  bool FpsLimiter::testRefreshHeuristic(TimerDuration interval, TimePoint now) {
    if (m_heuristicEnable)
      return true;

    // Use a sliding window to determine whether the current
    // frame rate is higher than the targeted refresh rate
    uint32_t heuristicWindow = m_heuristicFrameTimes.size();
    auto windowStart = m_heuristicFrameTimes[m_heuristicFrameCount % heuristicWindow];
    auto windowDuration = std::chrono::duration_cast<TimerDuration>(now - windowStart);

    m_heuristicFrameTimes[m_heuristicFrameCount % heuristicWindow] = now;
    m_heuristicFrameCount += 1;

    // The first window of frames may contain faster frames as the
    // internal swap chain queue fills up, so we should ignore it.
    if (m_heuristicFrameCount < 2 * heuristicWindow)
      return false;

    // Test whether we should engage the frame rate limiter. It will
    // stay enabled until the refresh rate or vsync enablement change.
    m_heuristicEnable = (103 * windowDuration) < (100 * heuristicWindow) * interval;

    if (m_heuristicEnable) {
      double got = (double(heuristicWindow) * double(TimerDuration::period::den))
                 / (double(windowDuration.count()) * double(TimerDuration::period::num));
      double refresh = double(TimerDuration::period::den) / (double(TimerDuration::period::num) * double(interval.count()));

      Logger::info(str::format("Detected frame rate (~", uint32_t(got), ") higher than selected refresh rate of ~",
        uint32_t(refresh), " Hz.\n", "Engaging frame rate limiter."));
    }

    return m_heuristicEnable;
  }

}
