#pragma once

// Common methods for device-tied objects.
// - AddRef, Release from IUnknown
// - GetDevice from various classes including IDirect3DResource8

#include "d3d8_include.h"
#include "d3d8_wrapped_object.h"

namespace dxvk {

  class D3D8Device;

  template <typename D3D9, typename D3D8>
  class D3D8DeviceChild : public D3D8WrappedObject<D3D9, D3D8> {

  public:

    D3D8DeviceChild(D3D8Device* pDevice, Com<D3D9>&& Object)
      : D3D8WrappedObject<D3D9, D3D8>(std::move(Object))
      , m_parent( pDevice ) { }

    ULONG STDMETHODCALLTYPE AddRef() {
      uint32_t refCount = this->m_refCount++;
      if (unlikely(!refCount)) {
        this->AddRefPrivate();
        GetDevice()->AddRef();
      }

      return refCount + 1;
    }
    
    ULONG STDMETHODCALLTYPE Release() {
      // ignore Release calls on objects with 0 refCount
      if(unlikely(!this->m_refCount))
        return this->m_refCount;

      uint32_t refCount = --this->m_refCount;
      if (unlikely(!refCount)) {
        auto* pDevice = GetDevice();
        this->ReleasePrivate();
        pDevice->Release();
      }
      return refCount;
    }

    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8** ppDevice) {
      InitReturnPtr(ppDevice);

      if (ppDevice == nullptr)
        return D3DERR_INVALIDCALL;

      *ppDevice = ref(GetDevice());
      return D3D_OK;
    }

    IDirect3DDevice8* GetDevice() {
      return reinterpret_cast<IDirect3DDevice8*>(m_parent);
    }

    D3D8Device* GetParent() {
      return m_parent;
    }

  protected:

    D3D8Device* m_parent;

  };

}