#pragma once

#include "dxvk_include.h"

namespace dxvk::util {
  
  /**
   * \brief Gets pipeline stage flags for shader stages
   * 
   * \param [in] shaderStages Shader stage flags
   * \returns Corresponding pipeline stage flags
   */
  VkPipelineStageFlags pipelineStages(
          VkShaderStageFlags shaderStages);
  
  /**
   * \brief Computes number of mip levels for an image
   * 
   * \param [in] imageSize Size of the image
   * \returns Number of mipmap layers
   */
  uint32_t computeMipLevelCount(VkExtent3D imageSize);
  
  /**
   * \brief Writes tightly packed image data to a buffer
   * 
   * \param [in] dstData Destination buffer pointer
   * \param [in] srcData Pointer to source data
   * \param [in] blockCount Number of blocks to copy
   * \param [in] blockSize Number of bytes per block
   * \param [in] pitchPerRow Number of bytes between rows
   * \param [in] pitchPerLayer Number of bytes between layers
   */
  void packImageData(
          char*             dstData,
    const char*             srcData,
          VkExtent3D        blockCount,
          VkDeviceSize      blockSize,
          VkDeviceSize      pitchPerRow,
          VkDeviceSize      pitchPerLayer);
  
  /**
   * \brief Computes block count for compressed images
   * 
   * Convenience function to compute the size, in
   * blocks, of compressed images subresources.
   * \param [in] imageSize The image size
   * \param [in] blockSize Size per pixel block
   * \returns Number of blocks in the image
   */
  inline VkExtent3D computeBlockCount(VkExtent3D imageSize, VkExtent3D blockSize) {
    return VkExtent3D {
      (imageSize.width  + blockSize.width  - 1) / blockSize.width,
      (imageSize.height + blockSize.height - 1) / blockSize.height,
      (imageSize.depth  + blockSize.depth  - 1) / blockSize.depth };
  }
  
  /**
   * \brief Computes number of pixels or blocks of an image
   * 
   * Basically returns the product of width, height and depth.
   * \param [in] extent Image extent, in pixels or blocks
   * \returns Flattened number of pixels or blocks
   */
  inline uint32_t flattenImageExtent(VkExtent3D extent) {
    return extent.width * extent.height * extent.depth;
  }
  
  /**
   * \brief Computes image data size, in bytes
   * 
   * Convenience method that can be used to compute the number
   * of bytes required to store image data in a given format.
   * \param [in] format The image format
   * \param [in] extent Image size, in pixels
   * \returns Data size, in bytes
   */
  VkDeviceSize computeImageDataSize(VkFormat format, VkExtent3D extent);
  
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
