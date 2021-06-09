#include <thread>

#include "util_fps_limit.h"

namespace dxvk {
  
  FpsLimiter::FpsLimiter()
  : FpsLimiter(0.0) { }


  FpsLimiter::FpsLimiter(double targetFrameRate)
  : m_targetInterval  (targetFrameRate > 0.0 ? 1.0 / double(targetFrameRate) : 0.0),
    m_refreshInterval (0.0),
    m_deviation       (0.0),
    m_lastFrame       (dxvk::high_resolution_clock::now()) {
    
  }


  FpsLimiter::~FpsLimiter() {

  }


  void FpsLimiter::setDisplayRefreshRate(double refreshRate) {
    m_refreshInterval = Duration(refreshRate > 0.0 ? 1.0 / refreshRate : 0.0);
  }


  void FpsLimiter::delay(uint32_t syncInterval) {
    if (m_targetInterval == Duration(0.0))
      return;

    // If the swap chain is known to have vsync enabled and the
    // refresh rate is similar to the target frame rate, disable
    // the limiter so it does not screw up frame times
    if (m_refreshInterval * double(syncInterval) > m_targetInterval * 0.97)
      return;

    auto t0 = m_lastFrame;
    auto t1 = dxvk::high_resolution_clock::now();

    if (Duration(t1 - t0) > m_targetInterval * 1.03 - m_deviation) {
      // If we have a slow frame, reset the deviation since we
      // do not want to compensate for low performance later on
      m_deviation = Duration(0.0f);
    } else {
      // Don't call sleep if the amount of time to sleep is shorter
      // than the time the function calls are likely going to take
      static const Duration threshold = Duration(0.00005);
      Duration sleepDuration = m_targetInterval - m_deviation - Duration(t1 - t0);

      if (sleepDuration > threshold) {
        sleep(sleepDuration - threshold);
        t1 = dxvk::high_resolution_clock::now();
      }

      // Compensate for any sleep inaccuracies in the next frame, and
      // limit cumulative deviation in order to avoid stutter in case we
      // have a number of slow frames immediately followed by a fast one.
      m_deviation += Duration(t1 - t0) - m_targetInterval;
      m_deviation = min(m_deviation, m_targetInterval / 16.0);
    }

    m_lastFrame = t1;
  }


  void FpsLimiter::sleep(Duration duration) {
    // NtDelayExecution maps to a select call with timeout in Wine, which is a
    // lot more precise than a Sleep-based implementation with 1ms granularity
    using NtDelayExecutionProc = UINT (WINAPI *) (BOOL, PLARGE_INTEGER);

    static auto NtDelayExecution = reinterpret_cast<NtDelayExecutionProc>(
      ::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "NtDelayExecution"));

    if (NtDelayExecution) {
      // This function expects the delay in units of 100ns
      using TickType = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;

      LARGE_INTEGER ticks;
      ticks.QuadPart = -std::chrono::duration_cast<TickType>(duration).count();

      NtDelayExecution(FALSE, &ticks);
    } else {
      std::this_thread::sleep_for(duration);
    }
  }

}
