#include "d3d9_device.h"

namespace dxvk {
  D3D9Device::~D3D9Device() = default;

  HRESULT D3D9Device::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D9Device::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
}
