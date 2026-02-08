#pragma once

#include "dxvk_include.h"
#include "dxvk_limits.h"

namespace dxvk {

  /**
   * \brief Legacy Vulkan descriptor info
   *
   * This structure can be used directly with
   * descriptor update templates.
   */
  union DxvkLegacyDescriptor {
    VkDescriptorBufferInfo buffer;
    VkDescriptorImageInfo image;
    VkBufferView bufferView;
  };


  /**
   * \brief Descriptor info
   *
   * Stores a resource or view descriptor.
   */
  struct DxvkDescriptor {
    /** Legacy view handle or buffer info, can be passed
     *  directly to WriteDescriptorSet and friends */
    DxvkLegacyDescriptor legacy;
    /** Explicit padding to make msvc happy */
    uint64_t reserved;
    /** Actual descriptor data */
    std::array<char, 256u> descriptor;

    /**
     * \brief Computes host address range for descriptor data
     *
     * For use with descriptor heaps.
     * \returns Host address range info
     */
    VkHostAddressRangeEXT getHostAddressRange() {
      VkHostAddressRangeEXT result = { };
      result.address = descriptor.data();
      result.size = descriptor.size();
      return result;
    }
  };


  /**
   * \brief Sampler descriptor info
   *
   * Stores info on a sampler descriptor.
   */
  struct DxvkSamplerDescriptor {
    VkSampler samplerObject = VK_NULL_HANDLE;
    uint16_t samplerIndex = 0u;
  };

}
