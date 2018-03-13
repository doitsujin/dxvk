#include "d3d11_device.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  /**
   * \brief Retrieves format mode from bind flags
   * 
   * Uses the bind flags to determine whether a resource
   * needs to be created with a color format or a depth
   * format, even if the DXGI format is typeless.
   * \param [in] BindFlags Image bind flags
   * \returns Format mode
   */
  static DxgiFormatMode GetFormatModeFromBindFlags(UINT BindFlags) {
    if (BindFlags & D3D11_BIND_RENDER_TARGET)
      return DxgiFormatMode::Color;
    
    if (BindFlags & D3D11_BIND_DEPTH_STENCIL)
      return DxgiFormatMode::Depth;
    
    return DxgiFormatMode::Any;
  }
  
  /**
   * \brief Optimizes image layout based on usage flags
   * 
   * \param [in] flags Image usage flag
   * \returns Optimized image layout
   */
  static VkImageLayout OptimizeLayout(VkImageUsageFlags flags) {
    const VkImageUsageFlags allFlags = flags;
    
    // Filter out unnecessary flags. Transfer operations
    // are handled by the backend in a transparent manner.
    flags &= ~(VK_IMAGE_USAGE_TRANSFER_DST_BIT
             | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    
    // If the image is used only as an attachment, we never
    // have to transform the image back to a different layout
    if (flags == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    if (flags == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    flags &= ~(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
             | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    
    // If the image is used for reading but not as a storage
    // image, we can optimize the image for texture access
    if (flags == VK_IMAGE_USAGE_SAMPLED_BIT) {
      return allFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    
    // Otherwise, we have to stick with the default layout
    return VK_IMAGE_LAYOUT_GENERAL;
  }
  
  
  /**
   * \brief Creates a buffer to map an image object
   * 
   * When mapping an image, some applications make the incorrect
   * assumption that image data is tightly packed, which may lead
   * to corrupted textures in memory. To prevent this, we create
   * a tightly packed buffer and use it when mapping the image.
   */
  Rc<DxvkBuffer> CreateImageBuffer(
    const Rc<DxvkDevice>&       device,
          VkFormat              format,
          VkExtent3D            extent) {
    const DxvkFormatInfo* formatInfo = imageFormatInfo(format);
    
    const VkExtent3D blockCount = {
      extent.width  / formatInfo->blockSize.width,
      extent.height / formatInfo->blockSize.height,
      extent.depth  / formatInfo->blockSize.depth };
    
    DxvkBufferCreateInfo info;
    info.size   = formatInfo->elementSize
                * blockCount.width
                * blockCount.height
                * blockCount.depth;
    info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT
                | VK_ACCESS_TRANSFER_WRITE_BIT;
    
    return device->createBuffer(info,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }
  
  
  /**
   * \brief Fills in image info stage and access flags
   * 
   * \param [in] pDevice Target device
   * \param [in] BindFLags Resource bind flags
   * \param [in] CPUAccessFlags CPU access flags
   * \param [in] MiscFlags Additional usage info
   * \param [out] pImageInfo DXVK image create info
   */
  static void GetImageStagesAndAccessFlags(
    const D3D11Device*          pDevice,
          UINT                  BindFlags,
          UINT                  CPUAccessFlags,
          UINT                  MiscFlags,
          DxvkImageCreateInfo*  pImageInfo) {
    
    if (BindFlags & D3D11_BIND_SHADER_RESOURCE) {
      pImageInfo->usage  |= VK_IMAGE_USAGE_SAMPLED_BIT;
      pImageInfo->stages |= pDevice->GetEnabledShaderStages();
      pImageInfo->access |= VK_ACCESS_SHADER_READ_BIT;
    }
    
    if (BindFlags & D3D11_BIND_RENDER_TARGET) {
      pImageInfo->usage  |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      pImageInfo->stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      pImageInfo->access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                         |  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    
    if (BindFlags & D3D11_BIND_DEPTH_STENCIL) {
      pImageInfo->usage  |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      pImageInfo->stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                         |  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      pImageInfo->access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                         |  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    
    if (BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
      pImageInfo->usage  |= VK_IMAGE_USAGE_STORAGE_BIT;
      pImageInfo->stages |= pDevice->GetEnabledShaderStages();
      pImageInfo->access |= VK_ACCESS_SHADER_READ_BIT
                         |  VK_ACCESS_SHADER_WRITE_BIT;
    }
    
    if (CPUAccessFlags != 0) {
      pImageInfo->stages |= VK_PIPELINE_STAGE_HOST_BIT;
      
      if (CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)
        pImageInfo->access |= VK_ACCESS_HOST_WRITE_BIT;
      
      if (CPUAccessFlags & D3D11_CPU_ACCESS_READ) {
        pImageInfo->access |= VK_ACCESS_HOST_READ_BIT;
        pImageInfo->tiling  = VK_IMAGE_TILING_LINEAR;
      }
    }
    
    if (MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
      pImageInfo->flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    
    if (pImageInfo->tiling == VK_IMAGE_TILING_OPTIMAL)
      pImageInfo->layout = OptimizeLayout(pImageInfo->usage);
  }
  
  
  /**
   * \brief Retrieves memory flags for image usage
   * 
   * If the host requires access to the image, we
   * should create it on a host-visible memory type.
   * \param [in] Usage Image usage flags
   * \returns Image memory properties
   */
  static VkMemoryPropertyFlags GetImageMemoryFlags(UINT CPUAccessFlags) {
    if (CPUAccessFlags & D3D11_CPU_ACCESS_READ) {
      return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
           | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
           | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    } else {
      // If only write access is required, we will emulate
      // image mapping through a buffer. Some games ignore
      // the row pitch when mapping images, which leads to
      // incorrect rendering.
      return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
  }
  
  
  D3D11Texture1D::D3D11Texture1D(
          D3D11Device*                pDevice,
    const D3D11_TEXTURE1D_DESC*       pDesc)
  : m_device    (pDevice),
    m_desc      (*pDesc) {
    
    const DxgiFormatMode formatMode
      = GetFormatModeFromBindFlags(m_desc.BindFlags);
    
    if (m_desc.MipLevels == 0) {
      m_desc.MipLevels = util::computeMipLevelCount(
        { m_desc.Width, 1u, 1u });
    }
    
    DxvkImageCreateInfo info;
    info.type           = VK_IMAGE_TYPE_1D;
    info.format         = pDevice->LookupFormat(m_desc.Format, formatMode).format;
    info.flags          = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    info.sampleCount    = VK_SAMPLE_COUNT_1_BIT;
    info.extent.width   = m_desc.Width;
    info.extent.height  = 1;
    info.extent.depth   = 1;
    info.numLayers      = m_desc.ArraySize;
    info.mipLevels      = m_desc.MipLevels;
    info.usage          = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                        | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.stages         = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access         = VK_ACCESS_TRANSFER_READ_BIT
                        | VK_ACCESS_TRANSFER_WRITE_BIT;
    info.tiling         = VK_IMAGE_TILING_OPTIMAL;
    info.layout         = VK_IMAGE_LAYOUT_GENERAL;
    
    GetImageStagesAndAccessFlags(
      pDevice,
      m_desc.BindFlags,
      m_desc.CPUAccessFlags,
      m_desc.MiscFlags,
      &info);
    
    // Create the image and, if necessary, the image buffer
    m_texInfo.formatMode  = formatMode;
    m_texInfo.image       = pDevice->GetDXVKDevice()->createImage(
      info, GetImageMemoryFlags(m_desc.CPUAccessFlags));
    m_texInfo.imageBuffer = m_desc.CPUAccessFlags != 0
      ? CreateImageBuffer(pDevice->GetDXVKDevice(), info.format, info.extent)
      : nullptr;
    
    m_texInfo.usage       = m_desc.Usage;
    m_texInfo.bindFlags   = m_desc.BindFlags;
  }
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 1 D
  D3D11Texture1D::~D3D11Texture1D() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Texture1D::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Resource);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Texture1D);
    
    Logger::warn("D3D11Texture1D::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
    
  void STDMETHODCALLTYPE D3D11Texture1D::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture1D::GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension) {
    *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE1D;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Texture1D::GetEvictionPriority() {
    Logger::warn("D3D11Texture1D::GetEvictionPriority: Stub");
    return DXGI_RESOURCE_PRIORITY_NORMAL;
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture1D::SetEvictionPriority(UINT EvictionPriority) {
    Logger::warn("D3D11Texture1D::SetEvictionPriority: Stub");
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture1D::GetDesc(D3D11_TEXTURE1D_DESC *pDesc) {
    *pDesc = m_desc;
  }
  
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 2 D
  D3D11Texture2D::D3D11Texture2D(
          D3D11Device*                pDevice,
    const D3D11_TEXTURE2D_DESC*       pDesc)
  : m_device    (pDevice),
    m_desc      (*pDesc) {
    
    const DxgiFormatMode formatMode
      = GetFormatModeFromBindFlags(m_desc.BindFlags);
    
    if (m_desc.MipLevels == 0) {
      m_desc.MipLevels = m_desc.SampleDesc.Count <= 1
        ? util::computeMipLevelCount({ m_desc.Width, m_desc.Height, 1u })
        : 1;
    }
    
    DxvkImageCreateInfo info;
    info.type           = VK_IMAGE_TYPE_2D;
    info.format         = pDevice->LookupFormat(m_desc.Format, formatMode).format;
    info.flags          = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    info.sampleCount    = VK_SAMPLE_COUNT_1_BIT;
    info.extent.width   = m_desc.Width;
    info.extent.height  = m_desc.Height;
    info.extent.depth   = 1;
    info.numLayers      = m_desc.ArraySize;
    info.mipLevels      = m_desc.MipLevels;
    info.usage          = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                        | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.stages         = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access         = VK_ACCESS_TRANSFER_READ_BIT
                        | VK_ACCESS_TRANSFER_WRITE_BIT;
    info.tiling         = VK_IMAGE_TILING_OPTIMAL;
    info.layout         = VK_IMAGE_LAYOUT_GENERAL;
    
    if (FAILED(GetSampleCount(m_desc.SampleDesc.Count, &info.sampleCount)))
      throw DxvkError(str::format("D3D11: Invalid sample count: ", m_desc.SampleDesc.Count));
    
    GetImageStagesAndAccessFlags(
      pDevice,
      m_desc.BindFlags,
      m_desc.CPUAccessFlags,
      m_desc.MiscFlags,
      &info);
    
    // Create the image and, if necessary, the image buffer
    m_texInfo.formatMode  = formatMode;
    m_texInfo.image       = pDevice->GetDXVKDevice()->createImage(
      info, GetImageMemoryFlags(m_desc.CPUAccessFlags));
    m_texInfo.imageBuffer = m_desc.CPUAccessFlags != 0
      ? CreateImageBuffer(pDevice->GetDXVKDevice(), info.format, info.extent)
      : nullptr;
    
    m_texInfo.usage       = m_desc.Usage;
    m_texInfo.bindFlags   = m_desc.BindFlags;
  }
  
  
  D3D11Texture2D::~D3D11Texture2D() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Texture2D::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Resource);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Texture2D);
    
    Logger::warn("D3D11Texture2D::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
    
  void STDMETHODCALLTYPE D3D11Texture2D::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture2D::GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension) {
    *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Texture2D::GetEvictionPriority() {
    Logger::warn("D3D11Texture2D::GetEvictionPriority: Stub");
    return DXGI_RESOURCE_PRIORITY_NORMAL;
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture2D::SetEvictionPriority(UINT EvictionPriority) {
    Logger::warn("D3D11Texture2D::SetEvictionPriority: Stub");
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture2D::GetDesc(D3D11_TEXTURE2D_DESC *pDesc) {
    *pDesc = m_desc;
  }
  
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 3 D
  D3D11Texture3D::D3D11Texture3D(
          D3D11Device*                pDevice,
    const D3D11_TEXTURE3D_DESC*       pDesc)
  : m_device    (pDevice),
    m_desc      (*pDesc) {
    
    const DxgiFormatMode formatMode
      = GetFormatModeFromBindFlags(m_desc.BindFlags);
    
    if (m_desc.MipLevels == 0) {
      m_desc.MipLevels = util::computeMipLevelCount(
        { m_desc.Width, m_desc.Height, m_desc.Depth });
    }
    
    DxvkImageCreateInfo info;
    info.type           = VK_IMAGE_TYPE_3D;
    info.format         = pDevice->LookupFormat(m_desc.Format, formatMode).format;
    info.flags          = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT
                        | VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR;
    info.sampleCount    = VK_SAMPLE_COUNT_1_BIT;
    info.extent.width   = m_desc.Width;
    info.extent.height  = m_desc.Height;
    info.extent.depth   = m_desc.Depth;
    info.numLayers      = 1;
    info.mipLevels      = m_desc.MipLevels;
    info.usage          = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                        | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.stages         = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access         = VK_ACCESS_TRANSFER_READ_BIT
                        | VK_ACCESS_TRANSFER_WRITE_BIT;
    info.tiling         = VK_IMAGE_TILING_OPTIMAL;
    info.layout         = VK_IMAGE_LAYOUT_GENERAL;
    
    GetImageStagesAndAccessFlags(
      pDevice,
      m_desc.BindFlags,
      m_desc.CPUAccessFlags,
      m_desc.MiscFlags,
      &info);
    
    // Create the image and, if necessary, the image buffer
    m_texInfo.formatMode  = formatMode;
    m_texInfo.image       = pDevice->GetDXVKDevice()->createImage(
      info, GetImageMemoryFlags(m_desc.CPUAccessFlags));
    m_texInfo.imageBuffer = m_desc.CPUAccessFlags != 0
      ? CreateImageBuffer(pDevice->GetDXVKDevice(), info.format, info.extent)
      : nullptr;
    
    m_texInfo.usage       = m_desc.Usage;
    m_texInfo.bindFlags   = m_desc.BindFlags;
  }
  
  
  D3D11Texture3D::~D3D11Texture3D() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Texture3D::QueryInterface(REFIID riid, void** ppvObject) {
    COM_QUERY_IFACE(riid, ppvObject, IUnknown);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11DeviceChild);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Resource);
    COM_QUERY_IFACE(riid, ppvObject, ID3D11Texture3D);
    
    Logger::warn("D3D11Texture3D::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
    
  void STDMETHODCALLTYPE D3D11Texture3D::GetDevice(ID3D11Device** ppDevice) {
    *ppDevice = m_device.ref();
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture3D::GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension) {
    *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Texture3D::GetEvictionPriority() {
    Logger::warn("D3D11Texture3D::GetEvictionPriority: Stub");
    return DXGI_RESOURCE_PRIORITY_NORMAL;
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture3D::SetEvictionPriority(UINT EvictionPriority) {
    Logger::warn("D3D11Texture3D::SetEvictionPriority: Stub");
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture3D::GetDesc(D3D11_TEXTURE3D_DESC *pDesc) {
    *pDesc = m_desc;
  }
  
  
  
  D3D11TextureInfo* GetCommonTextureInfo(ID3D11Resource* pResource) {
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&dimension);
    
    switch (dimension) {
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
        return static_cast<D3D11Texture1D*>(pResource)->GetTextureInfo();
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
        return static_cast<D3D11Texture2D*>(pResource)->GetTextureInfo();
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
        return static_cast<D3D11Texture3D*>(pResource)->GetTextureInfo();
      
      default:
        return nullptr;
    }
  }
  
  
  VkImageSubresource GetSubresourceFromIndex(
          VkImageAspectFlags    Aspect,
          UINT                  MipLevels,
          UINT                  Subresource) {
    VkImageSubresource result;
    result.aspectMask     = Aspect;
    result.mipLevel       = Subresource % MipLevels;
    result.arrayLayer     = Subresource / MipLevels;
    return result;
  }
  
}
