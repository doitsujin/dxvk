#pragma once

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
     * \brief Creates default frame rate limiter
     *
     * Same as creating a limiter with a target
     * frame rate of 0, i.e. disabling the limiter.
     */
    FpsLimiter();

    /**
     * \brief Creates frame rate limiter
     *
     * \param [in] targetFrameRate Target frames per second
     */
    FpsLimiter(double targetFrameRate);

    ~FpsLimiter();

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
     * \param [in] syncInterval Sync interval of the
     *    swap chain, or 0 if vsync is disabled.
     */
    void delay(uint32_t syncInterval);

  private:

    using TimePoint = dxvk::high_resolution_clock::time_point;

    using NtTimerDuration = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;
    using NtQueryTimerResolutionProc = UINT (WINAPI *) (ULONG*, ULONG*, ULONG*);
    using NtSetTimerResolutionProc = UINT (WINAPI *) (ULONG, BOOL, ULONG*);
    using NtDelayExecutionProc = UINT (WINAPI *) (BOOL, LARGE_INTEGER*);

    NtTimerDuration m_targetInterval;
    NtTimerDuration m_refreshInterval;
    NtTimerDuration m_deviation;
    TimePoint       m_lastFrame;

    uint64_t        m_frameCountBad  = 0;
    uint64_t        m_frameCountGood = 0;

    NtTimerDuration m_sleepGranularity = NtTimerDuration::zero();
    NtTimerDuration m_sleepThreshold   = NtTimerDuration::zero();

    NtDelayExecutionProc NtDelayExecution = nullptr;

    TimePoint sleep(TimePoint t0, NtTimerDuration duration);

  };

}
