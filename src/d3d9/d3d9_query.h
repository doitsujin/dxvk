#pragma once

#include "d3d9_device.h"

namespace dxvk {
  /// Implements query creation.
  class D3D9DeviceQuery: public virtual ComObject<D3D9Device> {
    HRESULT STDMETHODCALLTYPE CreateQuery(D3DQUERYTYPE Type,
      IDirect3DQuery9** ppQuery) final override;
  };
}
