#pragma once

#include <d3dcommon.h>

#include "../util/thread.h"
#include "../util/util_small_vector.h"

namespace dxvk {

  class D3DDestructionNotifier : public ID3DDestructionNotifier {

  public:

    D3DDestructionNotifier(IUnknown* pParent);

    ~D3DDestructionNotifier();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                    iid,
            void**                    ppvObject);

    HRESULT STDMETHODCALLTYPE RegisterDestructionCallback(
            PFN_DESTRUCTION_CALLBACK  pCallback,
            void*                     pData,
            UINT*                     pCallbackId);

    HRESULT STDMETHODCALLTYPE UnregisterDestructionCallback(
            UINT                      CallbackId);

    void Notify();

  private:

    struct Entry {
      uint32_t id = 0u;
      PFN_DESTRUCTION_CALLBACK cb = nullptr;
      void* data = nullptr;
    };

    IUnknown*   m_parent = nullptr;

    dxvk::mutex m_mutex;
    uint32_t    m_nextId = 0u;

    small_vector<Entry, 2> m_callbacks;

  };

}
