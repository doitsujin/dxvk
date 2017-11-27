#include <cstdlib>
#include <cstring>

#include "dxgi_adapter.h"
#include "dxgi_factory.h"
#include "dxgi_output.h"

namespace dxvk {

  DxgiAdapter::DxgiAdapter(
          DxgiFactory*      factory,
    const Rc<DxvkAdapter>&  adapter)
  : m_factory (factory),
    m_adapter (adapter) {
    
  }
  
  
  DxgiAdapter::~DxgiAdapter() {
    
  }
  
  
  HRESULT DxgiAdapter::QueryInterface(
          REFIID riid,
          void **ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIObject);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIAdapter);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIAdapter1);
    COM_QUERY_IFACE(riid, ppvObject, IDXGIAdapterPrivate);
    
    Logger::warn("DxgiAdapter::QueryInterface: Unknown interface query");
    return E_NOINTERFACE;
  }
  
  
  HRESULT DxgiAdapter::GetParent(
          REFIID riid,
          void   **ppParent) {
    return m_factory->QueryInterface(riid, ppParent);
  }
  
  
  HRESULT DxgiAdapter::CheckInterfaceSupport(
          REFGUID       InterfaceName,
          LARGE_INTEGER *pUMDVersion) {
    Logger::err("DxgiAdapter::CheckInterfaceSupport: No D3D10 support");
    return DXGI_ERROR_UNSUPPORTED;
  }
  
  
  HRESULT DxgiAdapter::EnumOutputs(
          UINT        Output,
          IDXGIOutput **ppOutput) {
    if (ppOutput == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    int numDisplays = SDL_GetNumVideoDisplays();
    
    if (numDisplays < 0) {
      Logger::err("DxgiAdapter::EnumOutputs: Failed to query display count");
      return DXGI_ERROR_DRIVER_INTERNAL_ERROR;
    }
    
    if (Output >= static_cast<uint32_t>(numDisplays))
      return DXGI_ERROR_NOT_FOUND;
    
    *ppOutput = ref(new DxgiOutput(this, Output));
    return S_OK;
  }
  
  
  HRESULT DxgiAdapter::GetDesc(DXGI_ADAPTER_DESC* pDesc) {
    DXGI_ADAPTER_DESC1 desc1;
    HRESULT hr = this->GetDesc1(&desc1);
    
    if (SUCCEEDED(hr)) {
      std::memcpy(
        pDesc->Description,
        desc1.Description,
        sizeof(pDesc->Description));
      
      pDesc->VendorId               = desc1.VendorId;
      pDesc->DeviceId               = desc1.DeviceId;
      pDesc->SubSysId               = desc1.SubSysId;
      pDesc->Revision               = desc1.Revision;
      pDesc->DedicatedVideoMemory   = desc1.DedicatedVideoMemory;
      pDesc->DedicatedSystemMemory  = desc1.DedicatedSystemMemory;
      pDesc->SharedSystemMemory     = desc1.SharedSystemMemory;
      pDesc->AdapterLuid            = desc1.AdapterLuid;
    }
    
    return hr;
  }
  
  
  HRESULT DxgiAdapter::GetDesc1(DXGI_ADAPTER_DESC1* pDesc) {
    if (pDesc == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    const auto deviceProp = m_adapter->deviceProperties();
    const auto memoryProp = m_adapter->memoryProperties();
    
    std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
    std::mbstowcs(pDesc->Description, deviceProp.deviceName, _countof(pDesc->Description) - 1);
    
    VkDeviceSize deviceMemory = 0;
    VkDeviceSize sharedMemory = 0;
    
    for (uint32_t i = 0; i < memoryProp.memoryHeapCount; i++) {
      VkMemoryHeap heap = memoryProp.memoryHeaps[i];
      
      if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        deviceMemory += heap.size;
      else
        sharedMemory += heap.size;
    }
    
    pDesc->VendorId               = deviceProp.vendorID;
    pDesc->DeviceId               = deviceProp.deviceID;
    pDesc->SubSysId               = 0;
    pDesc->Revision               = 0;
    pDesc->DedicatedVideoMemory   = deviceMemory;
    pDesc->DedicatedSystemMemory  = 0;
    pDesc->SharedSystemMemory     = sharedMemory;
    pDesc->AdapterLuid            = LUID { 0, 0 };  // TODO implement
    pDesc->Flags                  = 0;
    return S_OK;
  }
  
  
  Rc<DxvkAdapter> DxgiAdapter::GetDXVKAdapter() {
    return m_adapter;
  }
  
}
