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
    
  public:
    
    Spinlock() { }
    ~Spinlock() { }
    
    Spinlock             (const Spinlock&) = delete;
    Spinlock& operator = (const Spinlock&) = delete;
    
    void lock() {
      while (!this->try_lock())
        dxvk::this_thread::yield();
    }
    
    void unlock() {
      m_lock.store(0, std::memory_order_release);
    }
    
    bool try_lock() {
      uint32_t expected = 0;
      return m_lock.compare_exchange_strong(expected, 1,
        std::memory_order_acquire,
        std::memory_order_relaxed);
    }
    
  private:
    
    std::atomic<uint32_t> m_lock = { 0 };
    
  };
  
}
