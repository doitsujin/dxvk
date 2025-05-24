#pragma once

#include <array>
#include <utility>

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

  private:

    VkDeviceSize m_setAlignment = 0u;

    std::array<DxvkDescriptorTypeInfo, TypeCount> m_descriptorTypes = { };
    std::array<DxvkDescriptor,         TypeCount> m_nullDescriptors = { };

    void initDescriptorBufferProperties(const DxvkDevice* device);

    void logDescriptorProperties();

  };

}
