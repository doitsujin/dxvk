#pragma once

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
    const VkImageSubresourceRange&  range,
          uint32_t                  level,
          uint32_t                  layer) {
    VkImageSubresource subres;
    subres.aspectMask = range.aspectMask;
    subres.mipLevel   = range.baseMipLevel   + level;
    subres.arrayLayer = range.baseArrayLayer + layer;
    return subres;
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
