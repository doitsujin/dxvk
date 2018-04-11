#pragma once

#include "dxvk_include.h"

namespace dxvk {
  
  enum class DxvkFormatFlag {
    BlockCompressed = 0,  ///< Image format is block compressed
    SampledInteger  = 1,  ///< Sampled type is an integer type
  };
  
  using DxvkFormatFlags = Flags<DxvkFormatFlag>;
  
  /**
   * \brief Format info structure
   * 
   * Provides some useful information
   * about a Vulkan image format. 
   */
  struct DxvkFormatInfo {
    /// Size of an element in this format. For compressed
    /// formats, this is the size of a block, in bytes.
    VkDeviceSize elementSize = 0;
    
    /// Available image aspect flags
    VkImageAspectFlags aspectMask = 0;
    
    /// Some other format info flags
    DxvkFormatFlags flags = 0;
    
    /// Size, in pixels, of a compressed block. For
    /// non-block formats, all these values are 1.
    VkExtent3D blockSize = { 1, 1, 1 };
  };
  
  
  
  const DxvkFormatInfo* imageFormatInfo(VkFormat format);
  
}