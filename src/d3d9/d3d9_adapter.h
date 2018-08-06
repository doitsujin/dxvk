#pragma once

#include "d3d9_include.h"

struct IDXGIAdapter1;

namespace dxvk {
    class D3D9Adapter {
    public:
        D3D9Adapter(Com<IDXGIAdapter1>&& adapter);

        HRESULT GetIdentifier(D3DADAPTER_IDENTIFIER9& ident);

    private:
        Com<IDXGIAdapter1> m_adapter;
    };
}
