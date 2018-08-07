#pragma once

#include "d3d9_include.h"

namespace dxvk {
  /// Utility class inherited by classes which are created by D3D9Device.
  class D3D9DeviceChild {
  protected:
    D3D9DeviceChild(IDirect3DDevice9* parent)
      : m_parent(parent) {
    }

    HRESULT GetDevice(IDirect3DDevice9** ppDevice) {
      InitReturnPtr(ppDevice);
      CHECK_NOT_NULL(ppDevice);

      *ppDevice = m_parent.ref();

      return D3D_OK;
    }
  private:
    Com<IDirect3DDevice9> m_parent;
  };
}
