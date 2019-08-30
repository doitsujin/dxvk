#pragma once

#include "d3d9_include.h"

#include "d3d9_format.h"

#include "../dxso/dxso_common.h"
#include "../dxvk/dxvk_device.h"

#include "../util/util_matrix.h"

#include <d3dcommon.h>

namespace dxvk {

  struct D3D9MipFilter {
    bool                MipsEnabled;
    VkSamplerMipmapMode MipFilter;
  };

  struct D3D9BlendState {
    D3DBLEND   Src;
    D3DBLEND   Dst;
    D3DBLENDOP Op;
  };

  inline void FixupBlendState(D3D9BlendState& State) {
    // Old DirectX 6 HW feature that still exists...
    // Yuck!
    if (unlikely(State.Src == D3DBLEND_BOTHSRCALPHA)) {
      State.Src = D3DBLEND_SRCALPHA;
      State.Dst = D3DBLEND_INVSRCALPHA;
    }
    else if (unlikely(State.Src == D3DBLEND_BOTHINVSRCALPHA)) {
      State.Src = D3DBLEND_INVSRCALPHA;
      State.Dst = D3DBLEND_SRCALPHA;
    }
  }

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
    if (Sampler >= 17)
      return std::make_pair(DxsoProgramTypes::VertexShader, Sampler - 17);

    return std::make_pair(DxsoProgramTypes::PixelShader, Sampler);
  }

  inline std::pair<DxsoProgramType, DWORD> RemapSamplerShader(DWORD Sampler) {
    Sampler = RemapSamplerState(Sampler);

    return RemapStateSamplerShader(Sampler);
  }

  template <typename T, typename J>
  void CastRefPrivate(J* ptr, bool AddRef) {
    if (ptr == nullptr)
      return;

    T* castedPtr = reinterpret_cast<T*>(ptr);
    AddRef ? castedPtr->AddRefPrivate() : castedPtr->ReleasePrivate();
  }

  HRESULT DisassembleShader(
    const void*      pShader, 
          BOOL       EnableColorCode, 
          char*      pComments, 
          ID3DBlob** ppDisassembly);

  HRESULT DecodeMultiSampleType(
        D3DMULTISAMPLE_TYPE       MultiSample,
        DWORD                     MultisampleQuality,
        VkSampleCountFlagBits*    pCount);

  VkFormat GetPackedDepthStencilFormat(D3D9Format Format);

  VkFormatFeatureFlags GetImageFormatFeatures(DWORD Usage);

  VkImageUsageFlags GetImageUsageFlags(DWORD Usage);

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
    rgba[1] = (float)((color & 0x0000ff00) >> 8)  / 255.0f;
    rgba[2] = (float)((color & 0x000000ff))       / 255.0f;
  }

  inline VkFormat PickSRGB(VkFormat format, VkFormat srgbFormat, bool srgb) {
    return srgb ? srgbFormat : format;
  }

  inline VkShaderStageFlagBits GetShaderStage(DxsoProgramType ProgramType) {
    switch (ProgramType) {
      case DxsoProgramTypes::VertexShader:  return VK_SHADER_STAGE_VERTEX_BIT;
      case DxsoProgramTypes::PixelShader:   return VK_SHADER_STAGE_FRAGMENT_BIT;
      default:                              return VkShaderStageFlagBits(0);
    }
  }

  inline uint32_t GetTransformIndex(D3DTRANSFORMSTATETYPE Type) {
    if (Type == D3DTS_VIEW)
      return 0;

    if (Type == D3DTS_PROJECTION)
      return 1;

    if (Type >= D3DTS_TEXTURE0 && Type <= D3DTS_TEXTURE7)
      return 2 + (Type - D3DTS_TEXTURE0);

    return 10 + (Type - D3DTS_WORLD);
  }

  inline Matrix4 ConvertMatrix(const D3DMATRIX* Matrix) {
    if (Matrix == nullptr) // Identity.
      return Matrix4();

    return *(reinterpret_cast<const Matrix4*>(Matrix));
  }

  uint32_t GetVertexCount(D3DPRIMITIVETYPE type, UINT count);

  DxvkInputAssemblyState DecodeInputAssemblyState(D3DPRIMITIVETYPE type);

  VkBlendFactor DecodeBlendFactor(D3DBLEND BlendFactor, bool IsAlpha);

  VkBlendOp DecodeBlendOp(D3DBLENDOP BlendOp);

  VkFilter DecodeFilter(D3DTEXTUREFILTERTYPE Filter);

  D3D9MipFilter DecodeMipFilter(D3DTEXTUREFILTERTYPE Filter);

  bool IsAnisotropic(D3DTEXTUREFILTERTYPE Filter);

  VkSamplerAddressMode DecodeAddressMode(D3DTEXTUREADDRESS Mode);

  VkCompareOp DecodeCompareOp(D3DCMPFUNC Func);

  VkStencilOp DecodeStencilOp(D3DSTENCILOP Op);

  VkCullModeFlags DecodeCullMode(D3DCULL Mode);

  VkPolygonMode DecodeFillMode(D3DFILLMODE Mode);

  VkIndexType DecodeIndexType(D3D9Format Format);

  VkFormat DecodeDecltype(D3DDECLTYPE Type);

  uint32_t GetDecltypeSize(D3DDECLTYPE Type);

  void ConvertBox(D3DBOX box, VkOffset3D& offset, VkExtent3D& extent);

  void ConvertRect(RECT rect, VkOffset3D& offset, VkExtent3D& extent);

  void ConvertRect(RECT rect, VkOffset2D& offset, VkExtent2D& extent);

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

  bool IsDepthFormat(D3D9Format Format);

  inline bool IsPoolManaged(D3DPOOL Pool) {
    return Pool == D3DPOOL_MANAGED || Pool == D3DPOOL_MANAGED_EX;
  }

}