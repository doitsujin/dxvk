#pragma once

#include "d3d9_include.h"

#include "../util/com/com_private_data.h"

namespace dxvk {
  /// This class should be inherited by classes which are device resources.
  template <typename R>
  class D3D9Resource: public R {
  public:
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) final override {
      InitReturnPtr(ppDevice);
      CHECK_NOT_NULL(ppDevice);

      *ppDevice = m_parent.ref();

      return D3D_OK;
    }

    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Guid, const void* Data,
      DWORD Size, DWORD Flags) final override {
      CHECK_NOT_NULL(Data);

      HRESULT result;

      if (Flags & D3DSPD_IUNKNOWN) {
        const auto ptr = *reinterpret_cast<IUnknown* const*>(Data);

        if (Size != sizeof(ptr))
          return D3DERR_INVALIDCALL;

        result = m_privateData.setInterface(Guid, ptr);
      } else {
        result = m_privateData.setData(Guid, Size, Data);
      }

      if (FAILED(result)) {
        return D3DERR_INVALIDCALL;
      }

      return D3D_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Guid, void* Data,
      DWORD* Size) final override {
      CHECK_NOT_NULL(Data);

      UINT sz = *Size;

      if (FAILED(m_privateData.getData(Guid, &sz, Data)))
        return D3DERR_NOTFOUND;

      *Size = sz;

      return D3D_OK;
    }

    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID Guid) final override {
      if (FAILED(m_privateData.setData(Guid, 0, nullptr)))
        return D3DERR_INVALIDCALL;
      return D3D_OK;
    }

    // TODO: the following functions are hints
    // which we could use improve performance.

    DWORD STDMETHODCALLTYPE SetPriority(DWORD Priority) final override {
      const auto old = m_priority;
      m_priority = Priority;
      return old;
    }

    DWORD STDMETHODCALLTYPE GetPriority() final override {
      return m_priority;
    }

    void STDMETHODCALLTYPE PreLoad() final override {
    }

  protected:
    void InitParent(IDirect3DDevice9* parent) {
      m_parent = parent;
    }

    template <typename T>
    void InitParent(D3D9Resource<T>* resource) {
      m_parent = resource->m_parent;
    }

    template <typename T>
    friend class D3D9Resource;

  private:
    Com<IDirect3DDevice9> m_parent;
    ComPrivateData m_privateData;
    DWORD m_priority{ };
  };
}
