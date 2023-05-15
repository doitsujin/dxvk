#pragma once

#include "thread.h"
#include "util_time.h"

namespace dxvk {

  /**
   * \brief Utility class for accurate sleeping
   */
  class Sleep {

  public:

    using TimePoint = dxvk::high_resolution_clock::time_point;

    ~Sleep();

    /**
     * \brief Sleeps for a given period of time
     *
     * \param [in] t0 Current time
     * \param [in] duration Sleep duration
     * \returns Time after sleep has finished
     */
    template<typename Rep, typename Period>
    static TimePoint sleepFor(TimePoint t0, std::chrono::duration<Rep, Period> duration) {
      return s_instance.sleep(t0, std::chrono::duration_cast<TimerDuration>(duration));
    }

    /**
     * \brief Sleeps until a given time point
     *
     * Convenience function that sleeps for the
     * time difference between t1 and t0.
     * \param [in] t0 Current time
     * \param [in] t1 Target time
     * \returns Time after sleep has finished
     */
    static TimePoint sleepUntil(TimePoint t0, TimePoint t1) {
      return sleepFor(t0, t1 - t0);
    }

  private:

    static Sleep s_instance;

    dxvk::mutex       m_mutex;
    std::atomic<bool> m_initialized = { false };

#ifdef _WIN32
    // On Windows, we use NtDelayExecution which has units of 100ns.
    using TimerDuration = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;
    using NtQueryTimerResolutionProc = UINT (WINAPI *) (ULONG*, ULONG*, ULONG*);
    using NtSetTimerResolutionProc = UINT (WINAPI *) (ULONG, BOOL, ULONG*);
    using NtDelayExecutionProc = UINT (WINAPI *) (BOOL, LARGE_INTEGER*);
    NtDelayExecutionProc NtDelayExecution = nullptr;
#else
    // On other platforms, we use the std library, which calls through to nanosleep -- which is ns.
    using TimerDuration = std::chrono::nanoseconds;
#endif

    TimerDuration m_sleepGranularity = TimerDuration::zero();
    TimerDuration m_sleepThreshold   = TimerDuration::zero();

    Sleep();

    void initialize();

    void initializePlatformSpecifics();

    TimePoint sleep(TimePoint t0, TimerDuration duration);

    void systemSleep(TimerDuration duration);

  };

}
