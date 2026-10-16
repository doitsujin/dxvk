#include "d3d8_surface.h"
#include "d3d8_device.h"

#include "d3d8_util.h"

namespace dxvk {

  D3D8Surface::D3D8Surface(
          D3D8Device*                     pDevice,
    const D3DPOOL                         Pool,
          IDirect3DBaseTexture8*          pTexture,
          Com<d3d9::IDirect3DSurface9>&&  pSurface)
    : D3D8SurfaceBase (pDevice, Pool, std::move(pSurface), pTexture) {
  }

  // A surface does not need to be attached to a texture
  D3D8Surface::D3D8Surface(
          D3D8Device*                     pDevice,
    const D3DPOOL                         Pool,
          Com<d3d9::IDirect3DSurface9>&&  pSurface)
    : D3D8Surface (pDevice, Pool, nullptr, std::move(pSurface)) {
  }

  HRESULT STDMETHODCALLTYPE D3D8Surface::GetDesc(D3DSURFACE_DESC* pDesc) {
    if (unlikely(pDesc == nullptr))
      return D3DERR_INVALIDCALL;

    d3d9::D3DSURFACE_DESC desc;
    HRESULT res = GetD3D9()->GetDesc(&desc);
    if (unlikely(FAILED(res)))
      return res;

    ConvertSurfaceDesc8(&desc, pDesc);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Surface::LockRect(
      D3DLOCKED_RECT* pLockedRect,
      CONST RECT*     pRect,
      DWORD           Flags) {
    return GetD3D9()->LockRect(reinterpret_cast<d3d9::D3DLOCKED_RECT*>(pLockedRect), pRect, Flags);
  }

  HRESULT STDMETHODCALLTYPE D3D8Surface::UnlockRect() {
    return GetD3D9()->UnlockRect();
  }

}
