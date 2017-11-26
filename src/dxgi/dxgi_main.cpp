#include "dxgi_factory.h"
#include "dxgi_include.h"

namespace dxvk {
  
  struct SdlInstance {
    SdlInstance() {
      if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE))
        Logger::err("Instance::init: Failed to initialize SDL");
    }
    
    ~SdlInstance() {
      SDL_Quit();
    }
  };
  
  SdlInstance sdl;
  
  HRESULT createDxgiFactory(REFIID riid, void **ppFactory) {
    if (riid != __uuidof(IDXGIFactory)) {
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
  DLLEXPORT HRESULT __stdcall CreateDXGIFactory1(REFIID riid, void **ppFactory) {
    return dxvk::createDxgiFactory(riid, ppFactory);
  }
  
  DLLEXPORT HRESULT __stdcall CreateDXGIFactory(REFIID riid, void **ppFactory) {
    return dxvk::createDxgiFactory(riid, ppFactory);
  }
}