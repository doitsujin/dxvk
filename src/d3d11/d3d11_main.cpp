#include <array>

#include "../dxgi/dxgi_adapter.h"

#include "../dxvk/dxvk_instance.h"

#include "d3d11_device.h"
#include "d3d11_enums.h"
#include "d3d11_interop.h"
#include "d3d11_present.h"

namespace dxvk {
  Logger Logger::s_instance("d3d11.log");
}
  
extern "C" {
  using namespace dxvk;
  
  DLLEXPORT HRESULT __stdcall D3D11CoreCreateDevice(
          IDXGIFactory*       pFactory,
          IDXGIAdapter*       pAdapter,
          UINT                Flags,
    const D3D_FEATURE_LEVEL*  pFeatureLevels,
          UINT                FeatureLevels,
          ID3D11Device**      ppDevice) {
    Rc<DxvkAdapter>  dxvkAdapter;
    Rc<DxvkInstance> dxvkInstance;

    Com<IDXGIVkAdapter> dxgiVkAdapter;
    
    // Try to find the corresponding Vulkan device for the DXGI adapter
    if (SUCCEEDED(pAdapter->QueryInterface(__uuidof(IDXGIVkAdapter), reinterpret_cast<void**>(&dxgiVkAdapter)))) {
      dxvkAdapter  = dxgiVkAdapter->GetDXVKAdapter();
      dxvkInstance = dxvkAdapter->instance();
    } else {
      Logger::warn("D3D11CoreCreateDevice: Adapter is not a DXVK adapter");
      DXGI_ADAPTER_DESC desc;
      pAdapter->GetDesc(&desc);

      dxvkInstance = new DxvkInstance();
      dxvkAdapter  = dxvkInstance->findAdapterByLuid(&desc.AdapterLuid);

      if (dxvkAdapter == nullptr)
        dxvkAdapter = dxvkInstance->findAdapterByDeviceId(desc.VendorId, desc.DeviceId);
      
      if (dxvkAdapter == nullptr)
        dxvkAdapter = dxvkInstance->enumAdapters(0);

      if (dxvkAdapter == nullptr)
        return E_FAIL;
    }
    
    // Feature levels to probe if the
    // application does not specify any.
    std::array<D3D_FEATURE_LEVEL, 6> defaultFeatureLevels = {
      D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,
      D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1,
    };
    
    if (pFeatureLevels == nullptr || FeatureLevels == 0) {
      pFeatureLevels = defaultFeatureLevels.data();
      FeatureLevels  = defaultFeatureLevels.size();
    }
    
    // Find the highest feature level supported by the device.
    // This works because the feature level array is ordered.
    UINT flId;

    for (flId = 0 ; flId < FeatureLevels; flId++) {
      Logger::info(str::format("D3D11CoreCreateDevice: Probing ", pFeatureLevels[flId]));
      
      if (D3D11Device::CheckFeatureLevelSupport(dxvkAdapter, pFeatureLevels[flId]))
        break;
    }
    
    if (flId == FeatureLevels) {
      Logger::err("D3D11CoreCreateDevice: Requested feature level not supported");
      return E_INVALIDARG;
    }
    
    // Try to create the device with the given parameters.
    const D3D_FEATURE_LEVEL fl = pFeatureLevels[flId];
    
    try {
      Logger::info(str::format("D3D11CoreCreateDevice: Using feature level ", fl));
      Com<D3D11DXGIDevice> device = new D3D11DXGIDevice(
        pAdapter, dxvkAdapter.ptr(), fl, Flags);
      
      return device->QueryInterface(
        __uuidof(ID3D11Device),
        reinterpret_cast<void**>(ppDevice));
    } catch (const DxvkError& e) {
      Logger::err("D3D11CoreCreateDevice: Failed to create D3D11 device");
      return E_FAIL;
    }
  }
  
  
  DLLEXPORT HRESULT __stdcall D3D11CreateDevice(
          IDXGIAdapter*         pAdapter,
          D3D_DRIVER_TYPE       DriverType,
          HMODULE               Software,
          UINT                  Flags,
    const D3D_FEATURE_LEVEL*    pFeatureLevels,
          UINT                  FeatureLevels,
          UINT                  SDKVersion,
          ID3D11Device**        ppDevice,
          D3D_FEATURE_LEVEL*    pFeatureLevel,
          ID3D11DeviceContext** ppImmediateContext) {
    Com<IDXGIFactory> dxgiFactory = nullptr;
    Com<IDXGIAdapter> dxgiAdapter = pAdapter;
    
    if (dxgiAdapter == nullptr) {
      // We'll treat everything as hardware, even if the
      // Vulkan device is actually a software device.
      if (DriverType != D3D_DRIVER_TYPE_HARDWARE)
        Logger::warn("D3D11CreateDevice: Unsupported driver type");
      
      // We'll use the first adapter returned by a DXGI factory
      if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory)))) {
        Logger::err("D3D11CreateDevice: Failed to create a DXGI factory");
        return E_FAIL;
      }
      
      if (FAILED(dxgiFactory->EnumAdapters(0, &dxgiAdapter))) {
        Logger::err("D3D11CreateDevice: No default adapter available");
        return E_FAIL;
      }
      
    } else {
      // We should be able to query the DXGI factory from the adapter
      if (FAILED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory)))) {
        Logger::err("D3D11CreateDevice: Failed to query DXGI factory from DXGI adapter");
        return E_FAIL;
      }
      
      // In theory we could ignore these, but the Microsoft docs explicitly
      // state that we need to return E_INVALIDARG in case the arguments are
      // invalid. Both the driver type and software parameter can only be
      // set if the adapter itself is unspecified.
      // See: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476082(v=vs.85).aspx
      if (DriverType != D3D_DRIVER_TYPE_UNKNOWN || Software != nullptr)
        return E_INVALIDARG;
    }
    
    // Create the actual device
    Com<ID3D11Device> device;
    
    HRESULT hr = D3D11CoreCreateDevice(
      dxgiFactory.ptr(), dxgiAdapter.ptr(),
      Flags, pFeatureLevels, FeatureLevels,
      &device);
    
    if (FAILED(hr))
      return hr;
    
    // Write back whatever info the application requested
    if (pFeatureLevel != nullptr)
      *pFeatureLevel = device->GetFeatureLevel();
    
    if (ppDevice != nullptr)
      *ppDevice = device.ref();
    
    if (ppImmediateContext != nullptr)
      device->GetImmediateContext(ppImmediateContext);
    
    // If we were unable to write back the device and the
    // swap chain, the application has no way of working
    // with the device so we should report S_FALSE here.
    if (ppDevice == nullptr && ppImmediateContext == nullptr)
      return S_FALSE;
    
    return S_OK;
  }
  
  
  DLLEXPORT HRESULT __stdcall D3D11CreateDeviceAndSwapChain(
          IDXGIAdapter*         pAdapter,
          D3D_DRIVER_TYPE       DriverType,
          HMODULE               Software,
          UINT                  Flags,
    const D3D_FEATURE_LEVEL*    pFeatureLevels,
          UINT                  FeatureLevels,
          UINT                  SDKVersion,
    const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
          IDXGISwapChain**      ppSwapChain,
          ID3D11Device**        ppDevice,
          D3D_FEATURE_LEVEL*    pFeatureLevel,
          ID3D11DeviceContext** ppImmediateContext) {
    Com<ID3D11Device>        d3d11Device;
    Com<ID3D11DeviceContext> d3d11Context;
    
    if (ppSwapChain && !pSwapChainDesc)
      return E_INVALIDARG;
    
    // Try to create a device first.
    HRESULT status = D3D11CreateDevice(pAdapter, DriverType,
      Software, Flags, pFeatureLevels, FeatureLevels,
      SDKVersion, &d3d11Device, pFeatureLevel, &d3d11Context);
    
    if (FAILED(status))
      return status;
    
    // Again, the documentation does not exactly tell us what we
    // need to do in case one of the arguments is a null pointer.
    if (ppSwapChain) {
      Com<IDXGIDevice>  dxgiDevice  = nullptr;
      Com<IDXGIAdapter> dxgiAdapter = nullptr;
      Com<IDXGIFactory> dxgiFactory = nullptr;
      
      if (FAILED(d3d11Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice)))) {
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
      
      DXGI_SWAP_CHAIN_DESC desc = *pSwapChainDesc;
      if (FAILED(dxgiFactory->CreateSwapChain(d3d11Device.ptr(), &desc, ppSwapChain))) {
        Logger::err("D3D11CreateDeviceAndSwapChain: Failed to create swap chain");
        return E_FAIL;
      }
    }
    
    if (ppDevice != nullptr)
      *ppDevice = d3d11Device.ref();
    
    if (ppImmediateContext != nullptr)
      *ppImmediateContext = d3d11Context.ref();
    
    return S_OK;
  }
  
}