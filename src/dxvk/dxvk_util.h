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
   * \param [in] dstBytes Destination buffer pointer
   * \param [in] srcBytes Pointer to source data
   * \param [in] blockCount Number of blocks to copy
   * \param [in] blockSize Number of bytes per block
   * \param [in] pitchPerRow Number of bytes between rows
   * \param [in] pitchPerLayer Number of bytes between layers
   */
  void packImageData(
          void*             dstBytes,
    const void*             srcBytes,
          VkExtent3D        blockCount,
          VkDeviceSize      blockSize,
          VkDeviceSize      pitchPerRow,
          VkDeviceSize      pitchPerLayer);
  
  /**
   * \brief Repacks image data to a buffer
   * 
   * Note that passing destination pitches of 0 means that the data is
   * tightly packed, while a source pitch of 0 will not show this behaviour
   * in order to match client API behaviour for initialization.
   * \param [in] dstBytes Destination buffer pointer
   * \param [in] srcBytes Pointer to source data
   * \param [in] srcRowPitch Number of bytes between rows to read
   * \param [in] srcSlicePitch Number of bytes between layers to read
   * \param [in] dstRowPitch Number of bytes between rows to write
   * \param [in] dstSlicePitch Number of bytes between layers to write
   * \param [in] imageType Image type (2D, 3D etc)
   * \param [in] imageExtent Image extent, in pixels
   * \param [in] imageLayers Image layer count
   * \param [in] formatInfo Image format info
   * \param [in] aspectMask Image aspects to pack
   */
  void packImageData(
          void*             dstBytes,
    const void*             srcBytes,
          VkDeviceSize      srcRowPitch,
          VkDeviceSize      srcSlicePitch,
          VkDeviceSize      dstRowPitchIn,
          VkDeviceSize      dstSlicePitchIn,
          VkImageType       imageType,
          VkExtent3D        imageExtent,
          uint32_t          imageLayers,
    const DxvkFormatInfo*   formatInfo,
          VkImageAspectFlags aspectMask);
  
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
   * \brief Checks whether an offset and extent are block-aligned
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
        && ((extent.depth  % blockSize.depth  == 0) || (uint32_t(offset.z + extent.depth)  == imageSize.depth))
        && isBlockAligned(offset, blockSize);
  }
  
  /**
   * \brief Computes mip level extent
   *
   * \param [in] size Base mip level extent
   * \param [in] level mip level to compute
   * \returns Extent of the given mip level
   */
  inline VkExtent3D computeMipLevelExtent(VkExtent3D size, uint32_t level) {
    size.width  = std::max(1u, size.width  >> level);
    size.height = std::max(1u, size.height >> level);
    size.depth  = std::max(1u, size.depth  >> level);
    return size;
  }
  
  /**
   * \brief Computes mip level extent
   *
   * This function variant takes into account planar formats.
   * \param [in] size Base mip level extent
   * \param [in] level Mip level to compute
   * \param [in] format Image format
   * \param [in] aspect Image aspect to consider
   * \returns Extent of the given mip level
   */
  inline VkExtent3D computeMipLevelExtent(VkExtent3D size, uint32_t level, VkFormat format, VkImageAspectFlags aspect) {
    if (unlikely(!(aspect & (VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)))) {
      auto plane = &imageFormatInfo(format)->planes[vk::getPlaneIndex(aspect)];
      size.width  /= plane->blockSize.width;
      size.height /= plane->blockSize.height;
    }

    size.width  = std::max(1u, size.width  >> level);
    size.height = std::max(1u, size.height >> level);
    size.depth  = std::max(1u, size.depth  >> level);
    return size;
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
   * \brief Snaps block-aligned image extent to image edges
   * 
   * Fixes up an image extent that is aligned to a compressed
   * block so that it no longer exceeds the given image size.
   * \param [in] offset Aligned pixel offset
   * \param [in] extent Extent to clamp
   * \param [in] imageExtent Image size
   * \returns Number of blocks in the image
   */
  inline VkExtent3D snapExtent3D(VkOffset3D offset, VkExtent3D extent, VkExtent3D imageExtent) {
    return VkExtent3D {
      std::min(extent.width,  imageExtent.width  - uint32_t(offset.x)),
      std::min(extent.height, imageExtent.height - uint32_t(offset.y)),
      std::min(extent.depth,  imageExtent.depth  - uint32_t(offset.z)) };
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
   * \brief Checks whether the depth aspect is read-only in a layout
   * 
   * \param [in] layout Image layout. Must be a valid depth-stencil attachment laoyut.
   * \returns \c true if the depth aspect for images in this layout is read-only.
   */
  inline bool isDepthReadOnlyLayout(VkImageLayout layout) {
    return layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        || layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
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

  /**
   * \brief Applies a component mapping to a component mask
   * 
   * For each component, the component specified in the mapping
   * is used to look up the flag of the original component mask.
   * If the component mapping is zero or one, the corresponding
   * mask bit will be set to zero.
   * \param [in] mask The original component mask
   * \param [in] mapping Component mapping to apply
   * \returns Remapped component mask
   */
  VkColorComponentFlags remapComponentMask(
          VkColorComponentFlags       mask,
          VkComponentMapping          mapping);
  
  /**
   * \brief Inverts a component mapping
   *
   * Transforms a component mapping so that components can
   * be mapped back to their original location. Requires
   * that each component is used only once.
   * 
   * For example. when given a mapping of (0,0,0,R),
   * this function will return the mapping (A,0,0,0).
   * \returns Inverted component mapping
   */
  VkComponentMapping invertComponentMapping(
          VkComponentMapping          mapping);

  /**
   * \brief Resolves source component mapping
   *
   * Returns the source component mapping after rearranging
   * the destination mapping to be the identity mapping.
   * \param [in] dstMapping Destination mapping
   * \param [in] srcMapping Source mapping
   * \returns Adjusted src component mapping
   */
  VkComponentMapping resolveSrcComponentMapping(
          VkComponentMapping          dstMapping,
          VkComponentMapping          srcMapping);
  
  bool isIdentityMapping(
          VkComponentMapping          mapping);

  /**
   * \brief Computes component index for a component swizzle
   * 
   * \param [in] component The component swizzle
   * \param [in] identity Value for SWIZZLE_IDENTITY
   * \returns Component index
   */
  uint32_t getComponentIndex(
          VkComponentSwizzle          component,
          uint32_t                    identity);
  
  VkClearColorValue swizzleClearColor(
          VkClearColorValue           color,
          VkComponentMapping          mapping);
  
  bool isBlendConstantBlendFactor(
          VkBlendFactor               factor);
  
  bool isDualSourceBlendFactor(
          VkBlendFactor               factor);
  
}
