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
  union DxvkDescriptor {
    DxvkLegacyDescriptor legacy;
  };


  /**
   * \brief Sampler descriptor info
   *
   * Stores info on a sampler descriptor.
   */
  struct DxvkSamplerDescriptor {
    VkSampler samplerObject = VK_NULL_HANDLE;
  };

}
