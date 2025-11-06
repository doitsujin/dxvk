#include "d3d11_device.h"
#include "d3d11_context_imm.h"
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
    VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    std::tie(m_mapMode, memoryProperties) = DetermineMapMode(pDevice, &imageInfo);
    
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
    uint32_t subresourceCount = m_desc.ArraySize * m_desc.MipLevels;

    if (m_mapMode != D3D11_COMMON_TEXTURE_MAP_MODE_NONE) {
      m_mapInfo.resize(subresourceCount);

      for (uint32_t i = 0; i < subresourceCount; i++) {
        m_mapInfo[i].layout = DetermineSubresourceLayout(&imageInfo,
          GetSubresourceFromIndex(formatProperties->aspectMask, i));
      }
    }

    if (m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER
     || m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_STAGING
     || m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_DYNAMIC) {
      m_buffers.resize(subresourceCount);

      if (m_mapMode != D3D11_COMMON_TEXTURE_MAP_MODE_DYNAMIC) {
        for (uint32_t i = 0; i < subresourceCount; i++)
          CreateMappedBuffer(i);
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

    if (m_11on12.Resource != nullptr)
      vkImage = VkImage(m_11on12.VulkanHandle);

    if (!vkImage)
      m_image = m_device->GetDXVKDevice()->createImage(imageInfo, memoryProperties);
    else
      m_image = m_device->GetDXVKDevice()->importImage(imageInfo, vkImage, memoryProperties);

    if (m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT)
      m_mapPtr = m_image->mapPtr(0);

    if (imageInfo.sharing.mode == DxvkSharedHandleMode::Export) {
      if (m_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) {
        try {
          Rc<DxvkKeyedMutex> mutex = new DxvkKeyedMutex(m_device->GetDXVKDevice(), 0, !!(m_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE));
          m_image->setKeyedMutex(std::move(mutex));
        } catch (const DxvkError& e) {
          Logger::warn("D3D11CommonTexture: Failed to create keyed mutex");
        }
      }

      ExportImageInfo();
    }
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


  D3D11_COMMON_TEXTURE_SUBRESOURCE_LAYOUT D3D11CommonTexture::GetSubresourceLayout(
          VkImageAspectFlags    AspectMask,
          UINT                  Subresource) const {
    // Color is mapped directly and depth-stencil are interleaved
    // in packed formats, so just use the cached subresource layout
    constexpr VkImageAspectFlags PlaneAspects = VK_IMAGE_ASPECT_PLANE_0_BIT
      | VK_IMAGE_ASPECT_PLANE_1_BIT | VK_IMAGE_ASPECT_PLANE_2_BIT;

    if ((Subresource < m_mapInfo.size()) && !(AspectMask & PlaneAspects))
      return m_mapInfo[Subresource].layout;

    // Safe-guard against invalid subresource index
    if (Subresource >= m_desc.ArraySize * m_desc.MipLevels)
      return D3D11_COMMON_TEXTURE_SUBRESOURCE_LAYOUT();

    // Image info is only needed for direct-mapped images
    VkImageSubresource subresource = GetSubresourceFromIndex(AspectMask, Subresource);
    return DetermineSubresourceLayout(nullptr, subresource);
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


  void D3D11CommonTexture::SetDebugName(const char* pName) {
    if (m_image) {
      m_device->GetContext()->InjectCs(DxvkCsQueue::HighPriority, [
        cImage  = m_image,
        cName   = std::string(pName ? pName : "")
      ] (DxvkContext* ctx) {
        ctx->setDebugName(cImage, cName.c_str());
      });
    }

    if (m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_STAGING) {
      for (uint32_t i = 0; i < m_buffers.size(); i++) {
        m_device->GetContext()->InjectCs(DxvkCsQueue::HighPriority, [
          cBuffer = m_buffers[i].buffer,
          cName   = std::string(pName ? pName : "")
        ] (DxvkContext* ctx) {
          ctx->setDebugName(cBuffer, cName.c_str());
        });
      }
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

  
  std::pair<D3D11_COMMON_TEXTURE_MAP_MODE, VkMemoryPropertyFlags> D3D11CommonTexture::DetermineMapMode(
    const D3D11Device*          device,
    const DxvkImageCreateInfo*  pImageInfo) const {
    // Don't map an image unless the application requests it
    if (!m_desc.CPUAccessFlags)
      return { D3D11_COMMON_TEXTURE_MAP_MODE_NONE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    // For default images, always use a persistent staging buffer. Readback
    // may cause a GPU sync, but nobody seems to be using this feature anyway.
    if (m_desc.Usage == D3D11_USAGE_DEFAULT)
      return { D3D11_COMMON_TEXTURE_MAP_MODE_BUFFER, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    // If the resource cannot be used in the actual rendering pipeline, we
    // do not need to create an actual image and can instead implement copy
    // functions as buffer-to-image and image-to-buffer copies.
    if (m_desc.Usage == D3D11_USAGE_STAGING)
      return { D3D11_COMMON_TEXTURE_MAP_MODE_STAGING, 0u };

    // If the packed format and image format don't match, we need to use
    // a staging buffer and perform format conversion when mapping. The
    // same is true if the game is broken and requires tight packing.
    if (m_packedFormat != pImageInfo->format || device->GetOptions()->disableDirectImageMapping)
      return { D3D11_COMMON_TEXTURE_MAP_MODE_DYNAMIC, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    // Multi-plane and depth-stencil images have a special memory layout
    // in D3D11, so we can't expose those directly to the app
    auto formatInfo = lookupFormatInfo(pImageInfo->format);

    if (formatInfo->aspectMask != VK_IMAGE_ASPECT_COLOR_BIT)
      return { D3D11_COMMON_TEXTURE_MAP_MODE_DYNAMIC, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    // If we can't use linear tiling for this image, we have to use a buffer
    if (!CheckImageSupport(pImageInfo, VK_IMAGE_TILING_LINEAR))
      return { D3D11_COMMON_TEXTURE_MAP_MODE_DYNAMIC, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    // Determine memory flags for the actual image if we use direct mapping.
    // Depending on the concrete use case, we may fall back to different
    // memory types.
    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    bool useCached = (m_device->GetOptions()->cachedDynamicResources == ~0u)
                  || (m_device->GetOptions()->cachedDynamicResources & m_desc.BindFlags)
                  || (m_desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ);

    if (m_desc.Usage == D3D11_USAGE_STAGING || useCached)
      memoryFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    else if (m_desc.BindFlags)
      memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    // If there are multiple subresources, go through a buffer because
    // we can otherwise not really discard individual subresources.
    if (m_desc.ArraySize > 1u || m_desc.MipLevels != 1u)
      return { D3D11_COMMON_TEXTURE_MAP_MODE_DYNAMIC, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    // If the image is essentially linear already, expose it directly since
    // there won't be any tangible benefit to using optimal tiling anyway.
    VkExtent3D blockCount = util::computeBlockCount(pImageInfo->extent, formatInfo->blockSize);

    if (blockCount.height == 1u && blockCount.depth == 1u)
      return { D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT, memoryFlags };

    // If the image looks like a video, we can generally expect it to get
    // updated and read once per frame. This is one of the most common use
    // cases for a mapped image, expose it directly in order to avoid copies.
    if (blockCount.depth == 1u && blockCount.height >= 160 && formatInfo->elementSize <= 4u) {
      static const std::array<std::pair<uint32_t, uint32_t>, 3> videoApectRatios = {{
        {  4, 3 },
        { 16, 9 },
        { 21, 9 },
      }};

      bool isVideoAspectRatio = false;

      for (const auto& a : videoApectRatios) {
        // Due to codec limitations, video dimensions are often rounded to
        // a multiple of 8. Account for this when checking the size.
        isVideoAspectRatio |= blockCount.width > (a.first * (blockCount.height - 8u)) / a.second
                           && blockCount.width < (a.first * (blockCount.height + 8u)) / a.second;
      }

      if (isVideoAspectRatio) {
        // Keep video images in system memory to not waste precious HVV space
        return { D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT, memoryFlags & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
      }
    }

    // If the image exceeds a certain size, map it directly because the overhead
    // of potentially copying the whole thing every frame likely outweighs any
    // benefit we might get from faster memory and tiling. This solves such an
    // issue in Warhammer III, which discards a 48 MB texture every single frame.
    constexpr VkDeviceSize MaxImageStagingBufferSize = 1ull << 20;

    VkDeviceSize imageSize = util::flattenImageExtent(blockCount) * formatInfo->elementSize;

    if (imageSize > MaxImageStagingBufferSize)
      return { D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT, memoryFlags };

    // For smaller images, use a staging buffer. There are some common use
    // cases where the image will only get written once, e.g. SMAA look-up
    // tables in some games, which will benefit from faster GPU access.
    return { D3D11_COMMON_TEXTURE_MAP_MODE_DYNAMIC, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
  }


  D3D11_COMMON_TEXTURE_SUBRESOURCE_LAYOUT D3D11CommonTexture::DetermineSubresourceLayout(
    const DxvkImageCreateInfo*  pImageInfo,
    const VkImageSubresource&   subresource) const {
    auto formatInfo = lookupFormatInfo(m_packedFormat);

    if (m_mapMode == D3D11_COMMON_TEXTURE_MAP_MODE_DIRECT) {
      VkSubresourceLayout vkLayout = m_device->GetDXVKDevice()->queryImageSubresourceLayout(*pImageInfo, subresource);

      D3D11_COMMON_TEXTURE_SUBRESOURCE_LAYOUT result = { };
      result.Offset = vkLayout.offset;
      result.RowPitch = vkLayout.rowPitch;
      result.DepthPitch = vkLayout.depthPitch;

      // We will only ever use direct mapping for single-aspect images,
      // so ignore any sort of multi-plane shenanigans on this path
      auto mipExtent = MipLevelExtent(subresource.mipLevel);
      auto blockCount = util::computeBlockCount(mipExtent, formatInfo->blockSize);

      // If the image dimensions support it, try to look as close to a
      // linear buffer as we can. Some games use the depth pitch as a
      // subresource size and will crash if it includes any padding.
      if (blockCount.depth == 1u) {
        if (blockCount.height == 1u) {
          result.RowPitch = formatInfo->elementSize * blockCount.width;
          result.DepthPitch = result.RowPitch;
        } else {
          result.DepthPitch = vkLayout.rowPitch * blockCount.height;
        }
      }

      result.Size = blockCount.depth * result.DepthPitch;
      return result;
    } else {
      D3D11_COMMON_TEXTURE_SUBRESOURCE_LAYOUT result = { };

      VkImageAspectFlags aspects = formatInfo->aspectMask;
      VkExtent3D mipExtent = MipLevelExtent(subresource.mipLevel);

      while (aspects) {
        auto aspect = vk::getNextAspect(aspects);
        auto extent = mipExtent;
        auto elementSize = formatInfo->elementSize;

        if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
          auto plane = &formatInfo->planes[vk::getPlaneIndex(aspect)];
          extent.width  /= plane->blockSize.width;
          extent.height /= plane->blockSize.height;
          elementSize = plane->elementSize;
        }

        auto blockCount = util::computeBlockCount(extent, formatInfo->blockSize);

        if (!result.RowPitch) {
          result.RowPitch   = elementSize * blockCount.width;
          result.DepthPitch = elementSize * blockCount.width * blockCount.height;
        }

        VkDeviceSize size = elementSize * blockCount.width * blockCount.height * blockCount.depth;

        if (aspect & subresource.aspectMask)
          result.Size += size;
        else if (!result.Size)
          result.Offset += size;
      }

      return result;
    }
  }
  
  
  void D3D11CommonTexture::ExportImageInfo() {
    struct d3dkmt_d3d11_desc desc = { };
    desc.dxgi.size = sizeof(desc);
    desc.dxgi.version = 4;
    desc.dxgi.keyed_mutex = !!(m_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX);
    desc.dxgi.nt_shared = !!(m_desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE);
    desc.dimension = m_dimension;

    if (desc.dxgi.keyed_mutex) {
      auto keyedMutex = m_image->getKeyedMutex();
      desc.dxgi.mutex_handle = keyedMutex ? keyedMutex->kmtGlobal() : 0;

      auto syncObject = keyedMutex->getSyncObject();
      desc.dxgi.sync_handle = syncObject ? syncObject->kmtGlobal() : 0;
    }

    switch (m_dimension) {
      case D3D11_RESOURCE_DIMENSION_UNKNOWN: break;
      case D3D11_RESOURCE_DIMENSION_BUFFER: break;
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
        desc.d3d11_1d.Width = m_desc.Width;
        desc.d3d11_1d.MipLevels = m_desc.MipLevels;
        desc.d3d11_1d.ArraySize = m_desc.ArraySize;
        desc.d3d11_1d.Format = m_desc.Format;
        desc.d3d11_1d.Usage = m_desc.Usage;
        desc.d3d11_1d.BindFlags = m_desc.BindFlags;
        desc.d3d11_1d.CPUAccessFlags = m_desc.CPUAccessFlags;
        desc.d3d11_1d.MiscFlags = m_desc.MiscFlags;
        break;
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
        desc.d3d11_2d.Width = m_desc.Width;
        desc.d3d11_2d.Height = m_desc.Height;
        desc.d3d11_2d.MipLevels = m_desc.MipLevels;
        desc.d3d11_2d.ArraySize = m_desc.ArraySize;
        desc.d3d11_2d.Format = m_desc.Format;
        desc.d3d11_2d.SampleDesc = m_desc.SampleDesc;
        desc.d3d11_2d.Usage = m_desc.Usage;
        desc.d3d11_2d.BindFlags = m_desc.BindFlags;
        desc.d3d11_2d.CPUAccessFlags = m_desc.CPUAccessFlags;
        desc.d3d11_2d.MiscFlags = m_desc.MiscFlags;
        break;
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
        desc.d3d11_3d.Width = m_desc.Width;
        desc.d3d11_3d.Height = m_desc.Height;
        desc.d3d11_3d.Depth = m_desc.Depth;
        desc.d3d11_3d.MipLevels = m_desc.MipLevels;
        desc.d3d11_3d.Format = m_desc.Format;
        desc.d3d11_3d.Usage = m_desc.Usage;
        desc.d3d11_3d.BindFlags = m_desc.BindFlags;
        desc.d3d11_3d.CPUAccessFlags = m_desc.CPUAccessFlags;
        desc.d3d11_3d.MiscFlags = m_desc.MiscFlags;
        break;
    }

    D3DKMT_ESCAPE escape = { };
    escape.Type = D3DKMT_ESCAPE_UPDATE_RESOURCE_WINE;
    escape.pPrivateDriverData = &desc;
    escape.PrivateDriverDataSize = sizeof(desc);
    escape.hContext = m_image->storage()->kmtLocal();

    if (!D3DKMTEscape(&escape))
      return;

    /* try the legacy Proton shared resource implementation */

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


  void D3D11CommonTexture::CreateMappedBuffer(UINT Subresource) {
    const DxvkFormatInfo* formatInfo = lookupFormatInfo(
      m_device->LookupPackedFormat(m_desc.Format, GetFormatMode()).Format);

    DxvkBufferCreateInfo info;
    info.size   = GetSubresourceLayout(formatInfo->aspectMask, Subresource).Size;
    info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
                | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT
                | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT
                | VK_ACCESS_TRANSFER_WRITE_BIT
                | VK_ACCESS_SHADER_READ_BIT
                | VK_ACCESS_SHADER_WRITE_BIT;
    info.debugName = "Image buffer";

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

    auto& entry = m_buffers[Subresource];
    entry.buffer = m_device->GetDXVKDevice()->createBuffer(info, memType);
    entry.slice = entry.buffer->storage();
  }


  void D3D11CommonTexture::FreeMappedBuffer(
          UINT                  Subresource) {
    auto& entry = m_buffers[Subresource];
    entry.buffer = nullptr;
    entry.slice = nullptr;
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
    Usage &= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
      | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
      | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    // Storage images require GENERAL.
    if (Usage & VK_IMAGE_USAGE_STORAGE_BIT)
      return VK_IMAGE_LAYOUT_GENERAL;

    // Also use GENERAL if the image cannot be rendered to. This
    // should not harm any hardware in practice and may avoid some
    // redundant layout transitions for regular textures.
    if (!(Usage & ~VK_IMAGE_USAGE_SAMPLED_BIT))
      return VK_IMAGE_LAYOUT_GENERAL;

    // If the image is used only as an attachment, we never
    // have to transform the image back to a different layout.
    if (Usage == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    if (Usage == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Otherwise, pick a layout that can be used for reading.
    return usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
      ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
      : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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

    if (!m_locked.load(std::memory_order_acquire)) {
      // Need to make sure that the image cannot be relocated. This may
      // be entered by multiple threads, which is fine since the actual
      // work is serialized into the CS thread and only the first call
      // will actually modify any image state.
      Com<ID3D11Device> device;
      m_resource->GetDevice(&device);

      static_cast<D3D11Device*>(device.ptr())->LockImage(image, 0u);

      m_locked.store(true, std::memory_order_release);
    }

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
    m_d3d10   (this),
    m_destructionNotifier(this) {
    
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
    
    if (riid == __uuidof(ID3DDestructionNotifier)) {
      *ppvObject = ref(&m_destructionNotifier);
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


  void STDMETHODCALLTYPE D3D11Texture1D::SetDebugName(const char* pName) {
    m_texture.SetDebugName(pName);
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
    m_swapChain (nullptr),
    m_destructionNotifier(this) {
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
    m_swapChain (nullptr),
    m_destructionNotifier(this) {
    
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
    m_swapChain (pSwapChain),
    m_destructionNotifier(this) {
    
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
    
    if (riid == __uuidof(ID3DDestructionNotifier)) {
      *ppvObject = ref(&m_destructionNotifier);
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
  
  
  void STDMETHODCALLTYPE D3D11Texture2D::SetDebugName(const char* pName) {
    m_texture.SetDebugName(pName);
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
    m_d3d10   (this),
    m_destructionNotifier(this) {
    
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

    if (riid == __uuidof(ID3DDestructionNotifier)) {
      *ppvObject = ref(&m_destructionNotifier);
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
  
  
  void STDMETHODCALLTYPE D3D11Texture3D::SetDebugName(const char* pName) {
    m_texture.SetDebugName(pName);
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
