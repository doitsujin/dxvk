#include "d3d9_device.h"

namespace dxvk {
  HRESULT D3D9Device::CreateDepthStencilSurface(UINT Width, UINT Height,
    D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality,
    BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
}
