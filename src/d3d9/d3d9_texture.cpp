#include "d3d9_texture.h"

#include "d3d9_device.h"

namespace dxvk {
  HRESULT D3D9Device::CreateTexture(UINT Width, UINT Height, UINT Levels,
    DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
    IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Texture::GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Texture::GetSurfaceLevel(UINT Level, IDirect3DSurface9** ppSurfaceLevel) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Texture::AddDirtyRect(const RECT* pDirtyRect) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Texture::LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect,
    const RECT* pRect, DWORD Flags) {
    CHECK_NOT_NULL(pLockedRect);

    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Texture::UnlockRect(UINT Level) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
}
