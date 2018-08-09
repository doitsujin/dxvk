#include "d3d9_device.h"

namespace dxvk {
  HRESULT D3D9Device::CreateVolumeTexture(UINT Width, UINT Height,
    UINT Depth, UINT Levels, DWORD Usage,
    D3DFORMAT Format, D3DPOOL Pool,
    IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
}
