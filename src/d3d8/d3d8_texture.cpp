#include "d3d8_texture.h"

#include "d3d8_d3d9_util.h"

namespace dxvk {

  // D3D8Texture2D

  D3D8Texture2D::D3D8Texture2D(
          D3D8Device*                    pDevice,
    const D3DPOOL                        Pool,
          Com<d3d9::IDirect3DTexture9>&& pTexture)
    : D3D8Texture2DBase(pDevice, Pool, std::move(pTexture), pTexture->GetLevelCount()) {
  }

  D3DRESOURCETYPE STDMETHODCALLTYPE D3D8Texture2D::GetType() { return D3DRTYPE_TEXTURE; }

  HRESULT STDMETHODCALLTYPE D3D8Texture2D::GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) {
    if (unlikely(pDesc == nullptr))
      return D3DERR_INVALIDCALL;

    d3d9::D3DSURFACE_DESC surf;
    HRESULT res = GetD3D9()->GetLevelDesc(Level, &surf);

    if (likely(SUCCEEDED(res)))
      ConvertSurfaceDesc8(&surf, pDesc);

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Texture2D::GetSurfaceLevel(UINT Level, IDirect3DSurface8** ppSurfaceLevel) {
    return GetSubresource(Level, ppSurfaceLevel);
  }

  HRESULT STDMETHODCALLTYPE D3D8Texture2D::LockRect(
      UINT            Level,
      D3DLOCKED_RECT* pLockedRect,
      CONST RECT*     pRect,
      DWORD           Flags) {
    return GetD3D9()->LockRect(Level, reinterpret_cast<d3d9::D3DLOCKED_RECT*>(pLockedRect), pRect, Flags);
  }

  HRESULT STDMETHODCALLTYPE D3D8Texture2D::UnlockRect(UINT Level) {
    return GetD3D9()->UnlockRect(Level);
  }

  HRESULT STDMETHODCALLTYPE D3D8Texture2D::AddDirtyRect(CONST RECT* pDirtyRect) {
    return GetD3D9()->AddDirtyRect(pDirtyRect);
  }

  // D3D8Texture3D

  D3D8Texture3D::D3D8Texture3D(
          D3D8Device*                           pDevice,
    const D3DPOOL                               Pool,
          Com<d3d9::IDirect3DVolumeTexture9>&&  pVolumeTexture)
    : D3D8Texture3DBase(pDevice, Pool, std::move(pVolumeTexture), pVolumeTexture->GetLevelCount()) {}

  D3DRESOURCETYPE STDMETHODCALLTYPE D3D8Texture3D::GetType() { return D3DRTYPE_VOLUMETEXTURE; }

  HRESULT STDMETHODCALLTYPE D3D8Texture3D::GetLevelDesc(UINT Level, D3DVOLUME_DESC *pDesc) {
    if (unlikely(pDesc == nullptr))
      return D3DERR_INVALIDCALL;

    d3d9::D3DVOLUME_DESC vol;
    HRESULT res = GetD3D9()->GetLevelDesc(Level, &vol);

    if (likely(SUCCEEDED(res)))
      ConvertVolumeDesc8(&vol, pDesc);

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Texture3D::GetVolumeLevel(UINT Level, IDirect3DVolume8** ppVolumeLevel) {
    return GetSubresource(Level, ppVolumeLevel);
  }

  HRESULT STDMETHODCALLTYPE D3D8Texture3D::LockBox(
      UINT           Level,
      D3DLOCKED_BOX* pLockedBox,
      CONST D3DBOX*  pBox,
      DWORD          Flags) {
    return GetD3D9()->LockBox(
      Level,
      reinterpret_cast<d3d9::D3DLOCKED_BOX*>(pLockedBox),
      reinterpret_cast<const d3d9::D3DBOX*>(pBox),
      Flags
    );
  }

  HRESULT STDMETHODCALLTYPE D3D8Texture3D::UnlockBox(UINT Level) {
    return GetD3D9()->UnlockBox(Level);
  }

  HRESULT STDMETHODCALLTYPE D3D8Texture3D::AddDirtyBox(CONST D3DBOX* pDirtyBox) {
    return GetD3D9()->AddDirtyBox(reinterpret_cast<const d3d9::D3DBOX*>(pDirtyBox));
  }

  // D3D8TextureCube

  D3D8TextureCube::D3D8TextureCube(
          D3D8Device*                         pDevice,
    const D3DPOOL                             Pool,
          Com<d3d9::IDirect3DCubeTexture9>&&  pTexture)
    : D3D8TextureCubeBase(pDevice, Pool, std::move(pTexture), pTexture->GetLevelCount() * CUBE_FACES) {
  }

  D3DRESOURCETYPE STDMETHODCALLTYPE D3D8TextureCube::GetType() { return D3DRTYPE_CUBETEXTURE; }

  HRESULT STDMETHODCALLTYPE D3D8TextureCube::GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) {
    if (unlikely(pDesc == nullptr))
      return D3DERR_INVALIDCALL;

    d3d9::D3DSURFACE_DESC surf;
    HRESULT res = GetD3D9()->GetLevelDesc(Level, &surf);

    if (likely(SUCCEEDED(res)))
      ConvertSurfaceDesc8(&surf, pDesc);

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8TextureCube::GetCubeMapSurface(
      D3DCUBEMAP_FACES    Face,
      UINT                Level,
      IDirect3DSurface8** ppSurfaceLevel) {
    return GetSubresource((Level * CUBE_FACES) + Face, ppSurfaceLevel);
  }

  HRESULT STDMETHODCALLTYPE D3D8TextureCube::LockRect(
          D3DCUBEMAP_FACES Face,
          UINT Level,
          D3DLOCKED_RECT* pLockedRect,
          const RECT* pRect,
          DWORD Flags) {
    return GetD3D9()->LockRect(
      d3d9::D3DCUBEMAP_FACES(Face),
      Level,
      reinterpret_cast<d3d9::D3DLOCKED_RECT*>(pLockedRect),
      pRect,
      Flags);
  }

  HRESULT STDMETHODCALLTYPE D3D8TextureCube::UnlockRect(D3DCUBEMAP_FACES Face, UINT Level) {
    return GetD3D9()->UnlockRect(d3d9::D3DCUBEMAP_FACES(Face), Level);
  }

  HRESULT STDMETHODCALLTYPE D3D8TextureCube::AddDirtyRect(D3DCUBEMAP_FACES Face, const RECT* pDirtyRect) {
    return GetD3D9()->AddDirtyRect(d3d9::D3DCUBEMAP_FACES(Face), pDirtyRect);
  }

}