#include <array>

#include "../dxgi/dxgi_adapter.h"

#include "../dxvk/dxvk_instance.h"

#include "d3d11_device.h"
#include "d3d11_enums.h"
#include "d3d11_interop.h"

namespace dxvk {
  Logger Logger::s_instance("d3d11.log");
}
  
extern "C" {
  using namespace dxvk;
  
  HRESULT D3D11InternalCreateDevice(
          IDXGIFactory*       pFactory,
          IDXGIAdapter*       pAdapter,
          UINT                Flags,
    const D3D_FEATURE_LEVEL*  pFeatureLevels,
          UINT                FeatureLevels,
          ID3D11Device**      ppDevice) {
    InitReturnPtr(ppDevice);

    Rc<DxvkAdapter>  dxvkAdapter;
    Rc<DxvkInstance> dxvkInstance;

    Com<IDXGIDXVKAdapter> dxgiVkAdapter;
    
    // Try to find the corresponding Vulkan device for the DXGI adapter
    if (SUCCEEDED(pAdapter->QueryInterface(__uuidof(IDXGIDXVKAdapter), reinterpret_cast<void**>(&dxgiVkAdapter)))) {
      dxvkAdapter  = dxgiVkAdapter->GetDXVKAdapter();
      dxvkInstance = dxgiVkAdapter->GetDXVKInstance();
    } else {
      Logger::warn("D3D11InternalCreateDevice: Adapter is not a DXVK adapter");
      DXGI_ADAPTER_DESC desc;
      pAdapter->GetDesc(&desc);

      dxvkInstance = new DxvkInstance(0);
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
    
    if (!pFeatureLevels || !FeatureLevels) {
      pFeatureLevels = defaultFeatureLevels.data();
      FeatureLevels  = defaultFeatureLevels.size();
    }
    
    // Find the highest feature level supported by the device.
    // This works because the feature level array is ordered.
    D3D_FEATURE_LEVEL maxFeatureLevel = D3D11Device::GetMaxFeatureLevel(dxvkInstance, dxvkAdapter);
    D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL();
    D3D_FEATURE_LEVEL devFeatureLevel = D3D_FEATURE_LEVEL();

    Logger::info(str::format("D3D11InternalCreateDevice: Maximum supported feature level: ", maxFeatureLevel));

    for (uint32_t flId = 0 ; flId < FeatureLevels; flId++) {
      minFeatureLevel = pFeatureLevels[flId];

      if (minFeatureLevel <= maxFeatureLevel) {
        devFeatureLevel = minFeatureLevel;
        break;
      }
    }

    if (!devFeatureLevel) {
      Logger::err(str::format("D3D11InternalCreateDevice: Minimum required feature level ", minFeatureLevel, " not supported"));
      return E_INVALIDARG;
    }

    try {
      Logger::info(str::format("D3D11InternalCreateDevice: Using feature level ", devFeatureLevel));

      DxvkDeviceFeatures deviceFeatures = D3D11Device::GetDeviceFeatures(dxvkAdapter);
      Rc<DxvkDevice> dxvkDevice = dxvkAdapter->createDevice(dxvkInstance, deviceFeatures);

      Com<D3D11DXGIDevice> device = new D3D11DXGIDevice(
        pAdapter, nullptr, nullptr,
        dxvkInstance, dxvkAdapter, dxvkDevice,
        devFeatureLevel, Flags);

      return device->QueryInterface(
        __uuidof(ID3D11Device),
        reinterpret_cast<void**>(ppDevice));
    } catch (const DxvkError& e) {
      Logger::err("D3D11InternalCreateDevice: Failed to create D3D11 device");
      return E_FAIL;
    }
  }
  
  
  static HRESULT D3D11InternalCreateDeviceAndSwapChain(
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
    InitReturnPtr(ppDevice);
    InitReturnPtr(ppSwapChain);
    InitReturnPtr(ppImmediateContext);

    if (pFeatureLevel)
      *pFeatureLevel = D3D_FEATURE_LEVEL(0);

    HRESULT hr;

    Com<IDXGIFactory> dxgiFactory = nullptr;
    Com<IDXGIAdapter> dxgiAdapter = pAdapter;
    Com<ID3D11Device> device      = nullptr;
    
    if (ppSwapChain && !pSwapChainDesc)
      return E_INVALIDARG;
    
    if (!pAdapter) {
      // We'll treat everything as hardware, even if the
      // Vulkan device is actually a software device.
      if (DriverType != D3D_DRIVER_TYPE_HARDWARE)
        Logger::warn("D3D11CreateDevice: Unsupported driver type");
      
      // We'll use the first adapter returned by a DXGI factory
      hr = CreateDXGIFactory1(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory));

      if (FAILED(hr)) {
        Logger::err("D3D11CreateDevice: Failed to create a DXGI factory");
        return hr;
      }

      hr = dxgiFactory->EnumAdapters(0, &dxgiAdapter);

      if (FAILED(hr)) {
        Logger::err("D3D11CreateDevice: No default adapter available");
        return hr;
      }
    } else {
      // We should be able to query the DXGI factory from the adapter
      if (FAILED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&dxgiFactory)))) {
        Logger::err("D3D11CreateDevice: Failed to query DXGI factory from DXGI adapter");
        return E_INVALIDARG;
      }
      
      // In theory we could ignore these, but the Microsoft docs explicitly
      // state that we need to return E_INVALIDARG in case the arguments are
      // invalid. Both the driver type and software parameter can only be
      // set if the adapter itself is unspecified.
      // See: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476082(v=vs.85).aspx
      if (DriverType != D3D_DRIVER_TYPE_UNKNOWN || Software)
        return E_INVALIDARG;
    }
    
    // Create the actual device
    hr = D3D11InternalCreateDevice(
      dxgiFactory.ptr(), dxgiAdapter.ptr(),
      Flags, pFeatureLevels, FeatureLevels,
      &device);
    
    if (FAILED(hr))
      return hr;
    
    // Create the swap chain, if requested
    if (ppSwapChain) {
      DXGI_SWAP_CHAIN_DESC desc = *pSwapChainDesc;
      hr = dxgiFactory->CreateSwapChain(device.ptr(), &desc, ppSwapChain);

      if (FAILED(hr)) {
        Logger::err("D3D11CreateDevice: Failed to create swap chain");
        return hr;
      }
    }
    
    // Write back whatever info the application requested
    if (pFeatureLevel)
      *pFeatureLevel = device->GetFeatureLevel();
    
    if (ppDevice)
      *ppDevice = device.ref();
    
    if (ppImmediateContext)
      device->GetImmediateContext(ppImmediateContext);

    // If we were unable to write back the device and the
    // swap chain, the application has no way of working
    // with the device so we should report S_FALSE here.
    if (!ppDevice && !ppImmediateContext && !ppSwapChain)
      return S_FALSE;
    
    return S_OK;
  }
  

    DLLEXPORT HRESULT __stdcall D3D11CoreCreateDevice(
            IDXGIFactory*       pFactory,
            IDXGIAdapter*       pAdapter,
            D3D_DRIVER_TYPE     DriverType,
            HMODULE             Software,
            UINT                Flags,
      const D3D_FEATURE_LEVEL*  pFeatureLevels,
            UINT                FeatureLevels,
            UINT                SDKVersion,
            ID3D11Device**      ppDevice,
            D3D_FEATURE_LEVEL*  pFeatureLevel) {
    return D3D11InternalCreateDeviceAndSwapChain(
      pAdapter, DriverType, Software, Flags,
      pFeatureLevels, FeatureLevels, SDKVersion,
      nullptr, nullptr,
      ppDevice, pFeatureLevel, nullptr);
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
    return D3D11InternalCreateDeviceAndSwapChain(
      pAdapter, DriverType, Software, Flags,
      pFeatureLevels, FeatureLevels, SDKVersion,
      nullptr, nullptr,
      ppDevice, pFeatureLevel, ppImmediateContext);
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
    return D3D11InternalCreateDeviceAndSwapChain(
      pAdapter, DriverType, Software, Flags,
      pFeatureLevels, FeatureLevels, SDKVersion,
      pSwapChainDesc, ppSwapChain,
      ppDevice, pFeatureLevel, ppImmediateContext);
  }
  

  DLLEXPORT HRESULT __stdcall D3D11On12CreateDevice(
          IUnknown*             pDevice,
          UINT                  Flags,
    const D3D_FEATURE_LEVEL*    pFeatureLevels,
          UINT                  FeatureLevels,
          IUnknown* const*      ppCommandQueues,
          UINT                  NumQueues,
          UINT                  NodeMask,
          ID3D11Device**        ppDevice,
          ID3D11DeviceContext** ppImmediateContext,
          D3D_FEATURE_LEVEL*    pChosenFeatureLevel) {
    InitReturnPtr(ppDevice);
    InitReturnPtr(ppImmediateContext);

    if (pChosenFeatureLevel)
      *pChosenFeatureLevel = D3D_FEATURE_LEVEL(0);

    if (!pDevice)
      return E_INVALIDARG;

    // Figure out D3D12 objects
    Com<ID3D12Device> d3d12Device;
    Com<ID3D12CommandQueue> d3d12Queue;

    if (FAILED(pDevice->QueryInterface(__uuidof(ID3D12Device), reinterpret_cast<void**>(&d3d12Device)))) {
      Logger::err("D3D11On12CreateDevice: Device is not a valid D3D12 device");
      return E_INVALIDARG;
    }

    if (NodeMask & (NodeMask - 1)) {
      Logger::err("D3D11On12CreateDevice: Invalid node mask");
      return E_INVALIDARG;
    }

    if (!NumQueues || !ppCommandQueues || !ppCommandQueues[0]) {
      Logger::err("D3D11On12CreateDevice: No command queue specified");
      return E_INVALIDARG;
    }

    if (NumQueues > 1) {
      // Not sure what to do with more than one graphics queue
      Logger::warn("D3D11On12CreateDevice: Only one queue supported");
    }

    if (FAILED(ppCommandQueues[0]->QueryInterface(__uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&d3d12Queue)))) {
      Logger::err("D3D11On12CreateDevice: Queue is not a valid D3D12 command queue");
      return E_INVALIDARG;
    }

    // Determine feature level for the D3D11 device
    std::array<D3D_FEATURE_LEVEL, 4> defaultFeatureLevels = {{
      D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_12_1,
    }};

    D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevel = { };

    if (!FeatureLevels || !pFeatureLevels) {
      featureLevel.NumFeatureLevels = defaultFeatureLevels.size();
      featureLevel.pFeatureLevelsRequested = defaultFeatureLevels.data();
    } else {
      featureLevel.NumFeatureLevels = FeatureLevels;
      featureLevel.pFeatureLevelsRequested = pFeatureLevels;
    }

    HRESULT hr = d3d12Device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevel, sizeof(featureLevel));

    if (FAILED(hr) || !featureLevel.MaxSupportedFeatureLevel) {
      Logger::err(str::format("D3D11On12CreateDevice: Minimum required feature level not supported"));
      return hr;
    }

    Logger::info(str::format("D3D11On12CreateDevice: Chosen feature level: ", featureLevel.MaxSupportedFeatureLevel));

    Com<ID3D12DXVKInteropDevice> interopDevice;

    if (FAILED(d3d12Device->QueryInterface(__uuidof(ID3D12DXVKInteropDevice), reinterpret_cast<void**>(&interopDevice)))) {
      Logger::err("D3D11On12CreateDevice: Device not a vkd3d-proton device.");
      return E_INVALIDARG;
    }

    Com<IDXGIAdapter> dxgiAdapter;

    if (FAILED(interopDevice->GetDXGIAdapter(IID_PPV_ARGS(&dxgiAdapter)))) {
      Logger::err("D3D11On12CreateDevice: Failed to query DXGI adapter.");
      return E_INVALIDARG;
    }

    try {
      // Initialize DXVK instance
      DxvkInstanceImportInfo instanceInfo = { };
      DxvkDeviceImportInfo deviceInfo = { };
      VkPhysicalDevice vulkanAdapter = VK_NULL_HANDLE;

      interopDevice->GetVulkanHandles(&instanceInfo.instance, &vulkanAdapter, &deviceInfo.device);

      uint32_t instanceExtensionCount = 0;
      interopDevice->GetInstanceExtensions(&instanceExtensionCount, nullptr);

      std::vector<const char*> instanceExtensions(instanceExtensionCount);
      interopDevice->GetInstanceExtensions(&instanceExtensionCount, instanceExtensions.data());

      instanceInfo.extensionCount = instanceExtensions.size();
      instanceInfo.extensionNames = instanceExtensions.data();

      Rc<DxvkInstance> dxvkInstance = new DxvkInstance(instanceInfo, 0);

      // Find adapter by physical device handle
      Rc<DxvkAdapter> dxvkAdapter;

      for (uint32_t i = 0; i < dxvkInstance->adapterCount(); i++) {
        Rc<DxvkAdapter> curr = dxvkInstance->enumAdapters(i);

        if (curr->handle() == vulkanAdapter)
          dxvkAdapter = std::move(curr);
      }

      if (dxvkAdapter == nullptr) {
        Logger::err("D3D11On12CreateDevice: No matching adapter found");
        return E_INVALIDARG;
      }

      interopDevice->GetVulkanQueueInfo(d3d12Queue.ptr(), &deviceInfo.queue, &deviceInfo.queueFamily);
      interopDevice->GetDeviceFeatures(&deviceInfo.features);

      uint32_t deviceExtensionCount = 0;
      interopDevice->GetDeviceExtensions(&deviceExtensionCount, nullptr);

      std::vector<const char*> deviceExtensions(deviceExtensionCount);
      interopDevice->GetDeviceExtensions(&deviceExtensionCount, deviceExtensions.data());

      deviceInfo.extensionCount = deviceExtensions.size();
      deviceInfo.extensionNames = deviceExtensions.data();

      deviceInfo.queueCallback = [
        cDevice = interopDevice,
        cQueue = d3d12Queue
      ] (bool doLock) {
        HRESULT hr = doLock
          ? cDevice->LockCommandQueue(cQueue.ptr())
          : cDevice->UnlockCommandQueue(cQueue.ptr());

        if (FAILED(hr))
          Logger::err(str::format("Failed to lock vkd3d-proton device queue: ", hr));
      };

      Rc<DxvkDevice> dxvkDevice = dxvkAdapter->importDevice(dxvkInstance, deviceInfo);

      // Create and return the actual D3D11 device
      Com<D3D11DXGIDevice> device = new D3D11DXGIDevice(
        dxgiAdapter.ptr(), d3d12Device.ptr(), d3d12Queue.ptr(),
        dxvkInstance, dxvkAdapter, dxvkDevice,
        featureLevel.MaxSupportedFeatureLevel, Flags);

      Com<ID3D11Device> d3d11Device;
      device->QueryInterface(__uuidof(ID3D11Device), reinterpret_cast<void**>(&d3d11Device));

      if (ppDevice)
        *ppDevice = d3d11Device.ref();

      if (ppImmediateContext)
        d3d11Device->GetImmediateContext(ppImmediateContext);

      if (pChosenFeatureLevel)
        *pChosenFeatureLevel = d3d11Device->GetFeatureLevel();

      if (!ppDevice && !ppImmediateContext)
        return S_FALSE;

      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err("D3D11On12CreateDevice: Failed to create D3D11 device");
      return E_FAIL;
    }
  }

}