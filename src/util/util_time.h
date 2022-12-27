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
      return get_time_from_counter(get_counter());
    }

    static inline time_point get_time_from_counter(int64_t counter) {
      // Keep the frequency static, this doesn't change at all.
      static const int64_t freq = get_frequency();

      const int64_t whole = (counter / freq) * period::den;
      const int64_t part  = (counter % freq) * period::den / freq;

      return time_point(duration(whole + part));
    }

    static inline int64_t get_frequency() {
      LARGE_INTEGER freq;
      QueryPerformanceFrequency(&freq);

      return freq.QuadPart;
    }

    static inline int64_t get_counter() {
      LARGE_INTEGER count;
      QueryPerformanceCounter(&count);

      return count.QuadPart;
    }
  };
#else
  struct high_resolution_clock : public std::chrono::high_resolution_clock {
    static inline time_point get_time_from_counter(int64_t counter) {
      return time_point() + duration(counter);
    }

    static inline int64_t get_frequency() {
      return period::den;
    }

    static inline int64_t get_counter() {
      return now().time_since_epoch().count();
    }
  };
#endif

}