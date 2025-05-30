#pragma once

#include <array>
#include <utility>
#include <vector>

#include "../util/util_small_vector.h"

#include "dxvk_descriptor.h"

namespace dxvk {

  class DxvkDevice;

  /**
   * \brief Size and required alignment of a specific descriptor type
   *
   * The reported size is always going to be a multiple of the alignment.
   * Relevant for computing descriptor layouts and retrieving descriptors.
   */
  struct DxvkDescriptorTypeInfo {
    uint16_t size       = 0u;
    uint16_t alignment  = 0u;
  };


  /**
   * \brief Descriptor range properties
   */
  struct DxvkDescriptorUpdateRange {
    /** Descriptor offset, in bytes */
    uint16_t dstOffset        = 0u;
    /** First source descriptor to read */
    uint16_t srcIndex         = 0u;
    /** Number of descriptors to copy */
    uint16_t descriptorCount  = 0u;
    /** Descriptor size, in bytes. Relevant if no optimized
     *  function for the given descriptor size exists. */
    uint16_t descriptorSize   = 0u;
  };


  /**
   * \brief Descriptor update function
   *
   * Copies or pads descriptor memory. May be
   * optimized for a specific descriptor size.
   *
   * The parameters, in order, are:
   * - Base pointer to descriptor memory to write
   * - Base poitner to descriptor list to read
   * - Reference to copy metadata, used to determine
   *   offsets and how many descriptors to write.
   */
  using DxvkDescriptorUpdateFn = void (void*, const DxvkDescriptor**, const DxvkDescriptorUpdateRange&);


  /**
   * \brief Descriptor update entry
   *
   * Convenience struct that bundles update
   * info with an update function.
   */
  struct DxvkDescriptorUpdateEntry {
    DxvkDescriptorUpdateRange range = { };
    DxvkDescriptorUpdateFn*   fn    = nullptr;
  };


  /**
   * \brief Descriptor properties
   *
   * Stores the descriptor type, offset in the descriptor
   * set, and whether or not this is sourced from a raw
   * buffer address range or an actual view descriptor.
   */
  struct DxvkDescriptorUpdateInfo {
    VkDescriptorType  descriptorType  = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    uint32_t          offset          = 0u;
  };


  /**
   * \brief Descriptor update class
   *
   * List of descriptor update entries that
   */
  class DxvkDescriptorUpdateList {

  public:

    DxvkDescriptorUpdateList() = default;

    /**
     * \brief Builds descriptor update list
     *
     * Generates an optimized descriptor update list
     * specifically for the given set layout.
     * \param [in] setSize Total descriptor set size, in bytes
     * \param [in] descriptorCount Number of descriptors
     * \param [in] descriptorInfos Descriptor infos
     */
    DxvkDescriptorUpdateList(
            DxvkDevice*               device,
            uint32_t                  setSize,
            uint32_t                  descriptorCount,
      const DxvkDescriptorUpdateInfo* descriptorInfos);

    ~DxvkDescriptorUpdateList();

    /**
     * \brief Updates descriptor memory
     *
     * Note that descriptor and buffer lists must list descriptors
     * in the exact same order as they were passed into the constructor.
     * \param [in] dst Pointer to descriptor memory
     * \param [in] descriptors Pointer to source descriptor list
     * \param [in] buffers Pointer to raw buffer ranges
     */
    void update(
            void*                   dst,
      const DxvkDescriptor**        descriptors) const {
      for (size_t i = 0u; i < m_entries.size(); i++) {
        const auto& e = m_entries[i];
        e.fn(dst, descriptors, e.range);
      }
    }

  private:

    DxvkDevice* m_device = nullptr;

    small_vector<DxvkDescriptorUpdateEntry, 16u> m_entries;

    void addCopy(const DxvkDescriptorUpdateRange& range);

    void addPadding(uint32_t loOffset, uint32_t hiOffset);

    uint32_t getDescriptorSize(VkDescriptorType type) const;

    DxvkDescriptorUpdateFn* getCopyFn(uint32_t alignment, uint32_t size);

    DxvkDescriptorUpdateFn* getPaddingFn(uint32_t alignment, uint32_t size);

    static void copyGeneric(
            void*                       dst,
      const DxvkDescriptor**            descriptor,
      const DxvkDescriptorUpdateRange&  range);

    static void padGeneric(
            void*                       dst,
      const DxvkDescriptor**            descriptor,
      const DxvkDescriptorUpdateRange&  range);

    template<size_t Size>
    static void copyAligned(
            void*                       dst,
      const DxvkDescriptor**            descriptor,
      const DxvkDescriptorUpdateRange&  range);

    template<size_t Size>
    static void padAligned(
            void*                       dst,
      const DxvkDescriptor**            descriptor,
      const DxvkDescriptorUpdateRange&  range);

    static void padAlignedAnySize(
            void*                       dst,
      const DxvkDescriptor**            descriptor,
      const DxvkDescriptorUpdateRange&  range);

  };


  /**
   * \brief Descriptor properties
   *
   * Caches descriptor properties and null descriptors.
   * Not meaningful if the legacy descriptor model is used.
   */
  class DxvkDescriptorProperties {
    constexpr static uint32_t TypeCount = uint32_t(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) + 1u;
  public:

    DxvkDescriptorProperties(DxvkDevice* device);
    ~DxvkDescriptorProperties();

    /**
     * \brief Queries descriptor type properties
     *
     * \param [in] type Descriptor type to query
     * \returns Descriptor size and alignment
     */
    DxvkDescriptorTypeInfo getDescriptorTypeInfo(VkDescriptorType type) const {
      return m_descriptorTypes[uint32_t(type)];
    }

    /**
     * \brief Queries null descriptor
     *
     * Not valid for sampler descriptors.
     * \param [in] type Descriptor type
     * \returns Pointer to null descriptor
     */
    const DxvkDescriptor* getNullDescriptor(VkDescriptorType type) const {
      return &m_nullDescriptors[uint32_t(type)];
    }

    /**
     * \brief Queries descriptor set alignment
     *
     * All sets must be padded to this size.
     * \returns Descriptor set alignment
     */
    VkDeviceSize getDescriptorSetAlignment() const {
      return m_setAlignment;
    }

    /**
     * \brief Queries maximum descriptor size
     * \returns Size of the largest descriptor type
     */
    VkDeviceSize getMaxDescriptorSize() const {
      VkDeviceSize size = 0u;

      for (const auto& e : m_descriptorTypes)
        size = std::max<VkDeviceSize>(size, e.size);

      return size;
    }

  private:

    VkDeviceSize m_setAlignment = 0u;

    std::array<DxvkDescriptorTypeInfo, TypeCount> m_descriptorTypes = { };
    std::array<DxvkDescriptor,         TypeCount> m_nullDescriptors = { };

    void initDescriptorHeapProperties(const DxvkDevice* device);

    void initDescriptorBufferProperties(const DxvkDevice* device);

    void logDescriptorProperties();

  };

}
