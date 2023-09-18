#include "d3d9_common_texture.h"

#include "d3d9_util.h"
#include "d3d9_device.h"

#include "../util/util_shared_res.h"
#include "../util/util_win32_compat.h"

#include <algorithm>

namespace dxvk {

  D3D9CommonTexture::D3D9CommonTexture(
          D3D9DeviceEx*             pDevice,
          IUnknown*                 pInterface,
    const D3D9_COMMON_TEXTURE_DESC* pDesc,
          D3DRESOURCETYPE           ResourceType,
          HANDLE*                   pSharedHandle)
    : m_device(pDevice), m_desc(*pDesc), m_type(ResourceType), m_d3d9Interop(pInterface, this) {
    if (m_desc.Format == D3D9Format::Unknown)
      m_desc.Format = (m_desc.Usage & D3DUSAGE_DEPTHSTENCIL)
                    ? D3D9Format::D32
                    : D3D9Format::X8R8G8B8;

    m_exposedMipLevels = m_desc.MipLevels;

    if (m_desc.Usage & D3DUSAGE_AUTOGENMIPMAP)
      m_exposedMipLevels = 1;

    for (uint32_t i = 0; i < m_dirtyBoxes.size(); i++) {
      AddDirtyBox(nullptr, i);
    }

    if (m_desc.Pool != D3DPOOL_DEFAULT && pSharedHandle) {
      throw DxvkError("D3D9: Incompatible pool type for texture sharing.");
    }

    if (IsPoolManaged(m_desc.Pool)) {
      SetAllNeedUpload();
    }

    m_mapping = pDevice->LookupFormat(m_desc.Format);

    m_mapMode        = DetermineMapMode();
    m_shadow         = DetermineShadowState();
    m_upgradedToD32f = ConvertFormatUnfixed(m_desc.Format).FormatColor != m_mapping.FormatColor &&
                       (m_mapping.FormatColor == VK_FORMAT_D32_SFLOAT_S8_UINT || m_mapping.FormatColor == VK_FORMAT_D32_SFLOAT);
    m_supportsFetch4 = DetermineFetch4Compatibility();

    const bool createImage = m_desc.Pool != D3DPOOL_SYSTEMMEM && m_desc.Pool != D3DPOOL_SCRATCH && m_desc.Format != D3D9Format::NULL_FORMAT;
    if (createImage) {
      bool plainSurface = m_type == D3DRTYPE_SURFACE &&
                          !(m_desc.Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL));

      try {
        m_image = CreatePrimaryImage(ResourceType, plainSurface, pSharedHandle);
      }
      catch (const DxvkError& e) {
        // D3DUSAGE_AUTOGENMIPMAP and offscreen plain is mutually exclusive
        // so we can combine their retry this way.
        if (m_desc.Usage & D3DUSAGE_AUTOGENMIPMAP || plainSurface) {
          m_desc.Usage &= ~D3DUSAGE_AUTOGENMIPMAP;
          m_desc.MipLevels = 1;
          m_image = CreatePrimaryImage(ResourceType, false, pSharedHandle);
        }
        else
          throw e;
      }

      if (pSharedHandle && *pSharedHandle == nullptr) {
        *pSharedHandle = m_image->sharedHandle();
        ExportImageInfo();
      }

      CreateSampleView(0);

      if (!IsManaged()) {
        m_size = m_image->memory().length();
        if (!m_device->ChangeReportedMemory(-m_size))
          throw DxvkError("D3D9: Reporting out of memory from tracking.");
      }
    }

    for (uint32_t i = 0; i < CountSubresources(); i++) {
      m_memoryOffset[i] = m_totalSize;
      m_totalSize += GetMipSize(i);
    }

    // Initialization is handled by D3D9Initializer
    if (m_mapMode == D3D9_COMMON_TEXTURE_MAP_MODE_UNMAPPABLE)
      m_data = m_device->GetAllocator()->Alloc(m_totalSize);
    else if (m_mapMode != D3D9_COMMON_TEXTURE_MAP_MODE_NONE && m_desc.Pool != D3DPOOL_DEFAULT)
      CreateBuffer(false);
  }


  D3D9CommonTexture::~D3D9CommonTexture() {
    if (m_size != 0)
      m_device->ChangeReportedMemory(m_size);

    m_device->RemoveMappedTexture(this);

    if (m_desc.Pool == D3DPOOL_DEFAULT)
      m_device->DecrementLosableCounter();
  }


  VkImageSubresource D3D9CommonTexture::GetSubresourceFromIndex(
          VkImageAspectFlags    Aspect,
          UINT                  Subresource) const {
    VkImageSubresource result;
    result.aspectMask     = Aspect;
    result.mipLevel       = Subresource % m_desc.MipLevels;
    result.arrayLayer     = Subresource / m_desc.MipLevels;
    return result;
  }
  

  HRESULT D3D9CommonTexture::NormalizeTextureProperties(
          D3D9DeviceEx*             pDevice,
          D3D9_COMMON_TEXTURE_DESC* pDesc) {
    auto* options = pDevice->GetOptions();

    //////////////////////
    // Mapping Validation
    auto mapping = pDevice->LookupFormat(pDesc->Format);

    // Handle DisableA8RT hack for The Sims 2
    if (pDesc->Format == D3D9Format::A8       &&
       (pDesc->Usage & D3DUSAGE_RENDERTARGET) &&
        options->disableA8RT)
      return D3DERR_INVALIDCALL;

    // If the mapping is invalid then lets return invalid
    // Some edge cases:
    // NULL format does not map to anything, but should succeed
    // SCRATCH textures can still be made if the device does not support
    // the format at all.

    if (!mapping.IsValid() && pDesc->Format != D3D9Format::NULL_FORMAT) {
      auto info = pDevice->UnsupportedFormatInfo(pDesc->Format);

      if (pDesc->Pool != D3DPOOL_SCRATCH || info->elementSize == 0)
        return D3DERR_INVALIDCALL;
    }

    ///////////////////
    // Desc Validation

    if (pDesc->Width == 0 || pDesc->Height == 0 || pDesc->Depth == 0)
      return D3DERR_INVALIDCALL;
    
    if (FAILED(DecodeMultiSampleType(pDevice->GetDXVKDevice(), pDesc->MultiSample, pDesc->MultisampleQuality, nullptr)))
      return D3DERR_INVALIDCALL;

    // Using MANAGED pool with DYNAMIC usage is illegal
    if (IsPoolManaged(pDesc->Pool) && (pDesc->Usage & D3DUSAGE_DYNAMIC))
      return D3DERR_INVALIDCALL;

    // D3DUSAGE_WRITEONLY doesn't apply to textures.
    if (pDesc->Usage & D3DUSAGE_WRITEONLY)
      return D3DERR_INVALIDCALL;

    // RENDERTARGET and DEPTHSTENCIL must be default pool
    constexpr DWORD incompatibleUsages = D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL;
    if (pDesc->Pool != D3DPOOL_DEFAULT && (pDesc->Usage & incompatibleUsages))
      return D3DERR_INVALIDCALL;
    
    // Use the maximum possible mip level count if the supplied
    // mip level count is either unspecified (0) or invalid
    const uint32_t maxMipLevelCount = pDesc->MultiSample <= D3DMULTISAMPLE_NONMASKABLE
      ? util::computeMipLevelCount({ pDesc->Width, pDesc->Height, pDesc->Depth })
      : 1u;

    if (pDesc->Usage & D3DUSAGE_AUTOGENMIPMAP)
      pDesc->MipLevels = 0;
    
    if (pDesc->MipLevels == 0 || pDesc->MipLevels > maxMipLevelCount)
      pDesc->MipLevels = maxMipLevelCount;

    return D3D_OK;
  }


  void* D3D9CommonTexture::GetData(UINT Subresource) {
    if (unlikely(m_buffer != nullptr))
      return m_buffer->mapPtr(m_memoryOffset[Subresource]);

    m_data.Map();
    uint8_t* ptr = reinterpret_cast<uint8_t*>(m_data.Ptr());
    ptr += m_memoryOffset[Subresource];
    return ptr;
  }


  void D3D9CommonTexture::CreateBuffer(bool Initialize) {
    if (likely(m_buffer != nullptr))
      return;

    DxvkBufferCreateInfo info;
    info.size   = m_totalSize;
    info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
                | VK_PIPELINE_STAGE_HOST_BIT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT
                | VK_ACCESS_TRANSFER_WRITE_BIT
                | VK_ACCESS_HOST_WRITE_BIT
                | VK_ACCESS_HOST_READ_BIT;

    if (m_mapping.ConversionFormatInfo.FormatType != D3D9ConversionFormat_None) {
      info.usage  |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }

    VkMemoryPropertyFlags memType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                  | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    m_buffer = m_device->GetDXVKDevice()->createBuffer(info, memType);

    if (Initialize) {
      if (m_data) {
        m_data.Map();
        std::memcpy(m_buffer->mapPtr(0), m_data.Ptr(), m_totalSize);
      } else {
        std::memset(m_buffer->mapPtr(0), 0, m_totalSize);
      }
    }
    m_data = {};
  }


  VkDeviceSize D3D9CommonTexture::GetMipSize(UINT Subresource) const {
    const UINT MipLevel = Subresource % m_desc.MipLevels;

    const DxvkFormatInfo* formatInfo = m_mapping.FormatColor != VK_FORMAT_UNDEFINED
      ? lookupFormatInfo(m_mapping.FormatColor)
      : m_device->UnsupportedFormatInfo(m_desc.Format);

    const VkExtent3D mipExtent = util::computeMipLevelExtent(
      GetExtent(), MipLevel);

    VkExtent3D blockSize = formatInfo->blockSize;
    uint32_t elementSize = formatInfo->elementSize;
    if (unlikely(formatInfo->flags.test(DxvkFormatFlag::MultiPlane))) {
      // D3D9 doesn't allow specifying the plane when locking a texture.
      // So the subsampled planes inherit the pitch of the first plane.
      // That means the size is the size of plane 0 * plane count
      elementSize = formatInfo->planes[0].elementSize;
      blockSize = { formatInfo->planes[0].blockSize.width, formatInfo->planes[0].blockSize.height, 1u };
    }

    const VkExtent3D blockCount = util::computeBlockCount(
      mipExtent, blockSize);

    return std::min(GetPlaneCount(), 2u)
         * align(elementSize * blockCount.width, 4)
         * blockCount.height
         * blockCount.depth;
  }


  Rc<DxvkImage> D3D9CommonTexture::CreatePrimaryImage(D3DRESOURCETYPE ResourceType, bool TryOffscreenRT, HANDLE* pSharedHandle) const {
    DxvkImageCreateInfo imageInfo;
    imageInfo.type            = GetImageTypeFromResourceType(ResourceType);
    imageInfo.format          = m_mapping.ConversionFormatInfo.FormatColor != VK_FORMAT_UNDEFINED
                              ? m_mapping.ConversionFormatInfo.FormatColor
                              : m_mapping.FormatColor;
    imageInfo.flags           = 0;
    imageInfo.sampleCount     = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.extent.width    = m_desc.Width;
    imageInfo.extent.height   = m_desc.Height;
    imageInfo.extent.depth    = m_desc.Depth;
    imageInfo.numLayers       = m_desc.ArraySize;
    imageInfo.mipLevels       = m_desc.MipLevels;
    imageInfo.usage           = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                              | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                              | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.stages          = VK_PIPELINE_STAGE_TRANSFER_BIT
                              | m_device->GetEnabledShaderStages();
    imageInfo.access          = VK_ACCESS_TRANSFER_READ_BIT
                              | VK_ACCESS_TRANSFER_WRITE_BIT
                              | VK_ACCESS_SHADER_READ_BIT;
    imageInfo.tiling          = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.layout          = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.shared          = m_desc.IsBackBuffer;

    if (pSharedHandle) {
      imageInfo.sharing.type = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;
      imageInfo.sharing.mode = (*pSharedHandle == INVALID_HANDLE_VALUE || *pSharedHandle == nullptr)
        ? DxvkSharedHandleMode::Export
        : DxvkSharedHandleMode::Import;
      imageInfo.sharing.type = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;
      imageInfo.sharing.handle = *pSharedHandle;
      imageInfo.shared = true;
      // TODO: validate metadata?
    }

    if (m_mapping.ConversionFormatInfo.FormatType != D3D9ConversionFormat_None) {
      imageInfo.usage  |= VK_IMAGE_USAGE_STORAGE_BIT;
      imageInfo.stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }

    DecodeMultiSampleType(m_device->GetDXVKDevice(), m_desc.MultiSample, m_desc.MultisampleQuality, &imageInfo.sampleCount);

    // The image must be marked as mutable if it can be reinterpreted
    // by a view with a different format. Depth-stencil formats cannot
    // be reinterpreted in Vulkan, so we'll ignore those.
    auto formatProperties = lookupFormatInfo(m_mapping.FormatColor);

    bool isMutable     = m_mapping.FormatSrgb != VK_FORMAT_UNDEFINED;
    bool isColorFormat = (formatProperties->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0;

    if (isMutable && isColorFormat) {
      imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

      imageInfo.viewFormatCount = 2;
      imageInfo.viewFormats     = m_mapping.Formats;
    }

    const bool hasAttachmentFeedbackLoops =
      m_device->GetDXVKDevice()->features().extAttachmentFeedbackLoopLayout.attachmentFeedbackLoopLayout;
    const bool isRT = m_desc.Usage & D3DUSAGE_RENDERTARGET;
    const bool isDS = m_desc.Usage & D3DUSAGE_DEPTHSTENCIL;
    const bool isAutoGen = m_desc.Usage & D3DUSAGE_AUTOGENMIPMAP;

    // Are we an RT, need to gen mips or an offscreen plain surface?
    if (isRT || isAutoGen || TryOffscreenRT) {
      imageInfo.usage  |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      imageInfo.stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      imageInfo.access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                       |  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (isDS) {
      imageInfo.usage  |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      imageInfo.stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                       |  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      imageInfo.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                       |  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    if (ResourceType == D3DRTYPE_TEXTURE && (isRT || isDS) && hasAttachmentFeedbackLoops)
      imageInfo.usage |= VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;

    if (ResourceType == D3DRTYPE_CUBETEXTURE)
      imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    // Some image formats (i.e. the R32G32B32 ones) are
    // only supported with linear tiling on most GPUs
    if (!CheckImageSupport(&imageInfo, VK_IMAGE_TILING_OPTIMAL))
      imageInfo.tiling = VK_IMAGE_TILING_LINEAR;

    // We must keep LINEAR images in GENERAL layout, but we
    // can choose a better layout for the image based on how
    // it is going to be used by the game.
    if (imageInfo.tiling == VK_IMAGE_TILING_OPTIMAL && imageInfo.sharing.mode == DxvkSharedHandleMode::None)
      imageInfo.layout = OptimizeLayout(imageInfo.usage);

    // For some formats, we need to enable render target
    // capabilities if available, but these should
    // in no way affect the default image layout
    imageInfo.usage |= EnableMetaCopyUsage(imageInfo.format, imageInfo.tiling);

    // Check if we can actually create the image
    if (!CheckImageSupport(&imageInfo, imageInfo.tiling)) {
      throw DxvkError(str::format(
        "D3D9: Cannot create texture:",
        "\n  Type:    0x", std::hex, ResourceType, std::dec,
        "\n  Format:  ", m_desc.Format,
        "\n  Extent:  ", m_desc.Width,
                    "x", m_desc.Height,
                    "x", m_desc.Depth,
        "\n  Samples: ", m_desc.MultiSample,
        "\n  Layers:  ", m_desc.ArraySize,
        "\n  Levels:  ", m_desc.MipLevels,
        "\n  Usage:   0x", std::hex, m_desc.Usage, std::dec,
        "\n  Pool:    0x", std::hex, m_desc.Pool, std::dec));
    }

    return m_device->GetDXVKDevice()->createImage(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }


  Rc<DxvkImage> D3D9CommonTexture::CreateResolveImage() const {
    DxvkImageCreateInfo imageInfo = m_image->info();
    imageInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;

    return m_device->GetDXVKDevice()->createImage(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }


  BOOL D3D9CommonTexture::DetermineShadowState() const {
    constexpr std::array<D3D9Format, 3> blacklist = {
      D3D9Format::INTZ, D3D9Format::DF16, D3D9Format::DF24
    };

    return IsDepthFormat(m_desc.Format)
        && std::find(blacklist.begin(), blacklist.end(), m_desc.Format) == blacklist.end();
  }


  BOOL D3D9CommonTexture::DetermineFetch4Compatibility() const {
    constexpr std::array<D3D9Format, 8> singleChannelFormats = {
      D3D9Format::INTZ, D3D9Format::DF16, D3D9Format::DF24,
      D3D9Format::R16F, D3D9Format::R32F, D3D9Format::A8,
      D3D9Format::L8, D3D9Format::L16
    };

    return std::find(singleChannelFormats.begin(), singleChannelFormats.end(), m_desc.Format) != singleChannelFormats.end();
  }


  BOOL D3D9CommonTexture::CheckImageSupport(
    const DxvkImageCreateInfo*  pImageInfo,
          VkImageTiling         Tiling) const {
    DxvkFormatQuery formatQuery = { };
    formatQuery.format = pImageInfo->format;
    formatQuery.type = pImageInfo->type;
    formatQuery.tiling = Tiling;
    formatQuery.usage = pImageInfo->usage;
    formatQuery.flags = pImageInfo->flags;

    auto properties = m_device->GetDXVKDevice()->getFormatLimits(formatQuery);
    
    if (!properties)
      return FALSE;
    
    return (pImageInfo->extent.width  <= properties->maxExtent.width)
        && (pImageInfo->extent.height <= properties->maxExtent.height)
        && (pImageInfo->extent.depth  <= properties->maxExtent.depth)
        && (pImageInfo->numLayers     <= properties->maxArrayLayers)
        && (pImageInfo->mipLevels     <= properties->maxMipLevels)
        && (pImageInfo->sampleCount    & properties->sampleCounts);
  }


  VkImageUsageFlags D3D9CommonTexture::EnableMetaCopyUsage(
          VkFormat              Format,
          VkImageTiling         Tiling) const {
    VkFormatFeatureFlags2 requestedFeatures = 0;

    if (Format == VK_FORMAT_D16_UNORM || Format == VK_FORMAT_D32_SFLOAT)
      requestedFeatures |= VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (Format == VK_FORMAT_R16_UNORM || Format == VK_FORMAT_R32_SFLOAT)
      requestedFeatures |=  VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT;

    if (!requestedFeatures)
      return 0;

    // Enable usage flags for all supported and requested features
    DxvkFormatFeatures properties = m_device->GetDXVKDevice()->getFormatFeatures(Format);

    requestedFeatures &= Tiling == VK_IMAGE_TILING_OPTIMAL
      ? properties.optimal
      : properties.linear;
    
    VkImageUsageFlags requestedUsage = 0;
    
    if (requestedFeatures & VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT)
      requestedUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    
    if (requestedFeatures & VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT)
      requestedUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    return requestedUsage;
  }


  VkImageType D3D9CommonTexture::GetImageTypeFromResourceType(D3DRESOURCETYPE Type) {
    switch (Type) {
      case D3DRTYPE_SURFACE:
      case D3DRTYPE_TEXTURE:       return VK_IMAGE_TYPE_2D;
      case D3DRTYPE_VOLUMETEXTURE: return VK_IMAGE_TYPE_3D;
      case D3DRTYPE_CUBETEXTURE:   return VK_IMAGE_TYPE_2D;
      default: throw DxvkError("D3D9CommonTexture: Unhandled resource type");
    }
  }


  VkImageViewType D3D9CommonTexture::GetImageViewTypeFromResourceType(
          D3DRESOURCETYPE  Dimension,
          UINT             Layer) {
    switch (Dimension) {
      case D3DRTYPE_SURFACE:
      case D3DRTYPE_TEXTURE:       return VK_IMAGE_VIEW_TYPE_2D;
      case D3DRTYPE_VOLUMETEXTURE: return VK_IMAGE_VIEW_TYPE_3D;
      case D3DRTYPE_CUBETEXTURE:   return Layer == AllLayers
                                        ? VK_IMAGE_VIEW_TYPE_CUBE
                                        : VK_IMAGE_VIEW_TYPE_2D;
      default: throw DxvkError("D3D9CommonTexture: Unhandled resource type");
    }
  }


  VkImageLayout D3D9CommonTexture::OptimizeLayout(VkImageUsageFlags Usage) const {
    const VkImageUsageFlags usageFlags = Usage;
    
    // Filter out unnecessary flags. Transfer operations
    // are handled by the backend in a transparent manner.
    // Feedback loops are handled by hazard tracking.
    Usage &= ~(VK_IMAGE_USAGE_TRANSFER_DST_BIT
             | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
             | VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT);
    
    // Ignore sampled bit in case the image was created with
    // an image flag that only allows attachment usage
    if (m_desc.IsAttachmentOnly)
      Usage &= ~VK_IMAGE_USAGE_SAMPLED_BIT;

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

  D3D9_COMMON_TEXTURE_MAP_MODE D3D9CommonTexture::DetermineMapMode() const {
    if (m_desc.Format == D3D9Format::NULL_FORMAT)
      return D3D9_COMMON_TEXTURE_MAP_MODE_NONE;

#ifdef D3D9_ALLOW_UNMAPPING
    if (m_device->GetOptions()->textureMemory != 0 && m_desc.Pool != D3DPOOL_DEFAULT)
      return D3D9_COMMON_TEXTURE_MAP_MODE_UNMAPPABLE;
#endif

    if (m_desc.Pool == D3DPOOL_SYSTEMMEM || m_desc.Pool == D3DPOOL_SCRATCH)
      return D3D9_COMMON_TEXTURE_MAP_MODE_SYSTEMMEM;

    return D3D9_COMMON_TEXTURE_MAP_MODE_BACKED;
  }

  void D3D9CommonTexture::ExportImageInfo() {
    /* From MSDN:
      Textures being shared from D3D9 to D3D11 have the following restrictions.

      - Textures must be 2D
      - Only 1 mip level is allowed
      - Texture must have default usage
      - Texture must be write only
      - MSAA textures are not allowed
      - Bind flags must have SHADER_RESOURCE and RENDER_TARGET set
      - Only R10G10B10A2_UNORM, R16G16B16A16_FLOAT and R8G8B8A8_UNORM formats are allowed
    */
    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;

    switch (m_desc.Format) {
      case D3D9Format::A2B10G10R10: dxgiFormat = DXGI_FORMAT_R10G10B10A2_UNORM; break;
      case D3D9Format::A16B16G16R16F: dxgiFormat = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
      case D3D9Format::A8B8G8R8: dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM; break;
      case D3D9Format::X8B8G8R8: dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM; break; /* No RGBX in DXGI */
      case D3D9Format::A8R8G8B8: dxgiFormat = DXGI_FORMAT_B8G8R8A8_UNORM; break;
      case D3D9Format::X8R8G8B8: dxgiFormat = DXGI_FORMAT_B8G8R8X8_UNORM; break;
      default:
        Logger::warn(str::format("D3D9: Unsupported format for shared textures: ", m_desc.Format));
        return;
    }

    if (m_desc.Depth == 1 && m_desc.MipLevels == 1 && m_desc.MultiSample == D3DMULTISAMPLE_NONE &&
        m_desc.Usage & D3DUSAGE_RENDERTARGET && dxgiFormat != DXGI_FORMAT_UNKNOWN) {
      HANDLE ntHandle = openKmtHandle(m_image->sharedHandle());

      DxvkSharedTextureMetadata metadata;

      metadata.Width              = m_desc.Width;
      metadata.Height             = m_desc.Height;
      metadata.MipLevels          = m_desc.MipLevels;
      metadata.ArraySize          = m_desc.ArraySize;
      metadata.Format             = dxgiFormat;
      metadata.SampleDesc.Count   = 1;
      metadata.SampleDesc.Quality = 0;
      metadata.Usage              = D3D11_USAGE_DEFAULT;
      metadata.BindFlags          = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
      metadata.CPUAccessFlags     = 0;
      metadata.MiscFlags          = D3D11_RESOURCE_MISC_SHARED;
      metadata.TextureLayout      = D3D11_TEXTURE_LAYOUT_UNDEFINED;

      if (ntHandle == INVALID_HANDLE_VALUE || !setSharedMetadata(ntHandle, &metadata, sizeof(metadata)))
        Logger::warn("D3D9: Failed to write shared resource info for a texture");

      if (ntHandle != INVALID_HANDLE_VALUE)
        ::CloseHandle(ntHandle);
    }
  }


  Rc<DxvkImageView> D3D9CommonTexture::CreateView(
          UINT                   Layer,
          UINT                   Lod,
          VkImageUsageFlags      UsageFlags,
          bool                   Srgb) {    
    DxvkImageViewCreateInfo viewInfo;
    viewInfo.format    = m_mapping.ConversionFormatInfo.FormatColor != VK_FORMAT_UNDEFINED
                       ? PickSRGB(m_mapping.ConversionFormatInfo.FormatColor, m_mapping.ConversionFormatInfo.FormatSrgb, Srgb)
                       : PickSRGB(m_mapping.FormatColor, m_mapping.FormatSrgb, Srgb);
    viewInfo.aspect    = lookupFormatInfo(viewInfo.format)->aspectMask;
    viewInfo.swizzle   = m_mapping.Swizzle;
    viewInfo.usage     = UsageFlags;
    viewInfo.type      = GetImageViewTypeFromResourceType(m_type, Layer);
    viewInfo.minLevel  = Lod;
    viewInfo.numLevels = m_desc.MipLevels - Lod;
    viewInfo.minLayer  = Layer == AllLayers ? 0                : Layer;
    viewInfo.numLayers = Layer == AllLayers ? m_desc.ArraySize : 1;

    // Remove the stencil aspect if we are trying to create a regular image
    // view of a depth stencil format 
    if (UsageFlags != VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
      viewInfo.aspect &= ~VK_IMAGE_ASPECT_STENCIL_BIT;

    if (UsageFlags == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ||
        UsageFlags == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
      viewInfo.numLevels = 1;

    // Remove swizzle on depth views.
    if (UsageFlags == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
      viewInfo.swizzle = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                           VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };

    // Create the underlying image view object
    return m_device->GetDXVKDevice()->createImageView(GetImage(), viewInfo);
  }


  void D3D9CommonTexture::PreLoadAll() {
    if (!IsManaged())
      return;

    auto lock = m_device->LockDevice();
    m_device->UploadManagedTexture(this);
    m_device->MarkTextureUploaded(this);
  }


  void D3D9CommonTexture::PreLoadSubresource(UINT Subresource) {
    if (IsManaged()) {
      auto lock = m_device->LockDevice();

      if (NeedsUpload(Subresource)) {
        m_device->FlushImage(this, Subresource);
        SetNeedsUpload(Subresource, false);

        if (!NeedsAnyUpload())
          m_device->MarkTextureUploaded(this);
      }
    }
  }


  void D3D9CommonTexture::CreateSampleView(UINT Lod) {
    // This will be a no-op for SYSTEMMEM types given we
    // don't expose the cap to allow texturing with them.
    if (unlikely(m_mapMode == D3D9_COMMON_TEXTURE_MAP_MODE_SYSTEMMEM))
      return;

    m_sampleView.Color = CreateView(AllLayers, Lod, VK_IMAGE_USAGE_SAMPLED_BIT, false);

    if (IsSrgbCompatible())
      m_sampleView.Srgb = CreateView(AllLayers, Lod, VK_IMAGE_USAGE_SAMPLED_BIT, true);
  }


  const Rc<DxvkBuffer>& D3D9CommonTexture::GetBuffer() {
    return m_buffer;
  }


  DxvkBufferSlice D3D9CommonTexture::GetBufferSlice(UINT Subresource) {
    return DxvkBufferSlice(GetBuffer(), m_memoryOffset[Subresource], GetMipSize(Subresource));
  }

  
  uint32_t D3D9CommonTexture::GetPlaneCount() const {
    const DxvkFormatInfo* formatInfo = m_mapping.FormatColor != VK_FORMAT_UNDEFINED
      ? lookupFormatInfo(m_mapping.FormatColor)
      : m_device->UnsupportedFormatInfo(m_desc.Format);

    return vk::getPlaneCount(formatInfo->aspectMask);
  }

}
