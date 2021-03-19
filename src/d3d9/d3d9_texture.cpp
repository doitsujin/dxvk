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
    if (unlikely(Level >= m_texture.ExposedMipLevels()))
      return D3DERR_INVALIDCALL;

    return GetSubresource(Level)->GetDesc(pDesc);
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture2D::GetSurfaceLevel(UINT Level, IDirect3DSurface9** ppSurfaceLevel) {
    InitReturnPtr(ppSurfaceLevel);

    if (unlikely(Level >= m_texture.ExposedMipLevels()))
      return D3DERR_INVALIDCALL;

    if (unlikely(ppSurfaceLevel == nullptr))
      return D3DERR_INVALIDCALL;

    *ppSurfaceLevel = ref(GetSubresource(Level));
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture2D::LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) {
    if (unlikely(Level >= m_texture.ExposedMipLevels()))
      return D3DERR_INVALIDCALL;

    return GetSubresource(Level)->LockRect(pLockedRect, pRect, Flags);
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture2D::UnlockRect(UINT Level) {
    if (unlikely(Level >= m_texture.ExposedMipLevels()))
      return D3DERR_INVALIDCALL;

    return GetSubresource(Level)->UnlockRect();
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture2D::AddDirtyRect(CONST RECT* pDirtyRect) {
    if (pDirtyRect) {
      D3DBOX box = { UINT(pDirtyRect->left), UINT(pDirtyRect->top), UINT(pDirtyRect->right), UINT(pDirtyRect->bottom), 0, 1 };
      m_texture.AddDirtyBox(&box, 0);
    } else {
      m_texture.AddDirtyBox(nullptr, 0);
    }
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
    if (unlikely(Level >= m_texture.ExposedMipLevels()))
      return D3DERR_INVALIDCALL;

    return GetSubresource(Level)->GetDesc(pDesc);
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture3D::GetVolumeLevel(UINT Level, IDirect3DVolume9** ppVolumeLevel) {
    InitReturnPtr(ppVolumeLevel);

    if (unlikely(Level >= m_texture.ExposedMipLevels()))
      return D3DERR_INVALIDCALL;

    if (unlikely(ppVolumeLevel == nullptr))
      return D3DERR_INVALIDCALL;

    *ppVolumeLevel = ref(GetSubresource(Level));
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture3D::LockBox(UINT Level, D3DLOCKED_BOX* pLockedBox, CONST D3DBOX* pBox, DWORD Flags) {
    if (unlikely(Level >= m_texture.ExposedMipLevels()))
      return D3DERR_INVALIDCALL;

    return GetSubresource(Level)->LockBox(pLockedBox, pBox, Flags);
  }


  HRESULT STDMETHODCALLTYPE D3D9Texture3D::UnlockBox(UINT Level) {
    if (unlikely(Level >= m_texture.ExposedMipLevels()))
      return D3DERR_INVALIDCALL;

    return GetSubresource(Level)->UnlockBox();
  }

  HRESULT STDMETHODCALLTYPE D3D9Texture3D::AddDirtyBox(CONST D3DBOX* pDirtyBox) {
    m_texture.AddDirtyBox(pDirtyBox, 0);
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
    if (unlikely(Level >= m_texture.ExposedMipLevels()))
      return D3DERR_INVALIDCALL;

    return GetSubresource(Level)->GetDesc(pDesc);
  }


  HRESULT STDMETHODCALLTYPE D3D9TextureCube::GetCubeMapSurface(D3DCUBEMAP_FACES Face, UINT Level, IDirect3DSurface9** ppSurfaceLevel) {
    InitReturnPtr(ppSurfaceLevel);

    if (unlikely(Level >= m_texture.ExposedMipLevels() || Face >= D3DCUBEMAP_FACES(6)))
      return D3DERR_INVALIDCALL;

    if (unlikely(ppSurfaceLevel == nullptr))
      return D3DERR_INVALIDCALL;

    *ppSurfaceLevel = ref(GetSubresource(m_texture.CalcSubresource(UINT(Face), Level)));
    return D3D_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D9TextureCube::LockRect(D3DCUBEMAP_FACES Face, UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) {
    if (unlikely(Face > D3DCUBEMAP_FACE_NEGATIVE_Z || Level >= m_texture.ExposedMipLevels()))
      return D3DERR_INVALIDCALL;

    return GetSubresource(m_texture.CalcSubresource(UINT(Face), Level))->LockRect(pLockedRect, pRect, Flags);
  }


  HRESULT STDMETHODCALLTYPE D3D9TextureCube::UnlockRect(D3DCUBEMAP_FACES Face, UINT Level) {
    if (unlikely(Face > D3DCUBEMAP_FACE_NEGATIVE_Z || Level >= m_texture.ExposedMipLevels()))
      return D3DERR_INVALIDCALL;

    return GetSubresource(m_texture.CalcSubresource(UINT(Face), Level))->UnlockRect();
  }


  HRESULT STDMETHODCALLTYPE D3D9TextureCube::AddDirtyRect(D3DCUBEMAP_FACES Face, CONST RECT* pDirtyRect) {
    if (pDirtyRect) {
      D3DBOX box = { UINT(pDirtyRect->left), UINT(pDirtyRect->top), UINT(pDirtyRect->right), UINT(pDirtyRect->bottom), 0, 1 };
      m_texture.AddDirtyBox(&box, Face);
    } else {
      m_texture.AddDirtyBox(nullptr, Face);
    }
    return D3D_OK;
  }

}