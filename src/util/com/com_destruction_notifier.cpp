#include "com_destruction_notifier.h"

namespace dxvk {

  D3DDestructionNotifier::D3DDestructionNotifier(IUnknown* pParent)
  : m_parent(pParent) {

  }


  D3DDestructionNotifier::~D3DDestructionNotifier() {
    Notify();
  }


  ULONG STDMETHODCALLTYPE D3DDestructionNotifier::AddRef() {
    return m_parent->AddRef();
  }


  ULONG STDMETHODCALLTYPE D3DDestructionNotifier::Release() {
    return m_parent->Release();
  }


  HRESULT STDMETHODCALLTYPE D3DDestructionNotifier::QueryInterface(
          REFIID                    iid,
          void**                    ppvObject) {
    return m_parent->QueryInterface(iid, ppvObject);
  }


  HRESULT STDMETHODCALLTYPE D3DDestructionNotifier::RegisterDestructionCallback(
          PFN_DESTRUCTION_CALLBACK  pCallback,
          void*                     pData,
          UINT*                     pCallbackId) {
    std::lock_guard lock(m_mutex);

    if (!pCallback)
      return DXGI_ERROR_INVALID_CALL;

    auto& cb = m_callbacks.emplace_back();
    cb.cb = pCallback;
    cb.data = pData;

    if (pCallbackId) {
      cb.id = ++m_nextId;
      *pCallbackId = cb.id;
    }

    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3DDestructionNotifier::UnregisterDestructionCallback(
          UINT                      CallbackId) {
    std::lock_guard lock(m_mutex);

    if (!CallbackId)
      return DXGI_ERROR_NOT_FOUND;

    for (size_t i = 0; i < m_callbacks.size(); i++) {
      if (m_callbacks[i].id == CallbackId) {
        m_callbacks[i] = std::move(m_callbacks.back());
        m_callbacks.pop_back();
        return S_OK;
      }
    }

    return DXGI_ERROR_NOT_FOUND;
  }


  void D3DDestructionNotifier::Notify() {
    for (size_t i = 0; i < m_callbacks.size(); i++)
      m_callbacks[i].cb(m_callbacks[i].data);

    m_callbacks.clear();
  }

}
