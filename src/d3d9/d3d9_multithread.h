#pragma once

#include "d3d9_include.h"

namespace dxvk {

  class D3D9Multithread;

  /**
   * \brief Scoped device lock
   */
  class D3D9DeviceLock {

  public:

    D3D9DeviceLock() { }

    D3D9DeviceLock(D3D9Multithread& mutex)
    : m_mutex(&mutex) { }

    D3D9DeviceLock(D3D9DeviceLock&& other)
    : m_mutex(other.m_mutex) {
      other.m_mutex = nullptr;
    }

    D3D9DeviceLock& operator = (D3D9DeviceLock&& other) {
      Unlock();

      m_mutex = other.m_mutex;
      other.m_mutex = nullptr;
      return *this;
    }

    ~D3D9DeviceLock() {
      Unlock();
    }

  private:

    D3D9Multithread* m_mutex = nullptr;

    void Unlock();

  };


  /**
   * \brief D3D9 context lock
   */
  class D3D9Multithread {
    static constexpr uint32_t InvalidTid = -1u;

    friend D3D9DeviceLock;
  public:

    D3D9Multithread(BOOL Protected)
    : m_protected(Protected) { }

    /**
     * \brief Acquires lock
     *
     * If the calling thread already owns the lock, this will return
     * an empty lock guard and rely entirely on proper scoping.
     * \returns Lock guard
     */
    D3D9DeviceLock AcquireLock() {
      if (likely(!m_protected))
        return D3D9DeviceLock();

      uint32_t expected = 0u;
      uint32_t threadId = dxvk::this_thread::get_id();

      if (likely(m_owner.compare_exchange_weak(expected, threadId, std::memory_order_acquire)))
        return D3D9DeviceLock(*this);

      if (expected == threadId)
        return D3D9DeviceLock();

      return LockContested(threadId);
    }

  private:

    alignas(CACHE_LINE_SIZE)
    std::atomic<uint32_t>   m_owner     = { 0u };
    BOOL                    m_protected = false;

    D3D9DeviceLock LockContested(uint32_t threadId);

    void Unlock() {
      m_owner.store(0u, std::memory_order_release);
    }

  };

  inline void D3D9DeviceLock::Unlock() {
    if (m_mutex)
      m_mutex->Unlock();
  }

}
