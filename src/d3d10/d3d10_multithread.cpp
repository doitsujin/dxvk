#include "d3d10_device.h"

namespace dxvk {

  void D3D10DeviceMutex::lock() {
    while (!try_lock())
      dxvk::this_thread::yield();
  }


  void D3D10DeviceMutex::unlock() {
    if (likely(m_counter == 0))
      m_owner.store(0, std::memory_order_release);
    else
      m_counter -= 1;
  }


  bool D3D10DeviceMutex::try_lock() {
    uint32_t threadId = dxvk::this_thread::get_id();
    uint32_t expected = 0;

    bool status = m_owner.compare_exchange_weak(
      expected, threadId, std::memory_order_acquire);
    
    if (status)
      return true;
    
    if (expected != threadId)
      return false;
    
    m_counter += 1;
    return true;
  }


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