#include "d3d8_volume.h"

#include "d3d8_d3d9_util.h"

namespace dxvk {

  D3D8Volume::D3D8Volume(
          D3D8Device*                   pDevice,
    const D3DPOOL                       Pool,
          IDirect3DVolumeTexture8*      pTexture,
          Com<d3d9::IDirect3DVolume9>&& pVolume)
    : D3D8VolumeBase(pDevice, Pool, std::move(pVolume), pTexture) {
  }

  HRESULT STDMETHODCALLTYPE D3D8Volume::GetDesc(D3DVOLUME_DESC* pDesc) {
    if (unlikely(pDesc == nullptr))
      return D3DERR_INVALIDCALL;

    d3d9::D3DVOLUME_DESC desc;
    HRESULT res = GetD3D9()->GetDesc(&desc);

    if (likely(SUCCEEDED(res)))
      ConvertVolumeDesc8(&desc, pDesc);

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Volume::LockBox(D3DLOCKED_BOX* pLockedBox, CONST D3DBOX* pBox, DWORD Flags) {
    return GetD3D9()->LockBox(
      reinterpret_cast<d3d9::D3DLOCKED_BOX*>(pLockedBox),
      reinterpret_cast<const d3d9::D3DBOX*>(pBox),
      Flags
    );
  }

  HRESULT STDMETHODCALLTYPE D3D8Volume::UnlockBox() {
    return GetD3D9()->UnlockBox();
  }

}