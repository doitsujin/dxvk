#pragma once

#include <array>

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
     */
    void delay();

  private:

    using TimePoint = dxvk::high_resolution_clock::time_point;
    using TimerDuration = std::chrono::nanoseconds;

    dxvk::mutex     m_mutex;

    TimerDuration   m_targetInterval  = TimerDuration::zero();
    TimePoint       m_nextFrame       = TimePoint();

    bool            m_envOverride     = false;

    uint32_t                  m_heuristicFrameCount = 0;
    std::array<TimePoint, 16> m_heuristicFrameTimes = { };
    bool                      m_heuristicEnable = false;

    bool testRefreshHeuristic(TimerDuration interval, TimePoint now);

  };

}
