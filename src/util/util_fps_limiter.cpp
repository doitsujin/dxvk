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
      m_targetInterval = frameRate > 0.0
        ? TimerDuration(int64_t(double(TimerDuration::period::den) / frameRate))
        : TimerDuration::zero();

      if (isEnabled() && !m_initialized)
        initialize();
    }
  }


  void FpsLimiter::delay(bool vsyncEnabled) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!isEnabled())
      return;

    auto t0 = m_lastFrame;
    auto t1 = dxvk::high_resolution_clock::now();

    auto frameTime = std::chrono::duration_cast<TimerDuration>(t1 - t0);

    if (frameTime * 100 > m_targetInterval * 103 - m_deviation * 100) {
      // If we have a slow frame, reset the deviation since we
      // do not want to compensate for low performance later on
      m_deviation = TimerDuration::zero();
    } else {
      // Don't call sleep if the amount of time to sleep is shorter
      // than the time the function calls are likely going to take
      TimerDuration sleepDuration = m_targetInterval - m_deviation - frameTime;
      t1 = Sleep::sleepFor(t1, sleepDuration);

      // Compensate for any sleep inaccuracies in the next frame, and
      // limit cumulative deviation in order to avoid stutter in case we
      // have a number of slow frames immediately followed by a fast one.
      frameTime = std::chrono::duration_cast<TimerDuration>(t1 - t0);
      m_deviation += frameTime - m_targetInterval;
      m_deviation = std::min(m_deviation, m_targetInterval / 16);
    }

    m_lastFrame = t1;
  }


  void FpsLimiter::initialize() {
    m_lastFrame = dxvk::high_resolution_clock::now();
    m_initialized = true;
  }

}
