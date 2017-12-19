#pragma once

#include "../dxvk/dxvk_device.h"

#include "../dxbc/dxbc_util.h"

#include "d3d11_include.h"

namespace dxvk {
  
  HRESULT GetSampleCount(
          UINT                      Count,
          VkSampleCountFlagBits*    pCount);
  
  VkCompareOp DecodeCompareOp(
          D3D11_COMPARISON_FUNC     mode);
  
  VkMemoryPropertyFlags GetMemoryFlagsForUsage(
          D3D11_USAGE               usage);
  
}