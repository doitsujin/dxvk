#pragma once

#include "d3d9_device.h"

namespace dxvk {

  template <typename... Base>
  class Direct3DDeviceChild9 : public ComObject<Base...> {

  public:

    Direct3DDeviceChild9(Direct3DDevice9Ex* device)
      : m_parent{ device } {}

    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) {
      InitReturnPtr(ppDevice);

      if (ppDevice == nullptr)
        return D3DERR_INVALIDCALL;

      *ppDevice = ref(static_cast<IDirect3DDevice9Ex*>(m_parent));
      return D3D_OK;
    }

    Direct3DDevice9Ex* GetParent() {
      return m_parent;
    }

  protected:

    Direct3DDevice9Ex* m_parent;

  };

}