#pragma once

#include "d3d9_include.h"

#include "d3d9_format.h"

#include "../dxvk/dxvk_device.h"

namespace dxvk {

  template <typename T>
  inline void forEachSampler(T func) {
    for (uint32_t i = 0; i <= D3DVERTEXTEXTURESAMPLER3; i = (i != 15) ? (i + 1) : D3DVERTEXTEXTURESAMPLER0)
      func(i);
  }

  HRESULT DecodeMultiSampleType(
        D3DMULTISAMPLE_TYPE       MultiSample,
        VkSampleCountFlagBits*    pCount);

  bool    ResourceBindable(
        DWORD                     Usage,
        D3DPOOL                   Pool);

  VkPipelineStageFlags EnabledShaderStages();

  VkFormat GetPackedDepthStencilFormat(D3D9Format Format);

  VkFormatFeatureFlags GetImageFormatFeatures(DWORD Usage);

  VkImageUsageFlags GetImageUsageFlags(DWORD Usage);

}