#pragma once

#include <chrono>
#include <cstdint>

#if defined(_WIN32) && !defined(__WINE__)
#include <windows.h>
#endif

namespace dxvk {

#if defined(_WIN32) && !defined(__WINE__)
  struct high_resolution_clock {
    static constexpr bool is_steady = true;

    using rep        = int64_t;
    using period     = std::nano;
    using duration   = std::chrono::nanoseconds;
    using time_point = std::chrono::time_point<high_resolution_clock>;

    static inline time_point now() noexcept {
      // Keep the frequency static, this doesn't change at all.
      static const int64_t freq = getFrequency();
      const int64_t counter     = getCounter();

      const int64_t whole = (counter / freq) * period::den;
      const int64_t part  = (counter % freq) * period::den / freq;

      return time_point(duration(whole + part));
    }

    static inline int64_t getFrequency() {
      LARGE_INTEGER freq;
      QueryPerformanceFrequency(&freq);

      return freq.QuadPart;
    }

    static inline int64_t getCounter() {
      LARGE_INTEGER count;
      QueryPerformanceCounter(&count);

      return count.QuadPart;
    }
  };
#else
  using high_resolution_clock = std::chrono::high_resolution_clock;
#endif

}