#include "d3d9_common_texture.h"

#include "d3d9_util.h"
#include "d3d9_device.h"

#include <algorithm>

namespace dxvk {

  D3D9CommonTexture::D3D9CommonTexture(
          D3D9DeviceEx*             pDevice,
    const D3D9_COMMON_TEXTURE_DESC* pDesc,
          D3DRESOURCETYPE           ResourceType)
    : m_device(pDevice), m_desc(*pDesc), m_type(ResourceType) {
    if (m_desc.Format == D3D9Format::Unknown)
      m_desc.Format = (m_desc.Usage & D3DUSAGE_DEPTHSTENCIL)
                    ? D3D9Format::D32
                    : D3D9Format::X8R8G8B8;

    for (uint32_t i = 0; i < m_dirtyBoxes.size(); i++) {
      AddDirtyBox(nullptr, i);
    }

    if (m_desc.Pool != D3DPOOL_DEFAULT) {
      const uint32_t subresources = CountSubresources();
      for (uint32_t i = 0; i < subresources; i++) {
        SetNeedsUpload(i, true);
      }
    }

    m_mapping = pDevice->LookupFormat(m_desc.Format);

    m_mapMode = DetermineMapMode();
    m_shadow  = DetermineShadowState();

    if (m_mapMode == D3D9_COMMON_TEXTURE_MAP_MODE_BACKED) {
      bool plainSurface = m_type == D3DRTYPE_SURFACE &&
                          !(m_desc.Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL));

      try {
        m_image = CreatePrimaryImage(ResourceType, plainSurface);
      }
      catch (const DxvkError& e) {
        // D3DUSAGE_AUTOGENMIPMAP and offscreen plain is mutually exclusive
        // so we can combine their retry this way.
        if (m_desc.Usage & D3DUSAGE_AUTOGENMIPMAP || plainSurface) {
          m_desc.Usage &= ~D3DUSAGE_AUTOGENMIPMAP;
          m_desc.MipLevels = 1;
          m_image = CreatePrimaryImage(ResourceType, false);
        }
        else
          throw e;
      }

      CreateSampleView(0);

      if (!IsManaged()) {
        m_size = m_image->memSize();
        if (!m_device->ChangeReportedMemory(-m_size))
          throw DxvkError("D3D9: Reporting out of memory from tracking.");
      }
    }

    if (m_mapMode == D3D9_COMMON_TEXTURE_MAP_MODE_SYSTEMMEM)
      CreateBuffers();

    m_exposedMipLevels = m_desc.MipLevels;

    if (m_desc.Usage & D3DUSAGE_AUTOGENMIPMAP)
      m_exposedMipLevels = 1;
  }


  D3D9CommonTexture::~D3D9CommonTexture() {
    if (m_size != 0)
      m_device->ChangeReportedMemory(m_size);
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
    
    if (FAILED(DecodeMultiSampleType(pDesc->MultiSample, pDesc->MultisampleQuality, nullptr)))
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


  bool D3D9CommonTexture::CreateBufferSubresource(UINT Subresource) {
    if (m_buffers[Subresource] != nullptr)
      return false;

    DxvkBufferCreateInfo info;
    info.size   = GetMipSize(Subresource);
    info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT
                | VK_ACCESS_TRANSFER_WRITE_BIT;

    if (m_mapping.ConversionFormatInfo.FormatType != D3D9ConversionFormat_None) {
      info.usage  |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
      info.stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }

    VkMemoryPropertyFlags memType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                  | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    m_buffers[Subresource] = m_device->GetDXVKDevice()->createBuffer(info, memType);
    m_mappedSlices[Subresource] = m_buffers[Subresource]->getSliceHandle();

    return true;
  }


  VkDeviceSize D3D9CommonTexture::GetMipSize(UINT Subresource) const {
    const UINT MipLevel = Subresource % m_desc.MipLevels;

    const DxvkFormatInfo* formatInfo = m_mapping.FormatColor != VK_FORMAT_UNDEFINED
      ? imageFormatInfo(m_mapping.FormatColor)
      : m_device->UnsupportedFormatInfo(m_desc.Format);

    const VkExtent3D mipExtent = util::computeMipLevelExtent(
      GetExtent(), MipLevel);
    
    const VkExtent3D blockCount = util::computeBlockCount(
      mipExtent, formatInfo->blockSize);

    const uint32_t planeCount = m_mapping.ConversionFormatInfo.PlaneCount;

    return std::min(planeCount, 2u)
         * align(formatInfo->elementSize * blockCount.width, 4)
         * blockCount.height
         * blockCount.depth;
  }


  Rc<DxvkImage> D3D9CommonTexture::CreatePrimaryImage(D3DRESOURCETYPE ResourceType, bool TryOffscreenRT) const {
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

    if (m_mapping.ConversionFormatInfo.FormatType != D3D9ConversionFormat_None) {
      imageInfo.usage  |= VK_IMAGE_USAGE_STORAGE_BIT;
      imageInfo.stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }

    DecodeMultiSampleType(m_desc.MultiSample, m_desc.MultisampleQuality, &imageInfo.sampleCount);

    // The image must be marked as mutable if it can be reinterpreted
    // by a view with a different format. Depth-stencil formats cannot
    // be reinterpreted in Vulkan, so we'll ignore those.
    auto formatProperties = imageFormatInfo(m_mapping.FormatColor);

    bool isMutable     = m_mapping.FormatSrgb != VK_FORMAT_UNDEFINED;
    bool isColorFormat = (formatProperties->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0;

    if (isMutable && isColorFormat) {
      imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

      imageInfo.viewFormatCount = 2;
      imageInfo.viewFormats     = m_mapping.Formats;
    }

    // Are we an RT, need to gen mips or an offscreen plain surface?
    if (m_desc.Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_AUTOGENMIPMAP) || TryOffscreenRT) {
      imageInfo.usage  |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      imageInfo.stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      imageInfo.access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                       |  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    if (m_desc.Usage & D3DUSAGE_DEPTHSTENCIL) {
      imageInfo.usage  |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      imageInfo.stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                       |  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      imageInfo.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                       |  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    if (ResourceType == D3DRTYPE_CUBETEXTURE)
      imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    // Some image formats (i.e. the R32G32B32 ones) are
    // only supported with linear tiling on most GPUs
    if (!CheckImageSupport(&imageInfo, VK_IMAGE_TILING_OPTIMAL))
      imageInfo.tiling = VK_IMAGE_TILING_LINEAR;

    // We must keep LINEAR images in GENERAL layout, but we
    // can choose a better layout for the image based on how
    // it is going to be used by the game.
    if (imageInfo.tiling == VK_IMAGE_TILING_OPTIMAL)
      imageInfo.layout = OptimizeLayout(imageInfo.usage);

    // For some formats, we need to enable render target
    // capabilities if available, but these should
    // in no way affect the default image layout
    imageInfo.usage |= EnableMetaCopyUsage(imageInfo.format, imageInfo.tiling);

    // Check if we can actually create the image
    if (!CheckImageSupport(&imageInfo, imageInfo.tiling)) {
      throw DxvkError(str::format(
        "D3D9: Cannot create texture:",
        "\n  Type:    ", std::hex, ResourceType,
        "\n  Format:  ", m_desc.Format,
        "\n  Extent:  ", m_desc.Width,
                    "x", m_desc.Height,
                    "x", m_desc.Depth,
        "\n  Samples: ", m_desc.MultiSample,
        "\n  Layers:  ", m_desc.ArraySize,
        "\n  Levels:  ", m_desc.MipLevels,
        "\n  Usage:   ", std::hex, m_desc.Usage,
        "\n  Pool:    ", std::hex, m_desc.Pool));
    }

    return m_device->GetDXVKDevice()->createImage(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }


  Rc<DxvkImage> D3D9CommonTexture::CreateResolveImage() const {
    DxvkImageCreateInfo imageInfo = m_image->info();
    imageInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;

    return m_device->GetDXVKDevice()->createImage(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }


  BOOL D3D9CommonTexture::DetermineShadowState() const {
    static std::array<D3D9Format, 3> blacklist = {
      D3D9Format::INTZ, D3D9Format::DF16, D3D9Format::DF24
    };

    return IsDepthFormat(m_desc.Format)
        && std::find(blacklist.begin(), blacklist.end(), m_desc.Format) == blacklist.end();
  }


  BOOL D3D9CommonTexture::CheckImageSupport(
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


  VkImageUsageFlags D3D9CommonTexture::EnableMetaCopyUsage(
          VkFormat              Format,
          VkImageTiling         Tiling) const {
    VkFormatFeatureFlags requestedFeatures = 0;

    if (Format == VK_FORMAT_D16_UNORM || Format == VK_FORMAT_D32_SFLOAT)
      requestedFeatures |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (Format == VK_FORMAT_R16_UNORM || Format == VK_FORMAT_R32_SFLOAT)
      requestedFeatures |=  VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;

    if (requestedFeatures == 0)
      return 0;

    // Enable usage flags for all supported and requested features
    VkFormatProperties properties = m_device->GetDXVKDevice()->adapter()->formatProperties(Format);

    requestedFeatures &= Tiling == VK_IMAGE_TILING_OPTIMAL
      ? properties.optimalTilingFeatures
      : properties.linearTilingFeatures;
    
    VkImageUsageFlags requestedUsage = 0;
    
    if (requestedFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
      requestedUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    
    if (requestedFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
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
    Usage &= ~(VK_IMAGE_USAGE_TRANSFER_DST_BIT
             | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    
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


  Rc<DxvkImageView> D3D9CommonTexture::CreateView(
          UINT                   Layer,
          UINT                   Lod,
          VkImageUsageFlags      UsageFlags,
          bool                   Srgb) {
    DxvkImageViewCreateInfo viewInfo;
    viewInfo.format    = m_mapping.ConversionFormatInfo.FormatColor != VK_FORMAT_UNDEFINED
                       ? PickSRGB(m_mapping.ConversionFormatInfo.FormatColor, m_mapping.ConversionFormatInfo.FormatSrgb, Srgb)
                       : PickSRGB(m_mapping.FormatColor, m_mapping.FormatSrgb, Srgb);
    viewInfo.aspect    = imageFormatInfo(viewInfo.format)->aspectMask;
    viewInfo.swizzle   = m_mapping.Swizzle;
    viewInfo.usage     = UsageFlags;
    viewInfo.type      = GetImageViewTypeFromResourceType(m_type, Layer);
    viewInfo.minLevel  = Lod;
    viewInfo.numLevels  = m_desc.MipLevels - Lod;
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


}