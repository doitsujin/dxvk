#pragma once

#include <utility>
#include <vector>

#include "dxvk_buffer.h"
#include "dxvk_cmdlist.h"
#include "dxvk_image.h"

namespace dxvk {

  /**
   * \brief Resource access info
   *
   * Stores which parts of a resource are accessed,
   * as well as the respective access types.
   */
  struct DxvkResourceUse {
    DxvkBuffer* buffer = nullptr;
    VkDeviceSize bufferOffset = 0u;
    VkDeviceSize bufferSize = 0u;

    DxvkImage* image = nullptr;
    VkImageSubresourceRange imageSubresource = { };
    VkOffset3D imageOffset = { };
    VkExtent3D imageExtent = { };
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkPipelineStageFlags2 stages = 0u;
    VkAccessFlags2 access = 0u;

    VkBool32 discard = VK_FALSE;
  };


  /**
   * \brief Resource access batch
   *
   * Array wrapper with some convenience methods
   * to add resource use infos.
   */
  class DxvkResourceBatch {

  public:

    DxvkResourceUse& add() {
      return m_resources.emplace_back();
    }

    DxvkResourceUse& add(DxvkBuffer& buffer, VkDeviceSize offset, VkDeviceSize size,
      VkPipelineStageFlags2 stages, VkAccessFlags2 access) {
      auto& e = add();
      e.buffer = &buffer;
      e.bufferOffset = offset;
      e.bufferSize = size;
      e.stages = stages;
      e.access = access;
      return e;
    }

    DxvkResourceUse& add(DxvkBufferView& bufferView, VkPipelineStageFlags2 stages, VkAccessFlags2 access) {
      return add(*bufferView.buffer(), bufferView.info().offset, bufferView.info().size, stages, access);
    }

    DxvkResourceUse& add(DxvkBufferView& bufferView, VkDeviceSize offset, VkDeviceSize size,
      VkPipelineStageFlags2 stages, VkAccessFlags2 access) {
      const auto* formatInfo = bufferView.formatInfo();
      VkDeviceSize factor = formatInfo ? formatInfo->elementSize : 1u;

      return add(*bufferView.buffer(), offset * factor, size * factor, stages, access);
    }

    DxvkResourceUse& add(DxvkImage& image, const VkImageSubresourceRange subresource,
      VkImageLayout layout, VkPipelineStageFlags2 stages, VkAccessFlags2 access, VkBool32 discard) {
      auto& e = add();
      e.image = &image;
      e.imageSubresource = subresource;
      e.imageLayout = layout;
      e.stages = stages;
      e.access = access;
      e.discard = discard;
      return e;
    }

    DxvkResourceUse& add(DxvkImageView& imageView,
      VkImageLayout layout, VkPipelineStageFlags2 stages, VkAccessFlags2 access, VkBool32 discard) {
      return add(*imageView.image(), imageView.imageSubresources(),
        layout, stages, access, discard);
    }

    size_t size() const {
      return m_resources.size();
    }

    const DxvkResourceUse& operator [] (size_t idx) const {
      return m_resources[idx];
    }

  private:

    small_vector<DxvkResourceUse, 4u> m_resources;

  };


  /**
   * \brief Address range
   */
  struct DxvkAddressRange {
    /// Unique resource handle
    bit::uint48_t resource = bit::uint48_t(0u);
    /// Access modes used for the given address range
    DxvkAccessOp accessOp = DxvkAccessOp::None;
    /// Range start. For buffers, this shall be a byte offset,
    /// images can encode the first subresource index here.
    uint64_t rangeStart = 0u;
    /// Range end. For buffers, this is the offset of the last byte
    /// included in the range, i.e. offset + size - 1. For images,
    /// this is the last subresource included in the range.
    uint64_t rangeEnd = 0u;

    bool contains(const DxvkAddressRange& other) const {
      return uint64_t(resource) == uint64_t(other.resource)
          && rangeStart <= other.rangeStart
          && rangeEnd >= other.rangeEnd;
    }

    bool overlaps(const DxvkAddressRange& other) const {
      return uint64_t(resource) == uint64_t(other.resource)
          && rangeEnd >= other.rangeStart
          && rangeStart <= other.rangeEnd;
    }

    bool lt(const DxvkAddressRange& other) const {
      return (uint64_t(resource) < uint64_t(other.resource))
          || (uint64_t(resource) == uint64_t(other.resource) && rangeStart < other.rangeStart);
    }
  };


  /**
   * \brief Barrier tree node
   *
   * Node of a red-black tree, consisting of a packed node
   * header as well as aresource address range. GCC generates
   * weird code with bitfields here, so pack manually.
   */
  struct DxvkBarrierTreeNode {
    constexpr static uint64_t NodeIndexMask = (1u << 21) - 1u;

    // Packed header with node indices and the node color.
    // [0:0]: Set if the node is red, clear otherwise.
    // [21:1]: Index of the left child node, may be 0.
    // [42:22]: Index of the right child node, may be 0.
    // [43:63]: Index of the parent node, may be 0 for the root.
    uint64_t header = 0u;

    // Address range of the node
    DxvkAddressRange addressRange = { };

    void setRed(bool red) {
      header &= ~uint64_t(1u);
      header |= uint64_t(red);
    }

    bool isRed() const {
      return header & 1u;
    }

    void setParent(uint32_t node) {
      header &= ~(NodeIndexMask << 43);
      header |= uint64_t(node) << 43;
    }

    void setChild(uint32_t index, uint32_t node) {
      uint32_t shift = (index ? 22 : 1);
      header &= ~(NodeIndexMask << shift);
      header |= uint64_t(node) << shift;
    }

    uint32_t parent() const {
      return uint32_t((header >> 43) & NodeIndexMask);
    }

    uint32_t child(uint32_t index) const {
      uint32_t shift = (index ? 22 : 1);
      return uint32_t((header >> shift) & NodeIndexMask);
    }

    bool isRoot() const {
      return parent() == 0u;
    }
  };


  /**
   * \brief Barrier tracker
   *
   * Provides a two-part hash table for read and written resource
   * ranges, which is backed by binary trees to handle individual
   * address ranges as well as collisions.
   */
  class DxvkBarrierTracker {
    constexpr static uint32_t HashTableSize = 32u;
  public:

    DxvkBarrierTracker();

    ~DxvkBarrierTracker();

    /**
     * \brief Checks whether there is a pending access of a given type
     *
     * \param [in] range Resource range
     * \param [in] accessType Access type
     * \returns \c true if the range has a pending access
     */
    bool findRange(
      const DxvkAddressRange&           range,
            DxvkAccess                  accessType) const;

    /**
     * \brief Inserts address range for a given access type
     *
     * \param [in] range Resource range
     * \param [in] accessType Access type
     */
    void insertRange(
      const DxvkAddressRange&           range,
            DxvkAccess                  accessType);

    /**
     * \brief Clears the entire structure
     *
     * Invalidates all hash table entries and trees.
     */
    void clear();

    /**
     * \brief Checks whether any resources are dirty
     * \returns \c true if the tracker is empty.
     */
    bool empty() const {
      return !m_rootMaskValid;
    }

  private:

    uint64_t m_rootMaskValid = 0u;
    uint64_t m_rootMaskSubtree = 0u;

    std::vector<DxvkBarrierTreeNode>  m_nodes;
    std::vector<uint32_t>             m_free;

    uint32_t allocateNode();

    void freeNode(uint32_t node);

    uint32_t findNode(
      const DxvkAddressRange&           range,
            uint32_t                    rootIndex) const;

    uint32_t insertNode(
      const DxvkAddressRange&           range,
            uint32_t                    rootIndex);

    void removeNode(
            uint32_t                    nodeIndex,
            uint32_t                    rootIndex);

    void rebalancePostInsert(
            uint32_t                    nodeIndex,
            uint32_t                    rootIndex);

    void rotateLeft(
            uint32_t                    nodeIndex,
            uint32_t                    rootIndex);

    void rotateRight(
            uint32_t                    nodeIndex,
            uint32_t                    rootIndex);

    static uint32_t computeRootIndex(
      const DxvkAddressRange&           range,
            DxvkAccess                  access) {
      // TODO revisit once we use internal allocation
      // objects or resource cookies here.
      size_t hash = uint64_t(range.resource) * 93887;
             hash ^= (hash >> 16);

      // Reserve the upper half of the implicit hash table for written
      // ranges, and add 1 because 0 refers to the actual null node.
      return 1u + (hash % HashTableSize) + (access == DxvkAccess::Write ? HashTableSize : 0u);
    }

  };


  /**
   * \brief Barrier batch
   *
   * Simple helper class to accumulate barriers that can then
   * be recorded into a command buffer in a single step.
   */
  class DxvkBarrierBatch {

  public:

    DxvkBarrierBatch(DxvkCmdBuffer cmdBuffer);
    ~DxvkBarrierBatch();

    /**
     * \brief Adds a memory barrier
     *
     * Host read access will only be flushed
     * at the end of a command list.
     * \param [in] barrier Memory barrier
     */
    void addMemoryBarrier(
      const VkMemoryBarrier2&           barrier);

    /**
     * \brief Adds an image barrier
     *
     * This will automatically turn into a normal memory barrier
     * if no queue family ownership transfer or layout transition
     * happens.
     * \param [in] barrier Memory barrier
     */
    void addImageBarrier(
      const VkImageMemoryBarrier2&      barrier);

    /**
     * \brief Flushes batched memory barriers
     * \param [in] list Command list
     */
    void flush(
      const Rc<DxvkCommandList>&        list);

    /**
     * \brief Flushes batched memory and host barriers
     * \param [in] list Command list
     */
    void finalize(
      const Rc<DxvkCommandList>&        list);

    /**
     * \brief Check whether there are pending layout transitions
     * \returns \c true if there are any image layout transitions
     */
    bool hasLayoutTransitions() const {
      return !m_imageBarriers.empty();
    }

    /**
     * \brief Checks whether there are barriers using the given source stages
     * \returns \c true if any barriers use the given source stages
     */
    bool hasPendingStages(VkPipelineStageFlags2 stages) const {
      if (m_memoryBarrier.srcStageMask & stages)
        return true;

      for (const auto& b : m_imageBarriers) {
        if (b.srcStageMask & stages)
          return true;
      }

      return false;
    }

  private:

    DxvkCmdBuffer         m_cmdBuffer;

    VkMemoryBarrier2      m_memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };

    VkPipelineStageFlags2 m_hostSrcStages = 0u;
    VkAccessFlags2        m_hostDstAccess = 0u;

    std::vector<VkImageMemoryBarrier2> m_imageBarriers = { };

  };

}
