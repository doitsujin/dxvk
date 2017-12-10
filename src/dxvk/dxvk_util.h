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
  
  
}

bool operator == (VkExtent3D a, VkExtent3D b);
bool operator != (VkExtent3D a, VkExtent3D b);
