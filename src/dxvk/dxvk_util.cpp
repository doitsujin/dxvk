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
  
}
