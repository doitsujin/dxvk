#include <sstream>

#include "dxvk_buffer.h"
#include "dxvk_device.h"
#include "dxvk_image.h"
#include "dxvk_sparse.h"

namespace dxvk {

  std::atomic<uint64_t> DxvkPagedResource::s_cookie = { 0u };


  DxvkPagedResource::~DxvkPagedResource() {

  }


  DxvkResourceRef::~DxvkResourceRef() {
    auto resource = reinterpret_cast<DxvkPagedResource*>(m_ptr & ~AccessMask);
    resource->release(DxvkAccess(m_ptr & AccessMask));
  }


  DxvkSparseMapping::DxvkSparseMapping()
  : m_pool(nullptr),
    m_page(nullptr) {

  }


  DxvkSparseMapping::DxvkSparseMapping(
          Rc<DxvkSparsePageAllocator> allocator,
          Rc<DxvkResourceAllocation>  page)
  : m_pool(std::move(allocator)),
    m_page(std::move(page)) {

  }


  DxvkSparseMapping::DxvkSparseMapping(
          DxvkSparseMapping&&         other)
  : m_pool(std::move(other.m_pool)),
    m_page(std::move(other.m_page)) {
    // No need to acquire here. The only place from which
    // this constructor can be called does this atomically.
  }


  DxvkSparseMapping::DxvkSparseMapping(
    const DxvkSparseMapping&          other)
  : m_pool(other.m_pool),
    m_page(other.m_page) {
    this->acquire();
  }


  DxvkSparseMapping& DxvkSparseMapping::operator = (
          DxvkSparseMapping&&         other) {
    this->release();

    m_pool = std::move(other.m_pool);
    m_page = std::move(other.m_page);
    return *this;
  }


  DxvkSparseMapping& DxvkSparseMapping::operator = (
    const DxvkSparseMapping&          other) {
    other.acquire();
    this->release();

    m_pool = other.m_pool;
    m_page = other.m_page;
    return *this;
  }


  DxvkSparseMapping::~DxvkSparseMapping() {
    this->release();
  }


  void DxvkSparseMapping::acquire() const {
    if (m_page != nullptr)
      m_pool->acquirePage(m_page);
  }


  void DxvkSparseMapping::release() const {
    if (m_page != nullptr)
      m_pool->releasePage(m_page);
  }


  DxvkSparsePageAllocator::DxvkSparsePageAllocator(
          DxvkMemoryAllocator&  memoryAllocator)
  : m_memory(&memoryAllocator) {

  }


  DxvkSparsePageAllocator::~DxvkSparsePageAllocator() {

  }


  DxvkSparseMapping DxvkSparsePageAllocator::acquirePage(
          uint32_t              page) {
    std::lock_guard lock(m_mutex);

    if (unlikely(page >= m_pageCount))
      return DxvkSparseMapping();

    m_useCount += 1;
    return DxvkSparseMapping(this, m_pages[page]);
  }


  void DxvkSparsePageAllocator::setCapacity(
          uint32_t              pageCount) {
    std::lock_guard lock(m_mutex);

    if (pageCount < m_pageCount) {
      if (!m_useCount)
        m_pages.resize(pageCount);
    } else if (pageCount > m_pageCount) {
      std::vector<Rc<DxvkResourceAllocation>> newPages;
      newPages.reserve(pageCount - m_pageCount);

      for (size_t i = 0; i < pageCount - m_pageCount; i++)
        newPages.push_back(m_memory->createSparsePage());

      // Sort page by memory and offset to enable more
      // batching opportunities during page table updates
      std::sort(newPages.begin(), newPages.end(),
        [] (const Rc<DxvkResourceAllocation>& a, const Rc<DxvkResourceAllocation>& b) {
          auto aHandle = a->getMemoryInfo();
          auto bHandle = b->getMemoryInfo();

          // Ignore length here, the offsets cannot be the same anyway.
          if (aHandle.memory < bHandle.memory) return true;
          if (aHandle.memory > bHandle.memory) return false;

          return aHandle.offset < bHandle.offset;
        });

      for (auto& page : newPages)
        m_pages.push_back(std::move(page));
    }

    m_pageCount = pageCount;
  }


  void DxvkSparsePageAllocator::acquirePage(
    const Rc<DxvkResourceAllocation>& page) {
    std::lock_guard lock(m_mutex);
    m_useCount += 1;
  }


  void DxvkSparsePageAllocator::releasePage(
    const Rc<DxvkResourceAllocation>& page) {
    std::lock_guard lock(m_mutex);
    m_useCount -= 1;

    if (!m_useCount)
      m_pages.resize(m_pageCount);
  }


  DxvkSparsePageTable::DxvkSparsePageTable() {

  }


  DxvkSparsePageTable::DxvkSparsePageTable(
          DxvkDevice*             device,
    const VkBufferCreateInfo&     bufferInfo,
          VkBuffer                bufferHandle)
  : m_buffer(bufferHandle) {
    VkDeviceSize bufferSize = bufferInfo.size;

    // For linear buffers, the mapping is very simple
    // and consists of consecutive 64k pages
    size_t pageCount = align(bufferSize, SparseMemoryPageSize) / SparseMemoryPageSize;
    m_metadata.resize(pageCount);
    m_mappings.resize(pageCount);

    for (size_t i = 0; i < pageCount; i++) {
      VkDeviceSize pageOffset = SparseMemoryPageSize * i;
      m_metadata[i].type = DxvkSparsePageType::Buffer;
      m_metadata[i].buffer.offset = pageOffset;
      m_metadata[i].buffer.length = std::min(SparseMemoryPageSize, bufferSize - pageOffset);
    }

    // Initialize properties and subresource info so that we can
    // easily query this without having to know the resource type
    m_subresources.resize(1);
    m_subresources[0].pageCount = { uint32_t(pageCount), 1u, 1u };
    m_subresources[0].pageIndex = 0;

    m_properties.pageRegionExtent = { uint32_t(SparseMemoryPageSize), 1u, 1u };
  }


  DxvkSparsePageTable::DxvkSparsePageTable(
          DxvkDevice*             device,
    const VkImageCreateInfo&      imageInfo,
          VkImage                 imageHandle)
  : m_image(imageHandle) {
    auto vk = device->vkd();

    // Query sparse memory requirements
    uint32_t reqCount = 0;
    vk->vkGetImageSparseMemoryRequirements(vk->device(), imageHandle, &reqCount, nullptr);

    std::vector<VkSparseImageMemoryRequirements> req(reqCount);
    vk->vkGetImageSparseMemoryRequirements(vk->device(), imageHandle, &reqCount, req.data());

    // Find first non-metadata struct and use it to fill in the image properties
    bool foundMainAspect = false;

    for (const auto& r : req) {
      if (r.formatProperties.aspectMask & VK_IMAGE_ASPECT_METADATA_BIT) {
        VkDeviceSize metadataSize = r.imageMipTailSize;

        if (!(r.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT))
          metadataSize *= imageInfo.arrayLayers;

        m_properties.metadataPageCount += uint32_t(metadataSize / SparseMemoryPageSize);
      } else if (!foundMainAspect) {
        m_properties.flags = r.formatProperties.flags;
        m_properties.pageRegionExtent = r.formatProperties.imageGranularity;

        if (r.imageMipTailFirstLod < imageInfo.mipLevels && r.imageMipTailSize) {
          m_properties.pagedMipCount = r.imageMipTailFirstLod;
          m_properties.mipTailOffset = r.imageMipTailOffset;
          m_properties.mipTailSize = r.imageMipTailSize;
          m_properties.mipTailStride = r.imageMipTailStride;
        } else {
          m_properties.pagedMipCount = imageInfo.mipLevels;
        }

        foundMainAspect = true;
      } else {
        Logger::err(str::format("Found multiple aspects for sparse image:"
          "\n  Type:            ", imageInfo.imageType,
          "\n  Format:          ", imageInfo.format,
          "\n  Flags:           ", imageInfo.flags,
          "\n  Extent:          ", "(", imageInfo.extent.width,
                                  ",", imageInfo.extent.height,
                                  ",", imageInfo.extent.depth, ")",
          "\n  Mip levels:      ", imageInfo.mipLevels,
          "\n  Array layers:    ", imageInfo.arrayLayers,
          "\n  Samples:         ", imageInfo.samples,
          "\n  Usage:           ", imageInfo.usage,
          "\n  Tiling:          ", imageInfo.tiling));
      }
    }

    // Fill in subresource metadata and compute page count
    uint32_t totalPageCount = 0;
    uint32_t subresourceCount = imageInfo.arrayLayers * imageInfo.mipLevels;
    m_subresources.reserve(subresourceCount);

    for (uint32_t l = 0; l < imageInfo.arrayLayers; l++) {
      for (uint32_t m = 0; m < imageInfo.mipLevels; m++) {
        if (m < m_properties.pagedMipCount) {
          // Compute block count for current mip based on image properties
          VkExtent3D mipExtent = util::computeMipLevelExtent(imageInfo.extent, m);

          DxvkSparseImageSubresourceProperties subresourceInfo;
          subresourceInfo.isMipTail = VK_FALSE;
          subresourceInfo.pageCount = util::computeBlockCount(mipExtent, m_properties.pageRegionExtent);

          // Advance total page count by number of pages in the subresource
          subresourceInfo.pageIndex = totalPageCount;
          totalPageCount += util::flattenImageExtent(subresourceInfo.pageCount);

          m_subresources.push_back(subresourceInfo);
        } else {
          DxvkSparseImageSubresourceProperties subresourceInfo = { };
          subresourceInfo.isMipTail = VK_TRUE;
          subresourceInfo.pageCount = { 0u, 0u, 0u };
          subresourceInfo.pageIndex = 0u;
          m_subresources.push_back(subresourceInfo);
        }
      }
    }

    if (m_properties.mipTailSize) {
      m_properties.mipTailPageIndex = totalPageCount;

      // We may need multiple mip tails for the image
      uint32_t mipTailPageCount = m_properties.mipTailSize / SparseMemoryPageSize;

      if (!(m_properties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT))
        mipTailPageCount *= imageInfo.arrayLayers;

      totalPageCount += mipTailPageCount;
    }

    // Fill in page metadata
    m_metadata.reserve(totalPageCount);
    m_mappings.resize(totalPageCount);

    const DxvkFormatInfo* formatInfo = lookupFormatInfo(imageInfo.format);

    for (uint32_t l = 0; l < imageInfo.arrayLayers; l++) {
      for (uint32_t m = 0; m < m_properties.pagedMipCount; m++) {
        VkExtent3D mipExtent = util::computeMipLevelExtent(imageInfo.extent, m);
        VkExtent3D pageCount = util::computeBlockCount(mipExtent, m_properties.pageRegionExtent);

        for (uint32_t z = 0; z < pageCount.depth; z++) {
          for (uint32_t y = 0; y < pageCount.height; y++) {
            for (uint32_t x = 0; x < pageCount.width; x++) {
              DxvkSparsePageInfo pageInfo;
              pageInfo.type = DxvkSparsePageType::Image;
              pageInfo.image.subresource.aspectMask = formatInfo->aspectMask;
              pageInfo.image.subresource.mipLevel = m;
              pageInfo.image.subresource.arrayLayer = l;
              pageInfo.image.offset.x = x * m_properties.pageRegionExtent.width;
              pageInfo.image.offset.y = y * m_properties.pageRegionExtent.height;
              pageInfo.image.offset.z = z * m_properties.pageRegionExtent.depth;
              pageInfo.image.extent.width  = std::min(m_properties.pageRegionExtent.width,  mipExtent.width  - pageInfo.image.offset.x);
              pageInfo.image.extent.height = std::min(m_properties.pageRegionExtent.height, mipExtent.height - pageInfo.image.offset.y);
              pageInfo.image.extent.depth  = std::min(m_properties.pageRegionExtent.depth,  mipExtent.depth  - pageInfo.image.offset.z);
              m_metadata.push_back(pageInfo);
            }
          }
        }
      }
    }

    if (m_properties.mipTailSize) {
      uint32_t pageCount = m_properties.mipTailSize / SparseMemoryPageSize;
      uint32_t layerCount = 1;

      if (!(m_properties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT))
        layerCount = imageInfo.arrayLayers;

      for (uint32_t i = 0; i < layerCount; i++) {
        for (uint32_t j = 0; j < pageCount; j++) {
          DxvkSparsePageInfo pageInfo;
          pageInfo.type = DxvkSparsePageType::ImageMipTail;
          pageInfo.mipTail.resourceOffset = m_properties.mipTailOffset
            + i * m_properties.mipTailStride
            + j * SparseMemoryPageSize;
          pageInfo.mipTail.resourceLength = SparseMemoryPageSize;
          m_metadata.push_back(pageInfo);
        }
      }
    }
  }


  VkBuffer DxvkSparsePageTable::getBufferHandle() const {
    return m_buffer;
  }


  VkImage DxvkSparsePageTable::getImageHandle() const {
    return m_image;
  }


  uint32_t DxvkSparsePageTable::computePageIndex(
          uint32_t                subresource,
          VkOffset3D              regionOffset,
          VkExtent3D              regionExtent,
          VkBool32                regionIsLinear,
          uint32_t                pageIndex) const {
    auto subresourceInfo = getSubresourceProperties(subresource);

    // The mip tail is always linear
    if (subresourceInfo.isMipTail)
      return m_properties.mipTailPageIndex + pageIndex;

    // Compute offset into the given subresource
    VkOffset3D pageOffset = regionOffset;

    if (!regionIsLinear) {
      pageOffset.x += (pageIndex % regionExtent.width);
      pageOffset.y += (pageIndex / regionExtent.width) % regionExtent.height;
      pageOffset.z += (pageIndex / regionExtent.width) / regionExtent.height;
      pageIndex = 0;
    }

    uint32_t result = subresourceInfo.pageIndex + pageOffset.x
      + subresourceInfo.pageCount.width * (pageOffset.y
      + subresourceInfo.pageCount.height * pageOffset.z);

    return result + pageIndex;
  }


  DxvkSparseMapping DxvkSparsePageTable::getMapping(
          uint32_t                page) {
    return page < m_mappings.size()
      ? m_mappings[page]
      : DxvkSparseMapping();
  }


  void DxvkSparsePageTable::updateMapping(
          DxvkCommandList*        cmd,
          uint32_t                page,
          DxvkSparseMapping&&     mapping) {
    if (m_mappings[page] != mapping) {
      if (m_mappings[page])
        cmd->track(m_mappings[page].m_page);

      m_mappings[page] = std::move(mapping);
    }
  }


  DxvkSparseBindSubmission::DxvkSparseBindSubmission() {

  }


  DxvkSparseBindSubmission::~DxvkSparseBindSubmission() {

  }


  void DxvkSparseBindSubmission::waitSemaphore(
          VkSemaphore             semaphore,
          uint64_t                value) {
    m_waitSemaphores.push_back(semaphore);
    m_waitSemaphoreValues.push_back(value);
  }


  void DxvkSparseBindSubmission::signalSemaphore(
          VkSemaphore             semaphore,
          uint64_t                value) {
    m_signalSemaphores.push_back(semaphore);
    m_signalSemaphoreValues.push_back(value);
  }


  void DxvkSparseBindSubmission::bindBufferMemory(
    const DxvkSparseBufferBindKey& key,
    const DxvkResourceMemoryInfo& memory) {
    m_bufferBinds.insert_or_assign(key, memory);
  }


  void DxvkSparseBindSubmission::bindImageMemory(
    const DxvkSparseImageBindKey& key,
    const DxvkResourceMemoryInfo& memory) {
    m_imageBinds.insert_or_assign(key, memory);
  }


  void DxvkSparseBindSubmission::bindImageOpaqueMemory(
    const DxvkSparseImageOpaqueBindKey& key,
    const DxvkResourceMemoryInfo& memory) {
    m_imageOpaqueBinds.insert_or_assign(key, memory);
  }


  VkResult DxvkSparseBindSubmission::submit(
          DxvkDevice*             device,
          VkQueue                 queue) {
    auto vk = device->vkd();

    DxvkSparseBufferBindArrays buffer;
    this->processBufferBinds(buffer);

    DxvkSparseImageBindArrays image;
    this->processImageBinds(image);

    DxvkSparseImageOpaqueBindArrays opaque;
    this->processOpaqueBinds(opaque);

    // The sparse binding API has never been updated to take the new
    // semaphore submission info structs, so we have to do this instead
    VkTimelineSemaphoreSubmitInfo timelineInfo = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
    timelineInfo.waitSemaphoreValueCount = m_waitSemaphoreValues.size();
    timelineInfo.pWaitSemaphoreValues = m_waitSemaphoreValues.data();
    timelineInfo.signalSemaphoreValueCount = m_signalSemaphoreValues.size();
    timelineInfo.pSignalSemaphoreValues = m_signalSemaphoreValues.data();

    VkBindSparseInfo bindInfo = { VK_STRUCTURE_TYPE_BIND_SPARSE_INFO };

    if (!m_waitSemaphores.empty()) {
      bindInfo.pNext = &timelineInfo;
      bindInfo.waitSemaphoreCount = m_waitSemaphores.size();
      bindInfo.pWaitSemaphores = m_waitSemaphores.data();
    }

    if (!buffer.infos.empty()) {
      bindInfo.bufferBindCount = buffer.infos.size();
      bindInfo.pBufferBinds = buffer.infos.data();
    }

    if (!opaque.infos.empty()) {
      bindInfo.imageOpaqueBindCount = opaque.infos.size();
      bindInfo.pImageOpaqueBinds = opaque.infos.data();
    }

    if (!image.infos.empty()) {
      bindInfo.imageBindCount = image.infos.size();
      bindInfo.pImageBinds = image.infos.data();
    }

    if (!m_signalSemaphores.empty()) {
      bindInfo.pNext = &timelineInfo;
      bindInfo.signalSemaphoreCount = m_signalSemaphores.size();
      bindInfo.pSignalSemaphores = m_signalSemaphores.data();
    }

    VkResult vr = vk->vkQueueBindSparse(queue, 1, &bindInfo, VK_NULL_HANDLE);

    if (vr) {
      Logger::err(str::format("Sparse binding failed: ", vr));
      this->logSparseBindingInfo(LogLevel::Error, &bindInfo);
    }

    this->reset();
    return vr;
  }


  void DxvkSparseBindSubmission::reset() {
    m_waitSemaphoreValues.clear();
    m_waitSemaphores.clear();
    m_signalSemaphoreValues.clear();
    m_signalSemaphores.clear();

    m_bufferBinds.clear();
    m_imageBinds.clear();
    m_imageOpaqueBinds.clear();
  }


  bool DxvkSparseBindSubmission::tryMergeMemoryBind(
          VkSparseMemoryBind&               oldBind,
    const VkSparseMemoryBind&               newBind) {
    if (newBind.memory != oldBind.memory || newBind.flags != oldBind.flags)
      return false;

    // The resource region must be consistent
    if (newBind.resourceOffset != oldBind.resourceOffset + oldBind.size)
      return false;

    // If memory is not null, the memory range must also be consistent
    if (newBind.memory && newBind.memoryOffset != oldBind.memoryOffset + oldBind.size)
      return false;

    oldBind.size += newBind.size;
    return true;
  }


  bool DxvkSparseBindSubmission::tryMergeImageBind(
          std::pair<DxvkSparseImageBindKey, DxvkResourceMemoryInfo>& oldBind,
    const std::pair<DxvkSparseImageBindKey, DxvkResourceMemoryInfo>& newBind) {
    if (oldBind.first.image != newBind.first.image
     || oldBind.first.subresource.aspectMask != newBind.first.subresource.aspectMask
     || oldBind.first.subresource.mipLevel != newBind.first.subresource.mipLevel
     || oldBind.first.subresource.arrayLayer != newBind.first.subresource.arrayLayer)
      return false;

    if (oldBind.second.memory != newBind.second.memory)
      return false;

    if (oldBind.second.memory) {
      if (oldBind.second.offset + oldBind.second.size != newBind.second.offset)
        return false;
    }

    bool canMerge = false;

    VkOffset3D oldOffset = oldBind.first.offset;
    VkExtent3D oldExtent = oldBind.first.extent;
    VkOffset3D newOffset = newBind.first.offset;
    VkExtent3D newExtent = newBind.first.extent;
    VkExtent3D delta = { };

    if (uint32_t(oldOffset.x + oldExtent.width) == uint32_t(newOffset.x)) {
      canMerge = oldOffset.y == newOffset.y && oldExtent.height == newExtent.height
              && oldOffset.z == newOffset.z && oldExtent.depth == newExtent.depth;
      delta.width = newExtent.width;
    } else if (uint32_t(oldOffset.y + oldExtent.height) == uint32_t(newOffset.y)) {
      canMerge = oldOffset.x == newOffset.x && oldExtent.width == newExtent.width
              && oldOffset.z == newOffset.z && oldExtent.depth == newExtent.depth;
      delta.height = newExtent.height;
    } else if (uint32_t(oldOffset.z + oldExtent.depth) == uint32_t(newOffset.z)) {
      canMerge = oldOffset.x == newOffset.x && oldExtent.width == newExtent.width
              && oldOffset.y == newOffset.y && oldExtent.height == newExtent.height;
      delta.depth = newExtent.depth;
    }

    if (canMerge) {
      oldBind.first.extent.width  += delta.width;
      oldBind.first.extent.height += delta.height;
      oldBind.first.extent.depth  += delta.depth;

      if (oldBind.second.memory)
        oldBind.second.size += newBind.second.size;
    }

    return canMerge;
  }


  void DxvkSparseBindSubmission::processBufferBinds(
          DxvkSparseBufferBindArrays&       buffer) {
    std::vector<std::pair<VkBuffer, VkSparseMemoryBind>> ranges;
    ranges.reserve(m_bufferBinds.size());

    for (const auto& e : m_bufferBinds) {
      const auto& key = e.first;
      const auto& handle = e.second;

      VkSparseMemoryBind bind = { };
      bind.resourceOffset = key.offset;
      bind.size           = key.size;
      bind.memory         = handle.memory;
      bind.memoryOffset   = handle.offset;

      bool merged = false;

      if (!ranges.empty() && ranges.back().first == key.buffer)
        merged = tryMergeMemoryBind(ranges.back().second, bind);

      if (!merged)
        ranges.push_back({ key.buffer, bind });
    }

    populateOutputArrays(buffer.binds, buffer.infos, ranges);
  }


  void DxvkSparseBindSubmission::processImageBinds(
          DxvkSparseImageBindArrays&        image) {
    std::vector<std::pair<DxvkSparseImageBindKey, DxvkResourceMemoryInfo>> binds;
    binds.reserve(m_imageBinds.size());

    for (const auto& e : m_imageBinds) {
      std::pair<DxvkSparseImageBindKey, DxvkResourceMemoryInfo> newBind = e;

      while (!binds.empty()) {
        std::pair<DxvkSparseImageBindKey, DxvkResourceMemoryInfo> oldBind = binds.back();

        if (!tryMergeImageBind(oldBind, newBind))
          break;

        newBind = oldBind;
        binds.pop_back();
      }

      binds.push_back(newBind);
    }

    std::vector<std::pair<VkImage, VkSparseImageMemoryBind>> ranges;
    ranges.reserve(m_imageBinds.size());

    for (const auto& e : binds) {
      const auto& key = e.first;
      const auto& handle = e.second;

      VkSparseImageMemoryBind bind = { };
      bind.subresource    = key.subresource;
      bind.offset         = key.offset;
      bind.extent         = key.extent;
      bind.memory         = handle.memory;
      bind.memoryOffset   = handle.offset;

      ranges.push_back({ key.image, bind });
    }

    populateOutputArrays(image.binds, image.infos, ranges);
  }


  void DxvkSparseBindSubmission::processOpaqueBinds(
          DxvkSparseImageOpaqueBindArrays&  opaque) {
    std::vector<std::pair<VkImage, VkSparseMemoryBind>> ranges;
    ranges.reserve(m_imageOpaqueBinds.size());

    for (const auto& e : m_imageOpaqueBinds) {
      const auto& key = e.first;
      const auto& handle = e.second;

      VkSparseMemoryBind bind = { };
      bind.resourceOffset = key.offset;
      bind.size           = key.size;
      bind.memory         = handle.memory;
      bind.memoryOffset   = handle.offset;
      bind.flags          = key.flags;

      bool merged = false;

      if (!ranges.empty() && ranges.back().first == key.image)
        merged = tryMergeMemoryBind(ranges.back().second, bind);

      if (!merged)
        ranges.push_back({ key.image, bind });
    }

    populateOutputArrays(opaque.binds, opaque.infos, ranges);
  }


  template<typename HandleType, typename BindType, typename InfoType>
  void DxvkSparseBindSubmission::populateOutputArrays(
          std::vector<BindType>&            binds,
          std::vector<InfoType>&            infos,
    const std::vector<std::pair<HandleType, BindType>>& input) {
    HandleType handle = VK_NULL_HANDLE;

    // Resize bind array so that pointers remain
    // valid as we iterate over the input array
    binds.resize(input.size());

    for (size_t i = 0; i < input.size(); i++) {
      binds[i] = input[i].second;

      if (handle != input[i].first) {
        // Create new info entry if the handle
        // differs from that of the previous entry
        handle = input[i].first;
        infos.push_back({ handle, 1u, &binds[i] });
      } else {
        // Otherwise just increment the bind count
        infos.back().bindCount += 1;
      }
    }
  }


  void DxvkSparseBindSubmission::logSparseBindingInfo(
          LogLevel                          level,
    const VkBindSparseInfo*                 info) {
    std::stringstream str;
    str << "VkBindSparseInfo:" << std::endl;

    auto timelineInfo = static_cast<const VkTimelineSemaphoreSubmitInfo*>(info->pNext);

    if (info->waitSemaphoreCount) {
      str << "  Wait semaphores (" << std::dec << info->waitSemaphoreCount << "):" << std::endl;

      for (uint32_t i = 0; i < info->waitSemaphoreCount; i++)
        str << "    " << info->pWaitSemaphores[i] << " (" << timelineInfo->pWaitSemaphoreValues[i] << ")" << std::endl;
    }

    if (info->bufferBindCount) {
      str << "  Buffer binds (" << std::dec << info->bufferBindCount << "):" << std::endl;

      for (uint32_t i = 0; i < info->bufferBindCount; i++) {
        const auto* bindInfo = &info->pBufferBinds[i];
        str << "    VkBuffer " << bindInfo->buffer << " (" << bindInfo->bindCount << "):" << std::endl;

        for (uint32_t j = 0; j < bindInfo->bindCount; j++) {
          const auto* bind = &bindInfo->pBinds[j];
          str << "        " << bind->resourceOffset << " -> " << bind->memory
              << " (" << bind->memoryOffset << "," << bind->size << ")" << std::endl;
        }
      }
    }

    if (info->imageOpaqueBindCount) {
      str << "  Opaque image binds (" << std::dec << info->imageOpaqueBindCount << "):" << std::endl;

      for (uint32_t i = 0; i < info->imageOpaqueBindCount; i++) {
        const auto* bindInfo = &info->pImageOpaqueBinds[i];
        str << "    VkImage " << bindInfo->image << " (" << bindInfo->bindCount << "):" << std::endl;

        for (uint32_t j = 0; j < bindInfo->bindCount; j++) {
          const auto* bind = &bindInfo->pBinds[j];
          str << "        " << bind->resourceOffset << " -> " << bind->memory
              << " (" << bind->memoryOffset << "," << bind->size << ")" << std::endl;
        }
      }
    }

    if (info->imageBindCount) {
      str << "  Opaque image binds (" << std::dec << info->imageOpaqueBindCount << "):" << std::endl;

      for (uint32_t i = 0; i < info->imageBindCount; i++) {
        const auto* bindInfo = &info->pImageBinds[i];
        str << "    VkImage " << bindInfo->image << " (" << bindInfo->bindCount << "):" << std::endl;

        for (uint32_t j = 0; j < bindInfo->bindCount; j++) {
          const auto* bind = &bindInfo->pBinds[j];

          str << "        Aspect 0x" << std::hex << bind->subresource.aspectMask
              << ", Mip " << std::dec << bind->subresource.mipLevel
              << ", Layer " << bind->subresource.arrayLayer
              << ":" << std::endl;

          str << "        " << bind->offset.x << "," << bind->offset.y << "," << bind->offset.z << ":"
              << bind->extent.width << "x" << bind->extent.height << "x" << bind->extent.depth
              << " -> " << bind->memory << " (" << bind->memoryOffset << ")" << std::endl;
        }
      }
    }

    if (info->signalSemaphoreCount) {
      str << "  Signal semaphores (" << std::dec << info->signalSemaphoreCount << "):" << std::endl;

      for (uint32_t i = 0; i < info->signalSemaphoreCount; i++)
        str << "    " << info->pSignalSemaphores[i] << " (" << timelineInfo->pSignalSemaphoreValues[i] << ")" << std::endl;
    }

    Logger::log(level, str.str());
  }

}
