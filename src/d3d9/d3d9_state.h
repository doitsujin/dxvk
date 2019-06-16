#pragma once

#include "d3d9_caps.h"
#include "d3d9_constant_set.h"
#include "../dxso/dxso_common.h"
#include "../util/util_matrix.h"

#include <array>
#include <bitset>

namespace dxvk {

  static constexpr uint32_t RenderStateCount  = 256;
  static constexpr uint32_t SamplerStateCount = D3DSAMP_DMAPOFFSET + 1;
  static constexpr uint32_t SamplerCount      = 21;

  namespace hacks::PointSize {
    static constexpr DWORD AlphaToCoverageDisabled = MAKEFOURCC('A', '2', 'M', '0');
    static constexpr DWORD AlphaToCoverageEnabled  = MAKEFOURCC('A', '2', 'M', '1');;
  }

  class D3D9Surface;
  class D3D9VertexShader;
  class D3D9PixelShader;
  class D3D9VertexDecl;
  class D3D9VertexBuffer;
  class D3D9IndexBuffer;
  
  struct D3D9ClipPlane {
    float coeff[4];
  };

  struct D3D9RenderStateInfo {
    float alphaRef = 0.0f;
  };

  enum class D3D9RenderStateItem {
    AlphaRef = 0,
    Count    = 1
  };
  
  struct D3D9VBO {
    D3D9VertexBuffer* vertexBuffer = nullptr;
    UINT              offset = 0;
    UINT              stride = 0;
  };

  struct D3D9CapturableState {
    D3D9CapturableState() {
      for (uint32_t i = 0; i < textures.size(); i++)
        textures[i] = nullptr;

      for (uint32_t i = 0; i < clipPlanes.size(); i++)
        clipPlanes[i] = D3D9ClipPlane();

      for (uint32_t i = 0; i < streamFreq.size(); i++)
        streamFreq[i] = 1;
    }

    D3D9VertexDecl*                                  vertexDecl = nullptr;
    D3D9IndexBuffer*                                 indices    = nullptr;

    std::array<DWORD, RenderStateCount>              renderStates = { 0 };

    std::array<
      std::array<DWORD, SamplerStateCount>,
      SamplerCount>                                  samplerStates;

    std::array<D3D9VBO, caps::MaxStreams>            vertexBuffers;

    std::array<
      IDirect3DBaseTexture9*,
      SamplerCount>                                  textures;

    D3D9VertexShader*                                vertexShader = nullptr;
    D3D9PixelShader*                                 pixelShader  = nullptr;

    D3DVIEWPORT9                                     viewport;
    RECT                                             scissorRect;

    std::array<
      D3D9ClipPlane,
      caps::MaxClipPlanes>                           clipPlanes;

    D3D9ShaderConstants                              consts[DxsoProgramTypes::Count];

    std::array<UINT, caps::MaxStreams>               streamFreq;

    std::array<Matrix4, caps::MaxTransforms>         transforms;
  };

  template <
    DxsoProgramType  ProgramType,
    D3D9ConstantType ConstantType,
    typename         T>
  HRESULT UpdateStateConstants(
          D3D9CapturableState* pState,
          UINT                 StartRegister,
    const T*                   pConstantData,
          UINT                 Count) {
    auto& set = pState->consts[ProgramType];

    if constexpr (ConstantType == D3D9ConstantType::Float) {
      auto& consts = set.hardware.fConsts;

      auto begin = reinterpret_cast<const Vector4*>(pConstantData);
      auto end   = begin + Count;

      std::transform(begin, end, consts.begin() + StartRegister, replaceNaN);
    }
    else if constexpr (ConstantType == D3D9ConstantType::Int) {
      auto& consts = set.hardware.iConsts;

      auto begin = reinterpret_cast<const Vector4i*>(pConstantData);
      auto end   = begin + Count;

      std::copy(begin, end, consts.begin() + StartRegister);
    }
    else {
      uint32_t& bitfield = set.hardware.boolBitfield;

      for (uint32_t i = 0; i < Count; i++) {
        const uint32_t idx    = StartRegister + i;
        const uint32_t idxBit = 1u << idx;

        bitfield &= ~idxBit;
        if (pConstantData[i])
          bitfield |= idxBit;
      }
    }

    return D3D_OK;
  }

  enum class D3D9CapturedStateFlag : uint32_t {
    VertexDecl,
    Indices,
    RenderStates,
    SamplerStates,
    VertexBuffers,
    Textures,
    VertexShader,
    PixelShader,
    Viewport,
    ScissorRect,
    ClipPlanes,
    VsConstants,
    PsConstants,
    StreamFreq,
    Transforms
  };

  using D3D9CapturedStateFlags = Flags<D3D9CapturedStateFlag>;

  struct D3D9StateCaptures {
    D3D9CapturedStateFlags flags;

    std::bitset<RenderStateCount>                       renderStates;

    std::bitset<SamplerCount>                           samplers;
    std::array<
      std::bitset<SamplerStateCount>,
      SamplerCount>                                     samplerStates;

    std::bitset<caps::MaxStreams>                       vertexBuffers;
    std::bitset<SamplerCount>                           textures;
    std::bitset<caps::MaxClipPlanes>                    clipPlanes;
    std::bitset<caps::MaxStreams>                       streamFreq;
    std::bitset<caps::MaxTransforms>                    transforms;

    struct {
      std::bitset<caps::MaxFloatConstants>              fConsts;
      std::bitset<caps::MaxOtherConstants>              iConsts;
      std::bitset<caps::MaxOtherConstants>              bConsts;
    } vsConsts, psConsts;
  };

  struct Direct3DState9 : public D3D9CapturableState {
    Direct3DState9() {
      for (uint32_t i = 0; i < renderTargets.size(); i++)
        renderTargets[i] = nullptr;
    }

    std::array<D3D9Surface*, caps::MaxSimultaneousRenderTargets> renderTargets;

    D3D9Surface* depthStencil = nullptr;
    
  };

}