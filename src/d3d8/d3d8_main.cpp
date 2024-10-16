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

  DLLEXPORT HRESULT __stdcall ValidatePixelShader(
      const DWORD*     pPixelShader,
      const D3DCAPS8*  pCaps,
      BOOL             errorReturn,
      char**           pErrorString) {
    dxvk::Logger::warn("D3D8: ValidatePixelShader: Stub");

    if (unlikely(pPixelShader == nullptr))
      return E_FAIL;

    if (errorReturn && pErrorString != nullptr) {
      const char* errorMessage = "";
      *pErrorString = (char *) errorMessage;
    }

    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall ValidateVertexShader(
      const DWORD*     pVertexShader,
      const DWORD*     pVertexDecl,
      const D3DCAPS8*  pCaps,
      BOOL             errorReturn,
      char**           pErrorString) {
    dxvk::Logger::warn("D3D8: ValidateVertexShader: Stub");

    if (unlikely(pVertexShader == nullptr))
      return E_FAIL;

    if (errorReturn && pErrorString != nullptr) {
      const char* errorMessage = "";
      *pErrorString = (char *) errorMessage;
    }

    return S_OK;
  }

  DLLEXPORT void __stdcall DebugSetMute() {
    dxvk::Logger::debug("D3D8: DebugSetMute: Stub");
  }

  DLLEXPORT IDirect3D8* __stdcall Direct3DCreate8(UINT nSDKVersion) {
    IDirect3D8* pDirect3D = nullptr;
    dxvk::CreateD3D8(&pDirect3D);

    return pDirect3D;
  }

}
