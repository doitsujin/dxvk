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
      const DWORD*     pixelshader,
      const D3DCAPS8*  caps,
      BOOL             boolValue,
      char**           errorString) {
    dxvk::Logger::warn("D3D8: ValidatePixelShader: Stub");

    if (unlikely(pixelshader == nullptr))
      return E_FAIL;

    if (likely(errorString != nullptr)) {
      const char* errorMessage = "";
      *errorString = (char *) errorMessage;
    }

    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall ValidateVertexShader(
      const DWORD*     vertexShader,
      const DWORD*     vertexDecl,
      const D3DCAPS8*  caps,
      BOOL             boolValue,
      char**           errorString) {
    dxvk::Logger::warn("D3D8: ValidateVertexShader: Stub");

    if (unlikely(vertexShader == nullptr))
      return E_FAIL;

    if (likely(errorString != nullptr)) {
      const char* errorMessage = "";
      *errorString = (char *) errorMessage;
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
