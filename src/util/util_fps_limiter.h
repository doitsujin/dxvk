#pragma once

#include "thread.h"
#include "util_time.h"

namespace dxvk {
  
  /**
   * \brief Frame rate limiter
   *
   * Provides functionality to stall an application
   * thread in order to maintain a given frame rate.
   */
  class FpsLimiter {

  public:

    /**
     * \brief Creates frame rate limiter
     */
    FpsLimiter();

    ~FpsLimiter();

    /**
     * \brief Sets target frame rate
     * \param [in] frameRate Target frame rate
     */
    void setTargetFrameRate(double frameRate);

    /**
     * \brief Stalls calling thread as necessary
     *
     * Blocks the calling thread if the limiter is enabled
     * and the time since the last call to \ref delay is
     * shorter than the target interval.
     * \param [in] vsyncEnabled \c true if vsync is enabled
     */
    void delay(bool vsyncEnabled);

    /**
     * \brief Checks whether the frame rate limiter is enabled
     * \returns \c true if the target frame rate is non-zero.
     */
    bool isEnabled() const {
      return m_targetInterval != TimerDuration::zero();
    }

  private:

    using TimePoint = dxvk::high_resolution_clock::time_point;

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

    dxvk::mutex     m_mutex;

    TimerDuration   m_targetInterval  = TimerDuration::zero();
    TimerDuration   m_deviation       = TimerDuration::zero();
    TimePoint       m_lastFrame;

    bool            m_initialized     = false;
    bool            m_envOverride     = false;

    TimerDuration   m_sleepGranularity = TimerDuration::zero();
    TimerDuration   m_sleepThreshold   = TimerDuration::zero();

    TimePoint sleep(TimePoint t0, TimerDuration duration);

    void initialize();

    void updateSleepGranularity();

    void performSleep(TimerDuration sleepDuration);

  };

}
