#pragma once

#include "d3d9_include.h"

namespace dxvk {
  /// Implements functions related to getting the original device creation parameters.
  class D3D9DeviceParams: public virtual IDirect3DDevice9 {
  protected:
    D3D9DeviceParams(IDirect3D9* parent, const D3DDEVICE_CREATION_PARAMETERS& params);

  private:
    IDirect3D9* m_parent;
    D3DDEVICE_CREATION_PARAMETERS m_creationParams;

    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D9** ppD3D9) final override;
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS9* pCaps) final override;
    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters) final override;
  };
}
