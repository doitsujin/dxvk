#pragma once

#include "d3d9_include.h"

namespace dxvk {

  /**
   * \brief Device lock
   *
   * Lightweight RAII wrapper that implements
   * a subset of the functionality provided by
   * \c std::unique_lock, with the goal of being
   * cheaper to construct and destroy.
   */
  class D3D9DeviceLock {

  public:

    D3D9DeviceLock()
      : m_mutex(nullptr) { }

    D3D9DeviceLock(sync::RecursiveSpinlock& mutex)
      : m_mutex(&mutex) {
      mutex.lock();
    }

    D3D9DeviceLock(D3D9DeviceLock&& other)
      : m_mutex(other.m_mutex) {
      other.m_mutex = nullptr;
    }

    D3D9DeviceLock& operator = (D3D9DeviceLock&& other) {
      if (m_mutex)
        m_mutex->unlock();

      m_mutex = other.m_mutex;
      other.m_mutex = nullptr;
      return *this;
    }

    ~D3D9DeviceLock() {
      if (m_mutex != nullptr)
        m_mutex->unlock();
    }

  private:

    sync::RecursiveSpinlock* m_mutex;

  };


  /**
   * \brief D3D9 context lock
   */
  class D3D9Multithread {

  public:

    D3D9Multithread(
      BOOL                  Protected);

    D3D9DeviceLock AcquireLock() {
      return m_protected
        ? D3D9DeviceLock(m_mutex)
        : D3D9DeviceLock();
    }

  private:

    BOOL            m_protected;

    sync::RecursiveSpinlock m_mutex;

  };

}