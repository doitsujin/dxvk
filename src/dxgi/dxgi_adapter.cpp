#include <cstdlib>
#include <cstring>

#include <d3d10_1.h>

#include "dxgi_adapter.h"
#include "dxgi_device.h"
#include "dxgi_enums.h"
#include "dxgi_factory.h"
#include "dxgi_format.h"
#include "dxgi_options.h"
#include "dxgi_output.h"

#include "../dxvk/vulkan/dxvk_vulkan_names.h"

namespace dxvk {

  DxgiAdapter::DxgiAdapter(
          DxgiFactory*      factory,
    const Rc<DxvkAdapter>&  adapter)
  : m_factory (factory),
    m_adapter (adapter),
    m_formats (adapter) {
    
  }
  
  
  DxgiAdapter::~DxgiAdapter() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDXGIObject)
     || riid == __uuidof(IDXGIAdapter)
     || riid == __uuidof(IDXGIAdapter1)
     || riid == __uuidof(IDXGIAdapter2)
     || riid == __uuidof(IDXGIVkAdapter)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    Logger::warn("DxgiAdapter::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::GetParent(REFIID riid, void** ppParent) {
    return m_factory->QueryInterface(riid, ppParent);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::CheckInterfaceSupport(
          REFGUID                   InterfaceName,
          LARGE_INTEGER*            pUMDVersion) {
    if (pUMDVersion != nullptr)
      *pUMDVersion = LARGE_INTEGER();
    
    if (InterfaceName == __uuidof(ID3D10Device)
     || InterfaceName == __uuidof(ID3D10Device1)) {
      Logger::warn("DXGI: CheckInterfaceSupport: No D3D10 support");
      
      return m_factory->GetOptions()->fakeDx10Support
        ? S_OK : DXGI_ERROR_UNSUPPORTED;
    }
    
    Logger::err("DXGI: CheckInterfaceSupport: Unsupported interface");
    Logger::err(str::format(InterfaceName));
    return DXGI_ERROR_UNSUPPORTED;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::EnumOutputs(
          UINT                      Output,
          IDXGIOutput**             ppOutput) {
    InitReturnPtr(ppOutput);
    
    if (ppOutput == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    if (Output > 0) {
      *ppOutput = nullptr;
      return DXGI_ERROR_NOT_FOUND;
    }
    
    // TODO support multiple monitors
    HMONITOR monitor = ::MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    *ppOutput = ref(new DxgiOutput(this, monitor));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::GetDesc(DXGI_ADAPTER_DESC* pDesc) {
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;

    DXGI_ADAPTER_DESC2 desc;
    HRESULT hr = GetDesc2(&desc);
    
    if (SUCCEEDED(hr)) {
      std::memcpy(pDesc->Description, desc.Description, sizeof(pDesc->Description));
      
      pDesc->VendorId               = desc.VendorId;
      pDesc->DeviceId               = desc.DeviceId;
      pDesc->SubSysId               = desc.SubSysId;
      pDesc->Revision               = desc.Revision;
      pDesc->DedicatedVideoMemory   = desc.DedicatedVideoMemory;
      pDesc->DedicatedSystemMemory  = desc.DedicatedSystemMemory;
      pDesc->SharedSystemMemory     = desc.SharedSystemMemory;
      pDesc->AdapterLuid            = desc.AdapterLuid;
    }
    
    return hr;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::GetDesc1(DXGI_ADAPTER_DESC1* pDesc) {
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;

    DXGI_ADAPTER_DESC2 desc;
    HRESULT hr = GetDesc2(&desc);
    
    if (SUCCEEDED(hr)) {
      std::memcpy(pDesc->Description, desc.Description, sizeof(pDesc->Description));
      
      pDesc->VendorId               = desc.VendorId;
      pDesc->DeviceId               = desc.DeviceId;
      pDesc->SubSysId               = desc.SubSysId;
      pDesc->Revision               = desc.Revision;
      pDesc->DedicatedVideoMemory   = desc.DedicatedVideoMemory;
      pDesc->DedicatedSystemMemory  = desc.DedicatedSystemMemory;
      pDesc->SharedSystemMemory     = desc.SharedSystemMemory;
      pDesc->AdapterLuid            = desc.AdapterLuid;
      pDesc->Flags                  = desc.Flags;
    }
    
    return hr;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::GetDesc2(DXGI_ADAPTER_DESC2* pDesc) {
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    auto deviceProp = m_adapter->deviceProperties();
    auto memoryProp = m_adapter->memoryProperties();
    
    // Custom Vendor / Device ID
    const int32_t customVendorID = m_factory->GetOptions()->customVendorId;
    const int32_t customDeviceID = m_factory->GetOptions()->customDeviceId;
    
    if (customVendorID >= 0) {
      Logger::info(str::format("Using Custom PCI Vendor ID ", std::hex, customVendorID));
      deviceProp.vendorID = customVendorID;
    }
    
    if (customDeviceID >= 0) {
      Logger::info(str::format("Using Custom PCI Device ID ", std::hex, customDeviceID));
      deviceProp.deviceID = customDeviceID;
    }
    
    std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
    std::mbstowcs(pDesc->Description, deviceProp.deviceName, std::size(pDesc->Description) - 1);
    
    VkDeviceSize deviceMemory = 0;
    VkDeviceSize sharedMemory = 0;
    
    for (uint32_t i = 0; i < memoryProp.memoryHeapCount; i++) {
      VkMemoryHeap heap = memoryProp.memoryHeaps[i];
      
      if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        deviceMemory += heap.size;
      else
        sharedMemory += heap.size;
    }
    
    #ifndef _WIN64
    // The value returned by DXGI is a 32-bit value
    // on 32-bit platforms, so we need to clamp it
    VkDeviceSize maxMemory = 0xC0000000;
    deviceMemory = std::min(deviceMemory, maxMemory);
    sharedMemory = std::min(sharedMemory, maxMemory);
    #endif
    
    pDesc->VendorId                       = deviceProp.vendorID;
    pDesc->DeviceId                       = deviceProp.deviceID;
    pDesc->SubSysId                       = 0;
    pDesc->Revision                       = 0;
    pDesc->DedicatedVideoMemory           = deviceMemory;
    pDesc->DedicatedSystemMemory          = 0;
    pDesc->SharedSystemMemory             = sharedMemory;
    pDesc->AdapterLuid                    = LUID { 0, 0 };  // TODO implement
    pDesc->Flags                          = 0;
    pDesc->GraphicsPreemptionGranularity  = DXGI_GRAPHICS_PREEMPTION_DMA_BUFFER_BOUNDARY;
    pDesc->ComputePreemptionGranularity   = DXGI_COMPUTE_PREEMPTION_DMA_BUFFER_BOUNDARY;
    return S_OK;
  }
  
  
  Rc<DxvkAdapter> STDMETHODCALLTYPE DxgiAdapter::GetDXVKAdapter() {
    return m_adapter;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::CreateDevice(
          IDXGIObject*              pContainer,
    const DxvkDeviceFeatures*       pFeatures,
          IDXGIVkDevice**           ppDevice) {
    InitReturnPtr(ppDevice);
    
    try {
      *ppDevice = new dxvk::DxgiDevice(pContainer,
        this, m_factory->GetOptions(), pFeatures);
      return S_OK;
    } catch (const dxvk::DxvkError& e) {
      dxvk::Logger::err(e.message());
      return DXGI_ERROR_UNSUPPORTED;
    }
  }
  
  
  DXGI_VK_FORMAT_INFO STDMETHODCALLTYPE DxgiAdapter::LookupFormat(
          DXGI_FORMAT               Format,
          DXGI_VK_FORMAT_MODE       Mode) {
    return m_formats.GetFormatInfo(Format, Mode);
  }
  
  
  DXGI_VK_FORMAT_FAMILY STDMETHODCALLTYPE DxgiAdapter::LookupFormatFamily(
          DXGI_FORMAT               Format,
          DXGI_VK_FORMAT_MODE       Mode) {
    return m_formats.GetFormatFamily(Format, Mode);
  }
  
  
  HRESULT DxgiAdapter::GetOutputFromMonitor(
          HMONITOR                  Monitor,
          IDXGIOutput**             ppOutput) {
    if (ppOutput == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    for (uint32_t i = 0; SUCCEEDED(EnumOutputs(i, ppOutput)); i++) {
      DXGI_OUTPUT_DESC outputDesc;
      (*ppOutput)->GetDesc(&outputDesc);
      
      if (outputDesc.Monitor == Monitor)
        return S_OK;
      
      (*ppOutput)->Release();
      (*ppOutput) = nullptr;
    }
    
    // No such output found
    return DXGI_ERROR_NOT_FOUND;
  }
  
  
  HRESULT DxgiAdapter::GetOutputData(
          HMONITOR                  Monitor,
          DXGI_VK_OUTPUT_DATA*      pOutputData) {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    
    auto entry = m_outputData.find(Monitor);
    if (entry == m_outputData.end())
      return DXGI_ERROR_NOT_FOUND;
    
    if (pOutputData == nullptr)
      return S_FALSE;
    
    *pOutputData = entry->second;
    return S_OK;
  }
  
  
  HRESULT DxgiAdapter::SetOutputData(
          HMONITOR                  Monitor,
    const DXGI_VK_OUTPUT_DATA*      pOutputData) {
    std::lock_guard<std::mutex> lock(m_outputMutex);
    
    m_outputData.insert_or_assign(Monitor, *pOutputData);
    return S_OK;
  }
  
}
