#include "d3d9_device_params.h"

#include "d3d9_caps.h"

namespace dxvk {
  D3D9DeviceParams::D3D9DeviceParams(IDirect3D9* parent,
    const D3DDEVICE_CREATION_PARAMETERS& params)
    : m_parent(parent),
    m_creationParams(params) {
  }

  HRESULT D3D9DeviceParams::GetDirect3D(IDirect3D9** ppD3D9) {
    InitReturnPtr(ppD3D9);
    CHECK_NOT_NULL(ppD3D9);

    *ppD3D9 = ref(m_parent);

    return D3D_OK;
  }

  HRESULT D3D9DeviceParams::GetDeviceCaps(D3DCAPS9* pCaps) {
    CHECK_NOT_NULL(pCaps);

    // The caps were not passed in by the constructor,
    // but they're the same for all devices anyway.
    FillCaps(m_creationParams.AdapterOrdinal, *pCaps);

    return D3D_OK;
  }

  HRESULT D3D9DeviceParams::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters) {
    CHECK_NOT_NULL(pParameters);

    *pParameters = m_creationParams;

    return D3D_OK;
  }
}
