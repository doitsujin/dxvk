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
  
}

bool operator == (VkExtent3D a, VkExtent3D b) {
  return a.width  == b.width
      && a.height == b.height
      && a.depth  == b.depth;
}


bool operator != (VkExtent3D a, VkExtent3D b) {
  return a.width  != b.width
      || a.height != b.height
      || a.depth  != b.depth;
}
