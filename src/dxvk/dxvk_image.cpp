#include "dxvk_image.h"

#include "dxvk_device.h"

namespace dxvk {
  
  DxvkImage::DxvkImage(
          DxvkDevice*           device,
    const DxvkImageCreateInfo&  createInfo,
          DxvkMemoryAllocator&  memAlloc,
          VkMemoryPropertyFlags memFlags)
  : m_vkd(device->vkd()), m_allocator(&memAlloc), m_properties(memFlags), m_info(createInfo) {
    copyFormatList(createInfo.viewFormatCount, createInfo.viewFormats);

    // Determine whether the image is shareable before creating the resource
    VkImageCreateInfo imageInfo = getImageCreateInfo();
    m_shared = canShareImage(device, imageInfo, m_info.sharing);

    assignResource(createResource());
  }


  DxvkImage::DxvkImage(
          DxvkDevice*           device,
    const DxvkImageCreateInfo&  info,
          VkImage               imageHandle,
          DxvkMemoryAllocator&  memAlloc,
          VkMemoryPropertyFlags memFlags)
  : m_vkd(device->vkd()), m_allocator(&memAlloc), m_properties(memFlags), m_info(info) {
    copyFormatList(info.viewFormatCount, info.viewFormats);

    // Create backing storage for existing image resource
    VkImageCreateInfo imageInfo = getImageCreateInfo();
    assignResource(m_allocator->importImageResource(imageInfo, imageHandle));
  }


  DxvkImage::~DxvkImage() {

  }


  VkSubresourceLayout DxvkImage::querySubresourceLayout(
    const VkImageSubresource& subresource) const {
    VkSubresourceLayout result = { };

    m_vkd->vkGetImageSubresourceLayout(m_vkd->device(),
      m_imageInfo.image, &subresource, &result);

    return result;
  }


  HANDLE DxvkImage::sharedHandle() const {
    HANDLE handle = INVALID_HANDLE_VALUE;

    if (!m_shared)
      return INVALID_HANDLE_VALUE;

#ifdef _WIN32
    DxvkResourceMemoryInfo memoryInfo = m_storage->getMemoryInfo();

    VkMemoryGetWin32HandleInfoKHR handleInfo = { VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
    handleInfo.handleType = m_info.sharing.type;
    handleInfo.memory = memoryInfo.memory;

    if (m_vkd->vkGetMemoryWin32HandleKHR(m_vkd->device(), &handleInfo, &handle) != VK_SUCCESS)
      Logger::warn("DxvkImage::DxvkImage: Failed to get shared handle for image");
#endif

    return handle;
  }


  DxvkSparsePageTable* DxvkImage::getSparsePageTable() {
    return m_storage->getSparsePageTable();
  }


  Rc<DxvkResourceAllocation> DxvkImage::createResource() {
    const DxvkFormatInfo* formatInfo = lookupFormatInfo(m_info.format);

    VkImageCreateInfo imageInfo = getImageCreateInfo();

    // Set up view format list so that drivers can better enable
    // compression. Skip for planar formats due to validation errors.
    VkImageFormatListCreateInfo formatList = { VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO };

    if (!(formatInfo->aspectMask & VK_IMAGE_ASPECT_PLANE_0_BIT)) {
      formatList.viewFormatCount = m_info.viewFormatCount;
      formatList.pViewFormats    = m_info.viewFormats;
    }

    if ((m_info.flags & VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT) && formatList.viewFormatCount)
      formatList.pNext = std::exchange(imageInfo.pNext, &formatList);

    // Set up external memory parameters for shared images
    VkExternalMemoryImageCreateInfo externalInfo = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO };

    if (m_shared) {
      externalInfo.pNext = std::exchange(imageInfo.pNext, &externalInfo);
      externalInfo.handleTypes = m_info.sharing.type;
    }

    // Set up shared memory properties
    void* sharedMemoryInfo = nullptr;

    VkExportMemoryAllocateInfo sharedExport = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO };
    VkImportMemoryWin32HandleInfoKHR sharedImportWin32= { VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR };

    if (m_shared && m_info.sharing.mode == DxvkSharedHandleMode::Export) {
      sharedExport.pNext = std::exchange(sharedMemoryInfo, &sharedExport);
      sharedExport.handleTypes = m_info.sharing.type;
    }

    if (m_shared && m_info.sharing.mode == DxvkSharedHandleMode::Import) {
      sharedImportWin32.pNext = std::exchange(sharedMemoryInfo, &sharedImportWin32);
      sharedImportWin32.handleType = m_info.sharing.type;
      sharedImportWin32.handle = m_info.sharing.handle;
    }

    return m_allocator->createImageResource(imageInfo, m_properties, sharedMemoryInfo);
  }


  VkImageCreateInfo DxvkImage::getImageCreateInfo() const {
    VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    info.flags = m_info.flags;
    info.imageType = m_info.type;
    info.format = m_info.format;
    info.extent = m_info.extent;
    info.mipLevels = m_info.mipLevels;
    info.arrayLayers = m_info.numLayers;
    info.samples = m_info.sampleCount;
    info.tiling = m_info.tiling;
    info.usage = m_info.usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = m_info.initialLayout;

    return info;
  }


  void DxvkImage::copyFormatList(uint32_t formatCount, const VkFormat* formats) {
    m_viewFormats.resize(formatCount);

    for (uint32_t i = 0; i < formatCount; i++)
      m_viewFormats[i] = formats[i];

    m_info.viewFormats = m_viewFormats.data();
  }


  bool DxvkImage::canShareImage(DxvkDevice* device, const VkImageCreateInfo& createInfo, const DxvkSharedHandleInfo& sharingInfo) const {
    if (sharingInfo.mode == DxvkSharedHandleMode::None)
      return false;

    if (!device->features().khrExternalMemoryWin32) {
      Logger::err("Failed to create shared resource: VK_KHR_EXTERNAL_MEMORY_WIN32 not supported");
      return false;
    }

    if (createInfo.flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT) {
      Logger::err("Failed to create shared resource: Sharing sparse resources not supported");
      return false;
    }

    DxvkFormatQuery formatQuery = { };
    formatQuery.format = createInfo.format;
    formatQuery.type = createInfo.imageType;
    formatQuery.tiling = createInfo.tiling;
    formatQuery.usage = createInfo.usage;
    formatQuery.flags = createInfo.flags;
    formatQuery.handleType = sharingInfo.type;

    auto limits = device->getFormatLimits(formatQuery);

    if (!limits)
      return false;

    VkExternalMemoryFeatureFlagBits requiredFeature = sharingInfo.mode == DxvkSharedHandleMode::Export
      ? VK_EXTERNAL_MEMORY_FEATURE_EXPORTABLE_BIT
      : VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;

    if (!(limits->externalFeatures & requiredFeature)) {
      Logger::err("Failed to create shared resource: Image cannot be shared");
      return false;
    }

    return true;
  }





  DxvkImageView::DxvkImageView(
    const Rc<vk::DeviceFn>&         vkd,
    const Rc<DxvkImage>&            image,
    const DxvkImageViewCreateInfo&  info)
  : m_vkd(vkd), m_image(image), m_info(info) {
    for (uint32_t i = 0; i < ViewCount; i++)
      m_views[i] = VK_NULL_HANDLE;
    
    switch (m_info.type) {
      case VK_IMAGE_VIEW_TYPE_1D:
      case VK_IMAGE_VIEW_TYPE_1D_ARRAY: {
        this->createView(VK_IMAGE_VIEW_TYPE_1D,       1);
        this->createView(VK_IMAGE_VIEW_TYPE_1D_ARRAY, m_info.numLayers);
      } break;
      
      case VK_IMAGE_VIEW_TYPE_2D:
      case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
        this->createView(VK_IMAGE_VIEW_TYPE_2D, 1);
        [[fallthrough]];

      case VK_IMAGE_VIEW_TYPE_CUBE:
      case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY: {
        this->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, m_info.numLayers);
        
        if (m_image->info().flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) {
          uint32_t cubeCount = m_info.numLayers / 6;
        
          if (cubeCount > 0) {
            this->createView(VK_IMAGE_VIEW_TYPE_CUBE,       6);
            this->createView(VK_IMAGE_VIEW_TYPE_CUBE_ARRAY, 6 * cubeCount);
          }
        }
      } break;
        
      case VK_IMAGE_VIEW_TYPE_3D: {
        this->createView(VK_IMAGE_VIEW_TYPE_3D, 1);
        
        if (m_image->info().flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT && m_info.numLevels == 1) {
          this->createView(VK_IMAGE_VIEW_TYPE_2D,       1);
          this->createView(VK_IMAGE_VIEW_TYPE_2D_ARRAY, m_image->mipLevelExtent(m_info.minLevel).depth);
        }
      } break;
      
      default:
        throw DxvkError(str::format("DxvkImageView: Invalid view type: ", m_info.type));
    }
  }
  
  
  DxvkImageView::~DxvkImageView() {
    for (uint32_t i = 0; i < ViewCount; i++)
      m_vkd->vkDestroyImageView(m_vkd->device(), m_views[i], nullptr);
  }

  
  void DxvkImageView::createView(VkImageViewType type, uint32_t numLayers) {
    VkImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask     = m_info.aspect;
    subresourceRange.baseMipLevel   = m_info.minLevel;
    subresourceRange.levelCount     = m_info.numLevels;
    subresourceRange.baseArrayLayer = m_info.minLayer;
    subresourceRange.layerCount     = numLayers;

    VkImageViewUsageCreateInfo viewUsage = { VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
    viewUsage.usage           = m_info.usage;
    
    VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, &viewUsage };
    viewInfo.image            = m_image->handle();
    viewInfo.viewType         = type;
    viewInfo.format           = m_info.format;
    viewInfo.components       = m_info.swizzle;
    viewInfo.subresourceRange = subresourceRange;

    if (m_info.usage == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
      viewInfo.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    }
    
    if (m_vkd->vkCreateImageView(m_vkd->device(),
          &viewInfo, nullptr, &m_views[type]) != VK_SUCCESS) {
      throw DxvkError(str::format(
        "DxvkImageView: Failed to create image view:"
        "\n  View type:       ", viewInfo.viewType,
        "\n  View format:     ", viewInfo.format,
        "\n  Subresources:    ",
        "\n    Aspect mask:   ", std::hex, viewInfo.subresourceRange.aspectMask,
        "\n    Mip levels:    ", viewInfo.subresourceRange.baseMipLevel, " - ",
                                 viewInfo.subresourceRange.levelCount,
        "\n    Array layers:  ", viewInfo.subresourceRange.baseArrayLayer, " - ",
                                 viewInfo.subresourceRange.layerCount,
        "\n  Image properties:",
        "\n    Type:          ", m_image->info().type,
        "\n    Format:        ", m_image->info().format,
        "\n    Extent:        ", "(", m_image->info().extent.width,
                                 ",", m_image->info().extent.height,
                                 ",", m_image->info().extent.depth, ")",
        "\n    Mip levels:    ", m_image->info().mipLevels,
        "\n    Array layers:  ", m_image->info().numLayers,
        "\n    Samples:       ", m_image->info().sampleCount,
        "\n    Usage:         ", std::hex, m_image->info().usage,
        "\n    Tiling:        ", m_image->info().tiling));
    }
  }
  
}
