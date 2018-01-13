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
  
}

bool operator == (VkExtent3D a, VkExtent3D b);
bool operator != (VkExtent3D a, VkExtent3D b);

bool operator == (VkExtent2D a, VkExtent2D b);
bool operator != (VkExtent2D a, VkExtent2D b);
