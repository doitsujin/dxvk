#pragma once

#include <atomic>

#include "../thread.h"

#include "../util_bit.h"
#include "../util_likely.h"

namespace dxvk::sync {

  /**
   * \brief Generic spin function
   *
   * Blocks calling thread until a condition becomes
   * \c true, calling \c yield every few iterations.
   * \param [in] spinCount Number of probes between each yield
   * \param [in] fn Condition to test
   */
  template<typename Fn>
  void spin(uint32_t spinCount, const Fn& fn) {
    while (unlikely(!fn())) {
      for (uint32_t i = 1; i < spinCount; i++) {
        #if defined(DXVK_ARCH_X86)
        _mm_pause();
        #elif defined(DXVK_ARCH_ARM64)
        __asm__ __volatile__ ("yield");
        #else
        #error "Pause/Yield not implemented for this architecture."
        #endif
        if (fn())
          return;
      }

      dxvk::this_thread::yield();
    }
  }
  
  /**
   * \brief Spin lock
   * 
   * A low-overhead spin lock which can be used to
   * protect data structures for a short duration
   * in case the structure is not likely contested.
   */
  class Spinlock {

  public:
    
    Spinlock() { }
    ~Spinlock() { }
    
    Spinlock             (const Spinlock&) = delete;
    Spinlock& operator = (const Spinlock&) = delete;
    
    void lock() {
      spin(200, [this] { return try_lock(); });
    }
    
    void unlock() {
      m_lock.store(0, std::memory_order_release);
    }
    
    bool try_lock() {
      return likely(!m_lock.load())
          && likely(!m_lock.exchange(1, std::memory_order_acquire));
    }
    
  private:
    
    std::atomic<uint32_t> m_lock = { 0 };
    
  };
  
}
