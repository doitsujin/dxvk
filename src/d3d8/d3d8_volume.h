#pragma once

#include "d3d8_subresource.h"

namespace dxvk {

  using D3D8VolumeBase = D3D8Subresource<d3d9::IDirect3DVolume9, IDirect3DVolume8>;
  class D3D8Volume final : public D3D8VolumeBase {

  public:

    D3D8Volume(
            D3D8Device*                   pDevice,
      const D3DPOOL                       Pool,
            IDirect3DVolumeTexture8*      pTexture,
            Com<d3d9::IDirect3DVolume9>&& pVolume);

    HRESULT STDMETHODCALLTYPE GetDesc(D3DVOLUME_DESC* pDesc);

    HRESULT STDMETHODCALLTYPE LockBox(D3DLOCKED_BOX* pLockedBox, CONST D3DBOX* pBox, DWORD Flags) final;

    HRESULT STDMETHODCALLTYPE UnlockBox() final;

  };

}