#include <cstring>

#include "dxvk_format.h"
#include "dxvk_util.h"

namespace dxvk::util {
  
  uint32_t computeMipLevelCount(VkExtent3D imageSize) {
    uint32_t maxDim = std::max(imageSize.width, imageSize.height);
             maxDim = std::max(imageSize.depth, maxDim);
    uint32_t mipCnt = 0;
    
    while (maxDim > 0) {
      mipCnt += 1;
      maxDim /= 2;
    }
    
    return mipCnt;
  }
  
  
  void packImageData(
          void*             dstBytes,
    const void*             srcBytes,
          VkExtent3D        blockCount,
          VkDeviceSize      blockSize,
          VkDeviceSize      pitchPerRow,
          VkDeviceSize      pitchPerLayer) {
    auto dstData = reinterpret_cast<      char*>(dstBytes);
    auto srcData = reinterpret_cast<const char*>(srcBytes);
    
    const VkDeviceSize bytesPerRow   = blockCount.width  * blockSize;
    const VkDeviceSize bytesPerLayer = blockCount.height * bytesPerRow;
    const VkDeviceSize bytesTotal    = blockCount.depth  * bytesPerLayer;
    
    const bool directCopy = ((bytesPerRow   == pitchPerRow  ) || (blockCount.height == 1))
                         && ((bytesPerLayer == pitchPerLayer) || (blockCount.depth  == 1));
    
    if (directCopy) {
      std::memcpy(dstData, srcData, bytesTotal);
    } else {
      for (uint32_t i = 0; i < blockCount.depth; i++) {
        for (uint32_t j = 0; j < blockCount.height; j++) {
          std::memcpy(
            dstData + j * bytesPerRow,
            srcData + j * pitchPerRow,
            bytesPerRow);
        }
        
        srcData += pitchPerLayer;
        dstData += bytesPerLayer;
      }
    }
  }
  
  
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
          VkImageAspectFlags aspectMask) {
    auto dstData = reinterpret_cast<      char*>(dstBytes);
    auto srcData = reinterpret_cast<const char*>(srcBytes);

    for (uint32_t k = 0; k < imageLayers; k++) {
      for (auto aspects = aspectMask; aspects; ) {
        auto aspect = vk::getNextAspect(aspects);
        auto extent = imageExtent;
        auto elementSize = formatInfo->elementSize;

        if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
          auto plane = &formatInfo->planes[vk::getPlaneIndex(aspect)];
          extent.width  /= plane->blockSize.width;
          extent.height /= plane->blockSize.height;
          elementSize = plane->elementSize;
        }

        auto blockCount = computeBlockCount(extent, formatInfo->blockSize);

        VkDeviceSize bytesPerRow   = blockCount.width  * elementSize;
        VkDeviceSize bytesPerSlice = blockCount.height * bytesPerRow;
        VkDeviceSize bytesTotal    = blockCount.depth  * bytesPerSlice;

        VkDeviceSize dstRowPitch   = dstRowPitchIn   ? dstRowPitchIn   : bytesPerRow;
        VkDeviceSize dstSlicePitch = dstSlicePitchIn ? dstSlicePitchIn : bytesPerSlice;

        const bool directCopy = ((bytesPerRow   == srcRowPitch   && bytesPerRow   == dstRowPitch  ) || (blockCount.height == 1))
                             && ((bytesPerSlice == srcSlicePitch && bytesPerSlice == dstSlicePitch) || (blockCount.depth  == 1));

        if (directCopy) {
          std::memcpy(dstData, srcData, bytesTotal);

          switch (imageType) {
            case VK_IMAGE_TYPE_1D:
              srcData += srcRowPitch;
              dstData += dstRowPitch;
              break;
            case VK_IMAGE_TYPE_2D:
              srcData += blockCount.height * srcRowPitch;
              dstData += blockCount.height * dstRowPitch;
              break;
            case VK_IMAGE_TYPE_3D:
              srcData += blockCount.depth * srcSlicePitch;
              dstData += blockCount.depth * dstSlicePitch;
              break;
            default: ;
          }
        } else {
          for (uint32_t i = 0; i < blockCount.depth; i++) {
            for (uint32_t j = 0; j < blockCount.height; j++) {
              std::memcpy(
                dstData + j * dstRowPitch,
                srcData + j * srcRowPitch,
                bytesPerRow);
            }

            switch (imageType) {
              case VK_IMAGE_TYPE_1D:
                srcData += srcRowPitch;
                dstData += dstRowPitch;
                break;
              case VK_IMAGE_TYPE_2D:
                srcData += blockCount.height * srcRowPitch;
                dstData += blockCount.height * dstRowPitch;
                break;
              case VK_IMAGE_TYPE_3D:
                srcData += srcSlicePitch;
                dstData += dstSlicePitch;
                break;
              default: ;
            }
          }
        }
      }
    }
  }


  VkDeviceSize computeImageDataSize(VkFormat format, VkExtent3D extent) {
    const DxvkFormatInfo* formatInfo = lookupFormatInfo(format);
    return computeImageDataSize(format, extent, formatInfo->aspectMask);
  }


  VkDeviceSize computeImageDataSize(VkFormat format, VkExtent3D extent, VkImageAspectFlags aspects) {
    const DxvkFormatInfo* formatInfo = lookupFormatInfo(format);

    VkDeviceSize size = 0;

    while (aspects) {
      auto aspect = vk::getNextAspect(aspects);
      auto elementSize = formatInfo->elementSize;
      auto planeExtent = extent;

      if (formatInfo->flags.test(DxvkFormatFlag::MultiPlane)) {
        auto plane = &formatInfo->planes[vk::getPlaneIndex(aspect)];
        planeExtent.width  /= plane->blockSize.width;
        planeExtent.height /= plane->blockSize.height;
        elementSize = plane->elementSize;
      }

      size += elementSize * flattenImageExtent(computeBlockCount(planeExtent, formatInfo->blockSize));
    }

    return size;
  }


  static VkColorComponentFlags remapComponentFlag(
          VkColorComponentFlags       mask,
          VkComponentSwizzle          swizzle,
          VkColorComponentFlagBits    identity) {
    VkColorComponentFlags bit;

    switch (swizzle) {
      case VK_COMPONENT_SWIZZLE_IDENTITY: bit = identity;                 break;
      case VK_COMPONENT_SWIZZLE_R:        bit = VK_COLOR_COMPONENT_R_BIT; break;
      case VK_COMPONENT_SWIZZLE_G:        bit = VK_COLOR_COMPONENT_G_BIT; break;
      case VK_COMPONENT_SWIZZLE_B:        bit = VK_COLOR_COMPONENT_B_BIT; break;
      case VK_COMPONENT_SWIZZLE_A:        bit = VK_COLOR_COMPONENT_A_BIT; break;
      default:                            bit = 0; /* SWIZZLE_ZERO, SWIZZLE_ONE */
    }

    return (mask & bit) ? identity : 0;
  }


  VkColorComponentFlags remapComponentMask(
          VkColorComponentFlags       mask,
          VkComponentMapping          mapping) {
    VkColorComponentFlags result = 0;
    result |= remapComponentFlag(mask, mapping.r, VK_COLOR_COMPONENT_R_BIT);
    result |= remapComponentFlag(mask, mapping.g, VK_COLOR_COMPONENT_G_BIT);
    result |= remapComponentFlag(mask, mapping.b, VK_COLOR_COMPONENT_B_BIT);
    result |= remapComponentFlag(mask, mapping.a, VK_COLOR_COMPONENT_A_BIT);
    return result;
  }


  static VkComponentSwizzle findComponentSwizzle(
          VkComponentSwizzle          swizzle,
          VkComponentSwizzle          identity,
          VkComponentMapping          mapping) {
    if (identity == VK_COMPONENT_SWIZZLE_IDENTITY)
      return VK_COMPONENT_SWIZZLE_IDENTITY;
    
    if (mapping.r == swizzle)
      return VK_COMPONENT_SWIZZLE_R;
    if (mapping.g == swizzle)
      return VK_COMPONENT_SWIZZLE_G;
    if (mapping.b == swizzle)
      return VK_COMPONENT_SWIZZLE_B;
    if (mapping.a == swizzle)
      return VK_COMPONENT_SWIZZLE_A;
    
    return VK_COMPONENT_SWIZZLE_ZERO;
  }


  VkComponentMapping invertComponentMapping(VkComponentMapping mapping) {
    VkComponentMapping result;
    result.r = findComponentSwizzle(VK_COMPONENT_SWIZZLE_R, mapping.r, mapping);
    result.g = findComponentSwizzle(VK_COMPONENT_SWIZZLE_G, mapping.g, mapping);
    result.b = findComponentSwizzle(VK_COMPONENT_SWIZZLE_B, mapping.b, mapping);
    result.a = findComponentSwizzle(VK_COMPONENT_SWIZZLE_A, mapping.a, mapping);
    return result;
  }


  static VkComponentMapping normalizeComponentMapping(
          VkComponentMapping          mapping) {
    mapping.r = mapping.r == VK_COMPONENT_SWIZZLE_IDENTITY ? VK_COMPONENT_SWIZZLE_R : mapping.r;
    mapping.g = mapping.g == VK_COMPONENT_SWIZZLE_IDENTITY ? VK_COMPONENT_SWIZZLE_G : mapping.g;
    mapping.b = mapping.b == VK_COMPONENT_SWIZZLE_IDENTITY ? VK_COMPONENT_SWIZZLE_B : mapping.b;
    mapping.a = mapping.a == VK_COMPONENT_SWIZZLE_IDENTITY ? VK_COMPONENT_SWIZZLE_A : mapping.a;
    return mapping;
  }


  static VkComponentSwizzle resolveComponentSwizzle(
          VkComponentSwizzle          swizzle,
          VkComponentMapping          dstMapping,
          VkComponentMapping          srcMapping) {
    VkComponentSwizzle dstSwizzle = VK_COMPONENT_SWIZZLE_IDENTITY;
    if (dstMapping.r == swizzle) dstSwizzle = VK_COMPONENT_SWIZZLE_R;
    if (dstMapping.g == swizzle) dstSwizzle = VK_COMPONENT_SWIZZLE_G;
    if (dstMapping.b == swizzle) dstSwizzle = VK_COMPONENT_SWIZZLE_B;
    if (dstMapping.a == swizzle) dstSwizzle = VK_COMPONENT_SWIZZLE_A;
    
    switch (dstSwizzle) {
      case VK_COMPONENT_SWIZZLE_R: return srcMapping.r;
      case VK_COMPONENT_SWIZZLE_G: return srcMapping.g;
      case VK_COMPONENT_SWIZZLE_B: return srcMapping.b;
      case VK_COMPONENT_SWIZZLE_A: return srcMapping.a;
      default: return VK_COMPONENT_SWIZZLE_IDENTITY;
    }
  }


  VkComponentMapping resolveSrcComponentMapping(
          VkComponentMapping          dstMapping,
          VkComponentMapping          srcMapping) {
    dstMapping = normalizeComponentMapping(dstMapping);

    VkComponentMapping result;
    result.r = resolveComponentSwizzle(VK_COMPONENT_SWIZZLE_R, dstMapping, srcMapping);
    result.g = resolveComponentSwizzle(VK_COMPONENT_SWIZZLE_G, dstMapping, srcMapping);
    result.b = resolveComponentSwizzle(VK_COMPONENT_SWIZZLE_B, dstMapping, srcMapping);
    result.a = resolveComponentSwizzle(VK_COMPONENT_SWIZZLE_A, dstMapping, srcMapping);
    return result;
  }


  VkBlendFactor remapAlphaToColorBlendFactor(VkBlendFactor factor) {
    switch (factor) {
      // Make sure we use the red component from the
      // fragment shader since alpha may be undefined
      case VK_BLEND_FACTOR_SRC_ALPHA:
        return VK_BLEND_FACTOR_SRC_COLOR;

      case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;

      case VK_BLEND_FACTOR_SRC1_ALPHA:
        return VK_BLEND_FACTOR_SRC1_COLOR;

      case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;

      // This is defined to always be 1 for alpha
      case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
        return VK_BLEND_FACTOR_ONE;

      // Make sure we use the red component from the
      // attachment since there is no alpha component
      case VK_BLEND_FACTOR_DST_ALPHA:
        return VK_BLEND_FACTOR_DST_COLOR;

      case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;

      // For blend constants we actually need to do the
      // opposite and make sure we always use alpha
      case VK_BLEND_FACTOR_CONSTANT_COLOR:
        return VK_BLEND_FACTOR_CONSTANT_ALPHA;

      case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;

      default:
        return factor;
    }
  }


  bool isIdentityMapping(
          VkComponentMapping          mapping) {
    return (mapping.r == VK_COMPONENT_SWIZZLE_R || mapping.r == VK_COMPONENT_SWIZZLE_IDENTITY)
        && (mapping.g == VK_COMPONENT_SWIZZLE_G || mapping.g == VK_COMPONENT_SWIZZLE_IDENTITY)
        && (mapping.b == VK_COMPONENT_SWIZZLE_B || mapping.b == VK_COMPONENT_SWIZZLE_IDENTITY)
        && (mapping.a == VK_COMPONENT_SWIZZLE_A || mapping.a == VK_COMPONENT_SWIZZLE_IDENTITY);
  }


  uint32_t getComponentIndex(
          VkComponentSwizzle          component,
          uint32_t                    identity) {
    switch (component) {
      case VK_COMPONENT_SWIZZLE_R: return 0;
      case VK_COMPONENT_SWIZZLE_G: return 1;
      case VK_COMPONENT_SWIZZLE_B: return 2;
      case VK_COMPONENT_SWIZZLE_A: return 3;
      default: return identity; /* identity, zero, one */
    }
  }


  VkClearColorValue swizzleClearColor(
          VkClearColorValue           color,
          VkComponentMapping          mapping) {
    VkClearColorValue result;
    auto swizzles = &mapping.r;

    for (uint32_t i = 0; i < 4; i++) {
      uint32_t index = getComponentIndex(swizzles[i], i);
      result.uint32[i] = color.uint32[index];
    }

    return result;
  }


  bool isBlendConstantBlendFactor(VkBlendFactor factor) {
    return factor == VK_BLEND_FACTOR_CONSTANT_COLOR
        || factor == VK_BLEND_FACTOR_CONSTANT_ALPHA
        || factor == VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR
        || factor == VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
  }


  bool isDualSourceBlendFactor(VkBlendFactor factor) {
    return factor == VK_BLEND_FACTOR_SRC1_COLOR
        || factor == VK_BLEND_FACTOR_SRC1_ALPHA
        || factor == VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR
        || factor == VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
  }

}
