#pragma once

#include "d3d9_caps.h"
#include "d3d9_constant_set.h"
#include "../dxso/dxso_common.h"
#include "../util/util_matrix.h"

#include <array>
#include <bitset>
#include <optional>

namespace dxvk {

  static constexpr uint32_t RenderStateCount  = 256;
  static constexpr uint32_t SamplerStateCount = D3DSAMP_DMAPOFFSET + 1;
  static constexpr uint32_t SamplerCount      = 21;
  static constexpr uint32_t TextureStageStateCount = D3DTSS_CONSTANT + 1;

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
    std::array<float, 3> fogColor = { };
    float fogScale   = 0.0f;
    float fogEnd     = 1.0f;
    float fogDensity = 1.0f;

    float alphaRef   = 0.0f;

    float pointSize    = 1.0f;
    float pointSizeMin = 1.0f;
    float pointSizeMax = 64.0f;
    float pointScaleA  = 1.0f;
    float pointScaleB  = 0.0f;
    float pointScaleC  = 0.0f;
  };

  enum class D3D9RenderStateItem {
    FogColor   = 0,
    FogScale   = 1,
    FogEnd,
    FogDensity,
    AlphaRef,

    PointSize,
    PointSizeMin,
    PointSizeMax,
    PointScaleA,
    PointScaleB,
    PointScaleC,

    Count
  };


  // This is needed in fixed function for POSITION_T support.
  // These are constants we need to * and add to move
  // Window Coords -> Real Coords w/ respect to the viewport.
  struct D3D9ViewportInfo {
    Vector4 inverseOffset;
    Vector4 inverseExtent;
  };

  struct D3D9Light {
    D3D9Light(const D3DLIGHT9& light, Matrix4 viewMtx) {
      Diffuse  = Vector4(light.Diffuse.r,  light.Diffuse.g,  light.Diffuse.b,  light.Diffuse.a);
      Specular = Vector4(light.Specular.r, light.Specular.g, light.Specular.b, light.Specular.a);
      Ambient  = Vector4(light.Ambient.r,  light.Ambient.g,  light.Ambient.b,  light.Ambient.a);

      Position  = viewMtx * Vector4(light.Position.x,  light.Position.y,  light.Position.z,  1.0f);
      Direction = Vector4(light.Direction.x, light.Direction.y, light.Direction.z, 0.0f);
      Direction = normalize(viewMtx * Direction);

      Type         = light.Type;
      Range        = light.Range;
      Falloff      = light.Falloff;
      Attenuation0 = light.Attenuation0;
      Attenuation1 = light.Attenuation1;
      Attenuation2 = light.Attenuation2;
      Theta        = cosf(light.Theta / 2.0f);
      Phi          = cosf(light.Phi / 2.0f);
    }

    Vector4 Diffuse;
    Vector4 Specular;
    Vector4 Ambient;

    Vector4 Position;
    Vector4 Direction;

    D3DLIGHTTYPE Type;
    float Range;
    float Falloff;
    float Attenuation0;
    float Attenuation1;
    float Attenuation2;
    float Theta;
    float Phi;
  };


  struct D3D9FixedFunctionVS {
    Matrix4 WorldView;
    Matrix4 NormalMatrix;
    Matrix4 Projection;

    std::array<Matrix4, 8> TexcoordMatrices;

    D3D9ViewportInfo ViewportInfo;

    Vector4 GlobalAmbient;
    std::array<D3D9Light, caps::MaxEnabledLights> Lights;
    D3DMATERIAL9 Material;
  };


  struct D3D9FixedFunctionPS {
    Vector4 textureFactor;
  };

  struct D3D9SharedPS {
    struct Stage {
      float BumpEnvMat[2][2];
      float BumpEnvLScale;
      float BumpEnvLOffset;
    } Stages[8];
  };
  
  struct D3D9VBO {
    D3D9VertexBuffer* vertexBuffer = nullptr;
    UINT              offset = 0;
    UINT              stride = 0;
  };

  constexpr D3DLIGHT9 DefaultLight = {
    D3DLIGHT_DIRECTIONAL,     // Type
    {1.0f, 1.0f, 1.0f, 1.0f}, // Diffuse
    {0.0f, 0.0f, 0.0f, 0.0f}, // Specular
    {0.0f, 0.0f, 0.0f, 0.0f}, // Ambient
    {0.0f, 0.0f, 0.0f},       // Position
    {0.0f, 0.0f, 0.0f},       // Direction
    0.0f,                     // Range
    0.0f,                     // Falloff
    0.0f, 0.0f, 0.0f,         // Attenuations [constant, linear, quadratic]
    0.0f,                     // Theta
    0.0f                      // Phi
  };

  struct D3D9CapturableState {
    D3D9CapturableState() {
      for (uint32_t i = 0; i < textures.size(); i++)
        textures[i] = nullptr;

      for (uint32_t i = 0; i < clipPlanes.size(); i++)
        clipPlanes[i] = D3D9ClipPlane();

      for (uint32_t i = 0; i < streamFreq.size(); i++)
        streamFreq[i] = 1;

      for (uint32_t i = 0; i < enabledLightIndices.size(); i++)
        enabledLightIndices[i] = UINT32_MAX;
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

    std::array<
      std::array<DWORD, TextureStageStateCount>,
      caps::TextureStageCount>                       textureStages;

    D3D9ShaderConstantsVS                            vsConsts;
    D3D9ShaderConstantsPS                            psConsts;

    std::array<UINT, caps::MaxStreams>               streamFreq;

    std::array<Matrix4, caps::MaxTransforms>         transforms;

    D3DMATERIAL9                                     material = D3DMATERIAL9();

    std::vector<std::optional<D3DLIGHT9>>            lights;
    std::array<DWORD, caps::MaxEnabledLights>        enabledLightIndices;

    bool IsLightEnabled(DWORD Index) {
      const auto& indices = enabledLightIndices;
      return std::find(indices.begin(), indices.end(), Index) != indices.end();
    }
  };

  template <
    DxsoProgramType  ProgramType,
    D3D9ConstantType ConstantType,
    typename         T>
  HRESULT UpdateStateConstants(
          D3D9CapturableState* pState,
          UINT                 StartRegister,
    const T*                   pConstantData,
          UINT                 Count,
          bool                 FloatEmu) {
    auto UpdateHelper = [&] (auto& set) {
      if constexpr (ConstantType == D3D9ConstantType::Float) {
        auto begin = reinterpret_cast<const Vector4*>(pConstantData);
        auto end   = begin + Count;

        if (!FloatEmu)
          std::copy(begin, end, set.fConsts.begin() + StartRegister);
        else
          std::transform(begin, end, set.fConsts.begin() + StartRegister, replaceNaN);
      }
      else if constexpr (ConstantType == D3D9ConstantType::Int) {
        auto begin = reinterpret_cast<const Vector4i*>(pConstantData);
        auto end   = begin + Count;

        std::copy(begin, end, set.iConsts.begin() + StartRegister);
      }
      else {
        for (uint32_t i = 0; i < Count; i++) {
          const uint32_t constantIdx = StartRegister + i;
          const uint32_t arrayIdx    = constantIdx / 32;
          const uint32_t bitIdx      = constantIdx % 32;

          const uint32_t bit = 1u << bitIdx;

          set.bConsts[arrayIdx] &= ~bit;
          if (pConstantData[i])
            set.bConsts[arrayIdx] |= bit;
        }
      }

      return D3D_OK;
    };

    return ProgramType == DxsoProgramTypes::VertexShader
      ? UpdateHelper(pState->vsConsts)
      : UpdateHelper(pState->psConsts);
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
    Transforms,
    TextureStages,
    Material
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
    std::bitset<caps::TextureStageCount>                textureStages;
    std::array<
      std::bitset<D3DTSS_CONSTANT>,
      caps::TextureStageCount>                          textureStageStates;

    struct {
      std::bitset<caps::MaxFloatConstantsSoftware>      fConsts;
      std::bitset<caps::MaxOtherConstantsSoftware>      iConsts;
      std::bitset<caps::MaxOtherConstantsSoftware>      bConsts;
    } vsConsts;

    struct {
      std::bitset<caps::MaxFloatConstantsPS>            fConsts;
      std::bitset<caps::MaxOtherConstants>              iConsts;
      std::bitset<caps::MaxOtherConstants>              bConsts;
    } psConsts;
  };

  struct Direct3DState9 : public D3D9CapturableState {
    Direct3DState9() {
      for (uint32_t i = 0; i < renderTargets.size(); i++)
        renderTargets[i] = nullptr;
    }

    std::array<D3D9Surface*, caps::MaxSimultaneousRenderTargets> renderTargets;

    D3D9Surface* depthStencil = nullptr;
    
  };


  struct D3D9InputAssemblyState {
    D3DPRIMITIVETYPE primitiveType = D3DPRIMITIVETYPE(0);
    uint32_t streamsInstanced = 0;
    uint32_t streamsUsed      = 0;
  };

}