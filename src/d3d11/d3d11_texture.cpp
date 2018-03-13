#include "d3d11_device.h"
#include "d3d11_texture.h"

namespace dxvk {
  
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
      
      if (CPUAccessFlags & D3D11_CPU_ACCESS_READ)
        pImageInfo->access |= VK_ACCESS_HOST_READ_BIT;
    }
    
    if (MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
      pImageInfo->flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    
    if (pImageInfo->tiling == VK_IMAGE_TILING_OPTIMAL)
      pImageInfo->layout = OptimizeLayout(pImageInfo->usage);
  }
  
  
  D3D11CommonTexture::D3D11CommonTexture(
          D3D11Device*                pDevice,
    const D3D11_COMMON_TEXTURE_DESC*  pDesc,
          D3D11_RESOURCE_DIMENSION    Dimension)
  : m_device(pDevice), m_desc(*pDesc) {
    DxgiFormatInfo formatInfo = m_device->LookupFormat(m_desc.Format, GetFormatMode());
    
    DxvkImageCreateInfo info;
    info.type           = GetImageTypeFromResourceDim(Dimension);
    info.format         = formatInfo.format;
    info.flags          = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    info.sampleCount    = VK_SAMPLE_COUNT_1_BIT;
    info.extent.width   = m_desc.Width;
    info.extent.height  = m_desc.Height;
    info.extent.depth   = m_desc.Depth;
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
      m_device.ptr(),
      m_desc.BindFlags,
      m_desc.CPUAccessFlags,
      m_desc.MiscFlags,
      &info);
    
    m_image = m_device->GetDXVKDevice()->createImage(
      info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    m_buffer = m_desc.CPUAccessFlags != 0
      ? CreateImageBuffer(m_device->GetDXVKDevice(), info.format, info.extent)
      : nullptr;
    
    m_texinfo.formatMode = GetFormatMode();
    m_texinfo.imageBuffer = m_buffer;
    m_texinfo.image = m_image;
    m_texinfo.usage = m_desc.Usage;
    m_texinfo.bindFlags = m_desc.BindFlags;
  }
  
  
  D3D11CommonTexture::~D3D11CommonTexture() {
    
  }
  
  
  VkImageSubresource D3D11CommonTexture::GetSubresourceFromIndex(
          VkImageAspectFlags    Aspect,
          UINT                  Subresource) {
    VkImageSubresource result;
    result.aspectMask     = Aspect;
    result.mipLevel       = Subresource % m_desc.MipLevels;
    result.arrayLayer     = Subresource / m_desc.MipLevels;
    return result;
  }
  
  
  DxgiFormatMode D3D11CommonTexture::GetFormatMode() const {
    if (m_desc.BindFlags & D3D11_BIND_RENDER_TARGET)
      return DxgiFormatMode::Color;
    
    if (m_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
      return DxgiFormatMode::Depth;
    
    return DxgiFormatMode::Any;
  }
  
  
  void D3D11CommonTexture::GetDevice(ID3D11Device** ppDevice) const {
    *ppDevice = m_device.ref();
  }
  
  
  HRESULT D3D11CommonTexture::NormalizeTextureProperties(D3D11_COMMON_TEXTURE_DESC* pDesc) {
    if (pDesc->MipLevels == 0) {
      pDesc->MipLevels = pDesc->SampleDesc.Count <= 1
        ? util::computeMipLevelCount({ pDesc->Width, pDesc->Height, pDesc->Depth })
        : 1u;
    }
    
    return S_OK;
  }
  
  
  VkImageType D3D11CommonTexture::GetImageTypeFromResourceDim(D3D11_RESOURCE_DIMENSION Dimension) {
    switch (Dimension) {
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: return VK_IMAGE_TYPE_1D;
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: return VK_IMAGE_TYPE_2D;
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: return VK_IMAGE_TYPE_3D;
      default: throw DxvkError("D3D11CommonTexture: Unhandled resource dimension");
    }
  }
  
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 1 D
  D3D11Texture1D::D3D11Texture1D(
          D3D11Device*                pDevice,
    const D3D11_COMMON_TEXTURE_DESC*  pDesc)
  : m_texture(pDevice, pDesc, D3D11_RESOURCE_DIMENSION_TEXTURE1D) {
    
  }
  
  
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
    m_texture.GetDevice(ppDevice);
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
    pDesc->Width          = m_texture.Desc()->Width;
    pDesc->MipLevels      = m_texture.Desc()->MipLevels;
    pDesc->ArraySize      = m_texture.Desc()->ArraySize;
    pDesc->Format         = m_texture.Desc()->Format;
    pDesc->Usage          = m_texture.Desc()->Usage;
    pDesc->BindFlags      = m_texture.Desc()->BindFlags;
    pDesc->CPUAccessFlags = m_texture.Desc()->CPUAccessFlags;
    pDesc->MiscFlags      = m_texture.Desc()->MiscFlags;
  }
  
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 2 D
  D3D11Texture2D::D3D11Texture2D(
          D3D11Device*                pDevice,
    const D3D11_COMMON_TEXTURE_DESC*  pDesc)
  : m_texture(pDevice, pDesc, D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
    
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
    m_texture.GetDevice(ppDevice);
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
    pDesc->Width          = m_texture.Desc()->Width;
    pDesc->Height         = m_texture.Desc()->Height;
    pDesc->MipLevels      = m_texture.Desc()->MipLevels;
    pDesc->ArraySize      = m_texture.Desc()->ArraySize;
    pDesc->Format         = m_texture.Desc()->Format;
    pDesc->SampleDesc     = m_texture.Desc()->SampleDesc;
    pDesc->Usage          = m_texture.Desc()->Usage;
    pDesc->BindFlags      = m_texture.Desc()->BindFlags;
    pDesc->CPUAccessFlags = m_texture.Desc()->CPUAccessFlags;
    pDesc->MiscFlags      = m_texture.Desc()->MiscFlags;
  }
  
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 3 D
  D3D11Texture3D::D3D11Texture3D(
          D3D11Device*                pDevice,
    const D3D11_COMMON_TEXTURE_DESC*  pDesc)
  : m_texture(pDevice, pDesc, D3D11_RESOURCE_DIMENSION_TEXTURE3D) {
    
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
    m_texture.GetDevice(ppDevice);
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
    pDesc->Width          = m_texture.Desc()->Width;
    pDesc->Height         = m_texture.Desc()->Height;
    pDesc->Depth          = m_texture.Desc()->Depth;
    pDesc->MipLevels      = m_texture.Desc()->MipLevels;
    pDesc->Format         = m_texture.Desc()->Format;
    pDesc->Usage          = m_texture.Desc()->Usage;
    pDesc->BindFlags      = m_texture.Desc()->BindFlags;
    pDesc->CPUAccessFlags = m_texture.Desc()->CPUAccessFlags;
    pDesc->MiscFlags      = m_texture.Desc()->MiscFlags;
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
