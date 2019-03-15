#pragma once

#include "d3d9_include.h"

#include "d3d9_format.h"

#include "../dxso/dxso_common.h"
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

  VkFormat GetPackedDepthStencilFormat(D3D9Format Format);

  VkFormatFeatureFlags GetImageFormatFeatures(DWORD Usage);

  VkImageUsageFlags GetImageUsageFlags(DWORD Usage);

  VkMemoryPropertyFlags GetMemoryFlagsForUsage(
          DWORD                   Usage);

  template <typename T>
  void changePrivate(T*& srcPtr, T* newPtr) {
    if (srcPtr != nullptr)
      srcPtr->ReleasePrivate();

    if (newPtr != nullptr)
      newPtr->AddRefPrivate();

    srcPtr = newPtr;
  }

  inline void DecodeD3DCOLOR(D3DCOLOR color, float* rgba) {
    // Encoded in D3DCOLOR as argb
    rgba[3] = (float)((color & 0xff000000) >> 24) / 255.0f;
    rgba[0] = (float)((color & 0x00ff0000) >> 16) / 255.0f;
    rgba[1] = (float)((color & 0x0000ff00) >> 8) / 255.0f;
    rgba[2] = (float)((color & 0x000000ff)) / 255.0f;
  }

  inline VkFormat PickSRGB(VkFormat format, VkFormat srgbFormat, bool srgb) {
    if (srgbFormat == VK_FORMAT_UNDEFINED || !srgb)
      return format;

    return srgbFormat;
  }

  inline VkShaderStageFlagBits GetShaderStage(DxsoProgramType ProgramType) {
    switch (ProgramType) {
      case DxsoProgramType::VertexShader:   return VK_SHADER_STAGE_VERTEX_BIT;
      case DxsoProgramType::PixelShader:    return VK_SHADER_STAGE_FRAGMENT_BIT;
      default:                              return VkShaderStageFlagBits(0);
    }
  }

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

}