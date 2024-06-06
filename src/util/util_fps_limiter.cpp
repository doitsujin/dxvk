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

        if (m_targetInterval != TimerDuration::zero() && !m_initialized)
          initialize();
      }
    }
  }


  void FpsLimiter::delay() {
    std::unique_lock<dxvk::mutex> lock(m_mutex);
    auto interval = m_targetInterval;

    if (interval == TimerDuration::zero())
      return;

    auto t0 = m_lastFrame;
    auto t1 = dxvk::high_resolution_clock::now();

    if (interval < TimerDuration::zero()) {
      interval = -interval;

      if (!testRefreshHeuristic(interval, t1))
        return;
    }

    // Subsequent code must not access any class members
    // that can be written by setTargetFrameRate
    lock.unlock();

    auto frameTime = std::chrono::duration_cast<TimerDuration>(t1 - t0);

    if (frameTime * 100 > interval * 103 - m_deviation * 100) {
      // If we have a slow frame, reset the deviation since we
      // do not want to compensate for low performance later on
      m_deviation = TimerDuration::zero();
    } else {
      // Don't call sleep if the amount of time to sleep is shorter
      // than the time the function calls are likely going to take
      TimerDuration sleepDuration = interval - m_deviation - frameTime;
      t1 = Sleep::sleepFor(t1, sleepDuration);

      // Compensate for any sleep inaccuracies in the next frame, and
      // limit cumulative deviation in order to avoid stutter in case we
      // have a number of slow frames immediately followed by a fast one.
      frameTime = std::chrono::duration_cast<TimerDuration>(t1 - t0);
      m_deviation += frameTime - interval;
      m_deviation = std::min(m_deviation, interval / 16);
    }

    m_lastFrame = t1;
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


  void FpsLimiter::initialize() {
    m_lastFrame = dxvk::high_resolution_clock::now();
    m_initialized = true;
  }

}
