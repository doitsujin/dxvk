#pragma once

#include "d3d8_resource.h"
#include "d3d8_d3d9_util.h"

namespace dxvk {
  
  using D3D8VolumeBase = D3D8Resource<d3d9::IDirect3DVolume9, IDirect3DVolume8>;
  class D3D8Volume final : public D3D8VolumeBase {

  public:

    D3D8Volume(
      D3D8DeviceEx* pDevice,
      Com<d3d9::IDirect3DVolume9>&& pVolume);

    // TODO: QueryInterface
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      return D3D_OK;
    }

    HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void** ppContainer) {
      return GetD3D9()->GetContainer(riid, ppContainer);
    }

    HRESULT STDMETHODCALLTYPE GetDesc(D3DVOLUME_DESC* pDesc) {
      d3d9::D3DVOLUME_DESC desc;
      HRESULT res = GetD3D9()->GetDesc(&desc);
      ConvertVolumeDesc8(&desc, pDesc);
      return res;
    }

    HRESULT STDMETHODCALLTYPE LockBox(D3DLOCKED_BOX* pLockedBox, CONST D3DBOX* pBox, DWORD Flags) final {
      return GetD3D9()->LockBox(
        reinterpret_cast<d3d9::D3DLOCKED_BOX*>(pLockedBox),
        reinterpret_cast<const d3d9::D3DBOX*>(pBox),
        Flags
      );
    }

    HRESULT STDMETHODCALLTYPE UnlockBox() final {
      return GetD3D9()->UnlockBox();
    }

  };

}