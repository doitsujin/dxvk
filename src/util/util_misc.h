#pragma once

#include <d3d9types.h>

namespace dxvk {

  inline void DecodeD3DCOLOR(D3DCOLOR color, float* rgba) {
    // Encoded in D3DCOLOR as argb
    rgba[3] = (float)((color & 0xff000000) >> 24) / 255.0f;
    rgba[0] = (float)((color & 0x00ff0000) >> 16) / 255.0f;
    rgba[1] = (float)((color & 0x0000ff00) >> 8)  / 255.0f;
    rgba[2] = (float)((color & 0x000000ff))       / 255.0f;
  }

  /**
   * \brief Computes refresh period for a given display refresh rate
   *
   * \param [in] numerator Numerator of refresh rate
   * \param [in] denominator Denominator of refresh rate
   * \returns Refresh period, in nanoseconds
   */
  inline auto computeRefreshPeriod(uint64_t numerator, uint64_t denominator) {
    using unit = std::chrono::nanoseconds;
    unit::rep ns = unit::rep(unit::period::den * denominator)
                 / unit::rep(unit::period::num * numerator);
    return unit(ns);
  }

  /**
   * \brief Computes refresh count within a given time period
   *
   * \param [in] t0 Start time
   * \param [in] t1 End time
   * \param [in] refreshPeriod Refresh period
   * \returns Number of refreshes between t1 and t0
   */
  template<typename TimePoint, typename Duration>
  uint64_t computeRefreshCount(TimePoint t0, TimePoint t1, Duration refreshPeriod) {
    if (t1 < t0)
      return 0;

    auto duration = std::chrono::duration_cast<Duration>(t1 - t0);
    return duration.count() / refreshPeriod.count();
  }

  struct scoped_bool {
    scoped_bool(bool &v) : m_val(v) {
      m_val = true;
    }
    ~scoped_bool() {
      m_val = false;
    }

    bool& m_val;
  };
}
