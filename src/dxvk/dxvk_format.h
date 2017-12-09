#pragma once

#include "dxvk_include.h"

namespace dxvk {
  
  /**
   * \brief Format info structure
   * 
   * Provides some useful information
   * about a Vulkan image format. 
   */
  struct DxvkFormatInfo {
    /// Size of an element in this format
    uint32_t byteSize;
    
    /// Available image aspect flags
    VkImageAspectFlags aspectMask;
  };
  
  
  const DxvkFormatInfo* imageFormatInfo(VkFormat format);
  
}