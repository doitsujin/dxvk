#include "d3d9_query.h"

namespace dxvk {
  class D3D9Query final: public ComObject<IDirect3DQuery9> {
  };

  HRESULT D3D9DeviceQuery::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) {
    InitReturnPtr(ppQuery);
    CHECK_NOT_NULL(ppQuery);

    switch (Type) {
    default:
      Logger::err(str::format(__func__, " stub"));
      return D3DERR_INVALIDCALL;
    }

    return D3D_OK;
  }
}
