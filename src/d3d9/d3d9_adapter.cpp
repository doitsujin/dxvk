#include "d3d9_adapter.h"

#include "../util/util_error.h"

#include <dxgi.h>

#include <cstring>

namespace dxvk {
  D3D9Adapter::D3D9Adapter(Com<IDXGIAdapter1>&& adapter)
    : m_adapter(adapter) {
  }

  HRESULT D3D9Adapter::GetIdentifier(D3DADAPTER_IDENTIFIER9& ident) {
    DXGI_ADAPTER_DESC1 desc;

    if (FAILED(m_adapter->GetDesc1(&desc))) {
      Logger::err("Failed to retrieve adapter description");
      return D3DERR_INVALIDCALL;
    }

    // Zero out the memory.
    ident = {};

    // DXVK Adapter's description is simply the Vulkan device name.
    const auto name = str::fromws(desc.Description);
    std::strcpy(ident.DeviceName, name.data());

    std::strcpy(ident.Driver, "DXVK Device");
    std::strcpy(ident.Description, "DXVK D3D9 Vulkan Driver");

    ident.DriverVersion.QuadPart = 1;

    ident.VendorId = desc.VendorId;
    ident.DeviceId = desc.DeviceId;
    ident.SubSysId = desc.SubSysId;
    ident.Revision = desc.Revision;

    // LUID is only 64-bit long, but better something than nothing.
    std::memcpy(&ident.DeviceIdentifier, &desc.AdapterLuid, sizeof(LUID));

    // Just claim we're a validated driver.
    ident.WHQLLevel = 1;

    return D3D_OK;
  }
}
