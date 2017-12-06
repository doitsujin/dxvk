#include <cstdlib>
#include <cstring>

#include "dxgi_adapter.h"
#include "dxgi_enums.h"
#include "dxgi_factory.h"
#include "dxgi_output.h"

#include "../dxvk/vulkan/dxvk_vulkan_names.h"

namespace dxvk {

  DxgiAdapter::DxgiAdapter(
          DxgiFactory*      factory,
    const Rc<DxvkAdapter>&  adapter)
  : m_factory (factory),
    m_adapter (adapter) {
    SetupFormatTable();
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
  
  
  DxgiFormatPair DxgiAdapter::LookupFormat(DXGI_FORMAT format) {
    auto pair = m_formats.find(format);
    
    return pair != m_formats.end()
      ? pair->second
      : DxgiFormatPair();
  }
  
  
  void DxgiAdapter::AddFormat(
          DXGI_FORMAT                       srcFormat,
          VkFormat                          dstFormat,
    const std::initializer_list<VkFormat>&  fallbacks,
          VkFormatFeatureFlags              features) {
    DxgiFormatPair formatPair;
    formatPair.wanted = dstFormat;
    formatPair.actual = VK_FORMAT_UNDEFINED;
    
    if (this->HasFormatSupport(dstFormat, features)) {
      formatPair.actual = dstFormat;
    } else {
      for (VkFormat fmt : fallbacks) {
        if (this->HasFormatSupport(fmt, features)) {
          formatPair.actual = fmt;
          break;
        }
      }
    }
    
    if (formatPair.actual == VK_FORMAT_UNDEFINED)
      Logger::err(str::format("DxgiAdapter: ", srcFormat, " not supported"));
    else if (formatPair.actual != formatPair.wanted)
      Logger::warn(str::format("DxgiAdapter: ", srcFormat, " -> ", formatPair.actual));
    
    m_formats.insert(std::make_pair(srcFormat, formatPair));
  }
  
  
  void DxgiAdapter::SetupFormatTable() {
    AddFormat(
      DXGI_FORMAT_R8G8B8A8_TYPELESS,
      VK_FORMAT_R8G8B8A8_UINT, {}, 0);
    
    AddFormat(
      DXGI_FORMAT_R8G8B8A8_UINT,
      VK_FORMAT_R8G8B8A8_UINT, {},
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT               |
      VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT               |
      VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT        |
      VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT        |
      VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT               |
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
    
    AddFormat(
      DXGI_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_R8G8B8A8_UNORM, {},
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT               |
      VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT               |
      VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT        |
      VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT        |
      VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT               |
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT            |
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT      |
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
    
    AddFormat(
      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
      VK_FORMAT_R8G8B8A8_SRGB, {},
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT               |
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT            |
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT      |
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
    
    AddFormat(
      DXGI_FORMAT_R8G8B8A8_SINT,
      VK_FORMAT_R8G8B8A8_SINT, {},
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT               |
      VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT               |
      VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT        |
      VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT        |
      VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT               |
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
    
    AddFormat(
      DXGI_FORMAT_R8G8B8A8_SNORM,
      VK_FORMAT_R8G8B8A8_SNORM, {},
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT               |
      VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT               |
      VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT        |
      VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT        |
      VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT               |
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT            |
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT      |
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
    
    AddFormat(
      DXGI_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT, {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
      },
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT               |
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    
    AddFormat(
      DXGI_FORMAT_D16_UNORM,
      VK_FORMAT_D16_UNORM, {},
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT               |
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    
    // TODO finish me
  }
  
  
  bool DxgiAdapter::HasFormatSupport(
          VkFormat                          format,
          VkFormatFeatureFlags              features) const {
    VkFormatProperties info = m_adapter->formatProperties(format);
    return ((info.optimalTilingFeatures | info.bufferFeatures) & features) == features;
  }
  
}
