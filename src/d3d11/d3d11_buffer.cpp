#include "d3d11_buffer.h"
#include "d3d11_context.h"
#include "d3d11_device.h"

#include "../dxvk/dxvk_data.h"

namespace dxvk {
  
  D3D11Buffer::D3D11Buffer(
          D3D11Device*                pDevice,
    const D3D11_BUFFER_DESC*          pDesc)
  : D3D11DeviceChild<ID3D11Buffer>(pDevice),
    m_desc        (*pDesc),
    m_resource    (this),
    m_d3d10       (this) {
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
      info.stages |= m_parent->GetEnabledShaderStages();
      info.access |= VK_ACCESS_UNIFORM_READ_BIT;

      if (m_parent->GetOptions()->constantBufferRangeCheck)
        info.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_SHADER_RESOURCE) {
      info.usage  |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
                  |  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      info.stages |= m_parent->GetEnabledShaderStages();
      info.access |= VK_ACCESS_SHADER_READ_BIT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_STREAM_OUTPUT) {
      info.usage  |= VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;
      info.stages |= VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT;
      info.access |= VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT;
    }
    
    if (pDesc->BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
      info.usage  |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
                  |  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      info.stages |= m_parent->GetEnabledShaderStages();
      info.access |= VK_ACCESS_SHADER_READ_BIT
                  |  VK_ACCESS_SHADER_WRITE_BIT;
    }
    
    if (pDesc->MiscFlags & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS) {
      info.usage  |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
      info.access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }

    // Create the buffer and set the entire buffer slice as mapped,
    // so that we only have to update it when invalidating th buffer
    m_buffer = m_parent->GetDXVKDevice()->createBuffer(info, GetMemoryFlags());
    m_mapped = m_buffer->getSliceHandle();

    // For Stream Output buffers we need a counter
    if (pDesc->BindFlags & D3D11_BIND_STREAM_OUTPUT)
      m_soCounter = CreateSoCounterBuffer();
  }
  
  
  D3D11Buffer::~D3D11Buffer() {

  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Buffer::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

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

    if (riid == __uuidof(IDXGIObject)
     || riid == __uuidof(IDXGIDeviceSubObject)
     || riid == __uuidof(IDXGIResource)
     || riid == __uuidof(IDXGIResource1)) {
       *ppvObject = ref(&m_resource);
       return S_OK;
    }
    
    Logger::warn("D3D11Buffer::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
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
    if ((m_desc.BindFlags & BindFlags) != BindFlags)
      return false;

    // Structured buffer views use no format
    if (Format == DXGI_FORMAT_UNKNOWN)
      return (m_desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) != 0;

    // Check whether the given combination of buffer view
    // type and view format is supported by the device
    DXGI_VK_FORMAT_INFO viewFormat = m_parent->LookupFormat(Format, DXGI_VK_FORMAT_MODE_ANY);
    VkFormatFeatureFlags features = GetBufferFormatFeatures(BindFlags);

    return CheckFormatFeatureSupport(viewFormat.Format, features);
  }


  HRESULT D3D11Buffer::NormalizeBufferProperties(D3D11_BUFFER_DESC* pDesc) {
    // Zero-sized buffers are illegal
    if (!pDesc->ByteWidth)
      return E_INVALIDARG;

    // We don't support tiled resources
    if (pDesc->MiscFlags & (D3D11_RESOURCE_MISC_TILE_POOL | D3D11_RESOURCE_MISC_TILED))
      return E_INVALIDARG;
    
    // Constant buffer size must be a multiple of 16
    if ((pDesc->BindFlags & D3D11_BIND_CONSTANT_BUFFER)
     && (pDesc->ByteWidth & 0xF))
      return E_INVALIDARG;

    // Basic validation for structured buffers
    if ((pDesc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
     && ((pDesc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
      || (pDesc->StructureByteStride == 0)
      || (pDesc->StructureByteStride & 0x3)))
      return E_INVALIDARG;
    
    // Basic validation for raw buffers
    if ((pDesc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
     && (!(pDesc->BindFlags & (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS))))
      return E_INVALIDARG;

    // Mip generation obviously doesn't work for buffers
    if (pDesc->MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS)
      return E_INVALIDARG;
    
    if (!(pDesc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED))
      pDesc->StructureByteStride = 0;
    
    return S_OK;
  }


  BOOL D3D11Buffer::CheckFormatFeatureSupport(
          VkFormat              Format,
          VkFormatFeatureFlags  Features) const {
    VkFormatProperties properties = m_parent->GetDXVKDevice()->adapter()->formatProperties(Format);
    return (properties.bufferFeatures & Features) == Features;
  }


  VkMemoryPropertyFlags D3D11Buffer::GetMemoryFlags() const {
    VkMemoryPropertyFlags memoryFlags = 0;
    
    switch (m_desc.Usage) {
      case D3D11_USAGE_IMMUTABLE:
        memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;

      case D3D11_USAGE_DEFAULT:
        memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if ((m_desc.BindFlags & D3D11_BIND_CONSTANT_BUFFER) || m_desc.CPUAccessFlags) {
          memoryFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                      |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        }

        if (m_desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ) {
          memoryFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
          memoryFlags &= ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }
        break;
      
      case D3D11_USAGE_DYNAMIC:
        memoryFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                    |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        if (m_desc.BindFlags)
          memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
      
      case D3D11_USAGE_STAGING:
        memoryFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                    |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                    |  VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;
    }
    
    if (memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT && m_parent->GetOptions()->apitraceMode) {
      memoryFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                  |  VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }

    return memoryFlags;
  }


  Rc<DxvkBuffer> D3D11Buffer::CreateSoCounterBuffer() {
    Rc<DxvkDevice> device = m_parent->GetDXVKDevice();

    DxvkBufferCreateInfo info;
    info.size   = sizeof(D3D11SOCounter);
    info.usage  = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
                | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
                | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
                | VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT
                | VK_ACCESS_TRANSFER_WRITE_BIT
                | VK_ACCESS_INDIRECT_COMMAND_READ_BIT
                | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT
                | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT;
    return device->createBuffer(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }
  

  D3D11Buffer* GetCommonBuffer(ID3D11Resource* pResource) {
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&dimension);

    return dimension == D3D11_RESOURCE_DIMENSION_BUFFER
      ? static_cast<D3D11Buffer*>(pResource)
      : nullptr;
  }

}
