#pragma once

#include "d3d9_caps.h"
#include "d3d9_constant_set.h"

#include <array>
#include <bitset>

namespace dxvk {

  static constexpr uint32_t RenderStateCount  = D3DRS_BLENDOPALPHA + 1;
  static constexpr uint32_t SamplerStateCount = D3DSAMP_DMAPOFFSET + 1;
  static constexpr uint32_t SamplerCount      = 21;

  class Direct3DSurface9;
  class D3D9VertexShader;
  class D3D9PixelShader;
  class Direct3DVertexDeclaration9;
  class Direct3DVertexBuffer9;
  class Direct3DIndexBuffer9;
  
  struct D3D9ClipPlane {
    float coeff[4];
  };

  struct D3D9RenderStateInfo {
    float alphaRef = 0.0f;
  };
  
  struct D3D9VBO {
    Direct3DVertexBuffer9* vertexBuffer = nullptr;
    UINT                   offset = 0;
    UINT                   stride = 0;
  };

  struct D3D9CapturableState {
    D3D9CapturableState() {
      for (uint32_t i = 0; i < textures.size(); i++)
        textures[i] = nullptr;

      for (uint32_t i = 0; i < clipPlanes.size(); i++)
        clipPlanes[i] = D3D9ClipPlane();
    }

    Direct3DVertexDeclaration9*                      vertexDecl = nullptr;
    Direct3DIndexBuffer9*                            indices    = nullptr;

    std::array<DWORD, RenderStateCount>              renderStates;

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

    D3D9ShaderConstants                              vsConsts;
    D3D9ShaderConstants                              psConsts;
  };

  enum class CapturedStateFlag : uint32_t {
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
    PsConstants
  };

  using CapturedStateFlags = Flags<CapturedStateFlag>;

  struct D3D9StateCaptures {
    CapturedStateFlags flags;

    std::bitset<RenderStateCount>                       renderStates;

    std::array<
      std::bitset<SamplerStateCount>,
      SamplerCount>                                     samplerStates;

    std::bitset<caps::MaxStreams>                       vertexBuffers;
    std::bitset<SamplerCount>                           textures;
    std::bitset<caps::MaxClipPlanes>                    clipPlanes;
  };

  struct Direct3DState9 : public D3D9CapturableState {
    Direct3DState9() {
      for (uint32_t i = 0; i < renderTargets.size(); i++)
        renderTargets[i] = nullptr;
    }

    std::array<Direct3DSurface9*, caps::MaxSimultaneousRenderTargets> renderTargets;

    Direct3DSurface9* depthStencil = nullptr;
    
  };

}