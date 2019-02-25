#pragma once

#include "d3d9_include.h"

namespace dxvk {

  /**
   * \brief Device mutex
   *
   * Effectively implements a recursive spinlock
   * which is used to lock the D3D9 device.
   */
  class Direct3DDeviceMutex9 {

  public:

    void lock();

    void unlock();

    bool try_lock();

  private:

    std::atomic<uint32_t> m_owner = { 0u };
    uint32_t              m_counter = { 0u };

  };


  /**
   * \brief Device lock
   *
   * Lightweight RAII wrapper that implements
   * a subset of the functionality provided by
   * \c std::unique_lock, with the goal of being
   * cheaper to construct and destroy.
   */
  class Direct3DDeviceLock9 {

  public:

    Direct3DDeviceLock9()
      : m_mutex(nullptr) { }

    Direct3DDeviceLock9(Direct3DDeviceMutex9& mutex)
      : m_mutex(&mutex) {
      mutex.lock();
    }

    Direct3DDeviceLock9(Direct3DDeviceLock9&& other)
      : m_mutex(other.m_mutex) {
      other.m_mutex = nullptr;
    }

    Direct3DDeviceLock9& operator = (Direct3DDeviceLock9&& other) {
      if (m_mutex)
        m_mutex->unlock();

      m_mutex = other.m_mutex;
      other.m_mutex = nullptr;
      return *this;
    }

    ~Direct3DDeviceLock9() {
      if (m_mutex != nullptr)
        m_mutex->unlock();
    }

  private:

    Direct3DDeviceMutex9* m_mutex;

  };


  /**
   * \brief D3D9 context lock
   */
  class Direct3DMultithread9 {

  public:

    Direct3DMultithread9(
      BOOL                  Protected);

    ~Direct3DMultithread9();

    Direct3DDeviceLock9 AcquireLock() {
      return m_protected
        ? Direct3DDeviceLock9(m_mutex)
        : Direct3DDeviceLock9();
    }

  private:

    BOOL      m_protected;

    Direct3DDeviceMutex9 m_mutex;

  };

}