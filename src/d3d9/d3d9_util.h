#pragma once

#include "d3d9_include.h"

#include "d3d9_format.h"

#include "../dxso/dxso_common.h"
#include "../dxvk/dxvk_device.h"

namespace dxvk {

  inline bool InvalidSampler(DWORD Sampler) {
    if (Sampler > 15 && Sampler < D3DDMAPSAMPLER)
      return true;

    if (Sampler > D3DVERTEXTEXTURESAMPLER3)
      return true;
    
    return false;
  }

  inline DWORD RemapSamplerState(DWORD Sampler) {
    if (Sampler >= D3DDMAPSAMPLER)
      Sampler = 16 + (Sampler - D3DDMAPSAMPLER);

    return Sampler;
  }

  inline std::pair<DxsoProgramType, DWORD> RemapStateSamplerShader(DWORD Sampler) {
    if (Sampler > 16)
      return std::make_pair(DxsoProgramType::VertexShader, Sampler - 16);

    return std::make_pair(DxsoProgramType::PixelShader, Sampler);
  }

  inline std::pair<DxsoProgramType, DWORD> RemapSamplerShader(DWORD Sampler) {
    Sampler = RemapSamplerState(Sampler);

    return RemapStateSamplerShader(Sampler);
  }

  template <typename T, typename J>
  void CastRefPrivate(J* ptr, bool AddRef) {
    if (ptr == nullptr)
      return;

    T* castedPtr = static_cast<T*>(ptr);
    AddRef ? castedPtr->AddRefPrivate() : castedPtr->ReleasePrivate();
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

  uint32_t VertexCount(D3DPRIMITIVETYPE type, UINT count);
  DxvkInputAssemblyState InputAssemblyState(D3DPRIMITIVETYPE type);

  VkBlendFactor DecodeBlendFactor(D3DBLEND BlendFactor, bool IsAlpha);
  VkBlendOp DecodeBlendOp(D3DBLENDOP BlendOp);

  VkFilter DecodeFilter(D3DTEXTUREFILTERTYPE Filter);
  VkSamplerMipmapMode DecodeMipFilter(D3DTEXTUREFILTERTYPE Filter);
  bool IsAnisotropic(D3DTEXTUREFILTERTYPE Filter);
  VkSamplerAddressMode DecodeAddressMode(D3DTEXTUREADDRESS Mode);
  VkCompareOp DecodeCompareOp(D3DCMPFUNC Func);
  VkStencilOp DecodeStencilOp(D3DSTENCILOP Op);

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