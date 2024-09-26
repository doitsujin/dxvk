#pragma once

#include "d3d8_include.h"

namespace dxvk {

  /**
   * \brief Device lock
   *
   * Lightweight RAII wrapper that implements
   * a subset of the functionality provided by
   * \c std::unique_lock, with the goal of being
   * cheaper to construct and destroy.
   */
  class D3D8DeviceLock {

  public:

    D3D8DeviceLock()
      : m_mutex(nullptr) { }

    D3D8DeviceLock(sync::RecursiveSpinlock& mutex)
      : m_mutex(&mutex) {
      mutex.lock();
    }

    D3D8DeviceLock(D3D8DeviceLock&& other)
      : m_mutex(other.m_mutex) {
      other.m_mutex = nullptr;
    }

    D3D8DeviceLock& operator = (D3D8DeviceLock&& other) {
      if (m_mutex)
        m_mutex->unlock();

      m_mutex = other.m_mutex;
      other.m_mutex = nullptr;
      return *this;
    }

    ~D3D8DeviceLock() {
      if (m_mutex != nullptr)
        m_mutex->unlock();
    }

  private:

    sync::RecursiveSpinlock* m_mutex;

  };


  /**
   * \brief D3D8 context lock
   */
  class D3D8Multithread {

  public:

    D3D8Multithread(
      BOOL                  Protected);

    D3D8DeviceLock AcquireLock() {
      return m_protected
        ? D3D8DeviceLock(m_mutex)
        : D3D8DeviceLock();
    }

  private:

    BOOL            m_protected;

    sync::RecursiveSpinlock m_mutex;

  };

}