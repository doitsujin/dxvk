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
     * \brief Sets display refresh rate
     *
     * This information is used to decide whether or not
     * the limiter should be active in the first place in
     * case vertical synchronization is enabled.
     * \param [in] refreshRate Current refresh rate
     */
    void setDisplayRefreshRate(double refreshRate);

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
      return m_targetInterval != NtTimerDuration::zero();
    }

  private:

    using TimePoint = dxvk::high_resolution_clock::time_point;

    using NtTimerDuration = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;
    using NtQueryTimerResolutionProc = UINT (WINAPI *) (ULONG*, ULONG*, ULONG*);
    using NtSetTimerResolutionProc = UINT (WINAPI *) (ULONG, BOOL, ULONG*);
    using NtDelayExecutionProc = UINT (WINAPI *) (BOOL, LARGE_INTEGER*);

    dxvk::mutex     m_mutex;

    NtTimerDuration m_targetInterval  = NtTimerDuration::zero();
    NtTimerDuration m_refreshInterval = NtTimerDuration::zero();
    NtTimerDuration m_deviation       = NtTimerDuration::zero();
    TimePoint       m_lastFrame;

    bool            m_initialized     = false;
    bool            m_envOverride     = false;

    NtTimerDuration m_sleepGranularity = NtTimerDuration::zero();
    NtTimerDuration m_sleepThreshold   = NtTimerDuration::zero();

    NtDelayExecutionProc NtDelayExecution = nullptr;

    TimePoint sleep(TimePoint t0, NtTimerDuration duration);

    void initialize();

  };

}
