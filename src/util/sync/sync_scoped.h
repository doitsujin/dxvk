#pragma once

#include <atomic>

#include "../com/com_include.h"

#include "../thread.h"
#include "../util_math.h"

#include "sync_spinlock.h"

namespace dxvk::sync {

  class ScopedDeviceLock;

  /**
   * \brief Scoped recursive spinlock guard
   */
  class ScopedDeviceGuard {

  public:

    ScopedDeviceGuard() = default;

    ScopedDeviceGuard(ScopedDeviceLock* lock)
    : m_lock(lock) { }

    ScopedDeviceGuard(ScopedDeviceGuard&& other)
    : m_lock(std::exchange(m_lock, nullptr)) { }


    ScopedDeviceGuard& operator = (ScopedDeviceGuard&& other) {
      unlock();

      m_lock = std::exchange(other.m_lock, nullptr);
      return *this;
    }

    ~ScopedDeviceGuard() {
      unlock();
    }

  private:

    ScopedDeviceLock* m_lock = nullptr;

    void unlock();

  };


  /**
   * \brief Scoped recursive spinlock
   *
   * Implements a low-overhead recursive spinlock that can be conditionally
   * enabled, and supports use cases where locks are guaranteed to be released
   * in the same order they are taken. This allows us to avoid atomics on unlock,
   * unlike regular recursive locks.
   */
  class ScopedDeviceLock {
    static constexpr uint32_t InvalidTid = -1u;
    friend ScopedDeviceGuard;
  public:

    ScopedDeviceLock(bool enableProtection)
    : m_enable(enableProtection) { }

    ScopedDeviceGuard acquire() {
      if (likely(!m_enable))
        return ScopedDeviceGuard();

      // Try to take ownership of the lock
      uint32_t expected = 0u;
      uint32_t threadId = dxvk::this_thread::get_id();

      if (likely(m_owner.compare_exchange_weak(expected, threadId, std::memory_order_acquire)))
        return ScopedDeviceGuard(this);

      // Current thread already holds lock. We know the lock guard
      // that set the owner will be unlocked *after* this one.
      if (expected == threadId)
        return ScopedDeviceGuard();

      // Slow path
      return lockContested(threadId);
    }

  private:

    alignas(CACHE_LINE_SIZE)
    std::atomic<uint32_t> m_owner = { 0u };
    bool                  m_enable = false;

    ScopedDeviceGuard lockContested(uint32_t threadId);

    void unlock() {
      m_owner.store(0u);
    }

  };

  inline void ScopedDeviceGuard::unlock() {
    if (m_lock)
      m_lock->unlock();
  }

}
