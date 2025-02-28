#pragma once

#include "dxvk_framepacer_mode.h"

namespace dxvk {

  /*
   * Minimal latency is achieved here by waiting for the previous
   * frame to complete, which results in very much reduced fps.
   * Generally not recommended, but helpful to get insights to fine-tune
   * the low-latency mode, and possibly is useful for running games
   * in the cpu limit.
   */

  class MinLatencyMode : public FramePacerMode {

  public:

    MinLatencyMode(Mode mode, LatencyMarkersStorage* storage)
    : FramePacerMode(mode, storage, 0) {}

    ~MinLatencyMode() {}

    void startFrame( uint64_t frameId ) override {

      Sleep::TimePoint now = high_resolution_clock::now();
      int32_t frametime = std::chrono::duration_cast<std::chrono::microseconds>(
        now - m_lastStart ).count();
      int32_t frametimeDiff = std::max( 0, m_fpsLimitFrametime.load() - frametime );
      int32_t delay = std::max( 0, frametimeDiff );
      delay = std::min( delay, 20000 );

      Sleep::TimePoint nextStart = now + std::chrono::microseconds(delay);
      Sleep::sleepUntil( now, nextStart );
      m_lastStart = nextStart;

    }

  private:

    Sleep::TimePoint m_lastStart = { high_resolution_clock::now() };

  };

}
