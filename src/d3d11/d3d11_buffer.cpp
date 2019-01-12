#include "d3d11_buffer.h"
#include "d3d11_context.h"
#include "d3d11_device.h"

#include "../dxvk/dxvk_data.h"

namespace dxvk {
  
  D3D11Buffer::D3D11Buffer(
          D3D11Device*                pDevice,
    const D3D11_BUFFER_DESC*          pDesc)
  : m_device      (pDevice),
    m_desc        (*pDesc),
    m_d3d10       (this, pDevice->GetD3D10Interface()) {
    DxvkBufferCreateInfo  info;
    info.size   = pDesc->ByteWidth;
    info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT
                | VK_ACCESS_TRANSFER_WRITE_BIT;
    
    if (pDesc->BindFlags & D3D11_BIND_VERTEX_BUFFER) {
      info.usage  |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      info.access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_INDEX_BUFFER) {
      info.usage  |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      info.access |= VK_ACCESS_INDEX_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_CONSTANT_BUFFER) {
      info.usage  |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
      info.stages |= m_device->GetEnabledShaderStages();
      info.access |= VK_ACCESS_UNIFORM_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_SHADER_RESOURCE) {
      info.usage  |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
      info.stages |= m_device->GetEnabledShaderStages();
      info.access |= VK_ACCESS_SHADER_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_STREAM_OUTPUT) {
      info.usage  |= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;
      info.stages |= VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT;
      info.access |= VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
      info.usage  |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
      info.stages |= m_device->GetEnabledShaderStages();
      info.access |= VK_ACCESS_SHADER_READ_BIT
                  |  VK_ACCESS_SHADER_WRITE_BIT;
    }
    
    if (pDesc->CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) {
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      info.access |= VK_ACCESS_HOST_WRITE_BIT;
    }
    
    if (pDesc->CPUAccessFlags & D3D11_CPU_ACCESS_READ) {
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      info.access |= VK_ACCESS_HOST_READ_BIT;
    }
    
    if (pDesc->MiscFlags & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS) {
      info.usage  |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
      info.access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }

    if (pDesc->MiscFlags & (
        D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS |
        D3D11_RESOURCE_MISC_BUFFER_STRUCTURED))
      info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    
    // Default constant buffers may get updated frequently, in which
    // case mapping the buffer is faster than using update commands.
    VkMemoryPropertyFlags memoryFlags = GetMemoryFlagsForUsage(pDesc->Usage);

    if ((pDesc->Usage == D3D11_USAGE_DEFAULT) && (pDesc->BindFlags & D3D11_BIND_CONSTANT_BUFFER)) {
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      info.access |= VK_ACCESS_HOST_WRITE_BIT;
      
      memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    
    // AMD cards have a device-local, host-visible memory type where
    // we can put dynamic resources that need fast access by the GPU
    if (pDesc->Usage == D3D11_USAGE_DYNAMIC && pDesc->BindFlags)
      memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    // Create the buffer and set the entire buffer slice as mapped,
    // so that we only have to update it when invalidating th buffer
    m_buffer = m_device->GetDXVKDevice()->createBuffer(info, memoryFlags);
    m_mapped = m_buffer->getSliceHandle();

    // For Stream Output buffers we need a counter
    if (pDesc->BindFlags & D3D11_BIND_STREAM_OUTPUT)
      m_soCounter = m_device->AllocXfbCounterSlice();
  }
  
  
  D3D11Buffer::~D3D11Buffer() {
    if (m_soCounter.defined())
      m_device->FreeXfbCounterSlice(m_soCounter);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Buffer::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11Resource)
     || riid == __uuidof(ID3D11Buffer)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10Resource)
     || riid == __uuidof(ID3D10Buffer)) {
      *ppvObject = ref(&m_d3d10);
      return S_OK;
    }
    
    Logger::warn("D3D11Buffer::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11Buffer::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Buffer::GetEvictionPriority() {
    return DXGI_RESOURCE_PRIORITY_NORMAL;
  }
  
  
  void STDMETHODCALLTYPE D3D11Buffer::SetEvictionPriority(UINT EvictionPriority) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11Buffer::SetEvictionPriority: Stub");
  }
  
  
  void STDMETHODCALLTYPE D3D11Buffer::GetType(D3D11_RESOURCE_DIMENSION* pResourceDimension) {
    *pResourceDimension = D3D11_RESOURCE_DIMENSION_BUFFER;
  }
  
  
  void STDMETHODCALLTYPE D3D11Buffer::GetDesc(D3D11_BUFFER_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
  
  bool D3D11Buffer::CheckViewCompatibility(
          UINT                BindFlags,
          DXGI_FORMAT         Format) const {
    // Check whether the given bind flags are supported
    VkBufferUsageFlags usage = GetBufferUsageFlags(BindFlags);

    if ((m_buffer->info().usage & usage) != usage)
      return false;

    // Structured buffer views use no format
    if (Format == DXGI_FORMAT_UNKNOWN)
      return (m_desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) != 0;

    // Check whether the given combination of buffer view
    // type and view format is supported by the device
    DXGI_VK_FORMAT_INFO viewFormat = m_device->LookupFormat(Format, DXGI_VK_FORMAT_MODE_ANY);
    VkFormatFeatureFlags features = GetBufferFormatFeatures(BindFlags);

    return CheckFormatFeatureSupport(viewFormat.Format, features);
  }


  BOOL D3D11Buffer::CheckFormatFeatureSupport(
          VkFormat              Format,
          VkFormatFeatureFlags  Features) const {
    VkFormatProperties properties = m_device->GetDXVKDevice()->adapter()->formatProperties(Format);
    return (properties.bufferFeatures & Features) == Features;
  }
  

  D3D11Buffer* GetCommonBuffer(ID3D11Resource* pResource) {
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&dimension);

    return dimension == D3D11_RESOURCE_DIMENSION_BUFFER
      ? static_cast<D3D11Buffer*>(pResource)
      : nullptr;
  }

}
