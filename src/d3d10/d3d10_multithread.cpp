#include "d3d10_multithread.h"
#include "d3d10_device.h"

namespace dxvk {

  HRESULT STDMETHODCALLTYPE D3D10Multithread::QueryInterface(
          REFIID                    riid,
          void**                    ppvObject) {
    return m_device->QueryInterface(riid, ppvObject);
  }

  
  ULONG STDMETHODCALLTYPE D3D10Multithread::AddRef() {
    return m_device->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3D10Multithread::Release() {
    return m_device->Release();
  }


  void STDMETHODCALLTYPE D3D10Multithread::Enter() {
    m_lock = m_device->LockDevice();
  }


  void STDMETHODCALLTYPE D3D10Multithread::Leave() {
    m_lock.unlock();
  }


  BOOL STDMETHODCALLTYPE D3D10Multithread::GetMultithreadProtected() {
    return m_enabled;
  }


  BOOL STDMETHODCALLTYPE D3D10Multithread::SetMultithreadProtected(BOOL Enable) {
    return std::exchange(m_enabled, Enable);
  }

}