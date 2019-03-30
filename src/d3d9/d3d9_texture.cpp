#include "d3d9_texture.h"

#include "d3d9_util.h"

namespace dxvk {

  Direct3DTexture9::Direct3DTexture9(
        Direct3DDevice9Ex*      pDevice,
  const D3D9TextureDesc*        pDesc)
        : Direct3DTexture9Base ( pDevice, pDesc ) { }

  HRESULT STDMETHODCALLTYPE Direct3DTexture9::QueryInterface(REFIID riid, void** ppvObject) {
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

    Logger::warn("Direct3DTexture9::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  D3DRESOURCETYPE STDMETHODCALLTYPE Direct3DTexture9::GetType() {
    return D3DRTYPE_TEXTURE;
  }

  HRESULT STDMETHODCALLTYPE Direct3DTexture9::GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) {
    auto* surface = GetSubresource(Level);
    if (surface == nullptr)
      return D3DERR_INVALIDCALL;

    return surface->GetDesc(pDesc);
  }

  HRESULT STDMETHODCALLTYPE Direct3DTexture9::GetSurfaceLevel(UINT Level, IDirect3DSurface9** ppSurfaceLevel) {
    InitReturnPtr(ppSurfaceLevel);
    auto* surface = GetSubresource(Level);

    if (ppSurfaceLevel == nullptr || surface == nullptr)
      return D3DERR_INVALIDCALL;

    *ppSurfaceLevel = ref(surface);
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DTexture9::LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) {
    auto* surface = GetSubresource(Level);
    if (surface == nullptr || pLockedRect == nullptr)
      return D3DERR_INVALIDCALL;

    return surface->LockRect(pLockedRect, pRect, Flags);
  }

  HRESULT STDMETHODCALLTYPE Direct3DTexture9::UnlockRect(UINT Level) {
    auto* surface = GetSubresource(Level);
    if (surface == nullptr)
      return D3DERR_INVALIDCALL;

    return surface->UnlockRect();
  }

  HRESULT STDMETHODCALLTYPE Direct3DTexture9::AddDirtyRect(CONST RECT* pDirtyRect) {
    Logger::warn("Direct3DTexture9::AddDirtyRect: Stub");
    return D3D_OK;
  }
}