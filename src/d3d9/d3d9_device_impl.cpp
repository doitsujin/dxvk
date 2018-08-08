#include "d3d9_device_impl.h"

namespace dxvk {
  D3D9DeviceImpl::D3D9DeviceImpl(IDirect3D9* parent, D3D9Adapter& adapter,
    const D3DDEVICE_CREATION_PARAMETERS& cp, D3DPRESENT_PARAMETERS& pp)
    : D3D9Device(adapter, cp.hFocusWindow, pp),
    D3D9DeviceParams(parent, cp) {
  }

  HRESULT D3D9DeviceImpl::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D9Device::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  ULONG D3D9DeviceImpl::AddRef() {
    return ComObject::AddRef();
  }

  ULONG D3D9DeviceImpl::Release() {
    return ComObject::Release();
  }
}
