#include "dxvk_image.h"

#include "dxvk_device.h"

namespace dxvk {
  
  DxvkImage::DxvkImage(
          DxvkDevice*           device,
    const DxvkImageCreateInfo&  createInfo,
          DxvkMemoryAllocator&  memAlloc,
          VkMemoryPropertyFlags memFlags)
  : m_vkd           (device->vkd()),
    m_allocator     (&memAlloc),
    m_properties    (memFlags),
    m_shaderStages  (util::shaderStages(createInfo.stages)),
    m_info          (createInfo) {
    copyFormatList(createInfo.viewFormatCount, createInfo.viewFormats);

    // Determine whether the image is shareable before creating the resource
    VkImageCreateInfo imageInfo = getImageCreateInfo();
    m_shared = canShareImage(device, imageInfo, m_info.sharing);

    assignResource(createResource());
  }


  DxvkImage::DxvkImage(
          DxvkDevice*           device,
    const DxvkImageCreateInfo&  createInfo,
          VkImage               imageHandle,
          DxvkMemoryAllocator&  memAlloc,
          VkMemoryPropertyFlags memFlags)
  : m_vkd           (device->vkd()),
    m_allocator     (&memAlloc),
    m_properties    (memFlags),
    m_shaderStages  (util::shaderStages(createInfo.stages)),
    m_info          (createInfo) {
    copyFormatList(createInfo.viewFormatCount, createInfo.viewFormats);

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


  Rc<DxvkImageView> DxvkImage::createView(
    const DxvkImageViewCreateInfo& info) {
    DxvkImageViewKey key = { };
    key.viewType = info.type;
    key.format = info.format;
    key.usage = info.usage;
    key.aspects = info.aspect;
    key.mipIndex = info.minLevel;
    key.mipCount = info.numLevels;
    key.layerIndex = info.minLayer;
    key.layerCount = info.numLayers;

    if (info.usage == VK_IMAGE_USAGE_SAMPLED_BIT) {
      key.packedSwizzle =
        (uint16_t(info.swizzle.r) <<  0) |
        (uint16_t(info.swizzle.g) <<  4) |
        (uint16_t(info.swizzle.b) <<  8) |
        (uint16_t(info.swizzle.a) << 12);
    }

    std::unique_lock lock(m_viewMutex);

    auto entry = m_views.emplace(std::piecewise_construct,
      std::make_tuple(key), std::make_tuple(this, key));

    return &entry.first->second;
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
          DxvkImage*                image,
    const DxvkImageViewKey&         key)
  : m_image(image), m_key(key) {

  }


  DxvkImageView::~DxvkImageView() {

  }


  VkImageView DxvkImageView::createView(VkImageViewType type) const {
    DxvkImageViewKey key = m_key;
    key.viewType = type;

    // Only use one layer for non-arrayed view types
    if (type == VK_IMAGE_VIEW_TYPE_1D || type == VK_IMAGE_VIEW_TYPE_2D)
      key.layerCount = 1u;

    switch (m_image->info().type) {
      case VK_IMAGE_TYPE_1D: {
        // Trivial, just validate that view types are compatible
        if (type != VK_IMAGE_VIEW_TYPE_1D && type != VK_IMAGE_VIEW_TYPE_1D_ARRAY)
          return VK_NULL_HANDLE;
      } break;

      case VK_IMAGE_TYPE_2D: {
        if (type == VK_IMAGE_VIEW_TYPE_CUBE || type == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) {
          // Ensure that the image is compatible with cube maps
          if (key.layerCount < 6 || !(m_image->info().flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT))
            return VK_NULL_HANDLE;

          // Adjust layer count to make sure it's a multiple of 6
          key.layerCount = type == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
            ? key.layerCount - key.layerCount % 6u : 6u;
        } else if (type != VK_IMAGE_VIEW_TYPE_2D && type != VK_IMAGE_VIEW_TYPE_2D_ARRAY) {
          return VK_NULL_HANDLE;
        }
      } break;

      case VK_IMAGE_TYPE_3D: {
        if (type == VK_IMAGE_VIEW_TYPE_2D || type == VK_IMAGE_VIEW_TYPE_2D_ARRAY) {
          // Ensure that the image is actually compatible with 2D views
          if (!(m_image->info().flags & VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT))
            return VK_NULL_HANDLE;

          // In case the view's native type is 3D, we can only create 2D compat
          // views if there is only one mip and with the full set of array layers.
          if (m_key.viewType == VK_IMAGE_VIEW_TYPE_3D) {
            if (m_key.mipCount != 1u)
              return VK_NULL_HANDLE;

            key.layerIndex = 0u;
            key.layerCount = type == VK_IMAGE_VIEW_TYPE_2D_ARRAY
              ? m_image->mipLevelExtent(key.mipIndex).depth : 1u;
          }
        } else if (type != VK_IMAGE_VIEW_TYPE_3D) {
          return VK_NULL_HANDLE;
        }
      } break;

      default:
        return VK_NULL_HANDLE;
    }

    return m_image->m_storage->createImageView(key);
  }


  void DxvkImageView::updateViews() {
    // Update all views that are not currently null
    for (uint32_t i = 0; i < m_views.size(); i++) {
      if (m_views[i])
        m_views[i] = createView(VkImageViewType(i));
    }

    m_version = m_image->m_version;
  }
  
}
