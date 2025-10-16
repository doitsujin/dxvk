#pragma once

#include "ddraw_include.h"

namespace dxvk {

  /**
   * \brief Device lock
   *
   * Lightweight RAII wrapper that implements
   * a subset of the functionality provided by
   * \c std::unique_lock, with the goal of being
   * cheaper to construct and destroy.
   */
  class D3DDeviceLock {

  public:

    D3DDeviceLock()
      : m_mutex(nullptr) { }

    D3DDeviceLock(sync::RecursiveSpinlock& mutex)
      : m_mutex(&mutex) {
      mutex.lock();
    }

    D3DDeviceLock(D3DDeviceLock&& other)
      : m_mutex(other.m_mutex) {
      other.m_mutex = nullptr;
    }

    D3DDeviceLock& operator = (D3DDeviceLock&& other) {
      if (m_mutex)
        m_mutex->unlock();

      m_mutex = other.m_mutex;
      other.m_mutex = nullptr;
      return *this;
    }

    ~D3DDeviceLock() {
      if (m_mutex != nullptr)
        m_mutex->unlock();
    }

  private:

    sync::RecursiveSpinlock* m_mutex;

  };


  /**
   * \brief D3D context lock
   */
  class D3DMultithread {

  public:

    D3DMultithread(
      BOOL                  Protected);

    D3DDeviceLock AcquireLock() {
      return m_protected
        ? D3DDeviceLock(m_mutex)
        : D3DDeviceLock();
    }

  private:

    BOOL            m_protected;

    sync::RecursiveSpinlock m_mutex;

  };

}