#include "sync_scoped.h"

namespace dxvk::sync {

  ScopedDeviceGuard ScopedDeviceLock::lockContested(uint32_t threadId) {
    // Spin until we can take ownership of the lock.
    sync::spin(2000, [this, threadId] {
      uint32_t expected = 0u;
      return m_owner.compare_exchange_weak(expected, threadId, std::memory_order_acquire);
    });

    return ScopedDeviceGuard(this);
  }

}
