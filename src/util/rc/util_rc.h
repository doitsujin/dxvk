#pragma once

#include <atomic>

#include "../util_likely.h"

namespace dxvk {
  
  /**
   * \brief Reference-counted object
   */
  class RcObject {
    
  public:
    
    /**
     * \brief Increments reference count
     * \returns New reference count
     * Reference publication is synchronized separately from the count.
     */
    force_inline uint32_t incRef() {
      return m_refCount.fetch_add(1u, std::memory_order_relaxed) + 1u;
    }
    
    /**
     * \brief Decrements reference count
     * \returns New reference count
     * Release prior accesses and acquire the release sequence before destruction.
     */
    force_inline uint32_t decRef() {
      uint32_t count = m_refCount.fetch_sub(1u, std::memory_order_release) - 1u;

      if (unlikely(!count))
        (void) m_refCount.load(std::memory_order_acquire);

      return count;
    }
    
  private:
    
    std::atomic<uint32_t> m_refCount = { 0u };
    
  };
  
}
