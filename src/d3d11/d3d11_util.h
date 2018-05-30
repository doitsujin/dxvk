#pragma once

#include "../dxvk/dxvk_device.h"

#include "../dxbc/dxbc_util.h"

#include "d3d11_include.h"

namespace dxvk {
  
  template<typename T>
  UINT CompactSparseList(T* pData, UINT Mask) {
    uint32_t count = 0;
    
    while (Mask != 0) {
      uint32_t id = bit::tzcnt(Mask);
      pData[count++] = pData[id];
      Mask &= Mask - 1;
    }

    return count;
  }

  HRESULT DecodeSampleCount(
          UINT                      Count,
          VkSampleCountFlagBits*    pCount);
    
  VkSamplerAddressMode DecodeAddressMode(
          D3D11_TEXTURE_ADDRESS_MODE  mode);
  
  VkBorderColor DecodeBorderColor(
    const FLOAT                     BorderColor[4]);
  
  VkCompareOp DecodeCompareOp(
          D3D11_COMPARISON_FUNC     Mode);
  
  VkMemoryPropertyFlags GetMemoryFlagsForUsage(
          D3D11_USAGE               Usage);
  
}