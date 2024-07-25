#pragma once

#include <map>

#include "dxvk_memory.h"
#include "dxvk_resource.h"

namespace dxvk {

  class DxvkCommandList;
  class DxvkDevice;
  class DxvkBuffer;
  class DxvkImage;
  class DxvkPagedResource;
  class DxvkSparsePage;
  class DxvkSparsePageAllocator;
  class DxvkSparsePageTable;

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
   * \brief Sparse binding flags
   */
  enum class DxvkSparseBindFlag : uint32_t {
    SkipSynchronization,
  };

  using DxvkSparseBindFlags = Flags<DxvkSparseBindFlag>;


  /**
   * \brief Sparse page binding mode
   */
  enum class DxvkSparseBindMode : uint32_t {
    Null, ///< Unbind the given resource page
    Bind, ///< Bind to given allocator page
    Copy, ///< Copy bindig from source resource
  };


  /**
   * \brief Sparse page binding info for a given page
   *
   * Stores the resource page index as well as the index
   * of the allocator page that should be bound to that
   * resource page.
   */
  struct DxvkSparseBind {
    DxvkSparseBindMode        mode;
    uint32_t                  dstPage;
    uint32_t                  srcPage;
  };


  /**
   * \brief Sparse binding info
   *
   * Stores the resource to change page bindings for, the
   * allocator from which pages will be allocated, and
   * a list of page bidnings
   */
  struct DxvkSparseBindInfo {
    Rc<DxvkPagedResource>       dstResource;
    Rc<DxvkPagedResource>       srcResource;
    Rc<DxvkSparsePageAllocator> srcAllocator;
    std::vector<DxvkSparseBind> binds;
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
    friend DxvkSparsePageTable;
  public:

    DxvkSparseMapping();

    DxvkSparseMapping(DxvkSparseMapping&& other);
    DxvkSparseMapping(const DxvkSparseMapping& other);

    DxvkSparseMapping& operator = (DxvkSparseMapping&& other);
    DxvkSparseMapping& operator = (const DxvkSparseMapping& other);

    ~DxvkSparseMapping();

    /**
     * \brief Queries memory handle
     * \returns Memory information
     */
    DxvkSparsePageHandle getHandle() const {
      if (m_page == nullptr)
        return DxvkSparsePageHandle();

      return m_page->getHandle();
    }

    bool operator == (const DxvkSparseMapping& other) const {
      // Pool is a function of the page, so no need to check both
      return m_page == other.m_page;
    }

    bool operator != (const DxvkSparseMapping& other) const {
      return m_page != other.m_page;
    }

    explicit operator bool () const {
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
    explicit operator bool () const {
      return m_buffer || m_image;
    }

    /**
     * \brief Queries buffer handle
     * \returns Buffer handle
     */
    VkBuffer getBufferHandle() const;

    /**
     * \brief Queries image handle
     * \returns Image handle
     */
    VkImage getImageHandle() const;

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

    /**
     * \brief Computes page index within a given image region
     *
     * \param [in] subresource Subresource index
     * \param [in] regionOffset Region offset, in pages
     * \param [in] regionExtent Region extent, in pages
     * \param [in] regionIsLinear Whether to use the region extent
     * \param [in] pageIndex Page within the given region
     * \returns Page index. The returned number may be out
     *    of bounds if the given region is invalid.
     */
    uint32_t computePageIndex(
            uint32_t                subresource,
            VkOffset3D              regionOffset,
            VkExtent3D              regionExtent,
            VkBool32                regionIsLinear,
            uint32_t                pageIndex) const;

    /**
     * \brief Queries page mapping
     *
     * \param [in] page Page index
     * \returns Current page mapping
     */
    DxvkSparseMapping getMapping(
            uint32_t                page);

    /**
     * \brief Changes a page mapping
     *
     * Updates the given page mapping in the table, and ensures
     * that the previously mapped page does not get destroyed
     * prematurely by tracking it in the given command list.
     * \param [in] cmd Command list
     * \param [in] page Page index
     * \param [in] mapping New mapping
     */
    void updateMapping(
            DxvkCommandList*        cmd,
            uint32_t                page,
            DxvkSparseMapping&&     mapping);

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


  /**
   * \brief Key for sparse buffer binding entry
   *
   * Provides a strong ordering by resource, resource offset,
   * and finally, size. The ordering can be used to easily
   * merge adjacent ranges.
   */
  struct DxvkSparseBufferBindKey {
    VkBuffer                    buffer;
    VkDeviceSize                offset;
    VkDeviceSize                size;

    bool operator < (const DxvkSparseBufferBindKey& other) const {
      if (buffer < other.buffer) return true;
      if (buffer > other.buffer) return false;

      if (offset < other.offset) return true;
      if (offset > other.offset) return false;

      return size < other.size;
    }
  };


  /**
   * \brief Key for sparse image binding entry
   *
   * Provides a strong ordering by resource, subresource,
   * offset (z -> y -> x), and finally, extent (d -> h -> w).
   * The ordering can be used to easily merge adjacent regions.
   */
  struct DxvkSparseImageBindKey {
    VkImage                     image;
    VkImageSubresource          subresource;
    VkOffset3D                  offset;
    VkExtent3D                  extent;

    bool operator < (const DxvkSparseImageBindKey& other) const {
      if (image < other.image) return true;
      if (image > other.image) return false;

      uint64_t aSubresource = this->encodeSubresource();
      uint64_t bSubresource = other.encodeSubresource();

      if (aSubresource < bSubresource) return true;
      if (aSubresource > bSubresource) return false;

      uint64_t aOffset = this->encodeOffset();
      uint64_t bOffset = other.encodeOffset();

      if (aOffset < bOffset) return true;
      if (aOffset > bOffset) return false;

      uint64_t aExtent = this->encodeExtent();
      uint64_t bExtent = other.encodeExtent();

      return aExtent < bExtent;
    }

    uint64_t encodeSubresource() const {
      return uint64_t(subresource.aspectMask) << 48
           | uint64_t(subresource.arrayLayer) << 24
           | uint64_t(subresource.mipLevel);
    }

    uint64_t encodeOffset() const {
      return uint64_t(offset.z) << 48
           | uint64_t(offset.y) << 24
           | uint64_t(offset.x);
    }

    uint64_t encodeExtent() const {
      return uint64_t(extent.depth) << 48
           | uint64_t(extent.height) << 24
           | uint64_t(extent.width);
    }
  };


  /**
   * \brief Key for sparse opaque image binding entry
   *
   * Provides a strong ordering by resource, resource offset,
   * and finally, size. The ordering can be used to easily
   * merge adjacent ranges.
   */
  struct DxvkSparseImageOpaqueBindKey {
    VkImage                     image;
    VkDeviceSize                offset;
    VkDeviceSize                size;
    VkSparseMemoryBindFlags     flags;

    bool operator < (const DxvkSparseImageOpaqueBindKey& other) const {
      if (image < other.image) return true;
      if (image > other.image) return false;

      if (offset < other.offset) return true;
      if (offset > other.offset) return false;

      return size < other.size;
    }
  };


  /**
   * \brief Arrays required for buffer binds
   */
  struct DxvkSparseBufferBindArrays {
    std::vector<VkSparseMemoryBind> binds;
    std::vector<VkSparseBufferMemoryBindInfo> infos;
  };


  /**
   * \brief Arrays required for image binds
   */
  struct DxvkSparseImageBindArrays {
    std::vector<VkSparseImageMemoryBind> binds;
    std::vector<VkSparseImageMemoryBindInfo> infos;
  };


  /**
   * \brief Arrays required for opaque image binds
   */
  struct DxvkSparseImageOpaqueBindArrays {
    std::vector<VkSparseMemoryBind> binds;
    std::vector<VkSparseImageOpaqueMemoryBindInfo> infos;
  };


  /**
   * \brief Sparse bind submission
   *
   * Stores information for a single sparse binding operation,
   * and supports submitting that operation to a device queue.
   *
   * All methods to add bindings assume that the binding range is
   * either identical to an existing range, in which case the old
   * binding will be overwritten, or otherwise, that the range is
   * disjoint from all existing ranges. Overlapping ranges are not
   * supported. This condition is trivial to maintain when binding
   * only one sparse page at a time.
   */
  class DxvkSparseBindSubmission {

  public:

    DxvkSparseBindSubmission();

    ~DxvkSparseBindSubmission();

    /**
     * \brief Waits for a semaphore
     *
     * \param [in] semaphore Semaphore to wait for
     * \param [in] value Semaphore value to wait on
     */
    void waitSemaphore(
            VkSemaphore             semaphore,
            uint64_t                value);

    /**
     * \brief Signals a semaphore
     *
     * \param [in] semaphore Semaphore to signal
     * \param [in] value Calue to signal semaphore to
     */
    void signalSemaphore(
            VkSemaphore             semaphore,
            uint64_t                value);

    /**
     * \brief Adds a buffer memory bind
     *
     * \param [in] key Buffer range key
     * \param [in] memory Page handle
     */
    void bindBufferMemory(
      const DxvkSparseBufferBindKey& key,
      const DxvkSparsePageHandle&   memory);

    /**
     * \brief Adds an image memory bind
     *
     * \param [in] key Image region key
     * \param [in] memory Page handle
     */
    void bindImageMemory(
      const DxvkSparseImageBindKey& key,
      const DxvkSparsePageHandle&   memory);

    /**
     * \brief Adds an opaque image memory bind
     *
     * \param [in] key Opaque region key
     * \param [in] memory Page handle
     */
    void bindImageOpaqueMemory(
      const DxvkSparseImageOpaqueBindKey& key,
      const DxvkSparsePageHandle&   memory);

    /**
     * \brief Submits sparse binding operation
     *
     * Generates structures required for the sparse bind, resolving
     * any conflicts in the process and merging ranges where possible.
     * Note that this operation is slow. Resets object after the call.
     * \param [in] device DXVK device
     * \param [in] queue Queue to perform the operation on
     * \returns Return value of the sparse bind operation
     */
    VkResult submit(
            DxvkDevice*             device,
            VkQueue                 queue);

    /**
     * \brief Resets object
     *
     * Clears all internal structures.
     */
    void reset();

  private:

    std::vector<uint64_t>     m_waitSemaphoreValues;
    std::vector<VkSemaphore>  m_waitSemaphores;
    std::vector<uint64_t>     m_signalSemaphoreValues;
    std::vector<VkSemaphore>  m_signalSemaphores;

    std::map<DxvkSparseBufferBindKey,      DxvkSparsePageHandle> m_bufferBinds;
    std::map<DxvkSparseImageBindKey,       DxvkSparsePageHandle> m_imageBinds;
    std::map<DxvkSparseImageOpaqueBindKey, DxvkSparsePageHandle> m_imageOpaqueBinds;

    static bool tryMergeMemoryBind(
            VkSparseMemoryBind&               oldBind,
      const VkSparseMemoryBind&               newBind);

    static bool tryMergeImageBind(
            std::pair<DxvkSparseImageBindKey, DxvkSparsePageHandle>& oldBind,
      const std::pair<DxvkSparseImageBindKey, DxvkSparsePageHandle>& newBind);

    void processBufferBinds(
            DxvkSparseBufferBindArrays&       buffer);

    void processImageBinds(
            DxvkSparseImageBindArrays&        image);

    void processOpaqueBinds(
            DxvkSparseImageOpaqueBindArrays&  opaque);

    template<typename HandleType, typename BindType, typename InfoType>
    void populateOutputArrays(
            std::vector<BindType>&            binds,
            std::vector<InfoType>&            infos,
      const std::vector<std::pair<HandleType, BindType>>& input);

    void logSparseBindingInfo(
            LogLevel                          level,
      const VkBindSparseInfo*                 info);

  };

}
