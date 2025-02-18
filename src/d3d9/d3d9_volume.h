#pragma once

#include "d3d9_subresource.h"

#include "d3d9_common_texture.h"

namespace dxvk {

  using D3D9VolumeBase = D3D9Subresource<IDirect3DVolume9>;
  class D3D9Volume final : public D3D9VolumeBase {

  public:

    D3D9Volume(
            D3D9DeviceEx*             pDevice,
      const D3D9_COMMON_TEXTURE_DESC* pDesc,
      const bool                      Extended);

    D3D9Volume(
            D3D9DeviceEx*             pDevice,
      const bool                      Extended,
            D3D9CommonTexture*        pTexture,
            UINT                      Face,
            UINT                      MipLevel,
            IDirect3DBaseTexture9*    pContainer);

    void AddRefPrivate();

    void ReleasePrivate();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetDesc(D3DVOLUME_DESC *pDesc) final;

    HRESULT STDMETHODCALLTYPE LockBox(D3DLOCKED_BOX* pLockedBox, CONST D3DBOX* pBox, DWORD Flags) final;

    HRESULT STDMETHODCALLTYPE UnlockBox() final;

  };
}