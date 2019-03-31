#pragma once

#include "d3d9_subresource.h"

#include "d3d9_common_texture.h"

namespace dxvk {

  using Direct3DVolume9Base = Direct3DSubresource9<IDirect3DVolume9>;
  class Direct3DVolume9 final : public Direct3DVolume9Base {

  public:

    Direct3DVolume9(
            Direct3DDevice9Ex*        pDevice,
      const D3D9TextureDesc*          pDesc);

    Direct3DVolume9(
            Direct3DDevice9Ex*         pDevice,
            Direct3DCommonTexture9*    pTexture,
            UINT                       Face,
            UINT                       MipLevel,
            IUnknown*                  pContainer);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetDesc(D3DVOLUME_DESC *pDesc) final;

    HRESULT STDMETHODCALLTYPE LockBox(D3DLOCKED_BOX* pLockedBox, CONST D3DBOX* pBox, DWORD Flags) final;

    HRESULT STDMETHODCALLTYPE UnlockBox() final;

  };
}