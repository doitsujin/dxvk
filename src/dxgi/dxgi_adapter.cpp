#include <cstdlib>
#include <cstring>

#include <d3d10_1.h>

#include "dxgi_adapter.h"
#include "dxgi_enums.h"
#include "dxgi_factory.h"
#include "dxgi_format.h"
#include "dxgi_options.h"
#include "dxgi_output.h"

#include "../util/util_luid.h"
#include "../util/util_win32_compat.h"

#include "../wsi/wsi_monitor.h"

namespace dxvk {

  DxgiVkAdapter::DxgiVkAdapter(DxgiAdapter* pAdapter)
  : m_adapter(pAdapter) {

  }


  ULONG STDMETHODCALLTYPE DxgiVkAdapter::AddRef() {
    return m_adapter->AddRef();
  }
  

  ULONG STDMETHODCALLTYPE DxgiVkAdapter::Release() {
    return m_adapter->Release();
  }

  
  HRESULT STDMETHODCALLTYPE DxgiVkAdapter::QueryInterface(
          REFIID                    riid,
          void**                    ppvObject) {
    return m_adapter->QueryInterface(riid, ppvObject);
  }

  
  void STDMETHODCALLTYPE DxgiVkAdapter::GetVulkanHandles(
          VkInstance*               pInstance,
          VkPhysicalDevice*         pPhysDev) {
    auto adapter  = m_adapter->GetDXVKAdapter();
    auto instance = m_adapter->GetDXVKInstance();

    if (pInstance)
      *pInstance = instance->handle();
    
    if (pPhysDev)
      *pPhysDev = adapter->handle();
  }




  DxgiAdapter::DxgiAdapter(
          DxgiFactory*      factory,
    const Rc<DxvkAdapter>&  adapter,
          UINT              index)
  : m_factory (factory),
    m_adapter (adapter),
    m_interop (this),
    m_index   (index),
    m_desc    (GetAdapterDesc()) {
    
  }
  
  
  DxgiAdapter::~DxgiAdapter() {
    if (m_eventThread.joinable()) {
      std::unique_lock<dxvk::mutex> lock(m_mutex);
      m_eventCookie = ~0u;
      m_cond.notify_one();

      lock.unlock();
      m_eventThread.join();
    }
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDXGIObject)
     || riid == __uuidof(IDXGIAdapter)
     || riid == __uuidof(IDXGIAdapter1)
     || riid == __uuidof(IDXGIAdapter2)
     || riid == __uuidof(IDXGIAdapter3)
     || riid == __uuidof(IDXGIAdapter4)
     || riid == __uuidof(IDXGIDXVKAdapter)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(IDXGIVkInteropAdapter)) {
      *ppvObject = ref(&m_interop);
      return S_OK;
    }
    
    if (logQueryInterfaceError(__uuidof(IDXGIAdapter), riid)) {
      Logger::warn("DxgiAdapter::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::GetParent(REFIID riid, void** ppParent) {
    return m_factory->QueryInterface(riid, ppParent);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::CheckInterfaceSupport(
          REFGUID                   InterfaceName,
          LARGE_INTEGER*            pUMDVersion) {
    HRESULT hr = DXGI_ERROR_UNSUPPORTED;

    if (InterfaceName == __uuidof(IDXGIDevice)
     || InterfaceName == __uuidof(ID3D10Device)
     || InterfaceName == __uuidof(ID3D10Device1))
      hr = S_OK;

    // Windows drivers return something along the lines of 32.0.xxxxx.yyyy,
    // so just be conservative here and return a high number. We cannot
    // reconstruct meaningful UMD versions from Vulkan driver versions.
    if (SUCCEEDED(hr) && pUMDVersion) {
      pUMDVersion->HighPart = 0x00200000u;
      pUMDVersion->LowPart  = 0xffffffffu;
    }

    if (FAILED(hr)) {
      Logger::err("DXGI: CheckInterfaceSupport: Unsupported interface");
      Logger::err(str::format(InterfaceName));
    }

    return hr;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::EnumOutputs(
          UINT                      Output,
          IDXGIOutput**             ppOutput) {
    InitReturnPtr(ppOutput);
    
    if (ppOutput == nullptr)
      return E_INVALIDARG;

    const auto& deviceId = m_adapter->devicePropertiesExt().vk11;

    std::array<const LUID*, 2> adapterLUIDs = { };
    uint32_t numLUIDs = 0;

    if (m_adapter->isLinkedToDGPU())
      return DXGI_ERROR_NOT_FOUND;

    if (deviceId.deviceLUIDValid)
      adapterLUIDs[numLUIDs++] = reinterpret_cast<const LUID*>(deviceId.deviceLUID);

    auto linkedAdapter = m_adapter->linkedIGPUAdapter();

    // If either LUID is not valid, enumerate all monitors.
    if (numLUIDs && linkedAdapter != nullptr) {
      const auto& deviceId = linkedAdapter->devicePropertiesExt().vk11;

      if (deviceId.deviceLUIDValid)
        adapterLUIDs[numLUIDs++] = reinterpret_cast<const LUID*>(deviceId.deviceLUID);
      else
        numLUIDs = 0;
    }

    // Enumerate all monitors if the robustness fallback is active.
    if (m_factory->UseMonitorFallback())
      numLUIDs = 0;

    HMONITOR monitor = wsi::enumMonitors(adapterLUIDs.data(), numLUIDs, Output);

    if (monitor == nullptr)
      return DXGI_ERROR_NOT_FOUND;

    *ppOutput = ref(new DxgiOutput(m_factory, this, monitor));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::GetDesc(DXGI_ADAPTER_DESC* pDesc) {
    if (pDesc == nullptr)
      return E_INVALIDARG;

    std::memcpy(pDesc->Description, m_desc.Description, sizeof(pDesc->Description));
    pDesc->VendorId               = m_desc.VendorId;
    pDesc->DeviceId               = m_desc.DeviceId;
    pDesc->SubSysId               = m_desc.SubSysId;
    pDesc->Revision               = m_desc.Revision;
    pDesc->DedicatedVideoMemory   = m_desc.DedicatedVideoMemory;
    pDesc->DedicatedSystemMemory  = m_desc.DedicatedSystemMemory;
    pDesc->SharedSystemMemory     = m_desc.SharedSystemMemory;
    pDesc->AdapterLuid            = m_desc.AdapterLuid;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::GetDesc1(DXGI_ADAPTER_DESC1* pDesc) {
    if (pDesc == nullptr)
      return E_INVALIDARG;

    std::memcpy(pDesc->Description, m_desc.Description, sizeof(pDesc->Description));
    pDesc->VendorId               = m_desc.VendorId;
    pDesc->DeviceId               = m_desc.DeviceId;
    pDesc->SubSysId               = m_desc.SubSysId;
    pDesc->Revision               = m_desc.Revision;
    pDesc->DedicatedVideoMemory   = m_desc.DedicatedVideoMemory;
    pDesc->DedicatedSystemMemory  = m_desc.DedicatedSystemMemory;
    pDesc->SharedSystemMemory     = m_desc.SharedSystemMemory;
    pDesc->AdapterLuid            = m_desc.AdapterLuid;
    pDesc->Flags                  = m_desc.Flags;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::GetDesc2(DXGI_ADAPTER_DESC2* pDesc) {
    if (pDesc == nullptr)
      return E_INVALIDARG;

    std::memcpy(pDesc->Description, m_desc.Description, sizeof(pDesc->Description));
    pDesc->VendorId               = m_desc.VendorId;
    pDesc->DeviceId               = m_desc.DeviceId;
    pDesc->SubSysId               = m_desc.SubSysId;
    pDesc->Revision               = m_desc.Revision;
    pDesc->DedicatedVideoMemory   = m_desc.DedicatedVideoMemory;
    pDesc->DedicatedSystemMemory  = m_desc.DedicatedSystemMemory;
    pDesc->SharedSystemMemory     = m_desc.SharedSystemMemory;
    pDesc->AdapterLuid            = m_desc.AdapterLuid;
    pDesc->Flags                  = m_desc.Flags;
    pDesc->GraphicsPreemptionGranularity = m_desc.GraphicsPreemptionGranularity;
    pDesc->ComputePreemptionGranularity  = m_desc.ComputePreemptionGranularity;
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::GetDesc3(
          DXGI_ADAPTER_DESC3*       pDesc) {
    if (pDesc == nullptr)
      return E_INVALIDARG;
    
    *pDesc = m_desc;
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE DxgiAdapter::QueryVideoMemoryInfo(
          UINT                          NodeIndex,
          DXGI_MEMORY_SEGMENT_GROUP     MemorySegmentGroup,
          DXGI_QUERY_VIDEO_MEMORY_INFO* pVideoMemoryInfo) {
    if (NodeIndex > 0 || !pVideoMemoryInfo)
      return E_INVALIDARG;
    
    if (MemorySegmentGroup != DXGI_MEMORY_SEGMENT_GROUP_LOCAL
     && MemorySegmentGroup != DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL)
      return E_INVALIDARG;
    
    DxvkAdapterMemoryInfo memInfo = m_adapter->getMemoryHeapInfo();

    VkMemoryHeapFlags heapFlagMask = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
    VkMemoryHeapFlags heapFlags    = 0;

    if (MemorySegmentGroup == DXGI_MEMORY_SEGMENT_GROUP_LOCAL)
      heapFlags |= VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
    
    pVideoMemoryInfo->Budget = 0;
    pVideoMemoryInfo->CurrentUsage = 0;
    pVideoMemoryInfo->AvailableForReservation = 0;

    for (uint32_t i = 0; i < memInfo.heapCount; i++) {
      if ((memInfo.heaps[i].heapFlags & heapFlagMask) != heapFlags)
        continue;
      
      pVideoMemoryInfo->Budget += memInfo.heaps[i].memoryBudget;
      pVideoMemoryInfo->CurrentUsage += memInfo.heaps[i].memoryAllocated;
      pVideoMemoryInfo->AvailableForReservation += memInfo.heaps[i].heapSize / 2;
    }

    // We don't implement reservation, but the observable
    // behaviour should match that of Windows drivers
    uint32_t segmentId = uint32_t(MemorySegmentGroup);

    pVideoMemoryInfo->CurrentReservation = m_memReservation[segmentId];
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
    if (!hEvent || !pdwCookie)
      return DXGI_ERROR_INVALID_CALL;

    std::unique_lock<dxvk::mutex> lock(m_mutex);
    DWORD cookie = ++m_eventCookie;

    m_eventMap.insert({ cookie, hEvent });

    if (!m_eventThread.joinable())
      m_eventThread = dxvk::thread([this] { runEventThread(); });

    // This method seems to fire the
    // event immediately on Windows
    SetEvent(hEvent);

    *pdwCookie = cookie;
    return S_OK;
  }
  

  void STDMETHODCALLTYPE DxgiAdapter::UnregisterHardwareContentProtectionTeardownStatus(
          DWORD                         dwCookie) {
    Logger::err("DxgiAdapter::UnregisterHardwareContentProtectionTeardownStatus: Not implemented");
  }


  void STDMETHODCALLTYPE DxgiAdapter::UnregisterVideoMemoryBudgetChangeNotification(
          DWORD                         dwCookie) {
    std::unique_lock<dxvk::mutex> lock(m_mutex);
    m_eventMap.erase(dwCookie);
  }


  Rc<DxvkAdapter> STDMETHODCALLTYPE DxgiAdapter::GetDXVKAdapter() {
    return m_adapter;
  }


  Rc<DxvkInstance> STDMETHODCALLTYPE DxgiAdapter::GetDXVKInstance() {
    return m_factory->GetDXVKInstance();
  }


  DXGI_ADAPTER_DESC3 DxgiAdapter::GetAdapterDesc() const {
    DXGI_ADAPTER_DESC3 desc = { };

    const DxgiOptions* options = m_factory->GetOptions();

    auto deviceProp = m_adapter->deviceProperties();
    auto memoryProp = m_adapter->memoryProperties();
    auto vk11       = m_adapter->devicePropertiesExt().vk11;
    auto vk12       = m_adapter->devicePropertiesExt().vk12;

    // Custom Vendor / Device ID
    if (options->customVendorId >= 0)
      deviceProp.vendorID = options->customVendorId;

    if (options->customDeviceId >= 0)
      deviceProp.deviceID = options->customDeviceId;

    std::string description = options->customDeviceDesc.empty()
      ? std::string(deviceProp.deviceName)
      : options->customDeviceDesc;

    if (options->customVendorId < 0) {
      uint16_t fallbackVendor = 0xdead;
      uint16_t fallbackDevice = 0xbeef;

      if (!options->hideAmdGpu) {
        // AMD RX 6700 XT
        fallbackVendor = uint16_t(DxvkGpuVendor::Amd);
        fallbackDevice = 0x73df;
      } else if (!options->hideNvidiaGpu) {
        // Nvidia RTX 3060
        fallbackVendor = uint16_t(DxvkGpuVendor::Nvidia);
        fallbackDevice = 0x2487;
      }

      bool hideNvidiaGpu = vk12.driverID == VK_DRIVER_ID_NVIDIA_PROPRIETARY
        ? options->hideNvidiaGpu : options->hideNvkGpu;

      bool hideGpu = (deviceProp.vendorID == uint16_t(DxvkGpuVendor::Nvidia) && hideNvidiaGpu)
                  || (deviceProp.vendorID == uint16_t(DxvkGpuVendor::Amd) && options->hideAmdGpu)
                  || (deviceProp.vendorID == uint16_t(DxvkGpuVendor::Intel) && options->hideIntelGpu);

      if (hideGpu) {
        deviceProp.vendorID = fallbackVendor;

        if (options->customDeviceId < 0)
          deviceProp.deviceID = fallbackDevice;

        Logger::info(str::format("DXGI: Hiding actual GPU, reporting:\n",
                                 "  vendor ID: 0x", std::hex, deviceProp.vendorID, "\n",
                                 "  device ID: 0x", std::hex, deviceProp.deviceID, "\n"));
      }
    }

    // Convert device name
    str::transcodeString(desc.Description,
      sizeof(desc.Description) / sizeof(desc.Description[0]) - 1,
      description.c_str(), description.size());

    // Get amount of video memory based on the Vulkan heaps
    VkDeviceSize deviceMemory = 0;
    VkDeviceSize sharedMemory = 0;

    for (uint32_t i = 0; i < memoryProp.memoryHeapCount; i++) {
      VkMemoryHeap heap = memoryProp.memoryHeaps[i];

      if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
        // In general we'll have one large device-local heap, and an additional
        // smaller heap on dGPUs in case ReBAR is not supported. Assume that
        // the largest available heap is the total amount of available VRAM.
        deviceMemory = std::max(heap.size, deviceMemory);
      } else {
        // This is typically plain sysmem, don't care too much about limits here
        sharedMemory += heap.size;
      }
    }

    // This can happen on integrated GPUs with one memory heap, over-report
    // here since some games may be allergic to reporting no shared memory.
    if (!sharedMemory)
      sharedMemory = deviceMemory;

    // Some games will default to the GPU with the highest amount of dedicated memory,
    // which can be an integrated GPU on some systems. Report available memory as shared
    // memory and a small amount as dedicated carve-out if a dedicated GPU is present,
    // otherwise report memory normally to not unnecessarily confuse games on Deck.
    if ((m_adapter->isLinkedToDGPU() && deviceProp.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)) {
      sharedMemory = std::max(sharedMemory, deviceMemory);
      deviceMemory = 512ull << 20;
    }

    // Make sure to never return exact powers of two outside the 32-bit range
    // because some games don't understand the concept of actually having VRAM
    constexpr VkDeviceSize adjustment = 32ull << 20;

    if (deviceMemory && !(deviceMemory & 0xffffffffull))
      deviceMemory -= adjustment;

    if (sharedMemory && !(sharedMemory & 0xffffffffull))
      sharedMemory -= adjustment;

    // Some games are silly and need their memory limited
    if (options->maxDeviceMemory > 0
     && options->maxDeviceMemory < deviceMemory)
      deviceMemory = options->maxDeviceMemory;

    if (options->maxSharedMemory > 0
     && options->maxSharedMemory < sharedMemory)
      sharedMemory = options->maxSharedMemory;

    if (env::is32BitHostPlatform()) {
      // The value returned by DXGI is a 32-bit value
      // on 32-bit platforms, so we need to clamp it
      VkDeviceSize maxMemory = 0xC0000000;
      deviceMemory = std::min(deviceMemory, maxMemory);
      sharedMemory = std::min(sharedMemory, maxMemory);
    }

    desc.VendorId                       = deviceProp.vendorID;
    desc.DeviceId                       = deviceProp.deviceID;
    desc.SubSysId                       = 0;
    desc.Revision                       = 0;
    desc.DedicatedVideoMemory           = deviceMemory;
    desc.DedicatedSystemMemory          = 0;
    desc.SharedSystemMemory             = sharedMemory;
    desc.AdapterLuid                    = LUID { 0, 0 };
    desc.Flags                          = DXGI_ADAPTER_FLAG3_NONE;
    desc.GraphicsPreemptionGranularity  = DXGI_GRAPHICS_PREEMPTION_DMA_BUFFER_BOUNDARY;
    desc.ComputePreemptionGranularity   = DXGI_COMPUTE_PREEMPTION_DMA_BUFFER_BOUNDARY;

    if (vk11.deviceLUIDValid)
      std::memcpy(&desc.AdapterLuid, vk11.deviceLUID, VK_LUID_SIZE);
    else
      desc.AdapterLuid = GetAdapterLUID(m_index);

    return desc;
  }


  void DxgiAdapter::runEventThread() {
    env::setThreadName(str::format("dxvk-adapter-", m_index));

    std::unique_lock<dxvk::mutex> lock(m_mutex);
    DxvkAdapterMemoryInfo memoryInfoOld = m_adapter->getMemoryHeapInfo();

    while (true) {
      m_cond.wait_for(lock, std::chrono::milliseconds(1500),
        [this] { return m_eventCookie == ~0u; });

      if (m_eventCookie == ~0u)
        return;

      auto memoryInfoNew = m_adapter->getMemoryHeapInfo();
      bool budgetChanged = false;

      for (uint32_t i = 0; i < memoryInfoNew.heapCount; i++) {
        budgetChanged |= memoryInfoNew.heaps[i].memoryBudget
                      != memoryInfoOld.heaps[i].memoryBudget;
      }

      if (budgetChanged) {
        memoryInfoOld = memoryInfoNew;

        for (const auto& pair : m_eventMap)
          SetEvent(pair.second);
      }
    }
  }
  
}
