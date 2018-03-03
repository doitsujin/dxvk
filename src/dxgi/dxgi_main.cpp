#include "dxgi_factory.h"
#include "dxgi_include.h"

namespace dxvk {
  
  Logger Logger::s_instance("dxgi.log");
  
  HRESULT createDxgiFactory(REFIID riid, void **ppFactory) {
    if (riid != __uuidof(IDXGIFactory)
     && riid != __uuidof(IDXGIFactory1)) {
      Logger::err("CreateDXGIFactory: Requested version of IDXGIFactory not supported");
      return DXGI_ERROR_UNSUPPORTED;
    }
    
    try {
      *ppFactory = ref(new DxgiFactory());
      return S_OK;
    } catch (const DxvkError& err) {
      Logger::err(err.message());
      return DXGI_ERROR_UNSUPPORTED;
    }
  }
}

extern "C" {
  DLLEXPORT HRESULT __stdcall CreateDXGIFactory2(UINT Flags, REFIID riid, void **ppFactory) {
    dxvk::Logger::warn("CreateDXGIFactory2: Ignoring flags");
    return dxvk::createDxgiFactory(riid, ppFactory);
  }

  DLLEXPORT HRESULT __stdcall CreateDXGIFactory1(REFIID riid, void **ppFactory) {
    return dxvk::createDxgiFactory(riid, ppFactory);
  }
  
  DLLEXPORT HRESULT __stdcall CreateDXGIFactory(REFIID riid, void **ppFactory) {
    return dxvk::createDxgiFactory(riid, ppFactory);
  }
}