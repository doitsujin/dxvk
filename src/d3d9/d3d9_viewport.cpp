#include "d3d9_viewport.h"

namespace dxvk {
  HRESULT D3D9DeviceViewport::GetViewport(D3DVIEWPORT9* pViewport) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError();
  }

  HRESULT D3D9DeviceViewport::SetViewport(const D3DVIEWPORT9* pViewport) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError();
  }
}
