#include <cstring>

#include "dxvk_format.h"
#include "dxvk_util.h"

namespace dxvk::util {
  
  VkPipelineStageFlags pipelineStages(
          VkShaderStageFlags shaderStages) {
    VkPipelineStageFlags result = 0;
    if (shaderStages & VK_SHADER_STAGE_COMPUTE_BIT)
      result |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    if (shaderStages & VK_SHADER_STAGE_VERTEX_BIT)
      result |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    if (shaderStages & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
      result |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
    if (shaderStages & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
      result |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
    if (shaderStages & VK_SHADER_STAGE_GEOMETRY_BIT)
      result |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    if (shaderStages & VK_SHADER_STAGE_FRAGMENT_BIT)
      result |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    return result;
  }
  
  
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
          char*             dstData,
    const char*             srcData,
          VkExtent3D        blockCount,
          VkDeviceSize      blockSize,
          VkDeviceSize      pitchPerRow,
          VkDeviceSize      pitchPerLayer) {
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
  
  
  VkDeviceSize computeImageDataSize(VkFormat format, VkExtent3D extent) {
    const DxvkFormatInfo* formatInfo = imageFormatInfo(format);
    return formatInfo->elementSize * flattenImageExtent(computeBlockCount(extent, formatInfo->blockSize));
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
