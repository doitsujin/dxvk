#include "d3d9_device.h"

namespace dxvk {
  void D3D9Device::SetGammaRamp(UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP* pRamp) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  void D3D9Device::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
}
