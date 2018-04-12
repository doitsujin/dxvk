#pragma once

#include "dxgi_include.h"

#include "../dxvk/dxvk_include.h"

namespace dxvk {
  
  /**
   * \brief Format information
   */
  enum DXGI_VK_FORMAT_FLAGS : uint32_t {
    DXGI_VK_FORMAT_TYPELESS = 0,
  };
  
  /**
   * \brief Format info
   * 
   * Stores a Vulkan image format for a given
   * DXGI format and some additional information
   * on how resources with the particular format
   * are supposed to be used.
   */
  struct DXGI_VK_FORMAT_INFO {
    VkFormat              format      = VK_FORMAT_UNDEFINED;
    VkImageAspectFlags    aspect      = 0;
    VkComponentMapping    swizzle     = {
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    UINT                  flags       = 0;
  };
  
  /**
   * \brief Format lookup mode
   * 
   * When looking up an image format, additional information
   * might be needed on how the image is going to be used.
   * This is used to properly map typeless formats and color
   * formats to depth formats if they are used on depth images.
   */
  enum class DxgiFormatMode : uint32_t {
    Any   = 0,
    Color = 1,
    Depth = 2,
  };
  
};