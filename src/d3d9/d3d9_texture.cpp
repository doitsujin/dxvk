#include "d3d9_texture.h"

#include "d3d9_util.h"

namespace dxvk {

  // Direct3DTexture9

  D3D9Texture2D::D3D9Texture2D(
          D3D9DeviceEx*             pDevice,
    const D3D9_COMMON_TEXTURE_DESC* pDesc)
    : D3D9Texture2DBase( pDevice, pDesc, D3DRTYPE_TEXTURE ) { }


  HRESULT STDMETHODCALLTYPE D3D9Texture2D::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DResource9)
     || riid == __uuidof(IDirect3DBaseTexture9)
     || riid == __uuidof(IDirect3DTexture9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D9Texture2D::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  D3DRESOURCETYPE STDMETHODCALLTYPE D3D9Texture2D::GetType() {
    return D3DRTYPE_TEXTURE;
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture2D::GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) {
    auto* surface = GetSubresource(Level);
    if (surface == nullptr)
      return D3DERR_INVALIDCALL;

    return surface->GetDesc(pDesc);
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture2D::GetSurfaceLevel(UINT Level, IDirect3DSurface9** ppSurfaceLevel) {
    InitReturnPtr(ppSurfaceLevel);
    auto* surface = GetSubresource(Level);

    if (ppSurfaceLevel == nullptr || surface == nullptr)
      return D3DERR_INVALIDCALL;

    *ppSurfaceLevel = ref(surface);
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture2D::LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) {
    auto* surface = GetSubresource(Level);
    if (surface == nullptr || pLockedRect == nullptr)
      return D3DERR_INVALIDCALL;

    return surface->LockRect(pLockedRect, pRect, Flags);
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture2D::UnlockRect(UINT Level) {
    auto* surface = GetSubresource(Level);
    if (surface == nullptr)
      return D3DERR_INVALIDCALL;

    return surface->UnlockRect();
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture2D::AddDirtyRect(CONST RECT* pDirtyRect) {
    return D3D_OK;
  }


  // Direct3DVolumeTexture9


  D3D9Texture3D::D3D9Texture3D(
          D3D9DeviceEx*             pDevice,
    const D3D9_COMMON_TEXTURE_DESC* pDesc)
    : D3D9Texture3DBase( pDevice, pDesc, D3DRTYPE_VOLUMETEXTURE ) { }


  HRESULT STDMETHODCALLTYPE D3D9Texture3D::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DResource9)
     || riid == __uuidof(IDirect3DBaseTexture9)
     || riid == __uuidof(IDirect3DVolumeTexture9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D9Texture3D::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  D3DRESOURCETYPE STDMETHODCALLTYPE D3D9Texture3D::GetType() {
    return D3DRTYPE_VOLUMETEXTURE;
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture3D::GetLevelDesc(UINT Level, D3DVOLUME_DESC *pDesc) {
    auto* volume = GetSubresource(Level);
    if (volume == nullptr)
      return D3DERR_INVALIDCALL;

    return volume->GetDesc(pDesc);
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture3D::GetVolumeLevel(UINT Level, IDirect3DVolume9** ppVolumeLevel) {
    InitReturnPtr(ppVolumeLevel);
    auto* volume = GetSubresource(Level);

    if (ppVolumeLevel == nullptr || volume == nullptr)
      return D3DERR_INVALIDCALL;

    *ppVolumeLevel = ref(volume);
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture3D::LockBox(UINT Level, D3DLOCKED_BOX* pLockedBox, CONST D3DBOX* pBox, DWORD Flags) {
    auto* volume = GetSubresource(Level);
    if (volume == nullptr || pLockedBox == nullptr)
      return D3DERR_INVALIDCALL;

    return volume->LockBox(pLockedBox, pBox, Flags);
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture3D::UnlockBox(UINT Level) {
    auto* volume = GetSubresource(Level);
    if (volume == nullptr)
      return D3DERR_INVALIDCALL;

    return volume->UnlockBox();
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture3D::AddDirtyBox(CONST D3DBOX* pDirtyBox) {
    return D3D_OK;
  }


  // Direct3DCubeTexture9


  D3D9TextureCube::D3D9TextureCube(
          D3D9DeviceEx*             pDevice,
    const D3D9_COMMON_TEXTURE_DESC* pDesc)
    : D3D9TextureCubeBase( pDevice, pDesc, D3DRTYPE_CUBETEXTURE ) { }


  HRESULT STDMETHODCALLTYPE D3D9TextureCube::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DResource9)
     || riid == __uuidof(IDirect3DBaseTexture9)
     || riid == __uuidof(IDirect3DCubeTexture9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D9TextureCube::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }


  D3DRESOURCETYPE STDMETHODCALLTYPE D3D9TextureCube::GetType() {
    return D3DRTYPE_CUBETEXTURE;
  }


  HRESULT STDMETHODCALLTYPE D3D9TextureCube::GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) {
    auto* surface = GetSubresource(Level);

    if (surface == nullptr)
      return D3DERR_INVALIDCALL;

    return surface->GetDesc(pDesc);
  }


  HRESULT STDMETHODCALLTYPE D3D9TextureCube::GetCubeMapSurface(D3DCUBEMAP_FACES Face, UINT Level, IDirect3DSurface9** ppSurfaceLevel) {
    InitReturnPtr(ppSurfaceLevel);

    if (Level >= m_texture.Desc()->MipLevels)
      return D3DERR_INVALIDCALL;

    auto* surface = GetSubresource(
      m_texture.CalcSubresource(UINT(Face), Level));

    if (ppSurfaceLevel == nullptr || surface == nullptr)
      return D3DERR_INVALIDCALL;

    *ppSurfaceLevel = ref(surface);
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9TextureCube::LockRect(D3DCUBEMAP_FACES Face, UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) {
    auto* surface = GetSubresource(
      m_texture.CalcSubresource(UINT(Face), Level));

    if (surface == nullptr || pLockedRect == nullptr)
      return D3DERR_INVALIDCALL;

    return surface->LockRect(pLockedRect, pRect, Flags);
  }


  HRESULT STDMETHODCALLTYPE D3D9TextureCube::UnlockRect(D3DCUBEMAP_FACES Face, UINT Level) {
    auto* surface = GetSubresource(
      m_texture.CalcSubresource(UINT(Face), Level));

    if (surface == nullptr)
      return D3DERR_INVALIDCALL;

    return surface->UnlockRect();
  }


  HRESULT STDMETHODCALLTYPE D3D9TextureCube::AddDirtyRect(D3DCUBEMAP_FACES Face, CONST RECT* pDirtyRect) {
    return D3D_OK;
  }

}