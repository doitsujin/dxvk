#include <utility>

#include "d3d10_device.h"

namespace dxvk {

  D3D10Multithread::D3D10Multithread(
          IUnknown*             pParent,
          BOOL                  Protected)
  : m_parent    (pParent),
    m_protected (Protected) {
    
  }


  D3D10Multithread::~D3D10Multithread() {

  }


  ULONG STDMETHODCALLTYPE D3D10Multithread::AddRef() {
    return m_parent->AddRef();
  }

  
  ULONG STDMETHODCALLTYPE D3D10Multithread::Release() {
    return m_parent->Release();
  }

  
  HRESULT STDMETHODCALLTYPE D3D10Multithread::QueryInterface(
          REFIID                riid,
          void**                ppvObject) {
    return m_parent->QueryInterface(riid, ppvObject);
  }

  
  void STDMETHODCALLTYPE D3D10Multithread::Enter() {
    if (m_protected)
      m_mutex.lock();
  }


  void STDMETHODCALLTYPE D3D10Multithread::Leave() {
    if (m_protected)
      m_mutex.unlock();
  }


  BOOL STDMETHODCALLTYPE D3D10Multithread::SetMultithreadProtected(
          BOOL                  bMTProtect) {
    return std::exchange(m_protected, bMTProtect);
  }


  BOOL STDMETHODCALLTYPE D3D10Multithread::GetMultithreadProtected() {
    return m_protected;
  }

}
