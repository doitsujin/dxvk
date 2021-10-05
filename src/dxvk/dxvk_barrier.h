#pragma once

#include <utility>
#include <vector>

#include "dxvk_buffer.h"
#include "dxvk_cmdlist.h"
#include "dxvk_image.h"

namespace dxvk {

  /**
   * \brief Buffer slice for barrier tracking
   *
   * Stores the offset and length of a buffer slice,
   * as well as access flags for the given range.
   */
  class DxvkBarrierBufferSlice {

  public:

    DxvkBarrierBufferSlice()
    : m_offset(0), m_length(0), m_access(0) { }

    DxvkBarrierBufferSlice(VkDeviceSize offset, VkDeviceSize length, DxvkAccessFlags access)
    : m_offset(offset), m_length(length), m_access(access) { }

    /**
     * \brief Checks whether two slices overlap
     *
     * \param [in] slice The other buffer slice to check
     * \returns \c true if the two slices overlap
     */
    bool overlaps(const DxvkBarrierBufferSlice& slice) const {
      return m_offset +       m_length > slice.m_offset
          && m_offset < slice.m_offset + slice.m_length;
    }

    /**
     * \brief Checks whether a given slice is dirty
     *
     * \param [in] slice The buffer slice to check
     * \returns \c true if the two slices overlap, and if
     *    at least one of the two slices have write access.
     */
    bool isDirty(const DxvkBarrierBufferSlice& slice) const {
      return (slice.m_access | m_access).test(DxvkAccess::Write) && overlaps(slice);
    }

    /**
     * \brief Checks whether two slices can be merged
     *
     * Two buffer slices can be merged if they overlap or are adjacent
     * and if the access flags are the same, or alternatively, if the
     * offset and size are the same and only the access flags differ.
     * \param [in] slice The other buffer slice to check
     * \returns \c true if the slices can be merged.
     */
    bool canMerge(const DxvkBarrierBufferSlice& slice) const {
      if (m_access == slice.m_access) {
        return m_offset +        m_length >= slice.m_offset
            && m_offset <= slice.m_offset +  slice.m_length;
      } else {
        return m_offset == slice.m_offset
            && m_length == slice.m_length;
      }
    }

    /**
     * \brief Merges two buffer slices
     *
     * The resulting slice is guaranteed to fully contain both slices,
     * including their access flags. If called when \c canMerge would
     * return \c false, this will be a strict superset of both slices.
     * \param [in] slice The slice to merge
     */
    void merge(const DxvkBarrierBufferSlice& slice) {
      VkDeviceSize end = std::max(m_offset + m_length, slice.m_offset + slice.m_length);

      m_offset = std::min(m_offset, slice.m_offset);
      m_length = end - m_offset;
      m_access.set(slice.m_access);
    }

    /**
     * \brief Queries access flags
     * \returns Access flags
     */
    DxvkAccessFlags getAccess() const {
      return m_access;
    }

  private:

    VkDeviceSize    m_offset;
    VkDeviceSize    m_length;
    DxvkAccessFlags m_access;

  };


  /**
   * \brief Image slice for barrier tracking
   *
   * Stores an image subresource range, as well as
   * access flags for the given image subresources.
   */
  class DxvkBarrierImageSlice {

  public:

    DxvkBarrierImageSlice()
    : m_range(VkImageSubresourceRange()), m_access(0) { }

    DxvkBarrierImageSlice(VkImageSubresourceRange range, DxvkAccessFlags access)
    : m_range(range), m_access(access) { }

    /**
     * \brief Checks whether two slices overlap
     *
     * \param [in] slice The other image slice to check
     * \returns \c true if the two slices overlap
     */
    bool overlaps(const DxvkBarrierImageSlice& slice) const {
      return (m_range.aspectMask     & slice.m_range.aspectMask)
          && (m_range.baseArrayLayer < slice.m_range.baseArrayLayer + slice.m_range.layerCount)
          && (m_range.baseArrayLayer +       m_range.layerCount     > slice.m_range.baseArrayLayer)
          && (m_range.baseMipLevel   < slice.m_range.baseMipLevel   + slice.m_range.levelCount)
          && (m_range.baseMipLevel   +       m_range.levelCount     > slice.m_range.baseMipLevel);
    }

    /**
     * \brief Checks whether a given slice is dirty
     *
     * \param [in] slice The image slice to check
     * \returns \c true if the two slices overlap, and if
     *    at least one of the two slices have write access.
     */
    bool isDirty(const DxvkBarrierImageSlice& slice) const {
      return (slice.m_access | m_access).test(DxvkAccess::Write) && overlaps(slice);
    }

    /**
     * \brief Checks whether two slices can be merged
     *
     * This is a simplified implementation that does not check for
     * adjacent or overlapping layers or levels, and instead only
     * returns \c true if both slices contain the same mip levels
     * and array layers. Access flags and image aspects may differ.
     * \param [in] slice The other image slice to check
     * \returns \c true if the slices can be merged.
     */
    bool canMerge(const DxvkBarrierImageSlice& slice) const {
      return m_range.baseMipLevel   == slice.m_range.baseMipLevel
          && m_range.levelCount     == slice.m_range.levelCount
          && m_range.baseArrayLayer == slice.m_range.baseArrayLayer
          && m_range.layerCount     == slice.m_range.layerCount;
    }

    /**
     * \brief Merges two image slices
     *
     * The resulting slice is guaranteed to fully contain both slices,
     * including their access flags. If called when \c canMerge would
     * return \c false, this will be a strict superset of both slices.
     * \param [in] slice The slice to merge
     */
    void merge(const DxvkBarrierImageSlice& slice) {
      uint32_t maxMipLevel = std::max(m_range.baseMipLevel + m_range.levelCount,
        slice.m_range.baseMipLevel + slice.m_range.levelCount);
      uint32_t maxArrayLayer = std::max(m_range.baseArrayLayer + m_range.layerCount,
        slice.m_range.baseArrayLayer + slice.m_range.layerCount);
      m_range.aspectMask     |= slice.m_range.aspectMask;
      m_range.baseMipLevel    = std::min(m_range.baseMipLevel, slice.m_range.baseMipLevel);
      m_range.levelCount      = maxMipLevel - m_range.baseMipLevel;
      m_range.baseArrayLayer  = std::min(m_range.baseMipLevel, slice.m_range.baseArrayLayer);
      m_range.layerCount      = maxArrayLayer - m_range.baseArrayLayer;
      m_access.set(slice.m_access);
    }

    /**
     * \brief Queries access flags
     * \returns Access flags
     */
    DxvkAccessFlags getAccess() const {
      return m_access;
    }

  private:

    VkImageSubresourceRange m_range;
    DxvkAccessFlags         m_access;

  };


  /**
   * \brief Resource slice set for barrier tracking
   *
   * Implements a versioned hash table for fast resource
   * lookup, with a single-linked list accurately storing
   * each accessed slice if necessary.
   * \tparam K Resource handle type
   * \tparam T Resource slice type
   */
  template<typename K, typename T>
  class DxvkBarrierSubresourceSet {
    constexpr static uint32_t NoEntry = ~0u;
  public:

    /**
     * \brief Queries access flags of a given resource slice
     *
     * \param [in] resource Resource handle
     * \param [in] slice Resource slice
     * \returns Or'd access flags of all known slices
     *    that overlap with the given slice.
     */
    DxvkAccessFlags getAccess(K resource, const T& slice) {
      HashEntry* entry = findHashEntry(resource);

      if (!entry)
        return DxvkAccessFlags();

      // Exit early if we know that there are no overlapping
      // slices, or if there is only one slice to check anyway.
      if (!entry->data.overlaps(slice))
        return DxvkAccessFlags();

      ListEntry* list = getListEntry(entry->next);

      if (!list)
        return entry->data.getAccess();

      // The early out condition just checks whether there are
      // any access flags left that may potentially get added
      DxvkAccessFlags access;

      while (list && access != entry->data.getAccess()) {
        if (list->data.overlaps(slice))
          access.set(list->data.getAccess());

        list = getListEntry(list->next);
      }

      return access;
    }

    /**
     * \brief Checks whether a given resource slice is dirty
     *
     * \param [in] resource Resourece handle
     * \param [in] slice Resource slice
     * \returns \c true if there is at least one slice that
     *    overlaps with the given slice, and either slice has
     *    the \c DxvkAccess::Write flag set.
     */
    bool isDirty(K resource, const T& slice) {
      HashEntry* entry = findHashEntry(resource);

      if (!entry)
        return false;

      // Exit early if there are no overlapping slices, or
      // if none of the slices have the write flag set.
      if (!entry->data.isDirty(slice))
        return false;

      // We know that some subresources are dirty, so if
      // there is no list, the given slice must be dirty.
      ListEntry* list = getListEntry(entry->next);

      if (!list)
        return true;

      // Exit earlier if we find one dirty slice
      bool dirty = false;

      while (list && !dirty) {
        dirty = list->data.isDirty(slice);
        list = getListEntry(list->next);
      }

      return dirty;
    }

    /**
     * \brief Inserts a given resource slice
     *
     * This will attempt to deduplicate and merge entries if
     * possible, so that lookup and further insertions remain
     * reasonably fast.
     * \param [in] resource Resource handle
     * \param [in] slice Resource slice
     */
    void insert(K resource, const T& slice) {
      HashEntry* hashEntry = insertHashEntry(resource, slice);

      if (hashEntry) {
        ListEntry* listEntry = getListEntry(hashEntry->next);

        // Only create the linear list if absolutely necessary
        if (!listEntry && !hashEntry->data.canMerge(slice))
          listEntry = insertListEntry(hashEntry->data, hashEntry);

        if (listEntry) {
          while (listEntry) {
            // Avoid adding new list entries if possible
            if (listEntry->data.canMerge(slice)) {
              listEntry->data.merge(slice);
              break;
            }

            listEntry = getListEntry(listEntry->next);
          }

          if (!listEntry)
            insertListEntry(slice, hashEntry);
        }

        // Merge hash entry data so that it stores
        // a superset of all slices in the list.
        hashEntry->data.merge(slice);
      }
    }

    /**
     * \brief Removes all resources from the set
     */
    void clear() {
      m_used = 0;
      m_version += 1;
      m_list.clear();
    }

  private:

    struct ListEntry {
      T         data;
      uint32_t  next;
    };

    struct HashEntry {
      uint64_t  version;
      K         key;
      T         data;
      uint32_t  next;
    };

    uint64_t m_version = 1ull;
    uint64_t m_used    = 0ull;

    std::vector<ListEntry> m_list;
    std::vector<HashEntry> m_hashMap;

    static size_t computeHash(K key) {
      return size_t(reinterpret_cast<uint64_t>(key));
    }

    size_t computeIndex(K key) const {
      return computeHash(key) % m_hashMap.size();
    }

    size_t advanceIndex(size_t index) const {
      size_t size = m_hashMap.size();
      size_t next = index + 1;
      return next < size ? next : 0;
    }

    HashEntry* findHashEntry(K key) {
      if (!m_used)
        return nullptr;

      size_t index = computeIndex(key);

      while (m_hashMap[index].version == m_version) {
        if (m_hashMap[index].key == key)
          return &m_hashMap[index];

        index = advanceIndex(index);
      }

      return nullptr;
    }

    HashEntry* insertHashEntry(K key, const T& data) {
      growHashMapBeforeInsert();

      // If we already have an entry for the given key, return
      // the old one and let the caller deal with it
      size_t index = computeIndex(key);

      while (m_hashMap[index].version == m_version) {
        if (m_hashMap[index].key == key)
          return &m_hashMap[index];

        index = advanceIndex(index);
      }

      HashEntry* entry = &m_hashMap[index];
      entry->version = m_version;
      entry->key     = key;
      entry->data    = data;
      entry->next    = NoEntry;

      m_used += 1;
      return nullptr;
    }

    void growHashMap(size_t newSize) {
      size_t oldSize = m_hashMap.size();
      m_hashMap.resize(newSize);

      // Relocate hash entries in place
      for (size_t i = 0; i < oldSize; i++) {
        HashEntry entry = m_hashMap[i];
        m_hashMap[i].version = 0;

        while (entry.version == m_version) {
          size_t index = computeIndex(entry.key);
          entry.version = m_version + 1;

          while (m_hashMap[index].version > m_version)
            index = advanceIndex(index);

          std::swap(entry, m_hashMap[index]);
        }
      }

      m_version += 1;
    }

    void growHashMapBeforeInsert() {
      // Allow a load factor of 0.7 for performance reasons
      size_t oldSize = m_hashMap.size();

      if (10 * m_used >= 7 * oldSize) {
        size_t newSize = oldSize ? (oldSize * 2 + 5) : 37;
        growHashMap(newSize);
      }
    }

    ListEntry* getListEntry(uint32_t index) {
      return index < NoEntry ? &m_list[index] : nullptr;
    }

    ListEntry* insertListEntry(const T& subresource, HashEntry* head) {
      uint32_t newIndex = uint32_t(m_list.size());
      m_list.push_back({ subresource, head->next });
      head->next = newIndex;
      return &m_list[newIndex];
    }

  };
  
  /**
   * \brief Barrier set
   * 
   * Accumulates memory barriers and provides a
   * method to record all those barriers into a
   * command buffer at once.
   */
  class DxvkBarrierSet {
    
  public:
    
    DxvkBarrierSet(DxvkCmdBuffer cmdBuffer);
    ~DxvkBarrierSet();

    void accessMemory(
            VkPipelineStageFlags      srcStages,
            VkAccessFlags             srcAccess,
            VkPipelineStageFlags      dstStages,
            VkAccessFlags             dstAccess);

    void accessBuffer(
      const DxvkBufferSliceHandle&    bufSlice,
            VkPipelineStageFlags      srcStages,
            VkAccessFlags             srcAccess,
            VkPipelineStageFlags      dstStages,
            VkAccessFlags             dstAccess);
    
    void accessImage(
      const Rc<DxvkImage>&            image,
      const VkImageSubresourceRange&  subresources,
            VkImageLayout             srcLayout,
            VkPipelineStageFlags      srcStages,
            VkAccessFlags             srcAccess,
            VkImageLayout             dstLayout,
            VkPipelineStageFlags      dstStages,
            VkAccessFlags             dstAccess);

    void releaseBuffer(
            DxvkBarrierSet&           acquire,
      const DxvkBufferSliceHandle&    bufSlice,
            uint32_t                  srcQueue,
            VkPipelineStageFlags      srcStages,
            VkAccessFlags             srcAccess,
            uint32_t                  dstQueue,
            VkPipelineStageFlags      dstStages,
            VkAccessFlags             dstAccess);

    void releaseImage(
            DxvkBarrierSet&           acquire,
      const Rc<DxvkImage>&            image,
      const VkImageSubresourceRange&  subresources,
            uint32_t                  srcQueue,
            VkImageLayout             srcLayout,
            VkPipelineStageFlags      srcStages,
            VkAccessFlags             srcAccess,
            uint32_t                  dstQueue,
            VkImageLayout             dstLayout,
            VkPipelineStageFlags      dstStages,
            VkAccessFlags             dstAccess);
    
    bool isBufferDirty(
      const DxvkBufferSliceHandle&    bufSlice,
            DxvkAccessFlags           bufAccess);

    bool isImageDirty(
      const Rc<DxvkImage>&            image,
      const VkImageSubresourceRange&  imgSubres,
            DxvkAccessFlags           imgAccess);
    
    DxvkAccessFlags getBufferAccess(
      const DxvkBufferSliceHandle&    bufSlice);
    
    DxvkAccessFlags getImageAccess(
      const Rc<DxvkImage>&            image,
      const VkImageSubresourceRange&  imgSubres);
    
    VkPipelineStageFlags getSrcStages() {
      return m_srcStages;
    }
    
    void recordCommands(
      const Rc<DxvkCommandList>&      commandList);
    
    void reset();

    static DxvkAccessFlags getAccessTypes(VkAccessFlags flags);
    
  private:

    struct BufSlice {
      DxvkBufferSliceHandle   slice;
      DxvkAccessFlags         access;
    };

    struct ImgSlice {
      VkImage                 image;
      VkImageSubresourceRange subres;
      DxvkAccessFlags         access;
    };

    DxvkCmdBuffer m_cmdBuffer;
    
    VkPipelineStageFlags m_srcStages = 0;
    VkPipelineStageFlags m_dstStages = 0;

    VkAccessFlags m_srcAccess = 0;
    VkAccessFlags m_dstAccess = 0;
    
    std::vector<VkBufferMemoryBarrier> m_bufBarriers;
    std::vector<VkImageMemoryBarrier>  m_imgBarriers;

    DxvkBarrierSubresourceSet<VkBuffer, DxvkBarrierBufferSlice> m_bufSlices;
    DxvkBarrierSubresourceSet<VkImage,  DxvkBarrierImageSlice>  m_imgSlices;
    
  };
  
}