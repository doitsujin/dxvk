#include "d3d9_device.h"

namespace dxvk {

  void D3D9DeviceMutex::lock() {
    while (!try_lock())
      dxvk::this_thread::yield();
  }


  void D3D9DeviceMutex::unlock() {
    if (likely(m_counter == 0))
      m_owner.store(0, std::memory_order_release);
    else
      m_counter -= 1;
  }


  bool D3D9DeviceMutex::try_lock() {
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


  D3D9Multithread::D3D9Multithread(
          BOOL                  Protected)
    : m_protected( Protected ) { }

}