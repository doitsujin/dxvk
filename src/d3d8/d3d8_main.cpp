
// Main: the location of Direct3DCreate8 and other core exported functions

#include "d3d8_interface.h"
//#include "d3d9_shader_validator.h"

class D3DFE_PROCESSVERTICES;
using PSGPERRORID = UINT;

namespace dxvk {
  Logger Logger::s_instance("d3d8.log");

  HRESULT CreateD3D8(
          UINT         SDKVersion,
          IDirect3D8** ppDirect3D8) {
    if (!ppDirect3D8)
      return D3DERR_INVALIDCALL;

    *ppDirect3D8 = ref(new D3D8InterfaceEx(SDKVersion));
    return D3D_OK;
  }
}

extern "C" {

  
  DLLEXPORT IDirect3D8* __stdcall Direct3DCreate8(UINT nSDKVersion) {
    //dxvk::CreateD3D9(false, &pDirect3D);

    dxvk::Logger::trace("Direct3DCreate8 called");

    IDirect3D8* pDirect3D = nullptr;
    dxvk::CreateD3D8(false, &pDirect3D);

    return pDirect3D;
  }

  DLLEXPORT void __stdcall DebugSetMute(void) {
  }

  DLLEXPORT int __stdcall DebugSetLevel(void) {
    return 0;
  }

  DLLEXPORT int __stdcall Direct3D9EnableMaximizedWindowedModeShim(UINT a) {
    return 0;
  }
}
