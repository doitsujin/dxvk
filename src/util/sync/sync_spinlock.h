#pragma once

#include <atomic>
#include "../thread.h"

namespace dxvk::sync {
  
  /**
   * \brief Spin lock
   * 
   * A low-overhead spin lock which can be used to
   * protect data structures for a short duration
   * in case the structure is not likely contested.
   */
  class Spinlock {
    constexpr static uint32_t SpinCount = 200;
  public:
    
    Spinlock() { }
    ~Spinlock() { }
    
    Spinlock             (const Spinlock&) = delete;
    Spinlock& operator = (const Spinlock&) = delete;
    
    void lock() {
      while (unlikely(!try_lock())) {
        for (uint32_t i = 1; i < SpinCount; i++) {
          if (try_lock())
            return;
        }

        dxvk::this_thread::yield();
      }
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
