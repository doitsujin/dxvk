#include "d3d8_surface.h"
#include "d3d8_device.h"

#include "d3d8_d3d9_util.h"

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

    if (likely(SUCCEEDED(res)))
      ConvertSurfaceDesc8(&desc, pDesc);

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Surface::LockRect(
      D3DLOCKED_RECT* pLockedRect,
      CONST RECT*     pRect,
      DWORD           Flags) {
    return GetD3D9()->LockRect((d3d9::D3DLOCKED_RECT*)pLockedRect, pRect, Flags);
  }

  HRESULT STDMETHODCALLTYPE D3D8Surface::UnlockRect() {
    return GetD3D9()->UnlockRect();
  }

  // TODO: Consider creating only one texture to
  // encompass all surface levels of a texture.
  Com<d3d9::IDirect3DSurface9> D3D8Surface::GetBlitImage() {
    if (unlikely(m_blitImage == nullptr)) {
      m_blitImage = CreateBlitImage();
    }

    return m_blitImage;
  }

  Com<d3d9::IDirect3DSurface9> D3D8Surface::CreateBlitImage() {
    d3d9::D3DSURFACE_DESC desc;
    GetD3D9()->GetDesc(&desc);

    // NOTE: This adds a D3DPOOL_DEFAULT resource to the
    // device, which counts as losable during device reset
    Com<d3d9::IDirect3DSurface9> image = nullptr;
    HRESULT res = GetParent()->GetD3D9()->CreateRenderTarget(
      desc.Width, desc.Height, desc.Format,
      d3d9::D3DMULTISAMPLE_NONE, 0,
      FALSE,
      &image,
      NULL);

    if (FAILED(res))
      throw DxvkError("D3D8: Failed to create blit image");

    return image;
  }

}