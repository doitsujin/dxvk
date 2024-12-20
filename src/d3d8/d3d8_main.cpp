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
    std::string errorMessage = "";

    if (unlikely(pPixelShader == nullptr)) {
      errorMessage = "D3D8: ValidatePixelShader: Null pPixelShader";
    } else {
      uint32_t majorVersion = (pPixelShader[0] >> 8) & 0xff;
      uint32_t minorVersion = pPixelShader[0] & 0xff;

      if (unlikely(majorVersion != 1 || minorVersion > 4)) {
        errorMessage = dxvk::str::format("D3D8: ValidatePixelShader: Unsupported PS version ",
                                          majorVersion, ".", minorVersion);
      } else if (unlikely(pCaps && pPixelShader[0] > pCaps->PixelShaderVersion)) {
        errorMessage = dxvk::str::format("D3D8: ValidatePixelShader: Caps: Unsupported PS version ",
                                          majorVersion, ".", minorVersion);
      }
    }

    const size_t errorMessageSize = errorMessage.size() + 1;

#ifdef _WIN32
    if (pErrorString != nullptr && errorReturn) {
      // Wine tests call HeapFree() on the returned error string,
      // so the expectation is for it to be allocated on the heap.
      *pErrorString = (char*) HeapAlloc(GetProcessHeap(), 0, errorMessageSize);
      if (*pErrorString)
        memcpy(*pErrorString, errorMessage.c_str(), errorMessageSize);
    }
#endif

    if (errorMessageSize > 1) {
      dxvk::Logger::warn(errorMessage);
      return E_FAIL;
    }

    return S_OK;
  }

  DLLEXPORT HRESULT __stdcall ValidateVertexShader(
      const DWORD*     pVertexShader,
      const DWORD*     pVertexDecl,
      const D3DCAPS8*  pCaps,
      BOOL             errorReturn,
      char**           pErrorString) {
    std::string errorMessage = "";

    if (unlikely(pVertexShader == nullptr)) {
      errorMessage = "D3D8: ValidateVertexShader: Null pVertexShader";
    } else {
      uint32_t majorVersion = (pVertexShader[0] >> 8) & 0xff;
      uint32_t minorVersion = pVertexShader[0] & 0xff;

      if (unlikely(majorVersion != 1 || minorVersion > 1)) {
        errorMessage = dxvk::str::format("D3D8: ValidateVertexShader: Unsupported VS version ",
                                          majorVersion, ".", minorVersion);
      } else if (unlikely(pCaps && pVertexShader[0] > pCaps->VertexShaderVersion)) {
        errorMessage = dxvk::str::format("D3D8: ValidateVertexShader: Caps: Unsupported VS version ",
                                          majorVersion, ".", minorVersion);
      }
    }

    const size_t errorMessageSize = errorMessage.size() + 1;

#ifdef _WIN32
    if (pErrorString != nullptr && errorReturn) {
      // Wine tests call HeapFree() on the returned error string,
      // so the expectation is for it to be allocated on the heap.
      *pErrorString = (char*) HeapAlloc(GetProcessHeap(), 0, errorMessageSize);
      if (*pErrorString)
        memcpy(*pErrorString, errorMessage.c_str(), errorMessageSize);
    }
#endif

    if (errorMessageSize > 1) {
      dxvk::Logger::warn(errorMessage);
      return E_FAIL;
    }

    return S_OK;
  }

  DLLEXPORT void __stdcall DebugSetMute() {}

  DLLEXPORT IDirect3D8* __stdcall Direct3DCreate8(UINT nSDKVersion) {
    IDirect3D8* pDirect3D = nullptr;
    dxvk::CreateD3D8(&pDirect3D);

    return pDirect3D;
  }

}
