#pragma once

#include <dxvk_device.h>

#include "d3d11_include.h"

namespace dxvk {
  
  VkCompareOp DecodeCompareOp(
          D3D11_COMPARISON_FUNC mode);
  
  VkMemoryPropertyFlags GetMemoryFlagsForUsage(
          D3D11_USAGE             usage);
  
}