#pragma once

#include <chrono>
#include <cstdint>

#if defined(_WIN32) && !defined(__WINE__)
#include <windows.h>
#elif defined(__linux__)
#include <time.h>
#endif

#include "../vulkan/vulkan_loader.h"

namespace dxvk {

#if defined(_WIN32) && !defined(__WINE__)
  struct high_resolution_clock {
    static constexpr bool is_steady = true;

    using rep        = int64_t;
    using period     = std::nano;
    using duration   = std::chrono::nanoseconds;
    using time_point = std::chrono::time_point<high_resolution_clock>;

    static time_point now() noexcept {
      return get_time_from_counter(get_counter());
    }

    static time_point get_time_from_counter(int64_t counter) {
      // Keep the frequency static, this doesn't change at all.
      static const int64_t freq = get_frequency();

      const int64_t whole = (counter / freq) * period::den;
      const int64_t part  = (counter % freq) * period::den / freq;

      return time_point(duration(whole + part));
    }

    static int64_t get_frequency() {
      LARGE_INTEGER freq;
      QueryPerformanceFrequency(&freq);

      return freq.QuadPart;
    }

    static int64_t get_counter() {
      LARGE_INTEGER count;
      QueryPerformanceCounter(&count);

      return count.QuadPart;
    }

    static VkTimeDomainKHR get_vk_time_domain() {
      return VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT;
    }
  };
#elif defined(__linux__)
  struct high_resolution_clock {
    static constexpr bool is_steady = true;

    using rep        = int64_t;
    using period     = std::nano;
    using duration   = std::chrono::nanoseconds;
    using time_point = std::chrono::time_point<high_resolution_clock>;

    static time_point now() noexcept {
      return get_time_from_counter(get_counter());
    }

    static time_point get_time_from_counter(int64_t counter) {
      return time_point(duration(counter));
    }

    static int64_t get_frequency() {
      return period::den;
    }

    static int64_t get_counter() {
      timespec tp = {};
      ::clock_gettime(CLOCK_MONOTONIC, &tp);
      return int64_t(tp.tv_sec) * 1'000'000'000 + int64_t(tp.tv_nsec);
    }

    static VkTimeDomainKHR get_vk_time_domain() {
      return VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT;
    }
  };
#else
  struct high_resolution_clock : public std::chrono::high_resolution_clock {
    static time_point get_time_from_counter(int64_t counter) {
      return time_point() + duration(counter);
    }

    static int64_t get_frequency() {
      return period::den;
    }

    static int64_t get_counter() {
      return now().time_since_epoch().count();
    }

    static VkTimeDomainKHR get_vk_time_domain() {
      return VK_TIME_DOMAIN_MAX_ENUM_KHR;
    }
  };
#endif

}
