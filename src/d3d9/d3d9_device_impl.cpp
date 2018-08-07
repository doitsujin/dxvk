#include "d3d9_device_impl.h"

namespace dxvk {
    D3D9DeviceImpl::D3D9DeviceImpl(IDirect3D9* parent, D3D9Adapter& adapter,
      const D3DDEVICE_CREATION_PARAMETERS& cp, D3DPRESENT_PARAMETERS& pp)
        : D3D9DeviceParams(parent, cp) {
    }
}
