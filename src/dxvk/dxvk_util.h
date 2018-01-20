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
