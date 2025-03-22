#pragma once

#include <utility>

#include "vulkan_loader.h"

#if defined(_MSC_VER)
// Unary minus on unsigned type
#pragma warning( disable : 4146 )
#endif

namespace dxvk::vk {

  constexpr static VkAccessFlags AccessReadMask
    = VK_ACCESS_INDIRECT_COMMAND_READ_BIT
    | VK_ACCESS_INDEX_READ_BIT
    | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
    | VK_ACCESS_UNIFORM_READ_BIT
    | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
    | VK_ACCESS_SHADER_READ_BIT
    | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
    | VK_ACCESS_TRANSFER_READ_BIT
    | VK_ACCESS_MEMORY_READ_BIT
    | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT;
    
  constexpr static VkAccessFlags AccessWriteMask
    = VK_ACCESS_SHADER_WRITE_BIT
    | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    | VK_ACCESS_TRANSFER_WRITE_BIT
    | VK_ACCESS_MEMORY_WRITE_BIT
    | VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT
    | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT;

  constexpr static VkAccessFlags AccessDeviceMask
    = AccessWriteMask | AccessReadMask;

  constexpr static VkAccessFlags AccessHostMask
    = VK_ACCESS_HOST_READ_BIT
    | VK_ACCESS_HOST_WRITE_BIT;

  constexpr static VkAccessFlags AccessGfxSideEffectMask
    = VK_ACCESS_SHADER_WRITE_BIT
    | VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT
    | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT;

  constexpr static VkPipelineStageFlags StageDeviceMask
    = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
    | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
    | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
    | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
    | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
    | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT
    | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
    | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
    | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
    | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
    | VK_PIPELINE_STAGE_TRANSFER_BIT
    | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
    | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
    | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
    | VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT;

  inline VkImageSubresourceRange makeSubresourceRange(
    const VkImageSubresourceLayers& layers) {
    VkImageSubresourceRange range;
    range.aspectMask      = layers.aspectMask;
    range.baseMipLevel    = layers.mipLevel;
    range.levelCount      = 1;
    range.baseArrayLayer  = layers.baseArrayLayer;
    range.layerCount      = layers.layerCount;
    return range;
  }

  inline VkImageSubresourceRange makeSubresourceRange(
    const VkImageSubresource& subres) {
    VkImageSubresourceRange range;
    range.aspectMask      = subres.aspectMask;
    range.baseMipLevel    = subres.mipLevel;
    range.levelCount      = 1;
    range.baseArrayLayer  = subres.arrayLayer;
    range.layerCount      = 1;
    return range;
  }

  inline VkImageSubresourceLayers makeSubresourceLayers(
    const VkImageSubresource& subres) {
    VkImageSubresourceLayers layers;
    layers.aspectMask     = subres.aspectMask;
    layers.mipLevel       = subres.mipLevel;
    layers.baseArrayLayer = subres.arrayLayer;
    layers.layerCount     = 1;
    return layers;
  }

  inline VkImageSubresourceLayers pickSubresourceLayers(
    const VkImageSubresourceRange&  range,
          uint32_t                  level) {
    VkImageSubresourceLayers layers;
    layers.aspectMask     = range.aspectMask;
    layers.mipLevel       = range.baseMipLevel + level;
    layers.baseArrayLayer = range.baseArrayLayer;
    layers.layerCount     = range.layerCount;
    return layers;
  }

  inline VkImageSubresource pickSubresource(
    const VkImageSubresourceLayers& range,
          uint32_t                  layer) {
    VkImageSubresource subres;
    subres.aspectMask = range.aspectMask;
    subres.mipLevel   = range.mipLevel;
    subres.arrayLayer = range.baseArrayLayer + layer;
    return subres;
  }

  inline VkImageSubresource pickSubresource(
    const VkImageSubresourceRange&  range,
          uint32_t                  level,
          uint32_t                  layer) {
    VkImageSubresource subres;
    subres.aspectMask = range.aspectMask;
    subres.mipLevel   = range.baseMipLevel   + level;
    subres.arrayLayer = range.baseArrayLayer + layer;
    return subres;
  }

  inline bool checkSubresourceRangeOverlap(
    const VkImageSubresourceRange&  a,
    const VkImageSubresourceRange&  b) {
    return a.baseMipLevel < b.baseMipLevel + b.levelCount
        && a.baseMipLevel + a.levelCount > b.baseMipLevel
        && a.baseArrayLayer < b.baseArrayLayer + b.layerCount
        && a.baseArrayLayer + a.layerCount > b.baseArrayLayer;
  }

  inline bool checkSubresourceRangeSuperset(
    const VkImageSubresourceRange&  a,
    const VkImageSubresourceRange&  b) {
    return a.baseMipLevel                <= b.baseMipLevel
        && a.baseMipLevel + a.levelCount >= b.baseMipLevel + b.levelCount
        && a.baseArrayLayer                <= b.baseArrayLayer
        && a.baseArrayLayer + a.layerCount >= b.baseArrayLayer + b.layerCount;
  }

  inline VkImageAspectFlags getWritableAspectsForLayout(VkImageLayout layout) {
    switch (layout) {
      case VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT:
      case VK_IMAGE_LAYOUT_GENERAL:
        return VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_IMAGE_ASPECT_COLOR_BIT;
      case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
      case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_IMAGE_ASPECT_STENCIL_BIT;
      case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return 0;
      default:
        Logger::err(str::format("Unhandled image layout ", layout));
        return 0;
    }
  }

  inline uint32_t getPlaneCount(VkImageAspectFlags aspects) {
    // Use a 16-bit integer as a lookup table. This works because
    // plane aspects use consecutive bits in the image aspect enum.
    const uint32_t shift = (aspects / VK_IMAGE_ASPECT_PLANE_0_BIT) * 2;
    const uint32_t counts = 0xffa5;
    return (counts >> shift) & 0x3;
  }

  inline uint32_t getPlaneIndex(VkImageAspectFlags aspect) {
    // Works for up to PLANE_2_BIT due to enum poperties
    return aspect / VK_IMAGE_ASPECT_PLANE_1_BIT;
  }

  inline VkImageAspectFlagBits getPlaneAspect(uint32_t plane) {
    return VkImageAspectFlagBits(VK_IMAGE_ASPECT_PLANE_0_BIT << plane);
  }

  inline VkImageAspectFlags getNextAspect(VkImageAspectFlags& mask) {
    if (likely(mask & (VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))) {
      // Depth-stencil isn't considered multi-planar
      return std::exchange(mask, VkImageAspectFlags(0));
    } else {
      VkImageAspectFlags result = mask & -mask;
      mask &= ~result;
      return result;
    }
  }

  template<typename T>
  struct ChainStruct {
    VkStructureType sType;
    T*              pNext;
  };

  template<typename T>
  void removeStructFromPNextChain(T** ppNext, VkStructureType sType) {
    while (*ppNext) {
      auto pStruct = reinterpret_cast<ChainStruct<T>*>(*ppNext);

      if (pStruct->sType == sType) {
        *ppNext = pStruct->pNext;
        return;
      }

      ppNext = &pStruct->pNext;
    }
  }


  inline uint64_t getObjectHandle(uint64_t handle) {
    return handle;
  }


  template<typename T>
  uint64_t getObjectHandle(T* object) {
    return reinterpret_cast<uintptr_t>(object);
  }


  inline bool isValidDebugName(const char* name) {
    return name && name[0];
  }


  /**
   * \brief Queries sRGB and non-sSRGB format pair
   *
   * \param [in] format Format to look up
   * \returns Pair of the corresponding non-SRGB and sRGB formats.
   *    If the format in quesion has no sRGB equivalent, this
   *    function returns \c VK_FORMAT_UNDEFINED.
   */
  inline std::pair<VkFormat, VkFormat> getSrgbFormatPair(VkFormat format) {
    static const std::array<std::pair<VkFormat, VkFormat>, 3> srgbFormatMap = {{
      { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB },
      { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB },
      { VK_FORMAT_A8B8G8R8_UNORM_PACK32, VK_FORMAT_A8B8G8R8_SRGB_PACK32 },
    }};

    for (const auto& f : srgbFormatMap) {
      if (f.first == format || f.second == format)
        return f;
    }

    return std::make_pair(VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED);
  }


  /**
   * \brief Makes debug label
   *
   * \param [in] color Color, as BGR with implied opaque alpha
   * \param [in] text Label text
   */
  inline VkDebugUtilsLabelEXT makeLabel(uint32_t color, const char* text) {
    VkDebugUtilsLabelEXT label = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
    label.color[0] = ((color >> 16u) & 0xffu) / 255.0f;
    label.color[1] = ((color >> 8u)  & 0xffu) / 255.0f;
    label.color[2] = ((color >> 0u)  & 0xffu) / 255.0f;
    label.color[3] = color ? 1.0f : 0.0f;
    label.pLabelName = text;
    return label;
  }

}


inline bool operator == (
  const VkImageSubresourceRange& a,
  const VkImageSubresourceRange& b) {
  return a.aspectMask     == b.aspectMask
      && a.baseMipLevel   == b.baseMipLevel
      && a.levelCount     == b.levelCount
      && a.baseArrayLayer == b.baseArrayLayer
      && a.layerCount     == b.layerCount;
}


inline bool operator != (
  const VkImageSubresourceRange& a,
  const VkImageSubresourceRange& b) {
  return !operator == (a, b);
}


inline bool operator == (
  const VkImageSubresourceLayers& a,
  const VkImageSubresourceLayers& b) {
  return a.aspectMask     == b.aspectMask
      && a.mipLevel       == b.mipLevel
      && a.baseArrayLayer == b.baseArrayLayer
      && a.layerCount     == b.layerCount;
}


inline bool operator != (
  const VkImageSubresourceLayers& a,
  const VkImageSubresourceLayers& b) {
  return !operator == (a, b);
}


inline bool operator == (VkExtent3D a, VkExtent3D b) {
  return a.width  == b.width
      && a.height == b.height
      && a.depth  == b.depth;
}


inline bool operator != (VkExtent3D a, VkExtent3D b) {
  return a.width  != b.width
      || a.height != b.height
      || a.depth  != b.depth;
}


inline bool operator == (VkExtent2D a, VkExtent2D b) {
  return a.width  == b.width
      && a.height == b.height;
}


inline bool operator != (VkExtent2D a, VkExtent2D b) {
  return a.width  != b.width
      || a.height != b.height;
}


inline bool operator == (VkOffset3D a, VkOffset3D b) {
  return a.x == b.x
      && a.y == b.y
      && a.z == b.z;
}


inline bool operator != (VkOffset3D a, VkOffset3D b) {
  return a.x != b.x
      || a.y != b.y
      || a.z != b.z;
}


inline bool operator == (VkOffset2D a, VkOffset2D b) {
  return a.x == b.x
      && a.y == b.y;
}


inline bool operator != (VkOffset2D a, VkOffset2D b) {
  return a.x != b.x
      || a.y != b.y;
}
