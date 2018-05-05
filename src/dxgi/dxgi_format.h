#pragma once

#include "dxgi_include.h"

#include "../dxvk/dxvk_include.h"

namespace dxvk {
  
  /**
   * \brief Format mapping
   * 
   * Maps a DXGI format to a set of Vulkan formats.
   */
  struct DXGI_VK_FORMAT_MAPPING {
    DXGI_FORMAT           FormatFamily  = DXGI_FORMAT_UNKNOWN;  ///< Typeless format family
    VkFormat              FormatColor   = VK_FORMAT_UNDEFINED;  ///< Corresponding color format
    VkFormat              FormatDepth   = VK_FORMAT_UNDEFINED;  ///< Corresponding depth format
    VkFormat              FormatRaw     = VK_FORMAT_UNDEFINED;  ///< Bit-compatible integer format
    VkImageAspectFlags    AspectColor   = 0;                    ///< Defined aspects for the color format
    VkImageAspectFlags    AspectDepth   = 0;                    ///< Defined aspects for the depth format
    VkComponentMapping    Swizzle       = {                     ///< Color component swizzle
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
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
    VkFormat              Format      = VK_FORMAT_UNDEFINED;  ///< Corresponding color format
    VkImageAspectFlags    Aspect      = 0;                    ///< Defined image aspect mask
    VkComponentMapping    Swizzle     = {                     ///< Component swizzle
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
  };
  
  /**
   * \brief Format lookup mode
   * 
   * When looking up an image format, additional information
   * might be needed on how the image is going to be used.
   * This is used to properly map typeless formats and color
   * formats to depth formats if they are used on depth images.
   */
  enum DXGI_VK_FORMAT_MODE {
    DXGI_VK_FORMAT_MODE_ANY   = 0,  ///< Color first, then depth
    DXGI_VK_FORMAT_MODE_COLOR = 1,  ///< Color only
    DXGI_VK_FORMAT_MODE_DEPTH = 2,  ///< Depth only
    DXGI_VK_FORMAT_MODE_RAW   = 3,  ///< Unsigned integer format
  };
  
  /**
   * \brief Retrieves a format mapping entry
   * 
   * \param [in] Format The format to look up
   * \returns Pointer to the map entry
   */
  const DXGI_VK_FORMAT_MAPPING* GetDXGIFormatMapping(
          DXGI_FORMAT         Format);
  
  /**
   * \brief Retrieves info for a given DXGI format
   * 
   * \param [in] Format The DXGI format to look up
   * \param [in] Mode the format lookup mode
   * \returns Format info
   */
  DXGI_VK_FORMAT_INFO GetDXGIFormatInfo(
          DXGI_FORMAT         Format,
          DXGI_VK_FORMAT_MODE Mode);
  
};