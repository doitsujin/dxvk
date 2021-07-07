#include "d3d11_device.h"
#include "d3d11_gdi.h"
#include "d3d11_texture.h"

namespace dxvk {
  
  D3D11CommonTexture::D3D11CommonTexture(
          D3D11Device*                pDevice,
    const D3D11_COMMON_TEXTURE_DESC*  pDesc,
          D3D11_RESOURCE_DIMENSION    Dimension,
          DXGI_USAGE                  DxgiUsage,
          VkImage                     vkImage)
  : m_device(pDevice), m_dimension(Dimension), m_desc(*pDesc), m_dxgiUsage(DxgiUsage) {
    DXGI_VK_FORMAT_MODE   formatMode   = GetFormatMode();
    DXGI_VK_FORMAT_INFO   formatInfo   = m_device->LookupFormat(m_desc.Format, formatMode);
    DXGI_VK_FORMAT_FAMILY formatFamily = m_device->LookupFamily(m_desc.Format, formatMode);
    DXGI_VK_FORMAT_INFO   formatPacked = m_device->LookupPackedFormat(m_desc.Format, formatMode);
    m_packedFormat = formatPacked.Format;

    DxvkImageCreateInfo imageInfo;
    imageInfo.type            = GetVkImageType();
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
    imageInfo.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.shared          = vkImage != VK_NULL_HANDLE;

    if (!pDevice->GetOptions()->disableMsaa)
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
    bool isMultiPlane = (formatProperties->aspectMask & VK_IMAGE_ASPECT_PLANE_0_BIT) != 0;
    bool isColorFormat = (formatProperties->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0;

    if (isMutable && (isColorFormat || isMultiPlane)) {
      imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

      // Typeless UAV images have relaxed reinterpretation rules
      if (!isTypeless || !(m_desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)) {
        imageInfo.viewFormatCount = formatFamily.FormatCount;
        imageInfo.viewFormats     = formatFamily.Formats;
      }
    }

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
    
    if (m_desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
      imageInfo.usage  |= VK_IMAGE_USAGE_STORAGE_BIT;
      imageInfo.stages |= pDevice->GetEnabledShaderStages();
      imageInfo.access |= VK_ACCESS_SHADER_READ_BIT
                       |  VK_ACCESS_SHADER_WRITE_BIT;

      // UAVs are not supported for sRGB formats on most drivers,
      // but we can still create linear views for the image
      if (formatProperties->flags.test(DxvkFormatFlag::ColorSpaceSrgb))
        imageInfo.flags |= VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
    }

    // Multi-plane formats need views to be created with color formats, and
    // may not report all relevant usage flags as supported on their own.
    // Also, enable sampled bit to enable use with video processor APIs.
    if (isMultiPlane) {
      imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
      imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT
                      |  VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
    }
    
    // Access pattern for meta-resolve operations
    if (imageInfo.sampleCount != VK_SAMPLE_COUNT_1_BIT && isColorFormat) {
      imageInfo.usage  |= VK_IMAGE_USAGE_SAMPLED_BIT;
      imageInfo.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      imageInfo.access |= VK_ACCESS_SHADER_READ_BIT;
    }
    
    if (m_desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
      imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    
    if (Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE3D &&
        (m_desc.BindFlags & D3D11_BIND_RENDER_TARGET))
      imageInfo.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    
    // Swap chain back buffers need to be shader readable
    if (DxgiUsage & DXGI_USAGE_BACK_BUFFER) {
      imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
      imageInfo.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      imageInfo.access |= VK_ACCESS_SHADER_READ_BIT;
      imageInfo.shared = VK_TRUE;
    }

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
      imageInfo.tiling        = VK_IMAGE_TILING_LINEAR;
      imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    }
    
    // If necessary, create the mapped linear buffer
    if (m_mapMode != D3D11_COMMON_TEXTURE_MAP_MODE_NONE) {
      for (uint32_t i = 0; i < m_desc.ArraySize; i++) {
        for (uint32_t j = 0; j < m_desc.MipLevels; j++) {
          if (m_mapMode != D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT)
            m_buffers.push_back(CreateMappedBuffer(j));

          m_mapTypes.push_back(D3D11_MAP(~0u));
        }
      }
    }

    // Skip image creation if possible
    if (m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_STAGING)
      return;

    // We must keep LINEAR images in GENERAL layout, but we
    // can choose a better layout for the image based on how
    // it is going to be used by the game.
    if (imageInfo.tiling == VK_IMAGE_TILING_OPTIMAL && !isMultiPlane)
      imageInfo.layout = OptimizeLayout(imageInfo.usage);

    // For some formats, we need to enable sampled and/or
    // render target capabilities if available, but these
    // should in no way affect the default image layout
    imageInfo.usage |= EnableMetaCopyUsage(imageInfo.format, imageInfo.tiling);
    imageInfo.usage |= EnableMetaPackUsage(imageInfo.format, m_desc.CPUAccessFlags);
    
    // Check if we can actually create the image
    if (!CheckImageSupport(&imageInfo, imageInfo.tiling)) {
      throw DxvkError(str::format(
        "D3D11: Cannot create texture:",
        "\n  Format:  ", m_desc.Format,
        "\n  Extent:  ", m_desc.Width,
                    "x", m_desc.Height,
                    "x", m_desc.Depth,
        "\n  Samples: ", m_desc.SampleDesc.Count,
        "\n  Layers:  ", m_desc.ArraySize,
        "\n  Levels:  ", m_desc.MipLevels,
        "\n  Usage:   ", std::hex, m_desc.BindFlags,
        "\n  Flags:   ", std::hex, m_desc.MiscFlags));
    }
    
    // Create the image on a host-visible memory type
    // in case it is going to be mapped directly.
    VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    
    if (m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT) {
      memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                       | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                       | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }
    
    if (vkImage == VK_NULL_HANDLE)
      m_image = m_device->GetDXVKDevice()->createImage(imageInfo, memoryProperties);
    else
      m_image = m_device->GetDXVKDevice()->createImageFromVkImage(imageInfo, vkImage);
  }
  
  
  D3D11CommonTexture::~D3D11CommonTexture() {
    
  }
  
  
  VkDeviceSize D3D11CommonTexture::ComputeMappedOffset(UINT Subresource, UINT Plane, VkOffset3D Offset) const {
    auto packedFormatInfo = imageFormatInfo(m_packedFormat);

    VkImageAspectFlags aspectMask = packedFormatInfo->aspectMask;
    VkDeviceSize elementSize = packedFormatInfo->elementSize;

    if (packedFormatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
      auto plane = &packedFormatInfo->planes[Plane];
      elementSize = plane->elementSize;
      Offset.x /= plane->blockSize.width;
      Offset.y /= plane->blockSize.height;
      aspectMask = vk::getPlaneAspect(Plane);
    }

    auto layout = GetSubresourceLayout(aspectMask, Subresource);
    auto blockOffset = util::computeBlockOffset(Offset, packedFormatInfo->blockSize);

    return VkDeviceSize(blockOffset.z) * layout.DepthPitch
         + VkDeviceSize(blockOffset.y) * layout.RowPitch
         + VkDeviceSize(blockOffset.x) * elementSize
         + VkDeviceSize(layout.Offset);
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
  
  
  D3D11_COMMON_TEXTURE_SUBRESOURCE_LAYOUT D3D11CommonTexture::GetSubresourceLayout(
          VkImageAspectFlags    AspectMask,
          UINT                  Subresource) const {
    VkImageSubresource subresource = GetSubresourceFromIndex(AspectMask, Subresource);
    D3D11_COMMON_TEXTURE_SUBRESOURCE_LAYOUT layout = { };

    switch (m_mapMode) {
      case D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT: {
        auto vkLayout = m_image->querySubresourceLayout(subresource);
        layout.Offset     = vkLayout.offset;
        layout.Size       = vkLayout.size;
        layout.RowPitch   = vkLayout.rowPitch;
        layout.DepthPitch = vkLayout.depthPitch;
      } break;

      case D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER:
      case D3D11_COMMON_TEXTURE_MAP_MODE_STAGING: {
        auto packedFormatInfo = imageFormatInfo(m_packedFormat);

        VkImageAspectFlags aspects = packedFormatInfo->aspectMask;
        VkExtent3D mipExtent = MipLevelExtent(subresource.mipLevel);

        while (aspects) {
          auto aspect = vk::getNextAspect(aspects);
          auto extent = mipExtent;
          auto elementSize = packedFormatInfo->elementSize;

          if (packedFormatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
            auto plane = &packedFormatInfo->planes[vk::getPlaneIndex(aspect)];
            extent.width  /= plane->blockSize.width;
            extent.height /= plane->blockSize.height;
            elementSize = plane->elementSize;
          }

          auto blockCount = util::computeBlockCount(extent, packedFormatInfo->blockSize);

          if (!layout.RowPitch) {
            layout.RowPitch   = elementSize * blockCount.width;
            layout.DepthPitch = elementSize * blockCount.width * blockCount.height;
          }

          VkDeviceSize size = elementSize * blockCount.width * blockCount.height * blockCount.depth;

          if (aspect & AspectMask)
            layout.Size += size;
          else if (!layout.Size)
            layout.Offset += size;
        }
      } break;

      case D3D11_COMMON_TEXTURE_MAP_MODE_NONE:
        break; /* no op */
    }

    // D3D wants us to return the total subresource size in some instances
    if (m_dimension < D3D11_RESOURCE_DIMENSION_TEXTURE2D) layout.RowPitch = layout.Size;
    if (m_dimension < D3D11_RESOURCE_DIMENSION_TEXTURE3D) layout.DepthPitch = layout.Size;
    return layout;
  }


  DXGI_VK_FORMAT_MODE D3D11CommonTexture::GetFormatMode() const {
    if (m_desc.BindFlags & D3D11_BIND_RENDER_TARGET)
      return DXGI_VK_FORMAT_MODE_COLOR;
    
    if (m_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
      return DXGI_VK_FORMAT_MODE_DEPTH;
    
    return DXGI_VK_FORMAT_MODE_ANY;
  }
  
  
  uint32_t D3D11CommonTexture::GetPlaneCount() const {
    return vk::getPlaneCount(m_image->formatInfo()->aspectMask);
  }


  bool D3D11CommonTexture::CheckViewCompatibility(UINT BindFlags, DXGI_FORMAT Format, UINT Plane) const {
    const DxvkImageCreateInfo& imageInfo = m_image->info();

    // Check whether the given bind flags are supported
    if ((m_desc.BindFlags & BindFlags) != BindFlags)
      return false;

    // Check whether the view format is compatible
    DXGI_VK_FORMAT_MODE formatMode = GetFormatMode();
    DXGI_VK_FORMAT_INFO viewFormat = m_device->LookupFormat(Format,        formatMode);
    DXGI_VK_FORMAT_INFO baseFormat = m_device->LookupFormat(m_desc.Format, formatMode);
    
    // Check whether the plane index is valid for the given format
    uint32_t planeCount = GetPlaneCount();

    if (Plane >= planeCount)
      return false;

    if (imageInfo.flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) {
      // Check whether the given combination of image
      // view type and view format is actually supported
      VkFormatFeatureFlags features = GetImageFormatFeatures(BindFlags);
      
      if (!CheckFormatFeatureSupport(viewFormat.Format, features))
        return false;

      // Using the image format itself is supported for non-planar formats
      if (viewFormat.Format == baseFormat.Format && planeCount == 1)
        return true;
      
      // If there is a list of compatible formats, the view format must be
      // included in that list. For planar formats, the list is laid out in
      // such a way that the n-th format is supported for the n-th plane. 
      for (size_t i = Plane; i < imageInfo.viewFormatCount; i += planeCount) {
        if (imageInfo.viewFormats[i] == viewFormat.Format) {
          return true;
        }
      }

      // Otherwise, all bit-compatible formats can be used.
      if (imageInfo.viewFormatCount == 0 && planeCount == 1) {
        auto baseFormatInfo = imageFormatInfo(baseFormat.Format);
        auto viewFormatInfo = imageFormatInfo(viewFormat.Format);
        
        return baseFormatInfo->aspectMask  == viewFormatInfo->aspectMask
            && baseFormatInfo->elementSize == viewFormatInfo->elementSize;
      }

      return false;
    } else {
      // For non-mutable images, the view format
      // must be identical to the image format.
      return viewFormat.Format == baseFormat.Format && planeCount == 1;
    }
  }
  
  
  HRESULT D3D11CommonTexture::NormalizeTextureProperties(D3D11_COMMON_TEXTURE_DESC* pDesc) {
    if (pDesc->Width == 0 || pDesc->Height == 0 || pDesc->Depth == 0 || pDesc->ArraySize == 0)
      return E_INVALIDARG;
    
    if (FAILED(DecodeSampleCount(pDesc->SampleDesc.Count, nullptr)))
      return E_INVALIDARG;
    
    if ((pDesc->MiscFlags & D3D11_RESOURCE_MISC_GDI_COMPATIBLE)
     && (pDesc->Usage == D3D11_USAGE_STAGING
      || (pDesc->Format != DXGI_FORMAT_B8G8R8A8_TYPELESS
       && pDesc->Format != DXGI_FORMAT_B8G8R8A8_UNORM
       && pDesc->Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)))
      return E_INVALIDARG;

    if ((pDesc->MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS)
     && (pDesc->BindFlags & (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET))
                         != (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET))
      return E_INVALIDARG;

    // TILE_POOL is invalid, but we don't support TILED either
    if (pDesc->MiscFlags & (D3D11_RESOURCE_MISC_TILE_POOL | D3D11_RESOURCE_MISC_TILED))
      return E_INVALIDARG;

    // Use the maximum possible mip level count if the supplied
    // mip level count is either unspecified (0) or invalid
    const uint32_t maxMipLevelCount = pDesc->SampleDesc.Count <= 1
      ? util::computeMipLevelCount({ pDesc->Width, pDesc->Height, pDesc->Depth })
      : 1u;
    
    if (pDesc->MipLevels == 0 || pDesc->MipLevels > maxMipLevelCount)
      pDesc->MipLevels = maxMipLevelCount;
    
    // Row-major is only supported for textures with one single
    // subresource and one sample and cannot have bind flags.
    if (pDesc->TextureLayout == D3D11_TEXTURE_LAYOUT_ROW_MAJOR
     && (pDesc->MipLevels != 1 || pDesc->SampleDesc.Count != 1 || pDesc->BindFlags))
      return E_INVALIDARG;

    // Standard swizzle is unsupported
    if (pDesc->TextureLayout == D3D11_TEXTURE_LAYOUT_64K_STANDARD_SWIZZLE)
      return E_INVALIDARG;

    return S_OK;
  }
  
  
  BOOL D3D11CommonTexture::CheckImageSupport(
    const DxvkImageCreateInfo*  pImageInfo,
          VkImageTiling         Tiling) const {
    const Rc<DxvkAdapter> adapter = m_device->GetDXVKDevice()->adapter();
    
    VkImageUsageFlags usage = pImageInfo->usage;

    if (pImageInfo->flags & VK_IMAGE_CREATE_EXTENDED_USAGE_BIT)
      usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkImageFormatProperties formatProps = { };
    VkResult status = adapter->imageFormatProperties(
      pImageInfo->format, pImageInfo->type, Tiling,
      usage, pImageInfo->flags, formatProps);
    
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
  
  
  VkImageUsageFlags D3D11CommonTexture::EnableMetaCopyUsage(
          VkFormat              Format,
          VkImageTiling         Tiling) const {
    VkFormatFeatureFlags requestedFeatures = 0;

    if (Format == VK_FORMAT_D16_UNORM || Format == VK_FORMAT_D32_SFLOAT) {
      requestedFeatures |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
                        |  VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if (Format == VK_FORMAT_R16_UNORM || Format == VK_FORMAT_R32_SFLOAT) {
      requestedFeatures |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
                        |  VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    }

    if (Format == VK_FORMAT_D32_SFLOAT_S8_UINT || Format == VK_FORMAT_D24_UNORM_S8_UINT)
      requestedFeatures |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (requestedFeatures == 0)
      return 0;

    // Enable usage flags for all supported and requested features
    VkFormatProperties properties = m_device->GetDXVKDevice()->adapter()->formatProperties(Format);

    requestedFeatures &= Tiling == VK_IMAGE_TILING_OPTIMAL
      ? properties.optimalTilingFeatures
      : properties.linearTilingFeatures;
    
    VkImageUsageFlags requestedUsage = 0;

    if (requestedFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
      requestedUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    
    if (requestedFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
      requestedUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    
    if (requestedFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
      requestedUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    return requestedUsage;
  }


  VkImageUsageFlags D3D11CommonTexture::EnableMetaPackUsage(
          VkFormat              Format,
          UINT                  CpuAccess) const {
    if ((CpuAccess & D3D11_CPU_ACCESS_READ) == 0)
      return 0;
    
    const auto dsMask = VK_IMAGE_ASPECT_DEPTH_BIT
                      | VK_IMAGE_ASPECT_STENCIL_BIT;

    auto formatInfo = imageFormatInfo(Format);

    return formatInfo->aspectMask == dsMask
      ? VK_IMAGE_USAGE_SAMPLED_BIT
      : 0;
  }

  
  D3D11_COMMON_TEXTURE_MAP_MODE D3D11CommonTexture::DetermineMapMode(
    const DxvkImageCreateInfo*  pImageInfo) const {
    // Don't map an image unless the application requests it
    if (!m_desc.CPUAccessFlags)
      return D3D11_COMMON_TEXTURE_MAP_MODE_NONE;
    
    // If the resource cannot be used in the actual rendering pipeline, we
    // do not need to create an actual image and can instead implement copy
    // functions as buffer-to-image and image-to-buffer copies.
    if (!m_desc.BindFlags && m_desc.Usage != D3D11_USAGE_DEFAULT)
      return D3D11_COMMON_TEXTURE_MAP_MODE_STAGING;

    // Write-only images should go through a buffer for multiple reasons:
    // 1. Some games do not respect the row and depth pitch that is returned
    //    by the Map() method, which leads to incorrect rendering (e.g. Nier)
    // 2. Since the image will most likely be read for rendering by the GPU,
    //    writing the image to device-local image may be more efficient than
    //    reading its contents from host memory.
    if (m_desc.Usage         == D3D11_USAGE_DYNAMIC
     && m_desc.TextureLayout != D3D11_TEXTURE_LAYOUT_ROW_MAJOR)
      return D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER;
    
    // Depth-stencil formats in D3D11 can be mapped and follow special
    // packing rules, so we need to copy that data into a buffer first
    if (GetPackedDepthStencilFormat(m_desc.Format))
      return D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER;

    // Multi-plane images have a special memory layout in D3D11
    if (imageFormatInfo(pImageInfo->format)->flags.test(DxvkFormatFlag::MultiPlane))
      return D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER;

    // Images that can be read by the host should be mapped directly in
    // order to avoid expensive synchronization with the GPU. This does
    // however require linear tiling, which may not be supported for all
    // combinations of image parameters.
    return this->CheckImageSupport(pImageInfo, VK_IMAGE_TILING_LINEAR)
      ? D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT
      : D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER;
  }
  
  
  D3D11CommonTexture::MappedBuffer D3D11CommonTexture::CreateMappedBuffer(UINT MipLevel) const {
    const DxvkFormatInfo* formatInfo = imageFormatInfo(
      m_device->LookupPackedFormat(m_desc.Format, GetFormatMode()).Format);
    
    DxvkBufferCreateInfo info;
    info.size   = GetSubresourceLayout(formatInfo->aspectMask, MipLevel).Size;
    info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
                | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
                | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT
                | VK_ACCESS_TRANSFER_WRITE_BIT
                | VK_ACCESS_SHADER_READ_BIT
                | VK_ACCESS_SHADER_WRITE_BIT;
    
    VkMemoryPropertyFlags memType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    if (m_desc.Usage == D3D11_USAGE_STAGING)
      memType |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    
    MappedBuffer result;
    result.buffer = m_device->GetDXVKDevice()->createBuffer(info, memType);
    result.slice = result.buffer->getSliceHandle();
    return result;
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
  
  


  D3D11DXGISurface::D3D11DXGISurface(
          ID3D11Resource*     pResource,
          D3D11CommonTexture* pTexture)
  : m_resource  (pResource),
    m_texture   (pTexture),
    m_gdiSurface(nullptr) {
    if (pTexture->Desc()->MiscFlags & D3D11_RESOURCE_MISC_GDI_COMPATIBLE)
      m_gdiSurface = new D3D11GDISurface(m_resource, 0);
  }

  
  D3D11DXGISurface::~D3D11DXGISurface() {
    if (m_gdiSurface)
      delete m_gdiSurface;
  }

  
  ULONG STDMETHODCALLTYPE D3D11DXGISurface::AddRef() {
    return m_resource->AddRef();
  }

  
  ULONG STDMETHODCALLTYPE D3D11DXGISurface::Release() {
    return m_resource->Release();
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGISurface::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_resource->QueryInterface(riid, ppvObject);
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGISurface::GetPrivateData(
          REFGUID                 Name,
          UINT*                   pDataSize,
          void*                   pData) {
    return m_resource->GetPrivateData(Name, pDataSize, pData);
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGISurface::SetPrivateData(
          REFGUID                 Name,
          UINT                    DataSize,
    const void*                   pData) {
    return m_resource->SetPrivateData(Name, DataSize, pData);
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGISurface::SetPrivateDataInterface(
          REFGUID                 Name,
    const IUnknown*               pUnknown) {
    return m_resource->SetPrivateDataInterface(Name, pUnknown);
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGISurface::GetParent(
          REFIID                  riid,
          void**                  ppParent) {
    return GetDevice(riid, ppParent);
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGISurface::GetDevice(
          REFIID                  riid,
          void**                  ppDevice) {
    Com<ID3D11Device> device;
    m_resource->GetDevice(&device);
    return device->QueryInterface(riid, ppDevice);
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGISurface::GetDesc(
          DXGI_SURFACE_DESC*      pDesc) {
    if (!pDesc)
      return DXGI_ERROR_INVALID_CALL;

    auto desc = m_texture->Desc();
    pDesc->Width      = desc->Width;
    pDesc->Height     = desc->Height;
    pDesc->Format     = desc->Format;
    pDesc->SampleDesc = desc->SampleDesc;
    return S_OK;
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGISurface::Map(
            DXGI_MAPPED_RECT*       pLockedRect,
            UINT                    MapFlags) {
    Com<ID3D11Device>        device;
    Com<ID3D11DeviceContext> context;

    m_resource->GetDevice(&device);
    device->GetImmediateContext(&context);

    if (pLockedRect) {
      pLockedRect->Pitch = 0;
      pLockedRect->pBits = nullptr;
    }

    D3D11_MAP mapType;

    if (MapFlags & (DXGI_MAP_READ | DXGI_MAP_WRITE))
      mapType = D3D11_MAP_READ_WRITE;
    else if (MapFlags & DXGI_MAP_READ)
      mapType = D3D11_MAP_READ;
    else if (MapFlags & (DXGI_MAP_WRITE | DXGI_MAP_DISCARD))
      mapType = D3D11_MAP_WRITE_DISCARD;
    else if (MapFlags & DXGI_MAP_WRITE)
      mapType = D3D11_MAP_WRITE;
    else
      return DXGI_ERROR_INVALID_CALL;
    
    D3D11_MAPPED_SUBRESOURCE sr;
    HRESULT hr = context->Map(m_resource, 0,
      mapType, 0, pLockedRect ? &sr : nullptr);

    if (hr != S_OK)
      return hr;

    pLockedRect->Pitch = sr.RowPitch;
    pLockedRect->pBits = reinterpret_cast<unsigned char*>(sr.pData);
    return hr;
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGISurface::Unmap() {
    Com<ID3D11Device>        device;
    Com<ID3D11DeviceContext> context;

    m_resource->GetDevice(&device);
    device->GetImmediateContext(&context);
    
    context->Unmap(m_resource, 0);
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGISurface::GetDC(
          BOOL                    Discard,
          HDC*                    phdc) {
    if (!m_gdiSurface)
      return DXGI_ERROR_INVALID_CALL;
    
    return m_gdiSurface->Acquire(Discard, phdc);
  }


  HRESULT STDMETHODCALLTYPE D3D11DXGISurface::ReleaseDC(
          RECT*                   pDirtyRect) {
    if (!m_gdiSurface)
      return DXGI_ERROR_INVALID_CALL;

    return m_gdiSurface->Release(pDirtyRect);
  }

  
  HRESULT STDMETHODCALLTYPE D3D11DXGISurface::GetResource(
          REFIID                  riid,
          void**                  ppParentResource,
          UINT*                   pSubresourceIndex) {
    HRESULT hr = m_resource->QueryInterface(riid, ppParentResource);
    if (pSubresourceIndex)
      *pSubresourceIndex = 0;
    return hr;
  }
  
  
  bool D3D11DXGISurface::isSurfaceCompatible() const {
    auto desc = m_texture->Desc();

    return desc->ArraySize == 1
        && desc->MipLevels == 1;
  }




  D3D11VkInteropSurface::D3D11VkInteropSurface(
          ID3D11Resource*     pResource,
          D3D11CommonTexture* pTexture)
  : m_resource(pResource),
    m_texture (pTexture) {
      
  }
  
  
  D3D11VkInteropSurface::~D3D11VkInteropSurface() {
    
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11VkInteropSurface::AddRef() {
    return m_resource->AddRef();
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11VkInteropSurface::Release() {
    return m_resource->Release();
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11VkInteropSurface::QueryInterface(
          REFIID                  riid,
          void**                  ppvObject) {
    return m_resource->QueryInterface(riid, ppvObject);
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11VkInteropSurface::GetDevice(
          IDXGIVkInteropDevice**  ppDevice) {
    Com<ID3D11Device> device;
    m_resource->GetDevice(&device);
    
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
  : D3D11DeviceChild<ID3D11Texture1D>(pDevice),
    m_texture (pDevice, pDesc, D3D11_RESOURCE_DIMENSION_TEXTURE1D, 0, VK_NULL_HANDLE),
    m_interop (this, &m_texture),
    m_surface (this, &m_texture),
    m_resource(this),
    m_d3d10   (this) {
    
  }
  
  
  D3D11Texture1D::~D3D11Texture1D() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Texture1D::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

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
    
    if (m_surface.isSurfaceCompatible()
     && (riid == __uuidof(IDXGISurface)
      || riid == __uuidof(IDXGISurface1)
      || riid == __uuidof(IDXGISurface2))) {
      *ppvObject = ref(&m_surface);
      return S_OK;
    }

    if (riid == __uuidof(IDXGIObject)
     || riid == __uuidof(IDXGIDeviceSubObject)
     || riid == __uuidof(IDXGIResource)
     || riid == __uuidof(IDXGIResource1)) {
       *ppvObject = ref(&m_resource);
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
  
  
  void STDMETHODCALLTYPE D3D11Texture1D::GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension) {
    *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE1D;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Texture1D::GetEvictionPriority() {
    return DXGI_RESOURCE_PRIORITY_NORMAL;
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture1D::SetEvictionPriority(UINT EvictionPriority) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
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
  : D3D11DeviceChild<ID3D11Texture2D1>(pDevice),
    m_texture (pDevice, pDesc, D3D11_RESOURCE_DIMENSION_TEXTURE2D, 0, VK_NULL_HANDLE),
    m_interop (this, &m_texture),
    m_surface (this, &m_texture),
    m_resource(this),
    m_d3d10   (this) {
    
  }


  D3D11Texture2D::D3D11Texture2D(
          D3D11Device*                pDevice,
    const D3D11_COMMON_TEXTURE_DESC*  pDesc,
          DXGI_USAGE                  DxgiUsage,
          VkImage                     vkImage)
  : D3D11DeviceChild<ID3D11Texture2D1>(pDevice),
    m_texture (pDevice, pDesc, D3D11_RESOURCE_DIMENSION_TEXTURE2D, DxgiUsage, vkImage),
    m_interop (this, &m_texture),
    m_surface (this, &m_texture),
    m_resource(this),
    m_d3d10   (this) {
    
  }
  
  
  D3D11Texture2D::~D3D11Texture2D() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Texture2D::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11Resource)
     || riid == __uuidof(ID3D11Texture2D)
     || riid == __uuidof(ID3D11Texture2D1)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10Resource)
     || riid == __uuidof(ID3D10Texture2D)) {
      *ppvObject = ref(&m_d3d10);
      return S_OK;
    }

    if (m_surface.isSurfaceCompatible()
     && (riid == __uuidof(IDXGISurface)
      || riid == __uuidof(IDXGISurface1)
      || riid == __uuidof(IDXGISurface2))) {
      *ppvObject = ref(&m_surface);
      return S_OK;
    }
    
    if (riid == __uuidof(IDXGIObject)
     || riid == __uuidof(IDXGIDeviceSubObject)
     || riid == __uuidof(IDXGIResource)
     || riid == __uuidof(IDXGIResource1)) {
       *ppvObject = ref(&m_resource);
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
  
  
  void STDMETHODCALLTYPE D3D11Texture2D::GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension) {
    *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Texture2D::GetEvictionPriority() {
    return DXGI_RESOURCE_PRIORITY_NORMAL;
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture2D::SetEvictionPriority(UINT EvictionPriority) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11Texture2D::SetEvictionPriority: Stub");
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture2D::GetDesc(D3D11_TEXTURE2D_DESC* pDesc) {
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
  
  
  void STDMETHODCALLTYPE D3D11Texture2D::GetDesc1(D3D11_TEXTURE2D_DESC1* pDesc) {
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
    pDesc->TextureLayout  = m_texture.Desc()->TextureLayout;
  }
  
  
  ///////////////////////////////////////////
  //      D 3 D 1 1 T E X T U R E 3 D
  D3D11Texture3D::D3D11Texture3D(
          D3D11Device*                pDevice,
    const D3D11_COMMON_TEXTURE_DESC*  pDesc)
  : D3D11DeviceChild<ID3D11Texture3D1>(pDevice),
    m_texture (pDevice, pDesc, D3D11_RESOURCE_DIMENSION_TEXTURE3D, 0, VK_NULL_HANDLE),
    m_interop (this, &m_texture),
    m_resource(this),
    m_d3d10   (this) {
    
  }
  
  
  D3D11Texture3D::~D3D11Texture3D() {
    
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11Texture3D::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11Resource)
     || riid == __uuidof(ID3D11Texture3D)
     || riid == __uuidof(ID3D11Texture3D1)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10Resource)
     || riid == __uuidof(ID3D10Texture3D)) {
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
    
    if (riid == __uuidof(IDXGIVkInteropSurface)) {
      *ppvObject = ref(&m_interop);
      return S_OK;
    }
    
    Logger::warn("D3D11Texture3D::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture3D::GetType(D3D11_RESOURCE_DIMENSION *pResourceDimension) {
    *pResourceDimension = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
  }
  
  
  UINT STDMETHODCALLTYPE D3D11Texture3D::GetEvictionPriority() {
    return DXGI_RESOURCE_PRIORITY_NORMAL;
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture3D::SetEvictionPriority(UINT EvictionPriority) {
    static bool s_errorShown = false;

    if (!std::exchange(s_errorShown, true))
      Logger::warn("D3D11Texture3D::SetEvictionPriority: Stub");
  }
  
  
  void STDMETHODCALLTYPE D3D11Texture3D::GetDesc(D3D11_TEXTURE3D_DESC* pDesc) {
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
  
  
  void STDMETHODCALLTYPE D3D11Texture3D::GetDesc1(D3D11_TEXTURE3D_DESC1* pDesc) {
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
