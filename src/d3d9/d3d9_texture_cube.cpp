#include "d3d9_device.h"

namespace dxvk {
  HRESULT D3D9Device::CreateCubeTexture(UINT EdgeLength, UINT Levels,
    DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
    IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
}
