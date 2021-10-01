#pragma once

#include <utility>

#include "vulkan_loader.h"

namespace dxvk::vk {

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
