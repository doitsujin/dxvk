#include "d3d9_device.h"

namespace dxvk {

  D3D9DeviceLock D3D9Multithread::LockContested(uint32_t threadId) {
    // Spin until we can take ownership of the lock.
    sync::spin(2000, [this, threadId] {
      uint32_t expected = 0u;
      return m_owner.compare_exchange_weak(expected, threadId, std::memory_order_acquire);
    });

    return D3D9DeviceLock(*this);
  }

}
