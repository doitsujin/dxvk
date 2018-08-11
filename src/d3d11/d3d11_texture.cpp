#include "d3d11_device.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  D3D11CommonTexture::D3D11CommonTexture(
          D3D11Device*                pDevice,
    const D3D11_COMMON_TEXTURE_DESC*  pDesc,
          D3D11_RESOURCE_DIMENSION    Dimension)
  : m_device(pDevice), m_desc(*pDesc) {
    DXGI_VK_FORMAT_MODE   formatMode   = GetFormatMode();
    DXGI_VK_FORMAT_INFO   formatInfo   = m_device->LookupFormat(m_desc.Format, formatMode);
    DXGI_VK_FORMAT_FAMILY formatFamily = m_device->LookupFamily(m_desc.Format, formatMode);

    DxvkImageCreateInfo imageInfo;
    imageInfo.type            = GetImageTypeFromResourceDim(Dimension);
    imageInfo.format          = formatInfo.Format;
    imageInfo.flags           = 0;
    imageInfo.sampleCount     = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.extent.width    = m_desc.Width;
    imageInfo.extent.height   = m_desc.Height;
    imageInfo.extent.depth    = m_desc.Depth;
    imageInfo.numLayers       = m_desc.ArraySize;
    imageInfo.mipLevels       = m_desc.MipLevels;
    imageInfo.usage           = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                              | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.stages          = VK_PIPELINE_STAGE_TRANSFER_BIT;
    imageInfo.access          = VK_ACCESS_TRANSFER_READ_BIT
                              | VK_ACCESS_TRANSFER_WRITE_BIT;
    imageInfo.tiling          = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.layout          = VK_IMAGE_LAYOUT_GENERAL;

    DecodeSampleCount(m_desc.SampleDesc.Count, &imageInfo.sampleCount);

    // Integer clear operations on UAVs are implemented using
    // a view with a bit-compatible integer format, so we'll
    // have to include that format in the format family
    if (m_desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
      DXGI_VK_FORMAT_INFO formatBase = m_device->LookupFormat(
        m_desc.Format, DXGI_VK_FORMAT_MODE_RAW);

      if (formatBase.Format != formatInfo.Format
       && formatBase.Format != VK_FORMAT_UNDEFINED) {
        formatFamily.Add(formatBase.Format);
        formatFamily.Add(formatInfo.Format);
      }
    }

    // The image must be marked as mutable if it can be reinterpreted
    // by a view with a different format. Depth-stencil formats cannot
    // be reinterpreted in Vulkan, so we'll ignore those.
    auto formatProperties = imageFormatInfo(formatInfo.Format);
    
    bool isTypeless = formatInfo.Aspect == 0;
    bool isMutable = formatFamily.FormatCount > 1;

    if (isMutable && (formatProperties->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)) {
      imageInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

      // Typeless UAV images have relaxed reinterpretation rules
      if (!isTypeless || !(m_desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)) {
        imageInfo.viewFormatCount = formatFamily.FormatCount;
        imageInfo.viewFormats     = formatFamily.Formats;
      }
    }

    // Some games will try to create an SRGB image with the UAV
    // bind flag set. This works on Windows, but no UAVs can be
    // created for the image in practice.
    bool noUav = formatProperties->flags.test(DxvkFormatFlag::ColorSpaceSrgb)
      && !CheckFormatFeatureSupport(formatInfo.Format, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);
    
    // Adjust image flags based on the corresponding D3D flags
    if (m_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
      imageInfo.usage  |= VK_IMAGE_USAGE_SAMPLED_BIT;
      imageInfo.stages |= pDevice->GetEnabledShaderStages();
      imageInfo.access |= VK_ACCESS_SHADER_READ_BIT;
    }
    
    if (m_desc.BindFlags & D3D11_BIND_RENDER_TARGET) {
      imageInfo.usage  |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      imageInfo.stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      imageInfo.access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                       |  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    
    if (m_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
      imageInfo.usage  |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      imageInfo.stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                       |  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      imageInfo.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                       |  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    
    if (m_desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS && !noUav) {
      imageInfo.usage  |= VK_IMAGE_USAGE_STORAGE_BIT;
      imageInfo.stages |= pDevice->GetEnabledShaderStages();
      imageInfo.access |= VK_ACCESS_SHADER_READ_BIT
                       |  VK_ACCESS_SHADER_WRITE_BIT;
    }
    
    // Access pattern for meta-resolve operations
    if (imageInfo.sampleCount != VK_SAMPLE_COUNT_1_BIT) {
      imageInfo.usage  |= VK_IMAGE_USAGE_SAMPLED_BIT;
      imageInfo.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      imageInfo.access |= VK_ACCESS_SHADER_READ_BIT;
    }
    
    if (m_desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
      imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    
    if (Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
      imageInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR;
    
    // Some image formats (i.e. the R32G32B32 ones) are
    // only supported with linear tiling on most GPUs
    if (!CheckImageSupport(&imageInfo, VK_IMAGE_TILING_OPTIMAL))
      imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
    
    // Determine map mode based on our findings
    m_mapMode = DetermineMapMode(&imageInfo);
    
    // If the image is mapped directly to host memory, we need
    // to enable linear tiling, and DXVK needs to be aware that
    // the image can be accessed by the host.
    if (m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT) {
      imageInfo.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      imageInfo.tiling  = VK_IMAGE_TILING_LINEAR;
      
      if (m_desc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)
        imageInfo.access |= VK_ACCESS_HOST_WRITE_BIT;
      
      if (m_desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ)
        imageInfo.access |= VK_ACCESS_HOST_READ_BIT;
    }
    
    // We must keep LINEAR images in GENERAL layout, but we
    // can choose a better layout for the image based on how
    // it is going to be used by the game.
    if (imageInfo.tiling == VK_IMAGE_TILING_OPTIMAL)
      imageInfo.layout = OptimizeLayout(imageInfo.usage);
    
    // Check if we can actually create the image
    if (!CheckImageSupport(&imageInfo, imageInfo.tiling)) {
      throw DxvkError(str::format(
        "D3D11: Cannot create texture:",
        "\n  Format:  ", imageInfo.format,
        "\n  Extent:  ", imageInfo.extent.width,
                    "x", imageInfo.extent.height,
                    "x", imageInfo.extent.depth,
        "\n  Samples: ", imageInfo.sampleCount,
        "\n  Layers:  ", imageInfo.numLayers,
        "\n  Levels:  ", imageInfo.mipLevels,
        "\n  Usage:   ", std::hex, imageInfo.usage));
    }
    
    // If necessary, create the mapped linear buffer
    if (m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER)
      m_buffer = CreateMappedBuffer();
    
    // Create the image on a host-visible memory type
    // in case it is going to be mapped directly.
    VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    
    if (m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT) {
      memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                       | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                       | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }
    
    m_image = m_device->GetDXVKDevice()->createImage(imageInfo, memoryProperties);
  }
  
  
  D3D11CommonTexture::~D3D11CommonTexture() {
    
  }
  
  
  VkImageSubresource D3D11CommonTexture::GetSubresourceFromIndex(
          VkImageAspectFlags    Aspect,
          UINT                  Subresource) const {
    VkImageSubresource result;
    result.aspectMask     = Aspect;
    result.mipLevel       = Subresource % m_desc.MipLevels;
    result.arrayLayer     = Subresource / m_desc.MipLevels;
    return result;
  }
  
  
  DXGI_VK_FORMAT_MODE D3D11CommonTexture::GetFormatMode() const {
    if (m_desc.BindFlags & D3D11_BIND_RENDER_TARGET)
      return DXGI_VK_FORMAT_MODE_COLOR;
    
    if (m_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
      return DXGI_VK_FORMAT_MODE_DEPTH;
    
    return DXGI_VK_FORMAT_MODE_ANY;
  }
  
  
  void D3D11CommonTexture::GetDevice(ID3D11Device** ppDevice) const {
    *ppDevice = m_device.ref();
  }
  
  
  bool D3D11CommonTexture::CheckViewCompatibility(UINT BindFlags, DXGI_FORMAT Format) const {
    const DxvkImageCreateInfo& imageInfo = m_image->info();

    // Check whether the given bind flags are supported
    VkImageUsageFlags usage = GetImageUsageFlags(BindFlags);

    if ((imageInfo.usage & usage) != usage)
      return false;

    // Check whether the view format is compatible
    DXGI_VK_FORMAT_MODE formatMode = GetFormatMode();
    DXGI_VK_FORMAT_INFO viewFormat = m_device->LookupFormat(Format,        formatMode);
    DXGI_VK_FORMAT_INFO baseFormat = m_device->LookupFormat(m_desc.Format, formatMode);
    
    if (imageInfo.flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) {
      // Check whether the given combination of image
      // view type and view format is actually supported
      VkFormatFeatureFlags features = GetImageFormatFeatures(BindFlags);
      
      if (!CheckFormatFeatureSupport(viewFormat.Format, features))
        return false;

      // Using the image format itself is always legal
      if (viewFormat.Format == baseFormat.Format)
        return true;
      
      // If there is a list of compatible formats, the
      // view format must be included in that list.
      for (size_t i = 0; i < imageInfo.viewFormatCount; i++) {
        if (imageInfo.viewFormats[i] == viewFormat.Format)
          return true;
      }

      // Otherwise, all bit-compatible formats can be used.
      if (imageInfo.viewFormatCount == 0) {
        auto baseFormatInfo = imageFormatInfo(baseFormat.Format);
        auto viewFormatInfo = imageFormatInfo(viewFormat.Format);
        
        return baseFormatInfo->aspectMask  == viewFormatInfo->aspectMask
            && baseFormatInfo->elementSize == viewFormatInfo->elementSize;
      }

      return false;
    } else {
      // For non-mutable images, the view format
      // must be identical to the image format.
      return viewFormat.Format == baseFormat.Format;
    }
  }
  
  
  HRESULT D3D11CommonTexture::NormalizeTextureProperties(D3D11_COMMON_TEXTURE_DESC* pDesc) {
    if (FAILED(DecodeSampleCount(pDesc->SampleDesc.Count, nullptr)))
      return E_INVALIDARG;
    
    // Use the maximum possible mip level count if the supplied
    // mip level count is either unspecified (0) or invalid
    const uint32_t maxMipLevelCount = pDesc->SampleDesc.Count <= 1
      ? util::computeMipLevelCount({ pDesc->Width, pDesc->Height, pDesc->Depth })
      : 1u;
    
    if (pDesc->MipLevels == 0 || pDesc->MipLevels > maxMipLevelCount)
      pDesc->MipLevels = maxMipLevelCount;
    
    return S_OK;
  }
  
  
  BOOL D3D11CommonTexture::CheckImageSupport(
    const DxvkImageCreateInfo*  pImageInfo,
          VkImageTiling         Tiling) const {
    const Rc<DxvkAdapter> adapter = m_device->GetDXVKDevice()->adapter();
    
    VkImageFormatProperties formatProps = { };
    
    VkResult status = adapter->imageFormatProperties(
      pImageInfo->format, pImageInfo->type, Tiling,
      pImageInfo->usage, pImageInfo->flags, formatProps);
    
    if (status != VK_SUCCESS)
      return FALSE;
    
    return (pImageInfo->extent.width  <= formatProps.maxExtent.width)
        && (pImageInfo->extent.height <= formatProps.maxExtent.height)
        && (pImageInfo->extent.depth  <= formatProps.maxExtent.depth)
        && (pImageInfo->numLayers     <= formatProps.maxArrayLayers)
        && (pImageInfo->mipLevels     <= formatProps.maxMipLevels)
        && (pImageInfo->sampleCount    & formatProps.sampleCounts);
  }


  BOOL D3D11CommonTexture::CheckFormatFeatureSupport(
          VkFormat              Format,
          VkFormatFeatureFlags  Features) const {
    VkFormatProperties properties = m_device->GetDXVKDevice()->adapter()->formatProperties(Format);

    return (properties.linearTilingFeatures  & Features) == Features
        || (properties.optimalTilingFeatures & Features) == Features;
  }
  
  
  D3D11_COMMON_TEXTURE_MAP_MODE D3D11CommonTexture::DetermineMapMode(
    const DxvkImageCreateInfo*  pImageInfo) const {
    // Don't map an image unless the application requests it
    if (m_desc.CPUAccessFlags == 0)
      return D3D11_COMMON_TEXTURE_MAP_MODE_NONE;
    
    // Write-only images should go through a buffer for multiple reasons:
    // 1. Some games do not respect the row and depth pitch that is returned
    //    by the Map() method, which leads to incorrect rendering (e.g. Nier)
    // 2. Since the image will most likely be read for rendering by the GPU,
    //    writing the image to device-local image may be more efficient than
    //    reading its contents from host-visible memory.
    if (m_desc.Usage == D3D11_USAGE_DYNAMIC)
      return D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER;
    
    // Images that can be read by the host should be mapped directly in
    // order to avoid expensive synchronization with the GPU. This does
    // however require linear tiling, which may not be supported for all
    // combinations of image parameters.
    return this->CheckImageSupport(pImageInfo, VK_IMAGE_TILING_LINEAR)
      ? D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT
      : D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER;
  }
  
  
  Rc<DxvkBuffer> D3D11CommonTexture::CreateMappedBuffer() const {
    const DxvkFormatInfo* formatInfo = imageFormatInfo(
      m_device->LookupFormat(m_desc.Format, GetFormatMode()).Format);
    
    const VkExtent3D blockCount = util::computeBlockCount(
      VkExtent3D { m_desc.Width, m_desc.Height, m_desc.Depth },
      formatInfo->blockSize);
    
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
    
    return m_device->GetDXVKDevice()->createBuffer(info,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }
  
  
  VkImageType D3D11CommonTexture::GetImageTypeFromResourceDim(D3D11_RESOURCE_DIMENSION Dimension) {
    switch (Dimension) {
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: return VK_IMAGE_TYPE_1D;
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: return VK_IMAGE_TYPE_2D;
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: return VK_IMAGE_TYPE_3D;
      default: throw DxvkError("D3D11CommonTexture: Unhandled resource dimension");
    }
  }
  
  
  VkImageLayout D3D11CommonTexture::OptimizeLayout(VkImageUsageFlags Usage) {
    const VkImageUsageFlags usageFlags = Usage;
    
    // Filter out unnecessary flags. Transfer operations
    // are handled by the backend in a transparent manner.
    Usage &= ~(VK_IMAGE_USAGE_TRANSFER_DST_BIT
             | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    
    // If the image is used only as an attachment, we never
    // have to transform the image back to a different layout
    if (Usage == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    if (Usage == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    Usage &= ~(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
             | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    
    // If the image is used for reading but not as a storage
    // image, we can optimize the image for texture access
    if (Usage == VK_IMAGE_USAGE_SAMPLED_BIT) {
      return usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    
    // Otherwise, we have to stick with the default layout
    return VK_IMAGE_LAYOUT_GENERAL;
  }
  
  
  D3D11VkInteropSurface::D3D11VkInteropSurface(
          ID3D11DeviceChild*  pContainer,
          D3D11CommonTexture* pTexture)
  : m_container (pContainer),
    m_texture   (pTexture) {
      
  }
  
  
  D3D11VkInteropSurface::~D3D11VkInteropSurface() {
    
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11VkInteropSurface::AddRef() {
    return m_container->AddRef();
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11VkInteropSurface::Release() {
    return m_container->Release();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11VkInteropSurface::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_container->QueryInterface(riid, ppvObject);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11VkInteropSurface::GetDevice(
          IDXGIVkInteropDevice**  ppDevice) {
    Com<ID3D11Device> device;
    m_container->GetDevice(&device);
    
    return device->QueryInterface(
      __uuidof(IDXGIVkInteropDevice),
      reinterpret_cast<void**>(ppDevice));
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11VkInteropSurface::GetVulkanImageInfo(
          VkImage*              pHandle,
          VkImageLayout*        pLayout,
          VkImageCreateInfo*    pInfo) {
    const Rc<DxvkImage> image = m_texture->GetImage();
    const DxvkImageCreateInfo& info = image->info();
    
    if (pHandle != nullptr)
      *pHandle = image->handle();
    
    if (pLayout != nullptr)
      *pLayout = info.layout;
    
    if (pInfo != nullptr) {
      // We currently don't support any extended structures
      if (pInfo->sType != VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO
       || pInfo->pNext != nullptr)
        return E_INVALIDARG;
      
      pInfo->flags          = 0;
      pInfo->imageType      = info.type;
      pInfo->format         = info.format;
      pInfo->extent         = info.extent;
      pInfo->mipLevels      = info.mipLevels;
      pInfo->arrayLayers    = info.numLayers;
      pInfo->samples        = info.sampleCount;
      pInfo->tiling         = info.tiling;
      pInfo->usage          = info.usage;
      pInfo->sharingMode    = VK_SHARING_MODE_EXCLUSIVE;
      pInfo->queueFamilyIndexCount = 0;
      pInfo->initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    
    return S_OK;
  }
  
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 1 D
  D3D11Texture1D::D3D11Texture1D(
          D3D11Device*                pDevice,
    const D3D11_COMMON_TEXTURE_DESC*  pDesc)
  : m_texture (pDevice, pDesc, D3D11_RESOURCE_DIMENSION_TEXTURE1D),
    m_interop (this, &m_texture),
    m_d3d10   (this) {
    
  }
  
  
  D3D11Texture1D::~D3D11Texture1D() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Texture1D::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11Resource)
     || riid == __uuidof(ID3D11Texture1D)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10Resource)
     || riid == __uuidof(ID3D10Texture1D)) {
      *ppvObject = ref(&m_d3d10);
      return S_OK;
    }
    
    if (riid == __uuidof(IDXGIVkInteropSurface)) {
      *ppvObject = ref(&m_interop);
      return S_OK;
    }
    
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
  : m_texture (pDevice, pDesc, D3D11_RESOURCE_DIMENSION_TEXTURE2D),
    m_interop (this, &m_texture),
    m_d3d10   (this) {
    
  }
  
  
  D3D11Texture2D::~D3D11Texture2D() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Texture2D::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11Resource)
     || riid == __uuidof(ID3D11Texture2D)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10Resource)
     || riid == __uuidof(ID3D10Texture2D)) {
      *ppvObject = ref(&m_d3d10);
      return S_OK;
    }
    
    if (riid == __uuidof(IDXGIVkInteropSurface)) {
      *ppvObject = ref(&m_interop);
      return S_OK;
    }
    
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
  : m_texture (pDevice, pDesc, D3D11_RESOURCE_DIMENSION_TEXTURE3D),
    m_interop (this, &m_texture),
    m_d3d10   (this) {
    
  }
  
  
  D3D11Texture3D::~D3D11Texture3D() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Texture3D::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11Resource)
     || riid == __uuidof(ID3D11Texture3D)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10Resource)
     || riid == __uuidof(ID3D10Texture3D)) {
      *ppvObject = ref(&m_d3d10);
      return S_OK;
    }
    
    if (riid == __uuidof(IDXGIVkInteropSurface)) {
      *ppvObject = ref(&m_interop);
      return S_OK;
    }
    
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
  
  
  
  D3D11CommonTexture* GetCommonTexture(ID3D11Resource* pResource) {
    D3D11_RESOURCE_DIMENSION dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&dimension);
    
    switch (dimension) {
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
        return static_cast<D3D11Texture1D*>(pResource)->GetCommonTexture();
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
        return static_cast<D3D11Texture2D*>(pResource)->GetCommonTexture();
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
        return static_cast<D3D11Texture3D*>(pResource)->GetCommonTexture();
      
      default:
        return nullptr;
    }
  }
  
}
