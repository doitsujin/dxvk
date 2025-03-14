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
    m_allocator->registerResource(this);

    copyFormatList(createInfo.viewFormatCount, createInfo.viewFormats);

    // Assign debug name to image
    if (device->debugFlags().test(DxvkDebugFlag::Capture)) {
      m_debugName = createDebugName(createInfo.debugName);
      m_info.debugName = m_debugName.c_str();
    } else {
      m_info.debugName = nullptr;
    }

    // Always enable depth-stencil attachment usage for depth-stencil
    // formats since some internal operations rely on it. Read-only
    // versions of these make little sense to begin with.
    if (lookupFormatInfo(createInfo.format)->aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
      m_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    // Determine whether the image is shareable before creating the resource
    VkImageCreateInfo imageInfo = getImageCreateInfo(DxvkImageUsageInfo());
    m_shared = canShareImage(device, imageInfo, m_info.sharing);

    if (m_info.sharing.mode != DxvkSharedHandleMode::Import)
      m_uninitializedSubresourceCount = m_info.numLayers * m_info.mipLevels;

    assignStorage(allocateStorage());
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
    m_info          (createInfo),
    m_stableAddress (true) {
    m_allocator->registerResource(this);

    copyFormatList(createInfo.viewFormatCount, createInfo.viewFormats);

    // Create backing storage for existing image resource
    DxvkAllocationInfo allocationInfo = { };
    allocationInfo.resourceCookie = cookie();

    VkImageCreateInfo imageInfo = getImageCreateInfo(DxvkImageUsageInfo());
    assignStorage(m_allocator->importImageResource(imageInfo, allocationInfo, imageHandle));
  }


  DxvkImage::~DxvkImage() {
    m_allocator->unregisterResource(this);
  }


  bool DxvkImage::canRelocate() const {
    return !m_imageInfo.mapPtr && !m_shared && !m_stableAddress
        && !(m_info.flags & VK_IMAGE_CREATE_SPARSE_BINDING_BIT);
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


  Rc<DxvkResourceAllocation> DxvkImage::relocateStorage(
          DxvkAllocationModes         mode) {
    if (!canRelocate())
      return nullptr;

    return allocateStorageWithUsage(DxvkImageUsageInfo(), mode);
  }


  uint64_t DxvkImage::getTrackingAddress(uint32_t mip, uint32_t layer, VkOffset3D coord) const {
    // For 2D and 3D images, use morton codes to linearize the address ranges
    // of pixel blocks. This helps reduce false positives in common use cases
    // where the application copies aligned power-of-two blocks around.
    uint64_t base = getTrackingAddress(mip, layer);

    if (likely(m_info.type == VK_IMAGE_TYPE_2D))
      return base + bit::interleave(coord.x, coord.y);

    // For 1D we can simply use the pixel coordinate as-is
    if (m_info.type == VK_IMAGE_TYPE_1D)
      return base + coord.x;

    // 3D is uncommon, but there are different use cases. Assume that if the
    // format is block-compressed, the app will access one layer at a time.
    if (formatInfo()->flags.test(DxvkFormatFlag::BlockCompressed))
      return base + bit::interleave(coord.x, coord.y) + (uint64_t(coord.z) << 32u);

    // Otherwise, it may want to copy actual 3D blocks around.
    return base + bit::interleave(coord.x, coord.y, coord.z);
  }


  Rc<DxvkImageView> DxvkImage::createView(
    const DxvkImageViewKey& info) {
    std::unique_lock lock(m_viewMutex);

    auto entry = m_views.emplace(std::piecewise_construct,
      std::make_tuple(info), std::make_tuple(this, info));

    return &entry.first->second;
  }


  Rc<DxvkResourceAllocation> DxvkImage::allocateStorage() {
    return allocateStorageWithUsage(DxvkImageUsageInfo(), 0u);
  }


  Rc<DxvkResourceAllocation> DxvkImage::allocateStorageWithUsage(
    const DxvkImageUsageInfo&         usageInfo,
          DxvkAllocationModes         mode) {
    const DxvkFormatInfo* formatInfo = lookupFormatInfo(m_info.format);
    small_vector<VkFormat, 4> localViewFormats;

    VkImageCreateInfo imageInfo = getImageCreateInfo(usageInfo);

    // Set up view format list so that drivers can better enable
    // compression. Skip for planar formats due to validation errors.
    VkImageFormatListCreateInfo formatList = { VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO };

    if (!(formatInfo->aspectMask & VK_IMAGE_ASPECT_PLANE_0_BIT)) {
      if (usageInfo.viewFormatCount) {
        for (uint32_t i = 0; i < m_viewFormats.size(); i++)
          localViewFormats.push_back(m_viewFormats[i]);

        for (uint32_t i = 0; i < usageInfo.viewFormatCount; i++) {
          if (!isViewCompatible(usageInfo.viewFormats[i]))
            localViewFormats.push_back(usageInfo.viewFormats[i]);
        }

        formatList.viewFormatCount = localViewFormats.size();
        formatList.pViewFormats = localViewFormats.data();
      } else {
        formatList.viewFormatCount = m_viewFormats.size();
        formatList.pViewFormats = m_viewFormats.data();
      }
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
    VkImportMemoryWin32HandleInfoKHR sharedImportWin32 = { VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR };

    if (m_shared && m_info.sharing.mode == DxvkSharedHandleMode::Export) {
      sharedExport.pNext = std::exchange(sharedMemoryInfo, &sharedExport);
      sharedExport.handleTypes = m_info.sharing.type;
    }

    if (m_shared && m_info.sharing.mode == DxvkSharedHandleMode::Import) {
      sharedImportWin32.pNext = std::exchange(sharedMemoryInfo, &sharedImportWin32);
      sharedImportWin32.handleType = m_info.sharing.type;
      sharedImportWin32.handle = m_info.sharing.handle;
    }

    DxvkAllocationInfo allocationInfo = { };
    allocationInfo.resourceCookie = cookie();
    allocationInfo.properties = m_properties;
    allocationInfo.mode = mode;

    if (m_info.transient)
      allocationInfo.mode.set(DxvkAllocationMode::NoDedicated);

    return m_allocator->createImageResource(imageInfo,
      allocationInfo, sharedMemoryInfo);
  }


  Rc<DxvkResourceAllocation> DxvkImage::assignStorage(
          Rc<DxvkResourceAllocation>&& resource) {
    return assignStorageWithUsage(std::move(resource), DxvkImageUsageInfo());
  }


  Rc<DxvkResourceAllocation> DxvkImage::assignStorageWithUsage(
          Rc<DxvkResourceAllocation>&& resource,
    const DxvkImageUsageInfo&         usageInfo) {
    Rc<DxvkResourceAllocation> old = std::move(m_storage);

    // Self-assignment is possible here if we
    // just update the image properties
    bool invalidateViews = false;
    m_storage = std::move(resource);

    if (m_storage != old) {
      m_imageInfo = m_storage->getImageInfo();

      if (unlikely(m_info.debugName))
        updateDebugName();

      invalidateViews = true;
    }

    if ((m_info.access | usageInfo.access) != m_info.access)
      invalidateViews = true;

    m_info.flags |= usageInfo.flags;
    m_info.usage |= usageInfo.usage;
    m_info.stages |= usageInfo.stages;
    m_info.access |= usageInfo.access;

    if (usageInfo.layout != VK_IMAGE_LAYOUT_UNDEFINED) {
      m_info.layout = usageInfo.layout;
      invalidateViews = true;
    }

    if (usageInfo.colorSpace != VK_COLOR_SPACE_MAX_ENUM_KHR)
      m_info.colorSpace = usageInfo.colorSpace;

    for (uint32_t i = 0; i < usageInfo.viewFormatCount; i++) {
      if (!isViewCompatible(usageInfo.viewFormats[i]))
        m_viewFormats.push_back(usageInfo.viewFormats[i]);
    }

    if (!m_viewFormats.empty()) {
      m_info.viewFormatCount = m_viewFormats.size();
      m_info.viewFormats = m_viewFormats.data();
    }

    m_stableAddress |= usageInfo.stableGpuAddress;

    if (invalidateViews)
      m_version += 1u;

    return old;
  }


  void DxvkImage::trackInitialization(
    const VkImageSubresourceRange& subresources) {
    if (!m_uninitializedSubresourceCount)
      return;

    if (subresources.levelCount == m_info.mipLevels && subresources.layerCount == m_info.numLayers) {
      // Trivial case, everything gets initialized at once
      m_uninitializedSubresourceCount = 0u;
      m_uninitializedMipsPerLayer.clear();
    } else {
      // Partial initialization. Track each layer individually.
      if (m_uninitializedMipsPerLayer.empty()) {
        m_uninitializedMipsPerLayer.resize(m_info.numLayers);

        for (uint32_t i = 0; i < m_info.numLayers; i++)
          m_uninitializedMipsPerLayer[i] = uint16_t(1u << m_info.mipLevels) - 1u;
      }

      uint16_t mipMask = ((1u << subresources.levelCount) - 1u) << subresources.baseMipLevel;

      for (uint32_t i = subresources.baseArrayLayer; i < subresources.baseArrayLayer + subresources.layerCount; i++) {
        m_uninitializedSubresourceCount -= bit::popcnt(uint16_t(m_uninitializedMipsPerLayer[i] & mipMask));
        m_uninitializedMipsPerLayer[i] &= ~mipMask;
      }

      if (!m_uninitializedSubresourceCount)
        m_uninitializedMipsPerLayer.clear();
    }
  }


  bool DxvkImage::isInitialized(
    const VkImageSubresourceRange& subresources) const {
    if (likely(!m_uninitializedSubresourceCount))
      return true;

    if (m_uninitializedMipsPerLayer.empty())
      return false;

    uint16_t mipMask = ((1u << subresources.levelCount) - 1u) << subresources.baseMipLevel;

    for (uint32_t i = 0; i < subresources.layerCount; i++) {
      if (m_uninitializedMipsPerLayer[subresources.baseArrayLayer + i] & mipMask)
        return false;
    }

    return true;
  }


  void DxvkImage::setDebugName(const char* name) {
    if (likely(!m_info.debugName))
      return;

    m_debugName = createDebugName(name);
    m_info.debugName = m_debugName.c_str();

    updateDebugName();
  }


  void DxvkImage::updateDebugName() {
    if (m_storage->flags().test(DxvkAllocationFlag::OwnsImage)) {
      VkDebugUtilsObjectNameInfoEXT nameInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
      nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
      nameInfo.objectHandle = vk::getObjectHandle(m_imageInfo.image);
      nameInfo.pObjectName = m_info.debugName;

      m_vkd->vkSetDebugUtilsObjectNameEXT(m_vkd->device(), &nameInfo);
    }
  }


  std::string DxvkImage::createDebugName(const char* name) const {
    return str::format(vk::isValidDebugName(name) ? name : "Image", " (", cookie(), ")");
  }


  VkImageCreateInfo DxvkImage::getImageCreateInfo(
    const DxvkImageUsageInfo&         usageInfo) const {
    VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    info.flags = m_info.flags | usageInfo.flags;
    info.imageType = m_info.type;
    info.format = m_info.format;
    info.extent = m_info.extent;
    info.mipLevels = m_info.mipLevels;
    info.arrayLayers = m_info.numLayers;
    info.samples = m_info.sampleCount;
    info.tiling = m_info.tiling;
    info.usage = m_info.usage | usageInfo.usage;
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
  : m_image   (image),
    m_key     (key) {
    updateProperties();
  }


  DxvkImageView::~DxvkImageView() {

  }


  VkImageView DxvkImageView::createView(VkImageViewType type) const {
    constexpr VkImageUsageFlags ViewUsage =
      VK_IMAGE_USAGE_SAMPLED_BIT |
      VK_IMAGE_USAGE_STORAGE_BIT |
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    // Legalize view usage. We allow creating transfer-only view
    // objects so that some internal APIs can be more consistent.
    DxvkImageViewKey key = m_key;
    key.viewType = type;
    key.usage &= ViewUsage;

    if (!key.usage)
      return VK_NULL_HANDLE;

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

    // We need to expose RT and UAV swizzles to the backend,
    // but cannot legally pass them down to Vulkan
    if (key.usage != VK_IMAGE_USAGE_SAMPLED_BIT)
      key.packedSwizzle = 0u;

    return m_image->m_storage->createImageView(key);
  }


  void DxvkImageView::updateViews() {
    // Latch updated image properties
    updateProperties();

    // Update all views that are not currently null
    for (uint32_t i = 0; i < m_views.size(); i++) {
      if (m_views[i])
        m_views[i] = createView(VkImageViewType(i));
    }

    m_version = m_image->m_version;
  }


  void DxvkImageView::updateProperties() {
    m_properties.layout = m_image->info().layout;
    m_properties.samples = m_image->info().sampleCount;
    m_properties.access = m_image->info().access;
  }

}
