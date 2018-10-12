#pragma once

#include "d3d10_include.h"

namespace dxvk {

  using D3D10DeviceMutex = sync::Spinlock;
  using D3D10DeviceLock = std::unique_lock<D3D10DeviceMutex>;
  
  class D3D10Device;

  class D3D10Multithread : public ID3D10Multithread {

  public:

    D3D10Multithread(D3D10Device* pDevice)
    : m_device(pDevice) { }

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                    riid,
            void**                    ppvObject);
    
    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    void STDMETHODCALLTYPE Enter();

    void STDMETHODCALLTYPE Leave();

    BOOL STDMETHODCALLTYPE GetMultithreadProtected();

    BOOL STDMETHODCALLTYPE SetMultithreadProtected(BOOL Enable);

  private:

    D3D10Device*    m_device;
    D3D10DeviceLock m_lock;
    bool            m_enabled = true;

  };

}