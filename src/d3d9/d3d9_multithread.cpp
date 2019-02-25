#include "d3d9_device.h"

namespace dxvk {

  void Direct3DDeviceMutex9::lock() {
    while (!try_lock())
      dxvk::this_thread::yield();
  }


  void Direct3DDeviceMutex9::unlock() {
    if (likely(m_counter == 0))
      m_owner.store(0, std::memory_order_release);
    else
      m_counter -= 1;
  }


  bool Direct3DDeviceMutex9::try_lock() {
    uint32_t threadId = GetCurrentThreadId();
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


  Direct3DMultithread9::Direct3DMultithread9(
          BOOL                  Protected)
    : m_protected{ Protected } {
    
  }


  Direct3DMultithread9::~Direct3DMultithread9() {

  }

}