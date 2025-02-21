#pragma once

#define D3D11_NO_HELPERS

#include "d3d9_include.h"
#include "d3d9_caps.h"

#include "d3d9_format.h"

#include "../dxso/dxso_common.h"
#include "../dxvk/dxvk_device.h"

#include "../util/util_matrix.h"
#include "../util/util_misc.h"

#include <d3dcommon.h>

namespace dxvk {

  struct D3D9ShaderMasks {
    uint32_t samplerMask;
    uint32_t rtMask;
  };

  static constexpr D3D9ShaderMasks FixedFunctionMask =
    { 0b11111111, 0b1 };

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

  /**
   * @brief Returns whether or not the sampler index is valid
   *
   * @param Sampler Sampler index (according to the API)
   */
  inline bool InvalidSampler(DWORD Sampler) {
    if (Sampler >= caps::MaxTexturesPS && Sampler < D3DDMAPSAMPLER)
      return true;

    if (Sampler > D3DVERTEXTEXTURESAMPLER3)
      return true;

    return false;
  }

  /**
   * @brief The first sampler that belongs to the vertex shader according to our internal way of storing samplers
   */
  constexpr uint32_t FirstVSSamplerSlot = caps::MaxTexturesPS + 1;

  /**
   * @brief Remaps a sampler index by the API to an internal one
   *
   * Remaps the sampler index according to the way the API counts them to how we count and store them internally.
   *
   * @param Sampler Sampler index (according to API)
   * @return DWORD Sampler index (according to our internal way of storing samplers)
   */
  inline DWORD RemapSamplerState(DWORD Sampler) {
    if (Sampler >= D3DDMAPSAMPLER)
      Sampler = caps::MaxTexturesPS + (Sampler - D3DDMAPSAMPLER);

    return Sampler;
  }

  /**
   * @brief Remaps the sampler from an index applying to the entire pipeline to one relative to the shader stage and returns the shader type
   *
   * The displacement map sampler will be treated as a 17th pixel shader sampler.
   *
   * @param Sampler Sampler index (according to our internal way of storing samplers)
   * @return std::pair<DxsoProgramType, DWORD> Shader stage that it belongs to and the relative sampler index
   */
  inline std::pair<DxsoProgramType, DWORD> RemapStateSamplerShader(DWORD Sampler) {
    if (Sampler >= FirstVSSamplerSlot)
      return std::make_pair(DxsoProgramTypes::VertexShader, Sampler - FirstVSSamplerSlot);

    return std::make_pair(DxsoProgramTypes::PixelShader, Sampler);
  }

  /**
   * @brief Returns whether the sampler belongs to the vertex shader.
   *
   * The displacement map sampler is part of a fixed function feature,
   * so it does not belong to the vertex shader.
   * Use IsDMAPSampler to check for that.
   *
   * @param Sampler Sampler index (according to our internal way of storing samplers)
   */
  inline bool IsVSSampler(uint32_t Sampler) {
    return Sampler >= FirstVSSamplerSlot;
  }

  /**
   * @brief Returns whether the sampler belongs to the pixel shader.
   *
   * The displacement map sampler is part of a fixed function feature,
   * so (unlike in RemapStateSamplerShader) it does not belong to the pixel shader.
   * Use IsDMAPSampler to check for that.
   *
   * @param Sampler Sampler index (according to our internal way of storing samplers)
   */
  inline bool IsPSSampler(uint32_t Sampler) {
    return Sampler <= caps::MaxTexturesPS;
  }

  /**
   * @brief Returns whether the sampler is the displacement map sampler
   *
   * @param Sampler Sampler index (according to our internal way of storing samplers)
   */
  inline bool IsDMAPSampler(uint32_t Sampler) {
    return Sampler > caps::MaxTexturesPS;
  }

  /**
   * @brief Remaps the sampler from an index (counted according to the API) to one relative to the shader stage and returns the shader type
   *
   * @param Sampler Sampler index (according to the API)
   * @return std::pair<DxsoProgramType, DWORD> Shader stage that it belongs to and the relative sampler index
   */
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
    const Rc<DxvkDevice>&           pDevice,
          D3DMULTISAMPLE_TYPE       MultiSample,
          DWORD                     MultisampleQuality,
          VkSampleCountFlagBits*    pSampleCount);

  VkFormat GetPackedDepthStencilFormat(D3D9Format Format);

  VkFormatFeatureFlags2 GetImageFormatFeatures(DWORD Usage);

  VkImageUsageFlags GetImageUsageFlags(DWORD Usage);

  inline VkFormat PickSRGB(VkFormat format, VkFormat srgbFormat, bool srgb) {
    return srgb ? srgbFormat : format;
  }

  constexpr VkShaderStageFlagBits GetShaderStage(DxsoProgramType ProgramType) {
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

    return Matrix4(Matrix->m);
  }

  uint32_t GetVertexCount(D3DPRIMITIVETYPE type, UINT count);

  DxvkInputAssemblyState DecodeInputAssemblyState(D3DPRIMITIVETYPE type);

  VkBlendFactor DecodeBlendFactor(D3DBLEND BlendFactor, bool IsAlpha);

  VkBlendOp DecodeBlendOp(D3DBLENDOP BlendOp);

  inline VkFilter DecodeFilter(D3DTEXTUREFILTERTYPE Filter) {
    return Filter > D3DTEXF_POINT ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
  }

  inline VkSamplerMipmapMode DecodeMipFilter(D3DTEXTUREFILTERTYPE Filter) {
    return Filter > D3DTEXF_POINT ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
  }

  inline VkSamplerAddressMode DecodeAddressMode(D3DTEXTUREADDRESS Mode) {
    constexpr uint32_t Lut =
      (uint32_t(VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT)      << (3 * D3DTADDRESS_MIRROR)) |
      (uint32_t(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)        << (3 * D3DTADDRESS_CLAMP)) |
      (uint32_t(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)      << (3 * D3DTADDRESS_BORDER)) |
      (uint32_t(VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE) << (3 * D3DTADDRESS_MIRRORONCE));

    // VK_SAMPLER_ADDRESS_MODE_REPEAT has a value of 0, so we
    // get it for free if the app passes an unsupported value
    uint32_t shift = std::min(uint32_t(Mode) * 3u, 31u);
    return VkSamplerAddressMode((uint32_t(Lut) >> shift) & 0x7u);
  }

  VkCompareOp DecodeCompareOp(D3DCMPFUNC Func);

  VkStencilOp DecodeStencilOp(D3DSTENCILOP Op);

  VkCullModeFlags DecodeCullMode(D3DCULL Mode);

  VkPolygonMode DecodeFillMode(D3DFILLMODE Mode);

  VkIndexType DecodeIndexType(D3D9Format Format);

  VkFormat DecodeDecltype(D3DDECLTYPE Type);

  uint32_t GetDecltypeSize(D3DDECLTYPE Type);

  uint32_t GetDecltypeCount(D3DDECLTYPE Type);

  void ConvertBox(D3DBOX box, VkOffset3D& offset, VkExtent3D& extent);

  void ConvertRect(RECT rect, VkOffset3D& offset, VkExtent3D& extent);

  void ConvertRect(RECT rect, VkOffset2D& offset, VkExtent2D& extent);

  inline float GetDepthBufferRValue(VkFormat Format, int32_t vendorId, bool exact, bool forceUnorm) {
    switch (Format) {
      case VK_FORMAT_D16_UNORM_S8_UINT:
      case VK_FORMAT_D16_UNORM:
        return (vendorId == 0x10de && !exact) ? float(1 << 15) : float(1 << 16);

      case VK_FORMAT_D24_UNORM_S8_UINT:
        return (vendorId == 0x10de && !exact) ? float(1 << 23) : float(1 << 24);

      default:
      case VK_FORMAT_D32_SFLOAT_S8_UINT:
      case VK_FORMAT_D32_SFLOAT:
        return forceUnorm ? float(1 << 24) : float(1 << 23);
    }
  }

  template<typename T>
  UINT CompactSparseList(T* pData, UINT Mask) {
    uint32_t count = 0;

    for (uint32_t id : bit::BitMask(Mask))
      pData[count++] = pData[id];

    return count;
  }

  bool IsDepthFormat(D3D9Format Format);

  bool IsDepthStencilFormat(D3D9Format Format);

  bool IsLockableDepthStencilFormat(D3D9Format Format);


  inline bool IsPoolManaged(D3DPOOL Pool) {
    return Pool == D3DPOOL_MANAGED || Pool == D3DPOOL_MANAGED_EX;
  }

  inline D3DRENDERSTATETYPE ColorWriteIndex(uint32_t i) {
    return D3DRENDERSTATETYPE(i ? D3DRENDERSTATETYPE(D3DRS_COLORWRITEENABLE1 + i - 1) : D3DRS_COLORWRITEENABLE);
  }

  inline bool AreFormatsSimilar(D3D9Format srcFormat, D3D9Format dstFormat) {
    return (srcFormat == dstFormat)
        || (srcFormat == D3D9Format::A8B8G8R8 && dstFormat == D3D9Format::X8B8G8R8)
        || (srcFormat == D3D9Format::A8R8G8B8 && dstFormat == D3D9Format::X8R8G8B8)
        || (srcFormat == D3D9Format::A1R5G5B5 && dstFormat == D3D9Format::X1R5G5B5)
        || (srcFormat == D3D9Format::A4R4G4B4 && dstFormat == D3D9Format::X4R4G4B4);
  }

  inline bool IsBlitRegionInvalid(VkOffset3D offsets[2], VkExtent3D extent) {
    // Only bother checking x, y as we don't have 3D blits.
    return offsets[1].x < offsets[0].x ||
           offsets[1].y < offsets[0].y ||
           offsets[0].x < 0 ||
           offsets[0].y < 0 ||
           uint32_t(offsets[1].x) > extent.width ||
           uint32_t(offsets[1].y) > extent.height;
  }

  /**
   * @brief Mirrors D3DTEXTURESTAGESTATETYPE but starts at 0
   */
  enum D3D9TextureStageStateTypes : uint32_t
  {
      DXVK_TSS_COLOROP        =  0,
      DXVK_TSS_COLORARG1      =  1,
      DXVK_TSS_COLORARG2      =  2,
      DXVK_TSS_ALPHAOP        =  3,
      DXVK_TSS_ALPHAARG1      =  4,
      DXVK_TSS_ALPHAARG2      =  5,
      DXVK_TSS_BUMPENVMAT00   =  6,
      DXVK_TSS_BUMPENVMAT01   =  7,
      DXVK_TSS_BUMPENVMAT10   =  8,
      DXVK_TSS_BUMPENVMAT11   =  9,
      DXVK_TSS_TEXCOORDINDEX  = 10,
      DXVK_TSS_BUMPENVLSCALE  = 21,
      DXVK_TSS_BUMPENVLOFFSET = 22,
      DXVK_TSS_TEXTURETRANSFORMFLAGS = 23,
      DXVK_TSS_COLORARG0      = 25,
      DXVK_TSS_ALPHAARG0      = 26,
      DXVK_TSS_RESULTARG      = 27,
      DXVK_TSS_CONSTANT       = 31,
      DXVK_TSS_COUNT          = 32
  };

  constexpr uint32_t DXVK_TSS_TCI_PASSTHRU                      = 0x00000000;
  constexpr uint32_t DXVK_TSS_TCI_CAMERASPACENORMAL             = 0x00010000;
  constexpr uint32_t DXVK_TSS_TCI_CAMERASPACEPOSITION           = 0x00020000;
  constexpr uint32_t DXVK_TSS_TCI_CAMERASPACEREFLECTIONVECTOR   = 0x00030000;
  constexpr uint32_t DXVK_TSS_TCI_SPHEREMAP                     = 0x00040000;

  /**
   * @brief Remaps a texture stage type by the API to an internal one
   *
   * @param Type Texture stage type according to the API
   * @return D3D9TextureStageStateTypes Texture stage type according to our internal way of storing them
   */
  inline D3D9TextureStageStateTypes RemapTextureStageStateType(D3DTEXTURESTAGESTATETYPE Type) {
    return D3D9TextureStageStateTypes(Type - 1);
  }

}


inline bool operator == (const D3DVIEWPORT9& a, const D3DVIEWPORT9& b) {
  return a.X      == b.X      &&
         a.Y      == b.Y      &&
         a.Width  == b.Width  &&
         a.Height == b.Height &&
         a.MinZ   == b.MinZ   &&
         a.MaxZ   == b.MaxZ;
}

inline bool operator != (const D3DVIEWPORT9& a, const D3DVIEWPORT9& b) {
  return !(a == b);
}

inline bool operator == (const RECT& a, const RECT& b) {
  return a.left   == b.left  &&
         a.right  == b.right &&
         a.top    == b.top   &&
         a.bottom == b.bottom;
}

inline bool operator != (const RECT& a, const RECT& b) {
  return !(a == b);
}

inline bool operator == (const POINT& a, const POINT& b) {
  return a.x == b.x && a.y == b.y;
}

inline bool operator != (const POINT& a, const POINT& b) {
  return !(a == b);
}

inline bool operator == (const D3DDISPLAYMODEEX& a, const D3DDISPLAYMODEEX& b) {
  return a.Size             == b.Size             &&
         a.Width            == b.Width            &&
         a.Height           == b.Height           &&
         a.RefreshRate      == b.RefreshRate      &&
         a.Format           == b.Format           &&
         a.ScanLineOrdering == b.ScanLineOrdering;
}

