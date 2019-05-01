#include "../dxgi/dxgi_include.h"

#include "../dxvk/dxvk_instance.h"

#include "d3d9_interface.h"

namespace dxvk {
  Logger Logger::s_instance("d3d9.log");

  HRESULT CreateD3D9(bool ex, IDirect3D9Ex** ppDirect3D9Ex) {
    if (!ppDirect3D9Ex)
      return D3DERR_INVALIDCALL;

    *ppDirect3D9Ex = ref(new Direct3D9Ex{ ex });
    return D3D_OK;
  }
}

extern "C" {

  DLLEXPORT IDirect3D9* __stdcall Direct3DCreate9(UINT nSDKVersion) {
    IDirect3D9Ex* pDirect3D = nullptr;
    dxvk::CreateD3D9(false, &pDirect3D);

    return pDirect3D;
  }

  DLLEXPORT HRESULT __stdcall Direct3DCreate9Ex(UINT nSDKVersion, IDirect3D9Ex** ppDirect3D9Ex) {
    return dxvk::CreateD3D9(false, ppDirect3D9Ex);
  }

  DLLEXPORT int __stdcall D3DPERF_BeginEvent(D3DCOLOR col, LPCWSTR wszName) {
    return 0;
  }

  DLLEXPORT int __stdcall D3DPERF_EndEvent(void) {
    return 0;
  }

  DLLEXPORT void __stdcall D3DPERF_SetMarker(D3DCOLOR col, LPCWSTR wszName) {
  }

  DLLEXPORT void __stdcall D3DPERF_SetRegion(D3DCOLOR col, LPCWSTR wszName) {
  }

  DLLEXPORT BOOL __stdcall D3DPERF_QueryRepeatFrame(void) {
    return FALSE;
  }

  DLLEXPORT void __stdcall D3DPERF_SetOptions(DWORD dwOptions) {
  }

  DLLEXPORT DWORD __stdcall D3DPERF_GetStatus(void) {
    return 0;
  }

}