#include <d3d11.h>
#include <d3d10_1.h>

#include "../dxgi/dxgi_interfaces.h"

extern "C" {
  using namespace dxvk;

  HRESULT __stdcall D3D11CoreCreateDevice(
          IDXGIFactory*       pFactory,
          IDXGIAdapter*       pAdapter,
          UINT                Flags,
    const D3D_FEATURE_LEVEL*  pFeatureLevels,
          UINT                FeatureLevels,
          ID3D11Device**      ppDevice);


  DLLEXPORT HRESULT __stdcall D3D10CoreCreateDevice(
          IDXGIFactory*           pFactory,
          IDXGIAdapter*           pAdapter,
          UINT                    Flags,
          D3D_FEATURE_LEVEL       FeatureLevel,
          ID3D10Device**          ppDevice) {
    InitReturnPtr(ppDevice);

    Com<ID3D11Device> d3d11Device;

    HRESULT hr = pAdapter->CheckInterfaceSupport(
      __uuidof(ID3D10Device), nullptr);
    
    if (FAILED(hr))
      return hr;

    hr = D3D11CoreCreateDevice(pFactory, pAdapter,
      Flags, &FeatureLevel, 1, &d3d11Device);

    if (FAILED(hr))
      return hr;
    
    Com<ID3D10Multithread> multithread;
    d3d11Device->QueryInterface(__uuidof(ID3D10Multithread), reinterpret_cast<void**>(&multithread));
    multithread->SetMultithreadProtected(!(Flags & D3D10_CREATE_DEVICE_SINGLETHREADED));

    Com<IDXGIDXVKDevice> dxvkDevice;
    d3d11Device->QueryInterface(__uuidof(IDXGIDXVKDevice), reinterpret_cast<void**>(&dxvkDevice));
    dxvkDevice->SetAPIVersion(10);

    if (FAILED(d3d11Device->QueryInterface(
        __uuidof(ID3D10Device), reinterpret_cast<void**>(ppDevice))))
      return E_FAIL;
    
    return S_OK;
  }


  UINT64 STDMETHODCALLTYPE D3D10CoreGetVersion() {
    // Match the Windows 10 return value, but we
    // don't know the exact function signature
    return 0xa000100041770ull;
  }


  HRESULT STDMETHODCALLTYPE D3D10CoreRegisterLayers() {
    return E_NOTIMPL;
  }

}
