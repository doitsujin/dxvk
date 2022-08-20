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

}
