#pragma once

#include "d3d9_caps.h"

#include <array>

namespace dxvk {

  class Direct3DSurface9;
  class D3D9VertexShader;
  class D3D9PixelShader;
  class Direct3DVertexDeclaration9;
  class Direct3DVertexBuffer9;

  struct D3D9VBO {
    D3D9VBO() {
      vertexBuffer = nullptr;
    }

    Direct3DVertexBuffer9* vertexBuffer;
    UINT offset;
    UINT stride;
  };

  // We make an assumption later based on the packing of this struct for copying.
  #pragma pack(push, 1)
  struct D3D9ShaderConstants {
    using FloatVector = std::array<float, 4>;
    using IntVector   = std::array<int, 4>;

    std::array<FloatVector, caps::MaxFloatConstantsSoftware> fConsts;
    std::array<IntVector,   caps::MaxOtherConstantsSoftware> iConsts;
    std::array<int,         caps::MaxOtherConstantsSoftware> bConsts;
  };
  #pragma pack(pop)

  struct Direct3DState9 {
    Direct3DState9() {
      for (uint32_t i = 0; i < renderTargets.size(); i++)
        renderTargets[i] = nullptr;

      depthStencil = nullptr;
	  vertexShader = nullptr;
	  pixelShader = nullptr;
      vertexDecl = nullptr;
    }

    std::array<Direct3DSurface9*, caps::MaxSimultaneousRenderTargets> renderTargets;
    std::array<D3D9VBO, caps::MaxStreams> vertexBuffers;

    Direct3DSurface9* depthStencil;

    D3D9VertexShader* vertexShader;
    D3D9PixelShader* pixelShader;

    Direct3DVertexDeclaration9* vertexDecl;

    D3DVIEWPORT9 viewport;
    RECT scissorRect;

    std::array<DWORD, D3DRS_BLENDOPALPHA + 1> renderStates;
    std::array<std::array<DWORD, D3DSAMP_DMAPOFFSET + 1>, 21> samplerStates;
    std::array<IDirect3DBaseTexture9*, 21> textures;

    D3D9ShaderConstants vsConsts;
    D3D9ShaderConstants psConsts;
    
  };

}