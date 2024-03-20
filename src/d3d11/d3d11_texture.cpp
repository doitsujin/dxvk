#include "d3d11_device.h"
#include "d3d11_gdi.h"
#include "d3d11_texture.h"

#include "../util/util_shared_res.h"
#include "../util/util_win32_compat.h"

namespace dxvk {
  
  D3D11CommonTexture::D3D11CommonTexture(
          ID3D11Resource*             pInterface,
          D3D11Device*                pDevice,
    const D3D11_COMMON_TEXTURE_DESC*  pDesc,
    const D3D11_ON_12_RESOURCE_INFO*  p11on12Info,
          D3D11_RESOURCE_DIMENSION    Dimension,
          DXGI_USAGE                  DxgiUsage,
          VkImage                     vkImage,
          HANDLE                      hSharedHandle)
  : m_interface(pInterface), m_device(pDevice), m_dimension(Dimension), m_desc(*pDesc),
    m_11on12(p11on12Info ? *p11on12Info : D3D11_ON_12_RESOURCE_INFO()), m_dxgiUsage(DxgiUsage) {
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

    // Normalise hSharedhandle to INVALID_HANDLE_VALUE to allow passing in nullptr
    if (hSharedHandle == nullptr)
      hSharedHandle = INVALID_HANDLE_VALUE;

    const auto sharingFlags = D3D11_RESOURCE_MISC_SHARED|D3D11_RESOURCE_MISC_SHARED_NTHANDLE|D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    if (m_desc.MiscFlags & sharingFlags) {
      if (pDevice->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0 ||
          (m_desc.MiscFlags & (D3D11_RESOURCE_MISC_SHARED|D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX)) == (D3D11_RESOURCE_MISC_SHARED|D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) ||
          (m_desc.MiscFlags & sharingFlags) == D3D11_RESOURCE_MISC_SHARED_NTHANDLE)
        throw DxvkError(str::format("D3D11: Cannot create shared texture:",
          "\n  MiscFlags:  ", m_desc.MiscFlags,
          "\n  FeatureLevel:  ", pDevice->GetFeatureLevel()));

      if (m_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX)
        Logger::warn("D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX: not supported.");

      imageInfo.shared = true;
      imageInfo.sharing.mode = hSharedHandle == INVALID_HANDLE_VALUE ? DxvkSharedHandleMode::Export : DxvkSharedHandleMode::Import;
      imageInfo.sharing.type = (m_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE)
        ? VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
        : VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT;
      imageInfo.sharing.handle = hSharedHandle;
    }

    if (!pDevice->GetOptions()->disableMsaa)
      DecodeSampleCount(m_desc.SampleDesc.Count, &imageInfo.sampleCount);

    if ((m_desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) && IsR32UavCompatibleFormat(m_desc.Format)) {
      formatFamily.Add(formatInfo.Format);
      formatFamily.Add(VK_FORMAT_R32_SFLOAT);
      formatFamily.Add(VK_FORMAT_R32_UINT);
      formatFamily.Add(VK_FORMAT_R32_SINT);
    }

    // The image must be marked as mutable if it can be reinterpreted
    // by a view with a different format. Depth-stencil formats cannot
    // be reinterpreted in Vulkan, so we'll ignore those.
    auto formatProperties = lookupFormatInfo(formatInfo.Format);
    
    bool isMutable = formatFamily.FormatCount > 1;
    bool isMultiPlane = (formatProperties->aspectMask & VK_IMAGE_ASPECT_PLANE_0_BIT) != 0;
    bool isColorFormat = (formatProperties->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0;

    if (isMutable && (isColorFormat || isMultiPlane)) {
      imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
      imageInfo.viewFormatCount = formatFamily.FormatCount;
      imageInfo.viewFormats     = formatFamily.Formats;
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
    
    if (m_desc.MiscFlags & D3D11_RESOURCE_MISC_TILED) {
      imageInfo.flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT
                      |  VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT
                      |  VK_IMAGE_CREATE_SPARSE_ALIASED_BIT;
    }

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

      if (pDesc->Usage != D3D11_USAGE_DYNAMIC) {
        imageInfo.stages |= VK_PIPELINE_STAGE_HOST_BIT;
        imageInfo.access |= VK_ACCESS_HOST_READ_BIT;

        if (pDesc->CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)
          imageInfo.access |= VK_ACCESS_HOST_WRITE_BIT;
      }
    }
    
    // If necessary, create the mapped linear buffer
    if (m_mapMode != D3D11_COMMON_TEXTURE_MAP_MODE_NONE) {
      for (uint32_t i = 0; i < m_desc.ArraySize; i++) {
        for (uint32_t j = 0; j < m_desc.MipLevels; j++) {
          if (m_mapMode != D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT)
            m_buffers.push_back(CreateMappedBuffer(j));

          m_mapInfo.push_back({ D3D11_MAP(~0u), 0ull });
        }
      }
    }

    // Skip image creation if possible
    if (m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_STAGING)
      return;

    // We must keep LINEAR images in GENERAL layout, but we
    // can choose a better layout for the image based on how
    // it is going to be used by the game.
    if (imageInfo.tiling == VK_IMAGE_TILING_OPTIMAL && !isMultiPlane && imageInfo.sharing.mode == DxvkSharedHandleMode::None)
      imageInfo.layout = OptimizeLayout(imageInfo.usage);

    // For some formats, we need to enable sampled and/or
    // render target capabilities if available, but these
    // should in no way affect the default image layout
    imageInfo.usage |= EnableMetaPackUsage(imageInfo.format, m_desc.CPUAccessFlags);
    imageInfo.usage |= EnableMetaCopyUsage(imageInfo.format, imageInfo.tiling);

    for (uint32_t i = 0; i < imageInfo.viewFormatCount; i++)
      imageInfo.usage |= EnableMetaCopyUsage(imageInfo.viewFormats[i], imageInfo.tiling);

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
    
    if (m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT)
      memoryProperties = GetMemoryFlags();
    
    if (m_11on12.Resource != nullptr)
      vkImage = VkImage(m_11on12.VulkanHandle);

    if (!vkImage)
      m_image = m_device->GetDXVKDevice()->createImage(imageInfo, memoryProperties);
    else
      m_image = m_device->GetDXVKDevice()->importImage(imageInfo, vkImage, memoryProperties);

    if (imageInfo.sharing.mode == DxvkSharedHandleMode::Export)
      ExportImageInfo();
  }
  
  
  D3D11CommonTexture::~D3D11CommonTexture() {
    
  }
  
  
  VkDeviceSize D3D11CommonTexture::ComputeMappedOffset(UINT Subresource, UINT Plane, VkOffset3D Offset) const {
    auto packedFormatInfo = lookupFormatInfo(m_packedFormat);

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

      case D3D11_COMMON_TEXTURE_MAP_MODE_NONE:
      case D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER:
      case D3D11_COMMON_TEXTURE_MAP_MODE_STAGING: {
        auto packedFormatInfo = lookupFormatInfo(m_packedFormat);

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
      VkFormatFeatureFlags2 features = GetImageFormatFeatures(BindFlags);
      
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
        auto baseFormatInfo = lookupFormatInfo(baseFormat.Format);
        auto viewFormatInfo = lookupFormatInfo(viewFormat.Format);
        
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

    // TILE_POOL is invalid for textures
    if (pDesc->MiscFlags & D3D11_RESOURCE_MISC_TILE_POOL)
      return E_INVALIDARG;

    // Perform basic validation for tiled resources
    if (pDesc->MiscFlags & D3D11_RESOURCE_MISC_TILED) {
      UINT invalidFlags = D3D11_RESOURCE_MISC_SHARED
                        | D3D11_RESOURCE_MISC_SHARED_NTHANDLE
                        | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX
                        | D3D11_RESOURCE_MISC_GDI_COMPATIBLE;

      if ((pDesc->MiscFlags & invalidFlags)
       || (pDesc->Usage != D3D11_USAGE_DEFAULT)
       || (pDesc->CPUAccessFlags))
        return E_INVALIDARG;
    }

    // Use the maximum possible mip level count if the supplied
    // mip level count is either unspecified (0) or invalid
    const uint32_t maxMipLevelCount = (pDesc->SampleDesc.Count <= 1)
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
  
  
  HRESULT D3D11CommonTexture::GetDescFromD3D12(
          ID3D12Resource*         pResource,
    const D3D11_RESOURCE_FLAGS*   pResourceFlags,
          D3D11_COMMON_TEXTURE_DESC* pTextureDesc) {
    D3D12_RESOURCE_DESC desc12 = pResource->GetDesc();

    pTextureDesc->Width = desc12.Width;
    pTextureDesc->Height = desc12.Height;

    if (desc12.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
      pTextureDesc->Depth = desc12.DepthOrArraySize;
      pTextureDesc->ArraySize = 1;
    } else {
      pTextureDesc->Depth = 1;
      pTextureDesc->ArraySize = desc12.DepthOrArraySize;
    }

    pTextureDesc->MipLevels = desc12.MipLevels;
    pTextureDesc->Format = desc12.Format;
    pTextureDesc->SampleDesc = desc12.SampleDesc;
    pTextureDesc->Usage = D3D11_USAGE_DEFAULT;
    pTextureDesc->BindFlags = 0;
    pTextureDesc->CPUAccessFlags = 0;
    pTextureDesc->MiscFlags = 0;

    if (!(desc12.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
      pTextureDesc->BindFlags |= D3D11_BIND_SHADER_RESOURCE;

    if (desc12.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
      pTextureDesc->BindFlags |= D3D11_BIND_RENDER_TARGET;

    if (desc12.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
      pTextureDesc->BindFlags |= D3D11_BIND_DEPTH_STENCIL;

    if (desc12.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
      pTextureDesc->BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

    if (pResourceFlags) {
      pTextureDesc->BindFlags = pResourceFlags->BindFlags;
      pTextureDesc->MiscFlags |= pResourceFlags->MiscFlags;
      pTextureDesc->CPUAccessFlags = pResourceFlags->CPUAccessFlags;
    }

    return S_OK;
  }


  BOOL D3D11CommonTexture::CheckImageSupport(
    const DxvkImageCreateInfo*  pImageInfo,
          VkImageTiling         Tiling) const {
    // D3D12 images always use optimal tiling
    if (m_11on12.Resource != nullptr && Tiling != VK_IMAGE_TILING_OPTIMAL)
      return FALSE;

    DxvkFormatQuery formatQuery = { };
    formatQuery.format = pImageInfo->format;
    formatQuery.type = pImageInfo->type;
    formatQuery.tiling = Tiling;
    formatQuery.usage = pImageInfo->usage;
    formatQuery.flags = pImageInfo->flags;

    if (pImageInfo->flags & VK_IMAGE_CREATE_EXTENDED_USAGE_BIT)
      formatQuery.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

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


  BOOL D3D11CommonTexture::CheckFormatFeatureSupport(
          VkFormat              Format,
          VkFormatFeatureFlags2 Features) const {
    DxvkFormatFeatures support = m_device->GetDXVKDevice()->getFormatFeatures(Format);

    return (support.linear  & Features) == Features
        || (support.optimal & Features) == Features;
  }
  
  
  VkImageUsageFlags D3D11CommonTexture::EnableMetaCopyUsage(
          VkFormat              Format,
          VkImageTiling         Tiling) const {
    VkFormatFeatureFlags2 requestedFeatures = 0;

    if (Format == VK_FORMAT_D16_UNORM || Format == VK_FORMAT_D32_SFLOAT) {
      requestedFeatures |= VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT
                        |  VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    if (Format == VK_FORMAT_R16_UNORM || Format == VK_FORMAT_R32_SFLOAT) {
      requestedFeatures |= VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT
                        |  VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT;
    }

    if (Format == VK_FORMAT_D32_SFLOAT_S8_UINT || Format == VK_FORMAT_D24_UNORM_S8_UINT)
      requestedFeatures |= VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (!requestedFeatures)
      return 0;

    // Enable usage flags for all supported and requested features
    DxvkFormatFeatures support = m_device->GetDXVKDevice()->getFormatFeatures(Format);

    requestedFeatures &= Tiling == VK_IMAGE_TILING_OPTIMAL
      ? support.optimal
      : support.linear;
    
    VkImageUsageFlags requestedUsage = 0;

    if (requestedFeatures & VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT)
      requestedUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    
    if (requestedFeatures & VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT)
      requestedUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    
    if (requestedFeatures & VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT)
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

    auto formatInfo = lookupFormatInfo(Format);

    return formatInfo->aspectMask == dsMask
      ? VK_IMAGE_USAGE_SAMPLED_BIT
      : 0;
  }

  
  VkMemoryPropertyFlags D3D11CommonTexture::GetMemoryFlags() const {
    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    bool useCached = (m_device->GetOptions()->cachedDynamicResources == ~0u)
                  || (m_device->GetOptions()->cachedDynamicResources & m_desc.BindFlags);

    if (m_desc.Usage == D3D11_USAGE_STAGING || useCached)
      memoryFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    else if (m_desc.Usage == D3D11_USAGE_DEFAULT || m_desc.BindFlags)
      memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    return memoryFlags;
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

    // Depth-stencil formats in D3D11 can be mapped and follow special
    // packing rules, so we need to copy that data into a buffer first
    if (GetPackedDepthStencilFormat(m_desc.Format))
      return D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER;

    // Multi-plane images have a special memory layout in D3D11
    if (lookupFormatInfo(pImageInfo->format)->flags.test(DxvkFormatFlag::MultiPlane))
      return D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER;

    // If we can't use linear tiling for this image, we have to use a buffer
    if (!this->CheckImageSupport(pImageInfo, VK_IMAGE_TILING_LINEAR))
      return D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER;

    // If supported and requested, create a linear image. Default images
    // can be used for resolves and other operations regardless of bind
    // flags, so we need to use a proper image for those.
    if (m_desc.TextureLayout == D3D11_TEXTURE_LAYOUT_ROW_MAJOR)
      return D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT;

    // For default images, prefer direct mapping if the image is CPU readable
    // since mapping for reads would have to stall otherwise. If the image is
    // only writable, prefer a write-through buffer.
    if (m_desc.Usage == D3D11_USAGE_DEFAULT) {
      return (m_desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ)
        ? D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT
        : D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER;
    }

    // The overhead of frequently uploading large dynamic images may outweigh
    // the benefit of linear tiling, so use a linear image in those cases.
    VkDeviceSize threshold = m_device->GetOptions()->maxDynamicImageBufferSize;
    VkDeviceSize size = util::computeImageDataSize(pImageInfo->format, pImageInfo->extent);

    if (size > threshold)
      return D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT;

    // Dynamic images that can be sampled by a shader should generally go
    // through a buffer to allow optimal tiling and to avoid running into
    // bugs where games ignore the pitch when mapping the image.
    return D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER;
  }
  
  
  void D3D11CommonTexture::ExportImageInfo() {
    HANDLE hSharedHandle;

    if (m_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE)
      hSharedHandle = m_image->sharedHandle();
    else
      hSharedHandle = openKmtHandle( m_image->sharedHandle() );

    DxvkSharedTextureMetadata metadata;

    metadata.Width          = m_desc.Width;
    metadata.Height         = m_desc.Height;
    metadata.MipLevels      = m_desc.MipLevels;
    metadata.ArraySize      = m_desc.ArraySize;
    metadata.Format         = m_desc.Format;
    metadata.SampleDesc     = m_desc.SampleDesc;
    metadata.Usage          = m_desc.Usage;
    metadata.BindFlags      = m_desc.BindFlags;
    metadata.CPUAccessFlags = m_desc.CPUAccessFlags;
    metadata.MiscFlags      = m_desc.MiscFlags;
    metadata.TextureLayout  = m_desc.TextureLayout;

    if (hSharedHandle == INVALID_HANDLE_VALUE || !setSharedMetadata(hSharedHandle, &metadata, sizeof(metadata))) {
      Logger::warn("D3D11: Failed to write shared resource info for a texture");
    }

    if (hSharedHandle != INVALID_HANDLE_VALUE)
      CloseHandle(hSharedHandle);
  }
  
  
  BOOL D3D11CommonTexture::IsR32UavCompatibleFormat(
          DXGI_FORMAT           Format) {
    return Format == DXGI_FORMAT_R8G8B8A8_TYPELESS
        || Format == DXGI_FORMAT_B8G8R8A8_TYPELESS
        || Format == DXGI_FORMAT_B8G8R8X8_TYPELESS
        || Format == DXGI_FORMAT_R10G10B10A2_TYPELESS
        || Format == DXGI_FORMAT_R16G16_TYPELESS
        || Format == DXGI_FORMAT_R32_TYPELESS;
  }


  D3D11CommonTexture::MappedBuffer D3D11CommonTexture::CreateMappedBuffer(UINT MipLevel) const {
    const DxvkFormatInfo* formatInfo = lookupFormatInfo(
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

    // We may read mapped buffers even if it is
    // marked as CPU write-only on the D3D11 side.
    if (m_desc.Usage != D3D11_USAGE_DYNAMIC) {
      info.stages |= VK_PIPELINE_STAGE_HOST_BIT;
      info.access |= VK_ACCESS_HOST_READ_BIT;

      if (m_desc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)
        info.access |= VK_ACCESS_HOST_WRITE_BIT;
    }

    VkMemoryPropertyFlags memType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    bool useCached = m_device->GetOptions()->cachedDynamicResources == ~0u;

    if (m_desc.Usage == D3D11_USAGE_STAGING || useCached)
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
    const D3D11_COMMON_TEXTURE_DESC*  pDesc,
    const D3D11_ON_12_RESOURCE_INFO*  p11on12Info)
  : D3D11DeviceChild<ID3D11Texture1D>(pDevice),
    m_texture (this, pDevice, pDesc, p11on12Info, D3D11_RESOURCE_DIMENSION_TEXTURE1D, 0, VK_NULL_HANDLE, nullptr),
    m_interop (this, &m_texture),
    m_surface (this, &m_texture),
    m_resource(this, pDevice),
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

    if (riid == __uuidof(IDXGIKeyedMutex))
      return m_resource.GetKeyedMutex(ppvObject);

    if (riid == __uuidof(IDXGIVkInteropSurface)) {
      *ppvObject = ref(&m_interop);
      return S_OK;
    }
    
    if (logQueryInterfaceError(__uuidof(ID3D10Texture1D), riid)) {
      Logger::warn("D3D11Texture1D::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

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
    const D3D11_COMMON_TEXTURE_DESC*  pDesc,
    const D3D11_ON_12_RESOURCE_INFO*  p11on12Info,
          HANDLE                      hSharedHandle)
  : D3D11DeviceChild<ID3D11Texture2D1>(pDevice),
    m_texture   (this, pDevice, pDesc, p11on12Info, D3D11_RESOURCE_DIMENSION_TEXTURE2D, 0, VK_NULL_HANDLE, hSharedHandle),
    m_interop   (this, &m_texture),
    m_surface   (this, &m_texture),
    m_resource  (this, pDevice),
    m_d3d10     (this),
    m_swapChain (nullptr) {
  }


  D3D11Texture2D::D3D11Texture2D(
          D3D11Device*                pDevice,
    const D3D11_COMMON_TEXTURE_DESC*  pDesc,
          DXGI_USAGE                  DxgiUsage,
          VkImage                     vkImage)
  : D3D11DeviceChild<ID3D11Texture2D1>(pDevice),
    m_texture   (this, pDevice, pDesc, nullptr, D3D11_RESOURCE_DIMENSION_TEXTURE2D, DxgiUsage, vkImage, nullptr),
    m_interop   (this, &m_texture),
    m_surface   (this, &m_texture),
    m_resource  (this, pDevice),
    m_d3d10     (this),
    m_swapChain (nullptr) {
    
  }


  D3D11Texture2D::D3D11Texture2D(
          D3D11Device*                pDevice,
          IUnknown*                   pSwapChain,
    const D3D11_COMMON_TEXTURE_DESC*  pDesc,
          DXGI_USAGE                  DxgiUsage)
  : D3D11DeviceChild<ID3D11Texture2D1>(pDevice),
    m_texture   (this, pDevice, pDesc, nullptr, D3D11_RESOURCE_DIMENSION_TEXTURE2D, DxgiUsage, VK_NULL_HANDLE, nullptr),
    m_interop   (this, &m_texture),
    m_surface   (this, &m_texture),
    m_resource  (this, pDevice),
    m_d3d10     (this),
    m_swapChain (pSwapChain) {
    
  }
  
  
  D3D11Texture2D::~D3D11Texture2D() {
    
  }
  
  
  ULONG STDMETHODCALLTYPE D3D11Texture2D::AddRef() {
    uint32_t refCount = D3D11DeviceChild<ID3D11Texture2D1>::AddRef();

    if (unlikely(m_swapChain != nullptr)) {
      if (refCount == 1)
        m_swapChain->AddRef();
    }

    return refCount;
  }
  

  ULONG STDMETHODCALLTYPE D3D11Texture2D::Release() {
    IUnknown* swapChain = m_swapChain;
    uint32_t refCount = D3D11DeviceChild<ID3D11Texture2D1>::Release();

    if (unlikely(swapChain != nullptr)) {
      if (refCount == 0)
        swapChain->Release();
    }

    return refCount;
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

    if (riid == __uuidof(IDXGIKeyedMutex))
      return m_resource.GetKeyedMutex(ppvObject);
    
    if (riid == __uuidof(IDXGIVkInteropSurface)) {
      *ppvObject = ref(&m_interop);
      return S_OK;
    }
    
    if (logQueryInterfaceError(__uuidof(ID3D10Texture2D), riid)) {
      Logger::warn("D3D11Texture2D::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

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
    const D3D11_COMMON_TEXTURE_DESC*  pDesc,
    const D3D11_ON_12_RESOURCE_INFO*  p11on12Info)
  : D3D11DeviceChild<ID3D11Texture3D1>(pDevice),
    m_texture (this, pDevice, pDesc, p11on12Info, D3D11_RESOURCE_DIMENSION_TEXTURE3D, 0, VK_NULL_HANDLE, nullptr),
    m_interop (this, &m_texture),
    m_resource(this, pDevice),
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

    if (riid == __uuidof(IDXGIKeyedMutex))
      return m_resource.GetKeyedMutex(ppvObject);

    if (riid == __uuidof(IDXGIVkInteropSurface)) {
      *ppvObject = ref(&m_interop);
      return S_OK;
    }
    
    if (logQueryInterfaceError(__uuidof(ID3D10Texture3D), riid)) {
      Logger::warn("D3D11Texture3D::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

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
