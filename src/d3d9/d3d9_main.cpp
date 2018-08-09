#include "d3d9_core.h"
#include "d3d9_include.h"
#include "../util/util_error.h"

// This file initializes the logging output,
// and provides the API entry points.

namespace dxvk {
  Logger Logger::s_instance("d3d9.log");
}

extern "C" {
  using namespace dxvk;

  DLLEXPORT IDirect3D9* STDMETHODCALLTYPE Direct3DCreate9(UINT SDKVersion) {
    // Usually displays 32, which is version 9.0c
    Logger::info(str::format("Creating D3D9 context for SDK version: ", SDKVersion));

    try {
      return new Direct3D9();
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return nullptr;
    }
  }

  DLLEXPORT HRESULT STDMETHODCALLTYPE Direct3DCreate9Ex(UINT SDKVersion, IDirect3D9Ex** ptr) {
    // TODO: add support for D3D9Ex

    *ptr = nullptr;

    return D3DERR_NOTAVAILABLE;
  }
}
