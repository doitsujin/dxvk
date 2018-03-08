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
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::QueryInterface(
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
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::GetParent(
          REFIID riid,
          void   **ppParent) {
    return m_factory->QueryInterface(riid, ppParent);
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::CheckInterfaceSupport(
          REFGUID       InterfaceName,
          LARGE_INTEGER *pUMDVersion) {
    Logger::err("DxgiAdapter::CheckInterfaceSupport: No D3D10 support");
    return DXGI_ERROR_UNSUPPORTED;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::EnumOutputs(
          UINT        Output,
          IDXGIOutput **ppOutput) {
    if (ppOutput == nullptr)
      return DXGI_ERROR_INVALID_CALL;
    
    if (Output > 0)
      return DXGI_ERROR_NOT_FOUND;
    
    // TODO support multiple monitors
    HMONITOR monitor = ::MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    *ppOutput = ref(new DxgiOutput(this, monitor));
    return S_OK;
  }
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::GetDesc(DXGI_ADAPTER_DESC* pDesc) {
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
  
  
  HRESULT STDMETHODCALLTYPE DxgiAdapter::GetDesc1(DXGI_ADAPTER_DESC1* pDesc) {
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
  
  
  Rc<DxvkAdapter> STDMETHODCALLTYPE DxgiAdapter::GetDXVKAdapter() {
    return m_adapter;
  }
  
  
  DxgiFormatInfo STDMETHODCALLTYPE DxgiAdapter::LookupFormat(DXGI_FORMAT format, DxgiFormatMode mode) {
    // If the mode is 'Any', probe color formats first
    if (mode != DxgiFormatMode::Depth) {
      auto color = m_colorFormats.find(format);
      
      if (color != m_colorFormats.end())
        return color->second;
    }
    
    if (mode != DxgiFormatMode::Color) {
      auto depth = m_depthFormats.find(format);
      if (depth != m_depthFormats.end())
        return depth->second;
    }
    
    Logger::err(str::format("DxgiAdapter: No format mapping for ", format));
    return DxgiFormatInfo();
  }
  
  
  HRESULT DxgiAdapter::GetOutputFromMonitor(
          HMONITOR              Monitor,
          IDXGIOutput**         ppOutput) {
    Com<IDXGIOutput> output;
    
    for (uint32_t i = 0; SUCCEEDED(EnumOutputs(i, &output)); i++) {
      DXGI_OUTPUT_DESC outputDesc;
      output->GetDesc(&outputDesc);
      
      if (outputDesc.Monitor == Monitor) {
        *ppOutput = output.ref();
        return S_OK;
      }
    }
    
    // No such output found
    return DXGI_ERROR_NOT_FOUND;
  }
  
  
  void DxgiAdapter::AddColorFormatTypeless(
          DXGI_FORMAT                       srcFormat,
          VkFormat                          dstFormat) {
    DxgiFormatInfo formatPair;
    formatPair.format = dstFormat;
    formatPair.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    formatPair.swizzle = {
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY };
    formatPair.flags = DxgiFormatFlag::Typeless;
    m_colorFormats.insert(std::make_pair(srcFormat, formatPair));
  }
  
  
  void DxgiAdapter::AddColorFormat(
          DXGI_FORMAT                       srcFormat,
          VkFormat                          dstFormat,
          VkComponentMapping                swizzle) {
    DxgiFormatInfo formatPair;
    formatPair.format = dstFormat;
    formatPair.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    formatPair.swizzle = swizzle;
    formatPair.flags = DxgiFormatFlags();
    m_colorFormats.insert(std::make_pair(srcFormat, formatPair));
  }
  
  
  void DxgiAdapter::AddDepthFormatTypeless(
          DXGI_FORMAT                       srcFormat,
          VkFormat                          dstFormat) {
    DxgiFormatInfo formatPair;
    formatPair.format = dstFormat;
    formatPair.aspect = 0;
    formatPair.swizzle = {
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY };
    formatPair.flags = DxgiFormatFlag::Typeless;
    m_depthFormats.insert(std::make_pair(srcFormat, formatPair));
  }
  
  
  void DxgiAdapter::AddDepthFormat(
          DXGI_FORMAT                       srcFormat,
          VkFormat                          dstFormat,
          VkImageAspectFlags                srvAspect) {
    DxgiFormatInfo formatPair;
    formatPair.format = dstFormat;
    formatPair.aspect = srvAspect;
    formatPair.swizzle = {
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY };
    formatPair.flags = DxgiFormatFlags();
    m_depthFormats.insert(std::make_pair(srcFormat, formatPair));
  }
  
  
  void DxgiAdapter::SetupFormatTable() {
    /***********************************************************************************/
    /*                           C O L O R     F O R M A T S                           */
    AddColorFormat        (DXGI_FORMAT_UNKNOWN,                     VK_FORMAT_UNDEFINED);
    
    AddColorFormatTypeless(DXGI_FORMAT_R32G32B32A32_TYPELESS,       VK_FORMAT_R32G32B32A32_UINT);
    AddColorFormat        (DXGI_FORMAT_R32G32B32A32_FLOAT,          VK_FORMAT_R32G32B32A32_SFLOAT);
    AddColorFormat        (DXGI_FORMAT_R32G32B32A32_UINT,           VK_FORMAT_R32G32B32A32_UINT);
    AddColorFormat        (DXGI_FORMAT_R32G32B32A32_SINT,           VK_FORMAT_R32G32B32A32_SINT);
    
    AddColorFormatTypeless(DXGI_FORMAT_R32G32B32_TYPELESS,          VK_FORMAT_R32G32B32_UINT);
    AddColorFormat        (DXGI_FORMAT_R32G32B32_FLOAT,             VK_FORMAT_R32G32B32_SFLOAT);
    AddColorFormat        (DXGI_FORMAT_R32G32B32_UINT,              VK_FORMAT_R32G32B32_UINT);
    AddColorFormat        (DXGI_FORMAT_R32G32B32_SINT,              VK_FORMAT_R32G32B32_SINT);
    
    AddColorFormatTypeless(DXGI_FORMAT_R16G16B16A16_TYPELESS,       VK_FORMAT_R16G16B16A16_UINT);
    AddColorFormat        (DXGI_FORMAT_R16G16B16A16_FLOAT,          VK_FORMAT_R16G16B16A16_SFLOAT);
    AddColorFormat        (DXGI_FORMAT_R16G16B16A16_UNORM,          VK_FORMAT_R16G16B16A16_UNORM);
    AddColorFormat        (DXGI_FORMAT_R16G16B16A16_UINT,           VK_FORMAT_R16G16B16A16_UINT);
    AddColorFormat        (DXGI_FORMAT_R16G16B16A16_SNORM,          VK_FORMAT_R16G16B16A16_SNORM);
    AddColorFormat        (DXGI_FORMAT_R16G16B16A16_SINT,           VK_FORMAT_R16G16B16A16_SINT);
    
    AddColorFormatTypeless(DXGI_FORMAT_R32G32_TYPELESS,             VK_FORMAT_R32G32_UINT);
    AddColorFormat        (DXGI_FORMAT_R32G32_FLOAT,                VK_FORMAT_R32G32_SFLOAT);
    AddColorFormat        (DXGI_FORMAT_R32G32_UINT,                 VK_FORMAT_R32G32_UINT);
    AddColorFormat        (DXGI_FORMAT_R32G32_SINT,                 VK_FORMAT_R32G32_SINT);
    
    AddColorFormatTypeless(DXGI_FORMAT_R10G10B10A2_TYPELESS,        VK_FORMAT_A2B10G10R10_UINT_PACK32);
    AddColorFormat        (DXGI_FORMAT_R10G10B10A2_UINT,            VK_FORMAT_A2B10G10R10_UINT_PACK32);
    AddColorFormat        (DXGI_FORMAT_R10G10B10A2_UNORM,           VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    
    AddColorFormat        (DXGI_FORMAT_R11G11B10_FLOAT,             VK_FORMAT_B10G11R11_UFLOAT_PACK32);
    
    AddColorFormatTypeless(DXGI_FORMAT_R8G8B8A8_TYPELESS,           VK_FORMAT_R8G8B8A8_UINT);
    AddColorFormat        (DXGI_FORMAT_R8G8B8A8_UNORM,              VK_FORMAT_R8G8B8A8_UNORM);
    AddColorFormat        (DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,         VK_FORMAT_R8G8B8A8_SRGB);
    AddColorFormat        (DXGI_FORMAT_R8G8B8A8_UINT,               VK_FORMAT_R8G8B8A8_UINT);
    AddColorFormat        (DXGI_FORMAT_R8G8B8A8_SNORM,              VK_FORMAT_R8G8B8A8_SNORM);
    AddColorFormat        (DXGI_FORMAT_R8G8B8A8_SINT,               VK_FORMAT_R8G8B8A8_SINT);
    
    AddColorFormatTypeless(DXGI_FORMAT_R16G16_TYPELESS,             VK_FORMAT_R16G16_UINT);
    AddColorFormat        (DXGI_FORMAT_R16G16_FLOAT,                VK_FORMAT_R16G16_SFLOAT);
    AddColorFormat        (DXGI_FORMAT_R16G16_UNORM,                VK_FORMAT_R16G16_UNORM);
    AddColorFormat        (DXGI_FORMAT_R16G16_UINT,                 VK_FORMAT_R16G16_UINT);
    AddColorFormat        (DXGI_FORMAT_R16G16_SNORM,                VK_FORMAT_R16G16_SNORM);
    AddColorFormat        (DXGI_FORMAT_R16G16_SINT,                 VK_FORMAT_R16G16_SINT);
    
    AddColorFormatTypeless(DXGI_FORMAT_R32_TYPELESS,                VK_FORMAT_R32_UINT);
    AddColorFormat        (DXGI_FORMAT_R32_FLOAT,                   VK_FORMAT_R32_SFLOAT);
    AddColorFormat        (DXGI_FORMAT_R32_UINT,                    VK_FORMAT_R32_UINT);
    AddColorFormat        (DXGI_FORMAT_R32_SINT,                    VK_FORMAT_R32_SINT);
    
    AddColorFormatTypeless(DXGI_FORMAT_R8G8_TYPELESS,               VK_FORMAT_R8G8_UINT);
    AddColorFormat        (DXGI_FORMAT_R8G8_UNORM,                  VK_FORMAT_R8G8_UNORM);
    AddColorFormat        (DXGI_FORMAT_R8G8_UINT,                   VK_FORMAT_R8G8_UINT);
    AddColorFormat        (DXGI_FORMAT_R8G8_SNORM,                  VK_FORMAT_R8G8_SNORM);
    AddColorFormat        (DXGI_FORMAT_R8G8_SINT,                   VK_FORMAT_R8G8_SINT);
    
    AddColorFormatTypeless(DXGI_FORMAT_R16_TYPELESS,                VK_FORMAT_R16_UINT);
    AddColorFormat        (DXGI_FORMAT_R16_FLOAT,                   VK_FORMAT_R16_SFLOAT);
    AddColorFormat        (DXGI_FORMAT_R16_UNORM,                   VK_FORMAT_R16_UNORM);
    AddColorFormat        (DXGI_FORMAT_R16_UINT,                    VK_FORMAT_R16_UINT);
    AddColorFormat        (DXGI_FORMAT_R16_SNORM,                   VK_FORMAT_R16_SNORM);
    AddColorFormat        (DXGI_FORMAT_R16_SINT,                    VK_FORMAT_R16_SINT);
    
    AddColorFormatTypeless(DXGI_FORMAT_R8_TYPELESS,                 VK_FORMAT_R8_UINT);
    AddColorFormat        (DXGI_FORMAT_R8_UNORM,                    VK_FORMAT_R8_UNORM);
    AddColorFormat        (DXGI_FORMAT_R8_UINT,                     VK_FORMAT_R8_UINT);
    AddColorFormat        (DXGI_FORMAT_R8_SNORM,                    VK_FORMAT_R8_SNORM);
    AddColorFormat        (DXGI_FORMAT_R8_SINT,                     VK_FORMAT_R8_SINT);
    
    AddColorFormat        (DXGI_FORMAT_A8_UNORM,                    VK_FORMAT_R8_UNORM,
      { VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO,
        VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R });
    
//     AddColorFormat(DXGI_FORMAT_R1_UNORM,                    VK_FORMAT_UNDEFINED);
    
    AddColorFormat        (DXGI_FORMAT_R9G9B9E5_SHAREDEXP,          VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);
    
    AddColorFormat        (DXGI_FORMAT_R8G8_B8G8_UNORM,             VK_FORMAT_B8G8R8G8_422_UNORM_KHR,
      { VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE });
    
    AddColorFormat        (DXGI_FORMAT_G8R8_G8B8_UNORM,             VK_FORMAT_G8B8G8R8_422_UNORM_KHR,
      { VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ONE });
    
    AddColorFormat        (DXGI_FORMAT_B5G6R5_UNORM,                VK_FORMAT_B5G6R5_UNORM_PACK16);
    AddColorFormat        (DXGI_FORMAT_B5G5R5A1_UNORM,              VK_FORMAT_B5G5R5A1_UNORM_PACK16);
    
    AddColorFormatTypeless(DXGI_FORMAT_B8G8R8A8_TYPELESS,           VK_FORMAT_B8G8R8A8_UNORM);
    AddColorFormat        (DXGI_FORMAT_B8G8R8A8_UNORM,              VK_FORMAT_B8G8R8A8_UNORM);
    AddColorFormat        (DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,         VK_FORMAT_B8G8R8A8_SRGB);
    
    AddColorFormatTypeless(DXGI_FORMAT_B8G8R8X8_TYPELESS, VK_FORMAT_B8G8R8A8_UNORM);
    
    AddColorFormat(DXGI_FORMAT_B8G8R8X8_UNORM, VK_FORMAT_B8G8R8A8_UNORM,
      { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE });
    
    AddColorFormat(DXGI_FORMAT_B8G8R8X8_UNORM_SRGB, VK_FORMAT_B8G8R8A8_SRGB,
      { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
        VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE });
    
//     AddColorFormat(DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,  VK_FORMAT_UNDEFINED);
    
    AddColorFormat        (DXGI_FORMAT_B4G4R4A4_UNORM,              VK_FORMAT_B4G4R4A4_UNORM_PACK16);

    /***********************************************************************************/
    /*                           B L O C K     F O R M A T S                           */
    AddColorFormatTypeless(DXGI_FORMAT_BC1_TYPELESS,                VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
    AddColorFormat        (DXGI_FORMAT_BC1_UNORM,                   VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
    AddColorFormat        (DXGI_FORMAT_BC1_UNORM_SRGB,              VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
    
    AddColorFormatTypeless(DXGI_FORMAT_BC2_TYPELESS,                VK_FORMAT_BC2_UNORM_BLOCK);
    AddColorFormat        (DXGI_FORMAT_BC2_UNORM,                   VK_FORMAT_BC2_UNORM_BLOCK);
    AddColorFormat        (DXGI_FORMAT_BC2_UNORM_SRGB,              VK_FORMAT_BC2_SRGB_BLOCK);
    
    AddColorFormatTypeless(DXGI_FORMAT_BC3_TYPELESS,                VK_FORMAT_BC3_UNORM_BLOCK);
    AddColorFormat        (DXGI_FORMAT_BC3_UNORM,                   VK_FORMAT_BC3_UNORM_BLOCK);
    AddColorFormat        (DXGI_FORMAT_BC3_UNORM_SRGB,              VK_FORMAT_BC3_SRGB_BLOCK);
    
    AddColorFormatTypeless(DXGI_FORMAT_BC4_TYPELESS,                VK_FORMAT_BC4_UNORM_BLOCK);
    AddColorFormat        (DXGI_FORMAT_BC4_UNORM,                   VK_FORMAT_BC4_UNORM_BLOCK);
    AddColorFormat        (DXGI_FORMAT_BC4_SNORM,                   VK_FORMAT_BC4_SNORM_BLOCK);
    
    AddColorFormatTypeless(DXGI_FORMAT_BC5_TYPELESS,                VK_FORMAT_BC5_UNORM_BLOCK);
    AddColorFormat        (DXGI_FORMAT_BC5_UNORM,                   VK_FORMAT_BC5_UNORM_BLOCK);
    AddColorFormat        (DXGI_FORMAT_BC5_SNORM,                   VK_FORMAT_BC5_SNORM_BLOCK);
    
    AddColorFormatTypeless(DXGI_FORMAT_BC6H_TYPELESS,               VK_FORMAT_BC6H_UFLOAT_BLOCK);
    AddColorFormat        (DXGI_FORMAT_BC6H_UF16,                   VK_FORMAT_BC6H_UFLOAT_BLOCK);
    AddColorFormat        (DXGI_FORMAT_BC6H_SF16,                   VK_FORMAT_BC6H_SFLOAT_BLOCK);
    
    AddColorFormatTypeless(DXGI_FORMAT_BC7_TYPELESS,                VK_FORMAT_BC7_UNORM_BLOCK);
    AddColorFormat        (DXGI_FORMAT_BC7_UNORM,                   VK_FORMAT_BC7_UNORM_BLOCK);
    AddColorFormat        (DXGI_FORMAT_BC7_UNORM_SRGB,              VK_FORMAT_BC7_SRGB_BLOCK);
    
    /***********************************************************************************/
    /*                           D E P T H     F O R M A T S                           */
    AddDepthFormatTypeless(DXGI_FORMAT_R16_TYPELESS,                VK_FORMAT_D16_UNORM);
    AddDepthFormat        (DXGI_FORMAT_D16_UNORM,                   VK_FORMAT_D16_UNORM, 0);
    AddDepthFormat        (DXGI_FORMAT_R16_UNORM,                   VK_FORMAT_D16_UNORM, VK_IMAGE_ASPECT_DEPTH_BIT);
    
    AddDepthFormatTypeless(DXGI_FORMAT_R32_TYPELESS,                VK_FORMAT_D32_SFLOAT);
    AddDepthFormat        (DXGI_FORMAT_D32_FLOAT,                   VK_FORMAT_D32_SFLOAT, 0);
    AddDepthFormat        (DXGI_FORMAT_R32_FLOAT,                   VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
    
    AddDepthFormatTypeless(DXGI_FORMAT_R32G8X24_TYPELESS,           VK_FORMAT_D32_SFLOAT_S8_UINT);
    AddDepthFormat        (DXGI_FORMAT_D32_FLOAT_S8X24_UINT,        VK_FORMAT_D32_SFLOAT_S8_UINT, 0);
    AddDepthFormat        (DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,    VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);
    AddDepthFormat        (DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,     VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_ASPECT_STENCIL_BIT);
    
    // Vulkan implementations are not required to support 24-bit depth buffers natively
    // and AMD decided to not implement them, so we'll fall back to 32-bit depth buffers
    if (HasFormatSupport(VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) {
      AddDepthFormatTypeless(DXGI_FORMAT_R24G8_TYPELESS,            VK_FORMAT_D24_UNORM_S8_UINT);
      AddDepthFormat        (DXGI_FORMAT_D24_UNORM_S8_UINT,         VK_FORMAT_D24_UNORM_S8_UINT, 0);
      AddDepthFormat        (DXGI_FORMAT_R24_UNORM_X8_TYPELESS,     VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);
      AddDepthFormat        (DXGI_FORMAT_X24_TYPELESS_G8_UINT,      VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_STENCIL_BIT);
    } else {
      Logger::warn("DxgiAdapter: DXGI_FORMAT_D24_UNORM_S8_UINT -> VK_FORMAT_D32_SFLOAT_S8_UINT");
      AddDepthFormatTypeless(DXGI_FORMAT_R24G8_TYPELESS,            VK_FORMAT_D32_SFLOAT_S8_UINT);
      AddDepthFormat        (DXGI_FORMAT_R24_UNORM_X8_TYPELESS,     VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);
      AddDepthFormat        (DXGI_FORMAT_X24_TYPELESS_G8_UINT,      VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_ASPECT_STENCIL_BIT);
      AddDepthFormat        (DXGI_FORMAT_D24_UNORM_S8_UINT,         VK_FORMAT_D32_SFLOAT_S8_UINT, 0);
    }
  }
  
  
  bool DxgiAdapter::HasFormatSupport(
          VkFormat                          format,
          VkFormatFeatureFlags              features) const {
    const VkFormatProperties info = m_adapter->formatProperties(format);
    return ((info.optimalTilingFeatures | info.bufferFeatures) & features) == features;
  }
  
}
