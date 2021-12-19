#include "sync_recursive.h"
#include "sync_spinlock.h"

namespace dxvk::sync {

  void RecursiveSpinlock::lock() {
    uint32_t threadId = GetCurrentThreadId();
    spin(2000, [this, threadId] { return try_lock_impl(threadId); });
  }


  void RecursiveSpinlock::unlock() {
    if (likely(m_counter == 0))
      m_owner.store(0, std::memory_order_release);
    else
      m_counter -= 1;
  }


  bool RecursiveSpinlock::try_lock_impl(uint32_t threadId) {
    uint32_t expected = 0;

    bool status = m_owner.compare_exchange_weak(
      expected, threadId, std::memory_order_acquire);
    
    if (status)
      return true;
    
    if (expected != threadId)
      return false;
    
    m_counter += 1;
    return true;
  }

}
