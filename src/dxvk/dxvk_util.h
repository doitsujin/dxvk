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
   * \brief Computes minimum extent
   * 
   * \param [in] a First value
   * \param [in] b Second value
   * \returns Component-wise \c min
   */
  inline VkExtent3D minExtent3D(VkExtent3D a, VkExtent3D b) {
    return VkExtent3D {
      std::min(a.width,  b.width),
      std::min(a.height, b.height),
      std::min(a.depth,  b.depth) };
  }
  
  /**
   * \brief Checks whether an offset is block-aligned
   * 
   * An offset is considered block-aligned if it is
   * a multiple of the block size. Only non-negative
   * offset values are valid.
   * \param [in] offset The offset to check
   * \param [in] blockSize Block size
   * \returns \c true if \c offset is aligned
   */
  inline bool isBlockAligned(VkOffset3D offset, VkExtent3D blockSize) {
    return (offset.x % blockSize.width  == 0)
        && (offset.y % blockSize.height == 0)
        && (offset.z % blockSize.depth  == 0);
  }
  
  /**
   * \brief Checks whether an extent is block-aligned
   * 
   * A block-aligned extent can be used for image copy
   * operations that involve block-compressed images.
   * \param [in] offset The base offset
   * \param [in] extent The extent to check
   * \param [in] blockSize Compressed block size
   * \param [in] imageSize Image size
   * \returns \c true if all components of \c extent
   *          are aligned or touch the image border.
   */
  inline bool isBlockAligned(VkOffset3D offset, VkExtent3D extent, VkExtent3D blockSize, VkExtent3D imageSize) {
    return ((extent.width  % blockSize.width  == 0) || (uint32_t(offset.x + extent.width)  == imageSize.width))
        && ((extent.height % blockSize.height == 0) || (uint32_t(offset.y + extent.height) == imageSize.height))
        && ((extent.depth  % blockSize.depth  == 0) || (uint32_t(offset.z + extent.depth)  == imageSize.depth));
  }
  
  /**
   * \brief Computes block offset for compressed images
   * 
   * Convenience function to compute the block position
   * within a compressed image based on the block size.
   * \param [in] offset The offset
   * \param [in] blockSize Size of a pixel block
   * \returns The block offset
   */
  inline VkOffset3D computeBlockOffset(VkOffset3D offset, VkExtent3D blockSize) {
    return VkOffset3D {
      offset.x / int32_t(blockSize.width),
      offset.y / int32_t(blockSize.height),
      offset.z / int32_t(blockSize.depth) };
  }
  
  /**
   * \brief Computes block count for compressed images
   * 
   * Convenience function to compute the size, in
   * blocks, of compressed images subresources.
   * \param [in] extent The image size
   * \param [in] blockSize Size of a pixel block
   * \returns Number of blocks in the image
   */
  inline VkExtent3D computeBlockCount(VkExtent3D extent, VkExtent3D blockSize) {
    return VkExtent3D {
      (extent.width  + blockSize.width  - 1) / blockSize.width,
      (extent.height + blockSize.height - 1) / blockSize.height,
      (extent.depth  + blockSize.depth  - 1) / blockSize.depth };
  }
  
  /**
   * \brief Computes block count for compressed images
   * 
   * Given an aligned offset, this computes 
   * Convenience function to compute the size, in
   * blocks, of compressed images subresources.
   * \param [in] extent The image size
   * \param [in] blockSize Size of a pixel block
   * \returns Number of blocks in the image
   */
  inline VkExtent3D computeMaxBlockCount(VkOffset3D offset, VkExtent3D extent, VkExtent3D blockSize) {
    return VkExtent3D {
      (extent.width  + blockSize.width  - offset.x - 1) / blockSize.width,
      (extent.height + blockSize.height - offset.y - 1) / blockSize.height,
      (extent.depth  + blockSize.depth  - offset.z - 1) / blockSize.depth };
  }
  
  /**
   * \brief Computes block extent for compressed images
   * 
   * \param [in] blockCount The number of blocks
   * \param [in] blockSize Size of a pixel block
   * \returns Extent of the given blocks
   */
  inline VkExtent3D computeBlockExtent(VkExtent3D blockCount, VkExtent3D blockSize) {
    return VkExtent3D {
      blockCount.width  * blockSize.width,
      blockCount.height * blockSize.height,
      blockCount.depth  * blockSize.depth };
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
