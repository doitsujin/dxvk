#pragma once

#include "../dxvk/dxvk_device.h"

#include "../dxbc/dxbc_util.h"

#include "d3d11_include.h"

namespace dxvk {
  
  template<typename T>
  UINT CompactSparseList(T* pData, UINT Mask) {
    uint32_t count = 0;
    
    for (uint32_t id : bit::BitMask(Mask))
      pData[count++] = pData[id];

    return count;
  }

  HRESULT DecodeSampleCount(
          UINT                      Count,
          VkSampleCountFlagBits*    pCount);
    
  VkSamplerAddressMode DecodeAddressMode(
          D3D11_TEXTURE_ADDRESS_MODE  mode);
  
  VkCompareOp DecodeCompareOp(
          D3D11_COMPARISON_FUNC     Mode);
  
  VkConservativeRasterizationModeEXT DecodeConservativeRasterizationMode(
          D3D11_CONSERVATIVE_RASTERIZATION_MODE Mode);

  VkShaderStageFlagBits GetShaderStage(
          DxbcProgramType           ProgramType);
  
  VkFormatFeatureFlags GetBufferFormatFeatures(
          UINT                      BindFlags);

  VkFormatFeatureFlags GetImageFormatFeatures(
          UINT                      BindFlags);
  
  VkFormat GetPackedDepthStencilFormat(
          DXGI_FORMAT               Format);

}