#include "d3d8_interface.h"

namespace dxvk {
  Logger Logger::s_instance("d3d8.log");

  HRESULT CreateD3D8(IDirect3D8** ppDirect3D8) {
    if (!ppDirect3D8)
      return D3DERR_INVALIDCALL;

    *ppDirect3D8 = ref(new D3D8Interface());
    return D3D_OK;
  }
}

extern "C" {  
  DLLEXPORT IDirect3D8* __stdcall Direct3DCreate8(UINT nSDKVersion) {
    dxvk::Logger::trace("Direct3DCreate8 called");

    IDirect3D8* pDirect3D = nullptr;
    dxvk::CreateD3D8(&pDirect3D);

    return pDirect3D;
  }
}
