#include <thread>

#include "thread.h"
#include "util_env.h"
#include "util_fps_limiter.h"
#include "util_string.h"

#include "./log/log.h"

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
        ? NtTimerDuration(int64_t(double(NtTimerDuration::period::den) / frameRate))
        : NtTimerDuration::zero();

      if (isEnabled() && !m_initialized)
        initialize();
    }
  }


  void FpsLimiter::setDisplayRefreshRate(double refreshRate) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    m_refreshInterval = refreshRate > 0.0
      ? NtTimerDuration(int64_t(double(NtTimerDuration::period::den) / refreshRate))
      : NtTimerDuration::zero();
  }


  void FpsLimiter::delay(bool vsyncEnabled) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!isEnabled())
      return;

    // If the swap chain is known to have vsync enabled and the
    // refresh rate is similar to the target frame rate, disable
    // the limiter so it does not screw up frame times
    if (vsyncEnabled && !m_envOverride
     && m_refreshInterval * 100 > m_targetInterval * 97)
      return;

    auto t0 = m_lastFrame;
    auto t1 = dxvk::high_resolution_clock::now();

    auto frameTime = std::chrono::duration_cast<NtTimerDuration>(t1 - t0);

    if (frameTime * 100 > m_targetInterval * 103 - m_deviation * 100) {
      // If we have a slow frame, reset the deviation since we
      // do not want to compensate for low performance later on
      m_deviation = NtTimerDuration::zero();
    } else {
      // Don't call sleep if the amount of time to sleep is shorter
      // than the time the function calls are likely going to take
      NtTimerDuration sleepDuration = m_targetInterval - m_deviation - frameTime;
      t1 = sleep(t1, sleepDuration);

      // Compensate for any sleep inaccuracies in the next frame, and
      // limit cumulative deviation in order to avoid stutter in case we
      // have a number of slow frames immediately followed by a fast one.
      frameTime = std::chrono::duration_cast<NtTimerDuration>(t1 - t0);
      m_deviation += frameTime - m_targetInterval;
      m_deviation = std::min(m_deviation, m_targetInterval / 16);
    }

    m_lastFrame = t1;
  }


  FpsLimiter::TimePoint FpsLimiter::sleep(TimePoint t0, NtTimerDuration duration) {
    if (duration <= NtTimerDuration::zero())
      return t0;

    // On wine, we can rely on NtDelayExecution waiting for more or
    // less exactly the desired amount of time, and we want to avoid
    // spamming QueryPerformanceCounter for performance reasons.
    // On Windows, we busy-wait for the last couple of milliseconds
    // since sleeping is highly inaccurate and inconsistent.
    NtTimerDuration sleepThreshold = m_sleepThreshold;

    if (m_sleepGranularity != NtTimerDuration::zero())
      sleepThreshold += duration / 6;

    NtTimerDuration remaining = duration;
    TimePoint t1 = t0;

    while (remaining > sleepThreshold) {
      NtTimerDuration sleepDuration = remaining - sleepThreshold;

      if (NtDelayExecution) {
        LARGE_INTEGER ticks;
        ticks.QuadPart = -sleepDuration.count();

        NtDelayExecution(FALSE, &ticks);
      } else {
        std::this_thread::sleep_for(sleepDuration);
      }

      t1 = dxvk::high_resolution_clock::now();
      remaining -= std::chrono::duration_cast<NtTimerDuration>(t1 - t0);
      t0 = t1;
    }

    // Busy-wait until we have slept long enough
    while (remaining > NtTimerDuration::zero()) {
      t1 = dxvk::high_resolution_clock::now();
      remaining -= std::chrono::duration_cast<NtTimerDuration>(t1 - t0);
      t0 = t1;
    }

    return t1;
  }


  void FpsLimiter::initialize() {
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
        m_sleepGranularity = NtTimerDuration(cur);

        if (NtSetTimerResolution && !NtSetTimerResolution(max, TRUE, &cur)) {
          Logger::info(str::format("Setting timer interval to ", (double(max) / 10.0), " us"));
          m_sleepGranularity = NtTimerDuration(max);
        }
      }
    } else {
      // Assume 1ms sleep granularity by default
      m_sleepGranularity = NtTimerDuration(10000);
    }

    m_sleepThreshold = 4 * m_sleepGranularity;
    m_lastFrame = dxvk::high_resolution_clock::now();
    m_initialized = true;
  }

}
