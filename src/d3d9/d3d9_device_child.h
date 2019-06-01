#pragma once

#include "d3d9_device.h"

namespace dxvk {

  template <typename... Base>
  class D3D9DeviceChild : public ComObjectClamp<Base...> {

  public:

    D3D9DeviceChild(D3D9DeviceEx* pDevice)
      : m_parent( pDevice ) { }

    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) {
      InitReturnPtr(ppDevice);

      if (ppDevice == nullptr)
        return D3DERR_INVALIDCALL;

      *ppDevice = ref(static_cast<IDirect3DDevice9Ex*>(m_parent));
      return D3D_OK;
    }

    D3D9DeviceEx* GetParent() {
      return m_parent;
    }

  protected:

    D3D9DeviceEx* m_parent;

  };

}