#pragma once

/** Common methods for device-tied objects.
* - AddRef, Release from IUnknown
*   - Subclasses must provide AddRefPrivate and ReleasePrivate
* - GetDevice from various classes including IDirect3DResource8
*/

#include "d3d8_include.h"

namespace dxvk {

  class D3D8DeviceEx;

  template <typename Base>
  class D3D8DeviceChild : public ComObjectClamp<Base> {

  public:

    D3D8DeviceChild(D3D8DeviceEx* pDevice)
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

    D3D8DeviceEx* GetParent() {
      return m_parent;
    }

  protected:

    D3D8DeviceEx* m_parent;

  };

}