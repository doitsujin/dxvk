#include "d3d10_include.h"

#include "../dxgi/dxgi_adapter.h"
#include "../dxgi/dxgi_device.h"

namespace dxvk {
  Logger Logger::s_instance("d3d10.log");
}

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
    Com<ID3D11Device> d3d11Device;

    HRESULT hr = D3D11CoreCreateDevice(pFactory,
      pAdapter, Flags, &FeatureLevel, 1, &d3d11Device);
    
    if (FAILED(hr))
      return hr;
    
    if (FAILED(d3d11Device->QueryInterface(
        __uuidof(ID3D10Device), reinterpret_cast<void**>(ppDevice))))
      return E_FAIL;
    
    return S_OK;
  }


  DLLEXPORT HRESULT D3D10CreateDevice1(
          IDXGIAdapter*           pAdapter,
          D3D10_DRIVER_TYPE       DriverType,
          HMODULE                 Software,
          UINT                    Flags,
          D3D10_FEATURE_LEVEL1    HardwareLevel,
          UINT                    SDKVersion,
          ID3D10Device1**         ppDevice) {
    InitReturnPtr(ppDevice);

    // Get DXGI factory and adapter. This is mostly
    // copied from the equivalent D3D11 functions.
    Com<IDXGIFactory> dxgiFactory = nullptr;
    Com<IDXGIAdapter> dxgiAdapter = pAdapter;

    if (dxgiAdapter == nullptr) {
      if (DriverType != D3D10_DRIVER_TYPE_HARDWARE)
        Logger::warn("D3D10CreateDevice: Unsupported driver type");
      
      if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory)))) {
        Logger::err("D3D10CreateDevice: Failed to create a DXGI factory");
        return E_FAIL;
      }
      
      if (FAILED(dxgiFactory->EnumAdapters(0, &dxgiAdapter))) {
        Logger::err("D3D10CreateDevice: No default adapter available");
        return E_FAIL;
      }
      
    } else {
      if (FAILED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory)))) {
        Logger::err("D3D10CreateDevice: Failed to query DXGI factory from DXGI adapter");
        return E_FAIL;
      }
      
      if (DriverType != D3D10_DRIVER_TYPE_HARDWARE || Software != nullptr)
        return E_INVALIDARG;
    }

    // Create the actual device
    Com<ID3D10Device> device;

    HRESULT hr = D3D10CoreCreateDevice(
      dxgiFactory.ptr(), dxgiAdapter.ptr(),
      Flags, D3D_FEATURE_LEVEL(HardwareLevel),
      &device);
    
    if (FAILED(hr))
      return hr;
    
    hr = device->QueryInterface(
      __uuidof(ID3D10Device1),
      reinterpret_cast<void**>(ppDevice));
    
    if (FAILED(hr))
      return E_FAIL;
    
    return hr;
  }


  DLLEXPORT HRESULT D3D10CreateDevice(
          IDXGIAdapter*           pAdapter,
          D3D10_DRIVER_TYPE       DriverType,
          HMODULE                 Software,
          UINT                    Flags,
          UINT                    SDKVersion,
          ID3D10Device**          ppDevice) {
    InitReturnPtr(ppDevice);

    Com<ID3D10Device1> d3d10Device = nullptr;
    HRESULT hr = D3D10CreateDevice1(pAdapter,
      DriverType, Software, Flags,
      D3D10_FEATURE_LEVEL_10_0,
      SDKVersion, &d3d10Device);
    
    if (FAILED(hr))
      return hr;
    
    if (ppDevice != nullptr) {
      *ppDevice = d3d10Device.ref();
      return S_OK;
    } return S_FALSE;
  }


  DLLEXPORT HRESULT D3D10CreateDeviceAndSwapChain1(
          IDXGIAdapter*           pAdapter,
          D3D10_DRIVER_TYPE       DriverType,
          HMODULE                 Software,
          UINT                    Flags,
          D3D10_FEATURE_LEVEL1    HardwareLevel,
          UINT                    SDKVersion,
          DXGI_SWAP_CHAIN_DESC*   pSwapChainDesc,
          IDXGISwapChain**        ppSwapChain,
          ID3D10Device1**         ppDevice) {
    InitReturnPtr(ppDevice);
    InitReturnPtr(ppSwapChain);

    // Try to create the device as usual
    Com<ID3D10Device1> d3d10Device = nullptr;
    HRESULT hr = D3D10CreateDevice1(pAdapter,
      DriverType, Software, Flags, HardwareLevel,
      SDKVersion, &d3d10Device);
    
    if (FAILED(hr))
      return hr;

    // Create the swap chain if requested
    if (pSwapChainDesc == nullptr)
      return E_INVALIDARG;
    
    Com<IDXGIDevice>  dxgiDevice  = nullptr;
    Com<IDXGIAdapter> dxgiAdapter = nullptr;
    Com<IDXGIFactory> dxgiFactory = nullptr;

    if (FAILED(d3d10Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice)))) {
      Logger::err("D3D11CreateDeviceAndSwapChain: Failed to query DXGI device");
      return E_FAIL;
    }
    
    if (FAILED(dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&dxgiAdapter)))) {
      Logger::err("D3D11CreateDeviceAndSwapChain: Failed to query DXGI adapter");
      return E_FAIL;
    }
    
    if (FAILED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory)))) {
      Logger::err("D3D11CreateDeviceAndSwapChain: Failed to query DXGI factory");
      return E_FAIL;
    }
    
    if (FAILED(dxgiFactory->CreateSwapChain(d3d10Device.ptr(), pSwapChainDesc, ppSwapChain))) {
      Logger::err("D3D11CreateDeviceAndSwapChain: Failed to create swap chain");
      return E_FAIL;
    }

    // Write back device pointer
    if (ppDevice != nullptr) {
      *ppDevice = d3d10Device.ptr();
      return S_OK;
    } return S_FALSE;
  }


  DLLEXPORT HRESULT D3D10CreateDeviceAndSwapChain(
          IDXGIAdapter*           pAdapter,
          D3D10_DRIVER_TYPE       DriverType,
          HMODULE                 Software,
          UINT                    Flags,
          UINT                    SDKVersion,
          DXGI_SWAP_CHAIN_DESC*   pSwapChainDesc,
          IDXGISwapChain**        ppSwapChain,
          ID3D10Device**          ppDevice) {
    InitReturnPtr(ppDevice);
    InitReturnPtr(ppSwapChain);

    Com<ID3D10Device1> d3d10Device = nullptr;
    HRESULT hr = D3D10CreateDeviceAndSwapChain1(pAdapter,
      DriverType, Software, Flags, D3D10_FEATURE_LEVEL_10_0,
      SDKVersion, pSwapChainDesc, ppSwapChain, &d3d10Device);
    
    if (FAILED(hr))
      return hr;
    
    if (ppDevice != nullptr) {
      *ppDevice = d3d10Device.ref();
      return S_OK;
    } return S_FALSE;
  }

}


