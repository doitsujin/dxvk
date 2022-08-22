#include <sstream>

#include "dxvk_buffer.h"
#include "dxvk_device.h"
#include "dxvk_image.h"
#include "dxvk_sparse.h"

namespace dxvk {

  DxvkSparseMapping::DxvkSparseMapping()
  : m_pool(nullptr),
    m_page(nullptr) {

  }


  DxvkSparseMapping::DxvkSparseMapping(
          Rc<DxvkSparsePageAllocator> allocator,
          Rc<DxvkSparsePage>          page)
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
          DxvkDevice*           device,
          DxvkMemoryAllocator&  memoryAllocator)
  : m_device(device), m_memory(&memoryAllocator) {

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
      while (m_pages.size() < pageCount)
        m_pages.push_back(allocPage());
    }

    m_pageCount = pageCount;
  }


  Rc<DxvkSparsePage> DxvkSparsePageAllocator::allocPage() {
    DxvkMemoryRequirements memoryRequirements = { };
    memoryRequirements.core = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 };

    // We don't know what kind of resource the memory6
    // might be bound to, so just guess the memory types
    auto& core = memoryRequirements.core.memoryRequirements;
    core.size           = SparseMemoryPageSize;
    core.alignment      = SparseMemoryPageSize;
    core.memoryTypeBits = m_memory->getSparseMemoryTypes();

    DxvkMemoryProperties memoryProperties = { };
    memoryProperties.flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    DxvkMemory memory = m_memory->alloc(memoryRequirements,
      memoryProperties, DxvkMemoryFlag::GpuReadable);

    return new DxvkSparsePage(std::move(memory));
  }


  void DxvkSparsePageAllocator::acquirePage(
    const Rc<DxvkSparsePage>&   page) {
    std::lock_guard lock(m_mutex);
    m_useCount += 1;
  }


  void DxvkSparsePageAllocator::releasePage(
    const Rc<DxvkSparsePage>&   page) {
    std::lock_guard lock(m_mutex);
    m_useCount -= 1;

    if (!m_useCount)
      m_pages.resize(m_pageCount);
  }


  DxvkSparsePageTable::DxvkSparsePageTable() {

  }


  DxvkSparsePageTable::DxvkSparsePageTable(
          DxvkDevice*             device,
    const DxvkBuffer*             buffer)
  : m_buffer(buffer) {
    VkDeviceSize bufferSize = buffer->info().size;

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
    const DxvkImage*              image)
  : m_image(image) {
    auto vk = device->vkd();

    // Query sparse memory requirements
    uint32_t reqCount = 0;
    vk->vkGetImageSparseMemoryRequirements(vk->device(), image->handle(), &reqCount, nullptr);

    std::vector<VkSparseImageMemoryRequirements> req(reqCount);
    vk->vkGetImageSparseMemoryRequirements(vk->device(), image->handle(), &reqCount, req.data());

    // Find first non-metadata struct and use it to fill in the image properties
    bool foundMainAspect = false;

    for (const auto& r : req) {
      if (r.formatProperties.aspectMask & VK_IMAGE_ASPECT_METADATA_BIT) {
        VkDeviceSize metadataSize = r.imageMipTailSize;

        if (!(r.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT))
          metadataSize *= m_image->info().numLayers;

        m_properties.metadataPageCount += uint32_t(metadataSize / SparseMemoryPageSize);
      } else if (!foundMainAspect) {
        m_properties.flags = r.formatProperties.flags;
        m_properties.pageRegionExtent = r.formatProperties.imageGranularity;

        if (r.imageMipTailFirstLod < image->info().mipLevels && r.imageMipTailSize) {
          m_properties.pagedMipCount = r.imageMipTailFirstLod;
          m_properties.mipTailOffset = r.imageMipTailOffset;
          m_properties.mipTailSize = r.imageMipTailSize;
          m_properties.mipTailStride = r.imageMipTailStride;
        } else {
          m_properties.pagedMipCount = image->info().mipLevels;
        }

        foundMainAspect = true;
      } else {
        Logger::err(str::format("Found multiple aspects for sparse image:"
          "\n  Type:            ", image->info().type,
          "\n  Format:          ", image->info().format,
          "\n  Flags:           ", image->info().flags,
          "\n  Extent:          ", "(", image->info().extent.width,
                                  ",", image->info().extent.height,
                                  ",", image->info().extent.depth, ")",
          "\n  Mip levels:      ", image->info().mipLevels,
          "\n  Array layers:    ", image->info().numLayers,
          "\n  Samples:         ", image->info().sampleCount,
          "\n  Usage:           ", image->info().usage,
          "\n  Tiling:          ", image->info().tiling));
      }
    }

    // Fill in subresource metadata and compute page count
    uint32_t totalPageCount = 0;
    uint32_t subresourceCount = image->info().numLayers * image->info().mipLevels;
    m_subresources.reserve(subresourceCount);

    for (uint32_t l = 0; l < image->info().numLayers; l++) {
      for (uint32_t m = 0; m < image->info().mipLevels; m++) {
        if (m < m_properties.pagedMipCount) {
          // Compute block count for current mip based on image properties
          DxvkSparseImageSubresourceProperties subresourceInfo;
          subresourceInfo.isMipTail = VK_FALSE;
          subresourceInfo.pageCount = util::computeBlockCount(
            image->mipLevelExtent(m), m_properties.pageRegionExtent);

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
        mipTailPageCount *= m_image->info().numLayers;

      totalPageCount += mipTailPageCount;
    }

    // Fill in page metadata
    m_metadata.reserve(totalPageCount);
    m_mappings.resize(totalPageCount);

    for (uint32_t l = 0; l < image->info().numLayers; l++) {
      for (uint32_t m = 0; m < m_properties.pagedMipCount; m++) {
        VkExtent3D mipExtent = image->mipLevelExtent(m);
        VkExtent3D pageCount = util::computeBlockCount(mipExtent, m_properties.pageRegionExtent);

        for (uint32_t z = 0; z < pageCount.depth; z++) {
          for (uint32_t y = 0; y < pageCount.height; y++) {
            for (uint32_t x = 0; x < pageCount.width; x++) {
              DxvkSparsePageInfo pageInfo;
              pageInfo.type = DxvkSparsePageType::Image;
              pageInfo.image.subresource.aspectMask = image->formatInfo()->aspectMask;
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
        layerCount = image->info().numLayers;

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
    return m_buffer ? m_buffer->getSliceHandle().handle : VK_NULL_HANDLE;
  }


  VkImage DxvkSparsePageTable::getImageHandle() const {
    return m_image ? m_image->handle() : VK_NULL_HANDLE;
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
    if (m_mappings[page] != page) {
      if (m_mappings[page])
        cmd->trackResource<DxvkAccess::None>(m_mappings[page].m_page);

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
    const DxvkSparsePageHandle&   memory) {
    m_bufferBinds.insert_or_assign(key, memory);
  }


  void DxvkSparseBindSubmission::bindImageMemory(
    const DxvkSparseImageBindKey& key,
    const DxvkSparsePageHandle&   memory) {
    m_imageBinds.insert_or_assign(key, memory);
  }


  void DxvkSparseBindSubmission::bindImageOpaqueMemory(
    const DxvkSparseImageOpaqueBindKey& key,
    const DxvkSparsePageHandle&   memory) {
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
    std::vector<std::pair<VkImage, VkSparseImageMemoryBind>> ranges;
    ranges.reserve(m_imageBinds.size());

    for (const auto& e : m_imageBinds) {
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
