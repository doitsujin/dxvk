#include "d3d9_adapter.h"

#include <cstring>

namespace dxvk {
  // Use the first output of an adapter.
  static IDXGIOutput* getFirstOutput(IDXGIAdapter* adapter) {
    IDXGIOutput* output;
    if (FAILED(adapter->EnumOutputs(0, &output)))
      throw DxvkError("No monitors attached to adapter");
    return output;
  }

  // Cache the supported display modes for later.
  static std::vector<DXGI_MODE_DESC> getOutputModes(IDXGIOutput* output) {
    // Note: we just use a common format, and assume the same modes for other formats.
    UINT count = 0;
    output->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 0, &count, nullptr);

    std::vector<DXGI_MODE_DESC> modes(count);

    if (FAILED(output->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 0, &count, modes.data())))
      throw DxvkError("Failed to get display mode list");

    return modes;
  }

  D3D9Adapter::D3D9Adapter(Com<IDXGIAdapter1>&& adapter)
    : m_adapter(adapter),
      m_output(getFirstOutput(m_adapter.ptr())),
      m_modes(getOutputModes(m_output.ptr())) {
    if (FAILED(m_adapter->GetDesc(&m_desc)))
      throw DxvkError("Failed to retrieve adapter description");

    if (FAILED(m_output->GetDesc(&m_outputDesc)))
      throw DxvkError("Failed to retrieve output description");
  }

  HRESULT D3D9Adapter::GetIdentifier(D3DADAPTER_IDENTIFIER9& ident) {
    // Zero out the memory.
    ident = {};

    // DXVK Adapter's description is simply the Vulkan device name.
    const auto name = str::fromws(m_desc.Description);
    // This is what game GUIs usually display.
    const auto description = str::format(name, " (D3D9 DXVK Driver)");
    const auto driver = "DXVK";

    std::strcpy(ident.DeviceName, name.data());
    std::strcpy(ident.Description, description.data());
    std::strcpy(ident.Driver, driver);

    ident.DriverVersion.QuadPart = 1;

    ident.VendorId = m_desc.VendorId;
    ident.DeviceId = m_desc.DeviceId;
    ident.SubSysId = m_desc.SubSysId;
    ident.Revision = m_desc.Revision;

    // LUID is only 64-bit long, but better something than nothing.
    std::memcpy(&ident.DeviceIdentifier, &m_desc.AdapterLuid, sizeof(LUID));

    // Just claim we're a validated driver.
    ident.WHQLLevel = 1;

    return D3D_OK;
  }

  UINT D3D9Adapter::GetModeCount() const {
    return m_modes.size();
  }

  void D3D9Adapter::GetMode(UINT index, D3DDISPLAYMODE& mode) const {
    const auto& desc = m_modes[index];

    // DXVK always returns refresh rate as ((refresh rate * 1000) / 1000).
    const auto refresh = desc.RefreshRate.Numerator / desc.RefreshRate.Denominator;

    mode.Width = desc.Width;
    mode.Height = desc.Height;
    mode.RefreshRate = refresh;
  }

  HMONITOR D3D9Adapter::GetMonitor() const {
    return m_outputDesc.Monitor;
  }
}
