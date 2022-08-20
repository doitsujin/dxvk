#pragma once

#include "dxvk_memory.h"
#include "dxvk_resource.h"

namespace dxvk {

  class DxvkCommandList;
  class DxvkDevice;
  class DxvkBuffer;
  class DxvkImage;
  class DxvkSparsePage;
  class DxvkSparsePageAllocator;

  constexpr static VkDeviceSize SparseMemoryPageSize = 1ull << 16;

  /**
   * \brief Sparse page handle
   */
  struct DxvkSparsePageHandle {
    VkDeviceMemory  memory;
    VkDeviceSize    offset;
    VkDeviceSize    length;
  };


  /**
   * \brief Buffer info for sparse page
   *
   * Stores the buffer region backed by
   * any given page.
   */
  struct DxvkSparsePageBufferInfo {
    VkDeviceSize              offset;
    VkDeviceSize              length;
  };


  /**
   * \brief Image info for sparse page
   *
   * Stores the image region backed by
   * any given page.
   */
  struct DxvkSparsePageImageInfo {
    VkImageSubresource        subresource;
    VkOffset3D                offset;
    VkExtent3D                extent;
  };


  /**
   * \brief Image mip tail info for sparse page
   *
   * Stores the virtual resource offset and size
   * within the mip tail backed by any given page.
   */
  struct DxvkSparsePageMipTailInfo {
    VkDeviceSize              resourceOffset;
    VkDeviceSize              resourceLength;
  };


  /**
   * \brief Page type
   */
  enum class DxvkSparsePageType : uint32_t {
    None            = 0,
    Buffer          = 1,
    Image           = 2,
    ImageMipTail    = 3,
  };


  /**
   * \brief Sparse page table metadata
   *
   * Stores the resource region backed by any given page.
   */
  struct DxvkSparsePageInfo {
    DxvkSparsePageType type;
    union {
      DxvkSparsePageBufferInfo  buffer;
      DxvkSparsePageImageInfo   image;
      DxvkSparsePageMipTailInfo mipTail;
    };
  };


  /**
   * \brief Image tiling info
   */
  struct DxvkSparseImageProperties {
    VkSparseImageFormatFlags  flags;
    VkExtent3D                pageRegionExtent;
    uint32_t                  pagedMipCount;
    uint32_t                  metadataPageCount;
    uint32_t                  mipTailPageIndex;
    VkDeviceSize              mipTailOffset;
    VkDeviceSize              mipTailSize;
    VkDeviceSize              mipTailStride;
  };


  /**
   * \brief Image subresource tiling info
   */
  struct DxvkSparseImageSubresourceProperties {
    VkBool32                  isMipTail;
    VkExtent3D                pageCount;
    uint32_t                  pageIndex;
  };


  /**
   * \brief Sparse memory page
   *
   * Stores a single reference-counted page
   * of memory. The page size is 64k.
   */
  class DxvkSparsePage : public DxvkResource {

  public:

    DxvkSparsePage(DxvkMemory&& memory)
    : m_memory(std::move(memory)) { }

    /**
     * \brief Queries memory handle
     * \returns Memory information
     */
    DxvkSparsePageHandle getHandle() const {
      DxvkSparsePageHandle result;
      result.memory = m_memory.memory();
      result.offset = m_memory.offset();
      result.length = m_memory.length();
      return result;
    }

  private:

    DxvkMemory  m_memory;

  };


  /**
   * \brief Sparse page mapping
   *
   * Stores a reference to a page as well as the pool that the page
   * was allocated from, and automatically manages the use counter
   * of the pool as the reference is being moved or copied around.
   */
  class DxvkSparseMapping {
    friend DxvkSparsePageAllocator;
  public:

    DxvkSparseMapping();

    DxvkSparseMapping(DxvkSparseMapping&& other);
    DxvkSparseMapping(const DxvkSparseMapping& other);

    DxvkSparseMapping& operator = (DxvkSparseMapping&& other);
    DxvkSparseMapping& operator = (const DxvkSparseMapping& other);

    ~DxvkSparseMapping();

    Rc<DxvkSparsePage> getPage() const {
      return m_page;
    }

    bool operator == (const DxvkSparseMapping& other) const {
      // Pool is a function of the page, so no need to check both
      return m_page == other.m_page;
    }

    bool operator != (const DxvkSparseMapping& other) const {
      return m_page != other.m_page;
    }

    operator bool () const {
      return m_page != nullptr;
    }

  private:

    Rc<DxvkSparsePageAllocator> m_pool;
    Rc<DxvkSparsePage>          m_page;

    DxvkSparseMapping(
            Rc<DxvkSparsePageAllocator> allocator,
            Rc<DxvkSparsePage>          page);

    void acquire() const;

    void release() const;

  };


  /**
   * \brief Sparse memory allocator
   *
   * Provides an allocator for sparse pages with variable capacity.
   * Pages are use-counted to make sure they are not removed from
   * the allocator too early.
   */
  class DxvkSparsePageAllocator : public RcObject {
    friend DxvkSparseMapping;
  public:

    DxvkSparsePageAllocator(
            DxvkDevice*           device,
            DxvkMemoryAllocator&  memoryAllocator);

    ~DxvkSparsePageAllocator();

    /**
     * \brief Acquires page at the given offset
     *
     * If the offset is valid, this will atomically
     * increment the allocator's use count and return
     * a reference to the page.
     * \param [in] page Page index
     * \returns Page mapping object
     */
    DxvkSparseMapping acquirePage(
            uint32_t              page);

    /**
     * \brief Changes the allocator's maximum capacity
     *
     * Allocates new pages as necessary, and frees existing
     * pages if none of the pages are currently in use.
     * \param [in] pageCount New capacity, in pages
     */
    void setCapacity(
            uint32_t              pageCount);

  private:

    DxvkDevice*                       m_device;
    DxvkMemoryAllocator*              m_memory;

    dxvk::mutex                       m_mutex;
    uint32_t                          m_pageCount = 0u;
    uint32_t                          m_useCount = 0u;
    std::vector<Rc<DxvkSparsePage>>   m_pages;

    Rc<DxvkSparsePage> allocPage();

    void acquirePage(
      const Rc<DxvkSparsePage>&   page);

    void releasePage(
      const Rc<DxvkSparsePage>&   page);

  };


  /**
   * \brief Sparse page table
   *
   * Stores mappings from a resource region to a given memory page,
   * as well as mapping tile indices to the given resource region.
   */
  class DxvkSparsePageTable {

  public:

    DxvkSparsePageTable();

    DxvkSparsePageTable(
            DxvkDevice*             device,
      const DxvkBuffer*             buffer);

    DxvkSparsePageTable(
            DxvkDevice*             device,
      const DxvkImage*              image);

    /**
     * \brief Checks whether page table is defined
     * \returns \c true if the page table is defined
     */
    operator bool () const {
      return m_buffer || m_image;
    }

    /**
     * \brief Counts total number of pages in the resources
     *
     * Counts the number of pages for the entire resource, both
     * for paged subresources as well as the mip tail.
     * \returns Total number of pages
     */
    uint32_t getPageCount() const {
      return uint32_t(m_metadata.size());
    }

    /**
     * \brief Counts number of subresource infos
     * \returns Subresource info count
     */
    uint32_t getSubresourceCount() const {
      return uint32_t(m_subresources.size());
    }

    /**
     * \brief Retrieves image properties
     *
     * Only contains meaningful info if the page
     * table object was created for an image.
     * \returns Image properties
     */
    DxvkSparseImageProperties getProperties() const {
      return m_properties;
    }

    /**
     * \brief Retrieves image subresource properties
     *
     * \param [in] subresource The subresource to query
     * \returns Properties of the given subresource
     */
    DxvkSparseImageSubresourceProperties getSubresourceProperties(uint32_t subresource) const {
      return subresource < getSubresourceCount()
        ? m_subresources[subresource]
        : DxvkSparseImageSubresourceProperties();
    }

    /**
     * \brief Queries info for a given page
     *
     * \param [in] page Page index
     * \returns Page info
     */
    DxvkSparsePageInfo getPageInfo(uint32_t page) const {
      return page < getPageCount()
        ? m_metadata[page]
        : DxvkSparsePageInfo();
    }

  private:

    const DxvkBuffer* m_buffer  = nullptr;
    const DxvkImage*  m_image   = nullptr;

    DxvkSparseImageProperties                         m_properties    = { };
    std::vector<DxvkSparseImageSubresourceProperties> m_subresources;
    std::vector<DxvkSparsePageInfo>                   m_metadata;
    std::vector<DxvkSparseMapping>                    m_mappings;

  };


  /**
   * \brief Paged resource
   *
   * Base class for any resource that can
   * hold a sparse page table.
   */
  class DxvkPagedResource : public DxvkResource {

  public:

    /**
     * \brief Queries sparse page table
     * \returns Sparse page table, if defined
     */
    DxvkSparsePageTable* getSparsePageTable() {
      return m_sparsePageTable
        ? &m_sparsePageTable
        : nullptr;
    }

  protected:

    DxvkSparsePageTable m_sparsePageTable;

  };

}