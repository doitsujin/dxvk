#pragma once

#include "d3d9_include.h"

namespace dxvk {
  class D3D9Adapter {
  public:
    D3D9Adapter(Com<IDXGIAdapter1>&& adapter);

    HRESULT GetIdentifier(D3DADAPTER_IDENTIFIER9& ident);

    UINT GetModeCount() const;
    void GetMode(UINT index, D3DDISPLAYMODE& mode) const;

    HMONITOR GetMonitor() const;

  private:
    Com<IDXGIAdapter1> m_adapter;
    DXGI_ADAPTER_DESC m_desc;

    // D3D9 does not have the concept of multiple monitors per GPU,
    // therefore we only use the first one.
    Com<IDXGIOutput> m_output;
    DXGI_OUTPUT_DESC m_outputDesc;

    std::vector<DXGI_MODE_DESC> m_modes;

    // Allow the D3D9 device class to access these fields.
    friend class D3D9Device;
  };
}
