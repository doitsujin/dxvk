#include <cstdlib>
#include <cstring>

#include <d3d10_1.h>

#include "dxgi_adapter.h"
#include "dxgi_enums.h"
#include "dxgi_factory.h"
#include "dxgi_format.h"
#include "dxgi_options.h"
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
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDXGIObject)
     || riid == __uuidof(IDXGIAdapter)
     || riid == __uuidof(IDXGIAdapter1)
     || riid == __uuidof(IDXGIAdapter2)
     || riid == __uuidof(IDXGIAdapter3)
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
    const DxgiOptions* options = m_factory->GetOptions();

    if (pUMDVersion != nullptr)
      *pUMDVersion = LARGE_INTEGER();
    
    if (options->d3d10Enable) {
      if (InterfaceName == __uuidof(ID3D10Device)
       || InterfaceName == __uuidof(ID3D10Device1))
        return S_OK;
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
    
    const DxgiOptions* options = m_factory->GetOptions();
    
    auto deviceProp = m_adapter->deviceProperties();
    auto memoryProp = m_adapter->memoryProperties();
    auto deviceId   = m_adapter->devicePropertiesExt().coreDeviceId;
    
    // Custom Vendor / Device ID
    if (options->customVendorId >= 0)
      deviceProp.vendorID = options->customVendorId;
    
    if (options->customDeviceId >= 0)
      deviceProp.deviceID = options->customDeviceId;
    
    // XXX nvapi workaround for a lot of Unreal Engine 4 games
    if (options->customVendorId < 0 && options->customDeviceId < 0
     && options->nvapiHack && deviceProp.vendorID == uint16_t(DxvkGpuVendor::Nvidia)) {
      Logger::info("DXGI: NvAPI workaround enabled, reporting AMD GPU");
      deviceProp.vendorID = uint16_t(DxvkGpuVendor::Amd);
      deviceProp.deviceID = 0x67df; /* RX 480 */
    }
    
    // Convert device name
    std::memset(pDesc->Description, 0, sizeof(pDesc->Description));
    ::MultiByteToWideChar(CP_UTF8, 0, deviceProp.deviceName, -1,
        pDesc->Description, sizeof(pDesc->Description) / sizeof(*pDesc->Description));
    
    // Get amount of video memory
    // based on the Vulkan heaps
    VkDeviceSize deviceMemory = 0;
    VkDeviceSize sharedMemory = 0;
    
    for (uint32_t i = 0; i < memoryProp.memoryHeapCount; i++) {
      VkMemoryHeap heap = memoryProp.memoryHeaps[i];
      
      if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        deviceMemory += heap.size;
      else
        sharedMemory += heap.size;
    }
    
    // Some games are silly and need their memory limited
    if (options->maxDeviceMemory > 0
     && options->maxDeviceMemory < deviceMemory)
      deviceMemory = options->maxDeviceMemory;
    
    if (options->maxSharedMemory > 0
     && options->maxSharedMemory < sharedMemory)
      sharedMemory = options->maxSharedMemory;
    
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
    pDesc->AdapterLuid                    = LUID { 0, 0 };
    pDesc->Flags                          = 0;
    pDesc->GraphicsPreemptionGranularity  = DXGI_GRAPHICS_PREEMPTION_DMA_BUFFER_BOUNDARY;
    pDesc->ComputePreemptionGranularity   = DXGI_COMPUTE_PREEMPTION_DMA_BUFFER_BOUNDARY;

    if (deviceId.deviceLUIDValid)
      std::memcpy(&pDesc->AdapterLuid, deviceId.deviceLUID, VK_LUID_SIZE);
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::QueryVideoMemoryInfo(
          UINT                          NodeIndex,
          DXGI_MEMORY_SEGMENT_GROUP     MemorySegmentGroup,
          DXGI_QUERY_VIDEO_MEMORY_INFO* pVideoMemoryInfo) {
    if (NodeIndex > 0 || !pVideoMemoryInfo)
      return DXGI_ERROR_INVALID_CALL;
    
    if (MemorySegmentGroup != DXGI_MEMORY_SEGMENT_GROUP_LOCAL
     && MemorySegmentGroup != DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL)
      return DXGI_ERROR_INVALID_CALL;
    
    DxvkAdapterMemoryInfo memInfo = m_adapter->getMemoryHeapInfo();

    VkMemoryHeapFlags heapFlagMask = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
    VkMemoryHeapFlags heapFlags    = 0;

    if (MemorySegmentGroup == DXGI_MEMORY_SEGMENT_GROUP_LOCAL)
      heapFlags |= VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
    
    pVideoMemoryInfo->Budget       = 0;
    pVideoMemoryInfo->CurrentUsage = 0;

    for (uint32_t i = 0; i < memInfo.heapCount; i++) {
      if ((memInfo.heaps[i].heapFlags & heapFlagMask) != heapFlags)
        continue;
      
      pVideoMemoryInfo->Budget       += memInfo.heaps[i].memoryAvailable;
      pVideoMemoryInfo->CurrentUsage += memInfo.heaps[i].memoryAllocated;
    }

    // We don't implement reservation, but the observable
    // behaviour should match that of Windows drivers
    uint32_t segmentId = uint32_t(MemorySegmentGroup);

    pVideoMemoryInfo->AvailableForReservation = pVideoMemoryInfo->Budget / 2;
    pVideoMemoryInfo->CurrentReservation      = m_memReservation[segmentId];
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE DxgiAdapter::SetVideoMemoryReservation(
          UINT                          NodeIndex,
          DXGI_MEMORY_SEGMENT_GROUP     MemorySegmentGroup,
          UINT64                        Reservation) {
    DXGI_QUERY_VIDEO_MEMORY_INFO info;

    HRESULT hr = QueryVideoMemoryInfo(
      NodeIndex, MemorySegmentGroup, &info);
    
    if (FAILED(hr))
      return hr;
    
    if (Reservation > info.AvailableForReservation)
      return DXGI_ERROR_INVALID_CALL;
    
    uint32_t segmentId = uint32_t(MemorySegmentGroup);
    m_memReservation[segmentId] = Reservation;
    return S_OK;
  }

  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::RegisterHardwareContentProtectionTeardownStatusEvent(
          HANDLE                        hEvent,
          DWORD*                        pdwCookie) {
    Logger::err("DxgiAdapter::RegisterHardwareContentProtectionTeardownStatusEvent: Not implemented");
    return E_NOTIMPL;
  }


  HRESULT STDMETHODCALLTYPE DxgiAdapter::RegisterVideoMemoryBudgetChangeNotificationEvent(
          HANDLE                        hEvent,
          DWORD*                        pdwCookie) {
    Logger::err("DxgiAdapter::RegisterVideoMemoryBudgetChangeNotificationEvent: Not implemented");
    return E_NOTIMPL;
  }
  

  void STDMETHODCALLTYPE DxgiAdapter::UnregisterHardwareContentProtectionTeardownStatus(
          DWORD                         dwCookie) {
    Logger::err("DxgiAdapter::UnregisterHardwareContentProtectionTeardownStatus: Not implemented");
  }


  void STDMETHODCALLTYPE DxgiAdapter::UnregisterVideoMemoryBudgetChangeNotification(
          DWORD                         dwCookie) {
    Logger::err("DxgiAdapter::UnregisterVideoMemoryBudgetChangeNotification: Not implemented");
  }


  Rc<DxvkAdapter> STDMETHODCALLTYPE DxgiAdapter::GetDXVKAdapter() {
    return m_adapter;
  }
  
}
