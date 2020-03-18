#pragma once

#include "d3d10_include.h"

namespace dxvk {
  
  /**
   * \brief Device lock
   * 
   * Lightweight RAII wrapper that implements
   * a subset of the functionality provided by
   * \c std::unique_lock, with the goal of being
   * cheaper to construct and destroy.
   */  
  class D3D10DeviceLock {

  public:

    D3D10DeviceLock()
    : m_mutex(nullptr) { }

    D3D10DeviceLock(sync::RecursiveSpinlock& mutex)
    : m_mutex(&mutex) {
      mutex.lock();
    }

    D3D10DeviceLock(D3D10DeviceLock&& other)
    : m_mutex(other.m_mutex) {
      other.m_mutex = nullptr;
    }

    D3D10DeviceLock& operator = (D3D10DeviceLock&& other) {
      if (m_mutex)
        m_mutex->unlock();
      
      m_mutex = other.m_mutex;
      other.m_mutex = nullptr;
      return *this;
    }

    ~D3D10DeviceLock() {
      if (unlikely(m_mutex != nullptr))
        m_mutex->unlock();
    }

  private:

    sync::RecursiveSpinlock* m_mutex;
    
  };

  
  /**
   * \brief D3D10 device and D3D11 context lock
   * 
   * Can be queried from the D3D10 device or from
   * any D3D11 context in order to make individual
   * calls thread-safe. Provides methods to lock
   * the device or context explicitly.
   */
  class D3D10Multithread : public ID3D10Multithread {

  public:

    D3D10Multithread(
            IUnknown*             pParent,
            BOOL                  Protected);
    
    ~D3D10Multithread();

    ULONG STDMETHODCALLTYPE AddRef() final;
    
    ULONG STDMETHODCALLTYPE Release() final;
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                riid,
            void**                ppvObject) final;
    
    void STDMETHODCALLTYPE Enter() final;

    void STDMETHODCALLTYPE Leave() final;

    BOOL STDMETHODCALLTYPE SetMultithreadProtected(
            BOOL                  bMTProtect) final;

    BOOL STDMETHODCALLTYPE GetMultithreadProtected() final;

    D3D10DeviceLock AcquireLock() {
      return unlikely(m_protected)
        ? D3D10DeviceLock(m_mutex)
        : D3D10DeviceLock();
    }
    
  private:

    IUnknown* m_parent;
    BOOL      m_protected;

    sync::RecursiveSpinlock m_mutex;

  };

}