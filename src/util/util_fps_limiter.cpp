#include <thread>

#include "thread.h"
#include "util_env.h"
#include "util_fps_limiter.h"
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
      t1 = sleep(t1, sleepDuration);

      // Compensate for any sleep inaccuracies in the next frame, and
      // limit cumulative deviation in order to avoid stutter in case we
      // have a number of slow frames immediately followed by a fast one.
      frameTime = std::chrono::duration_cast<TimerDuration>(t1 - t0);
      m_deviation += frameTime - m_targetInterval;
      m_deviation = std::min(m_deviation, m_targetInterval / 16);
    }

    m_lastFrame = t1;
  }


  FpsLimiter::TimePoint FpsLimiter::sleep(TimePoint t0, TimerDuration duration) {
    if (duration <= TimerDuration::zero())
      return t0;

    // On wine, we can rely on NtDelayExecution waiting for more or
    // less exactly the desired amount of time, and we want to avoid
    // spamming QueryPerformanceCounter for performance reasons.
    // On Windows, we busy-wait for the last couple of milliseconds
    // since sleeping is highly inaccurate and inconsistent.
    TimerDuration sleepThreshold = m_sleepThreshold;

    if (m_sleepGranularity != TimerDuration::zero())
      sleepThreshold += duration / 6;

    TimerDuration remaining = duration;
    TimePoint t1 = t0;

    while (remaining > sleepThreshold) {
      TimerDuration sleepDuration = remaining - sleepThreshold;

      performSleep(sleepDuration);

      t1 = dxvk::high_resolution_clock::now();
      remaining -= std::chrono::duration_cast<TimerDuration>(t1 - t0);
      t0 = t1;
    }

    // Busy-wait until we have slept long enough
    while (remaining > TimerDuration::zero()) {
      t1 = dxvk::high_resolution_clock::now();
      remaining -= std::chrono::duration_cast<TimerDuration>(t1 - t0);
      t0 = t1;
    }

    return t1;
  }


  void FpsLimiter::initialize() {
    updateSleepGranularity();
    m_sleepThreshold = 4 * m_sleepGranularity;
    m_lastFrame = dxvk::high_resolution_clock::now();
    m_initialized = true;
  }


  void FpsLimiter::updateSleepGranularity() {
#ifdef _WIN32
    HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");

    if (ntdll) {
      NtDelayExecution = reinterpret_cast<NtDelayExecutionProc>(
        ::GetProcAddress(ntdll, "NtDelayExecution"));
      auto NtQueryTimerResolution = reinterpret_cast<NtQueryTimerResolutionProc>(
        ::GetProcAddress(ntdll, "NtQueryTimerResolution"));
      auto NtSetTimerResolution = reinterpret_cast<NtSetTimerResolutionProc>(
        ::GetProcAddress(ntdll, "NtSetTimerResolution"));

      ULONG min, max, cur;

      // Wine's implementation of these functions is a stub as of 6.10, which is fine
      // since it uses select() in NtDelayExecution. This is only relevant for Windows.
      if (NtQueryTimerResolution && !NtQueryTimerResolution(&min, &max, &cur)) {
        m_sleepGranularity = TimerDuration(cur);

        if (NtSetTimerResolution && !NtSetTimerResolution(max, TRUE, &cur)) {
          Logger::info(str::format("Setting timer interval to ", (double(max) / 10.0), " us"));
          m_sleepGranularity = TimerDuration(max);
        }
      }
    } else {
      // Assume 1ms sleep granularity by default
      m_sleepGranularity = TimerDuration(1ms);
    }
#else
    // Assume 0.5ms sleep granularity by default
    m_sleepGranularity = TimerDuration(500us);
#endif
  }


  void FpsLimiter::performSleep(TimerDuration sleepDuration) {
#ifdef _WIN32
    if (NtDelayExecution) {
      LARGE_INTEGER ticks;
      ticks.QuadPart = -sleepDuration.count();

      NtDelayExecution(FALSE, &ticks);
    } else {
      std::this_thread::sleep_for(sleepDuration);
    }
#else
    std::this_thread::sleep_for(sleepDuration);
#endif
  }

}
