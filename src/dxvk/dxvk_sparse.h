#pragma once

#include "dxvk_memory.h"
#include "dxvk_resource.h"

namespace dxvk {

  class DxvkCommandList;
  class DxvkDevice;
  class DxvkBuffer;
  class DxvkImage;
  class DxvkSparsePage;
  class DxvkSparsePagePool;

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

  };

}