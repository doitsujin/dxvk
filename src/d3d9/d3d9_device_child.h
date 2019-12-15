#pragma once

#include "d3d9_include.h"

namespace dxvk {

  class D3D9DeviceEx;

  template <typename Base>
  class D3D9DeviceChild : public ComObjectClamp<Base> {

  public:

    D3D9DeviceChild(D3D9DeviceEx* pDevice)
      : m_parent( pDevice ) { }

    ULONG STDMETHODCALLTYPE AddRef() {
      uint32_t refCount = this->m_refCount++;
      if (unlikely(!refCount)) {
        this->AddRefPrivate();
        GetDevice()->AddRef();
      }

      return refCount + 1;
    }
    
    ULONG STDMETHODCALLTYPE Release() {
      uint32_t refCount = --this->m_refCount;
      if (unlikely(!refCount)) {
        auto* pDevice = GetDevice();
        this->ReleasePrivate();
        pDevice->Release();
      }
      return refCount;
    }

    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) {
      InitReturnPtr(ppDevice);

      if (ppDevice == nullptr)
        return D3DERR_INVALIDCALL;

      *ppDevice = ref(GetDevice());
      return D3D_OK;
    }

    IDirect3DDevice9Ex* GetDevice() {
      return reinterpret_cast<IDirect3DDevice9Ex*>(m_parent);
    }

    D3D9DeviceEx* GetParent() {
      return m_parent;
    }

  protected:

    D3D9DeviceEx* m_parent;

  };

}