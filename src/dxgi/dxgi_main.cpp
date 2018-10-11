#include "dxgi_factory.h"
#include "dxgi_include.h"

namespace dxvk {
  
  Logger Logger::s_instance("dxgi.log");
  
  HRESULT createDxgiFactory(UINT Flags, REFIID riid, void **ppFactory) {
    Com<DxgiFactory> factory = new DxgiFactory(Flags);
    HRESULT hr = factory->QueryInterface(riid, ppFactory);

    if (FAILED(hr))
      return DXGI_ERROR_UNSUPPORTED;
    
    return S_OK;
  }
}

extern "C" {
  DLLEXPORT HRESULT __stdcall CreateDXGIFactory2(UINT Flags, REFIID riid, void **ppFactory) {
    dxvk::Logger::warn("CreateDXGIFactory2: Ignoring flags");
    return dxvk::createDxgiFactory(Flags, riid, ppFactory);
  }

  DLLEXPORT HRESULT __stdcall CreateDXGIFactory1(REFIID riid, void **ppFactory) {
    return dxvk::createDxgiFactory(0, riid, ppFactory);
  }
  
  DLLEXPORT HRESULT __stdcall CreateDXGIFactory(REFIID riid, void **ppFactory) {
    return dxvk::createDxgiFactory(0, riid, ppFactory);
  }
}