#include <dxgi_adapter.h>

#include "d3d11_device.h"

using namespace dxvk;

extern "C" {
  
  DLLEXPORT HRESULT __stdcall D3D11CreateDevice(
          IDXGIAdapter        *pAdapter,
          D3D_DRIVER_TYPE     DriverType,
          HMODULE             Software,
          UINT                Flags,
    const D3D_FEATURE_LEVEL   *pFeatureLevels,
          UINT                FeatureLevels,
          UINT                SDKVersion,
          ID3D11Device        **ppDevice,
          D3D_FEATURE_LEVEL   *pFeatureLevel,
          ID3D11DeviceContext **ppImmediateContext) {
    TRACE(pAdapter, DriverType, Software,
          Flags, pFeatureLevels, FeatureLevels,
          SDKVersion, ppDevice, pFeatureLevel,
          ppImmediateContext);
    Logger::err("D3D11CreateDevice: Not implemented");
    return E_NOTIMPL;
  }
  
  
  DLLEXPORT HRESULT __stdcall D3D11CreateDeviceAndSwapChain(
          IDXGIAdapter         *pAdapter,
          D3D_DRIVER_TYPE      DriverType,
          HMODULE              Software,
          UINT                 Flags,
    const D3D_FEATURE_LEVEL    *pFeatureLevels,
          UINT                 FeatureLevels,
          UINT                 SDKVersion,
    const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
          IDXGISwapChain       **ppSwapChain,
          ID3D11Device         **ppDevice,
          D3D_FEATURE_LEVEL    *pFeatureLevel,
          ID3D11DeviceContext  **ppImmediateContext) {
    Logger::err("D3D11CreateDeviceAndSwapChain: Not implemented");
    return E_NOTIMPL;
  }

}