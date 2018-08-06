#pragma once

#include "d3d9_include.h"

namespace dxvk {
    class D3D9Adapter {
    public:
        D3D9Adapter(Com<IDXGIAdapter1>&& adapter);

        HRESULT GetIdentifier(D3DADAPTER_IDENTIFIER9& ident);

        UINT GetModeCount() const;
        void GetMode(UINT index, D3DDISPLAYMODE& mode) const;

    private:
        Com<IDXGIAdapter1> m_adapter;
        // D3D9 does not have the concept of multiple monitors per GPU,
        // therefore we only use the first one.
        Com<IDXGIOutput> m_output;
        std::vector<DXGI_MODE_DESC> m_modes;
    };
}
