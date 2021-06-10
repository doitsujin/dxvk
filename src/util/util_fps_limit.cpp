#include <thread>

#include "thread.h"
#include "util_fps_limit.h"
#include "util_string.h"

#include "./log/log.h"

namespace dxvk {
  
  FpsLimiter::FpsLimiter()
  : FpsLimiter(0.0) { }


  FpsLimiter::FpsLimiter(double targetFrameRate)
  : m_targetInterval  (targetFrameRate > 0.0
      ? NtTimerDuration(int64_t(double(NtTimerDuration::period::den) / targetFrameRate))
      : NtTimerDuration::zero()),
    m_refreshInterval (NtTimerDuration::zero()),
    m_deviation       (NtTimerDuration::zero()) {
    if (m_targetInterval != NtTimerDuration::zero()) {
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

      m_sleepThreshold = 2 * m_sleepGranularity;
      m_lastFrame = dxvk::high_resolution_clock::now();
    }
  }


  FpsLimiter::~FpsLimiter() {

  }


  void FpsLimiter::setDisplayRefreshRate(double refreshRate) {
    m_refreshInterval = refreshRate > 0.0
      ? NtTimerDuration(int64_t(double(NtTimerDuration::period::den) / refreshRate))
      : NtTimerDuration::zero();
  }


  void FpsLimiter::delay(uint32_t syncInterval) {
    if (m_targetInterval == NtTimerDuration::zero())
      return;

    // If the swap chain is known to have vsync enabled and the
    // refresh rate is similar to the target frame rate, disable
    // the limiter so it does not screw up frame times
    if (m_refreshInterval * syncInterval * 100 > m_targetInterval * 97)
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

      // In case sleep frequently wakes up late enough to cause
      // noticeably inconsistent frame times, bump the threshold
      frameTime = std::chrono::duration_cast<NtTimerDuration>(t1 - t0);

      if (sleepDuration > NtTimerDuration::zero()) {
        if (frameTime > m_targetInterval + m_targetInterval / 8)
          m_frameCountBad += 1;
        else
          m_frameCountGood += 1;

        if (20 * m_frameCountBad > m_frameCountGood && m_frameCountBad > 10) {
          m_frameCountBad = 0;
          m_frameCountGood = 0;

          m_sleepThreshold += m_targetInterval / 8;

          Logger::info(str::format("Frame rate limiter: Sleep threshold increased to ",
            std::chrono::duration_cast<std::chrono::milliseconds>(m_sleepThreshold).count(), " ms"));
        }
      }

      // Compensate for any sleep inaccuracies in the next frame, and
      // limit cumulative deviation in order to avoid stutter in case we
      // have a number of slow frames immediately followed by a fast one.
      m_deviation += frameTime - m_targetInterval;
      m_deviation = std::min(m_deviation, m_targetInterval / 16);
    }

    m_lastFrame = t1;
  }


  FpsLimiter::TimePoint FpsLimiter::sleep(TimePoint t0, NtTimerDuration duration) {
    NtTimerDuration remaining = duration;
    TimePoint t1 = t0;

    // Only call sleep if the total duration covers several
    // timer ticks, since otherwise we'll frequently end up
    // waking up too late
    while (remaining > m_sleepThreshold) {
      NtTimerDuration sleepDuration = remaining - m_sleepThreshold;

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

}
