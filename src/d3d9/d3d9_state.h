#pragma once

#include "d3d9_caps.h"
#include "d3d9_constant_set.h"
#include "../dxso/dxso_common.h"
#include "../util/util_matrix.h"

#include "d3d9_surface.h"
#include "d3d9_shader.h"
#include "d3d9_vertex_declaration.h"
#include "d3d9_buffer.h"

#include "d3d9_shader_types.h"

#include <array>
#include <bitset>
#include <optional>

namespace dxvk {

  static constexpr uint32_t RenderStateCount  = 256;
  static constexpr uint32_t SamplerStateCount = D3DSAMP_DMAPOFFSET + 1;
  static constexpr uint32_t SamplerCount      = caps::MaxTexturesPS + caps::MaxTexturesVS + 1;
  static constexpr uint32_t TextureStageStateCount = DXVK_TSS_COUNT;

  namespace hacks::PointSize {
    static constexpr DWORD AlphaToCoverageDisabled = MAKEFOURCC('A', '2', 'M', '0');
    static constexpr DWORD AlphaToCoverageEnabled  = MAKEFOURCC('A', '2', 'M', '1');
  }
  
  struct D3D9ClipPlane {
    float coeff[4] = {};

    bool operator == (const D3D9ClipPlane& other) {
      return std::memcmp(this, &other, sizeof(D3D9ClipPlane)) == 0;
    }

    bool operator != (const D3D9ClipPlane& other) {
      return !this->operator == (other);
    }
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


  struct D3D9FixedFunctionPS {
    Vector4 textureFactor;
  };

  enum D3D9SharedPSStages {
    D3D9SharedPSStages_Constant,
    D3D9SharedPSStages_BumpEnvMat0,
    D3D9SharedPSStages_BumpEnvMat1,
    D3D9SharedPSStages_BumpEnvLScale,
    D3D9SharedPSStages_BumpEnvLOffset,
    D3D9SharedPSStages_Count,
  };

  struct D3D9SharedPS {
    struct Stage {
      float Constant[4];
      float BumpEnvMat[2][2];
      float BumpEnvLScale;
      float BumpEnvLOffset;
      float Padding[2];
    } Stages[8];
  };
  
  struct D3D9VBO {
    Com<D3D9VertexBuffer, false> vertexBuffer;

    UINT              offset = 0;
    UINT              stride = 0;
  };

  constexpr D3DLIGHT9 DefaultLight = {
    D3DLIGHT_DIRECTIONAL,     // Type
    {1.0f, 1.0f, 1.0f, 0.0f}, // Diffuse
    {0.0f, 0.0f, 0.0f, 0.0f}, // Specular
    {0.0f, 0.0f, 0.0f, 0.0f}, // Ambient
    {0.0f, 0.0f, 0.0f},       // Position
    {0.0f, 0.0f, 1.0f},       // Direction
    0.0f,                     // Range
    0.0f,                     // Falloff
    0.0f, 0.0f, 0.0f,         // Attenuations [constant, linear, quadratic]
    0.0f,                     // Theta
    0.0f                      // Phi
  };

  template <typename T>
  class dynamic_item {
  public:
          auto& operator [] (size_t idx)       { ensure(); return (*m_data)[idx]; }
    const auto& operator [] (size_t idx) const { ensure(); return (*m_data)[idx]; }

    T& operator=(const T& x) { ensure(); *m_data = x; return *m_data; }

    const T* operator -> () const { ensure(); return m_data.get(); }
          T* operator -> ()       { ensure(); return m_data.get(); }

    const T* operator & () const { ensure(); return m_data.get(); }
          T* operator & ()       { ensure(); return m_data.get(); }

    explicit operator bool() const { return m_data != nullptr; }
    operator T() { ensure(); return *m_data; }

    void ensure() const { if (!m_data) m_data = std::make_unique<T>(); }

    T& get() { ensure(); return *m_data; }
  private:
    mutable std::unique_ptr<T> m_data;
  };

  template <typename T>
  class static_item {
  public:
          auto& operator [] (size_t idx)       { return m_data[idx]; }
    const auto& operator [] (size_t idx) const { return m_data[idx]; }

    T& operator=(const T& x) { m_data = x; return m_data; }

    explicit operator bool() const { return true; }
    operator T() { return m_data; }

    const T* operator -> () const { return &m_data; }
          T* operator -> ()       { return &m_data; }

    const T* operator & () const { return &m_data; }
          T* operator & ()       { return &m_data; }

    T& get() { return m_data; }
  private:
    T m_data;
  };

  struct D3D9SamplerInfo {
    D3D9SamplerInfo(const std::array<DWORD, SamplerStateCount>& state)
    : addressU(D3DTEXTUREADDRESS(state[D3DSAMP_ADDRESSU]))
    , addressV(D3DTEXTUREADDRESS(state[D3DSAMP_ADDRESSV]))
    , addressW(D3DTEXTUREADDRESS(state[D3DSAMP_ADDRESSW]))
    , borderColor(D3DCOLOR(state[D3DSAMP_BORDERCOLOR]))
    , magFilter(D3DTEXTUREFILTERTYPE(state[D3DSAMP_MAGFILTER]))
    , minFilter(D3DTEXTUREFILTERTYPE(state[D3DSAMP_MINFILTER]))
    , mipFilter(D3DTEXTUREFILTERTYPE(state[D3DSAMP_MIPFILTER]))
    , mipLodBias(bit::cast<float>(state[D3DSAMP_MIPMAPLODBIAS]))
    , maxMipLevel(state[D3DSAMP_MAXMIPLEVEL])
    , maxAnisotropy(state[D3DSAMP_MAXANISOTROPY]) { }

    D3DTEXTUREADDRESS addressU;
    D3DTEXTUREADDRESS addressV;
    D3DTEXTUREADDRESS addressW;
    D3DCOLOR borderColor;
    D3DTEXTUREFILTERTYPE magFilter;
    D3DTEXTUREFILTERTYPE minFilter;
    D3DTEXTUREFILTERTYPE mipFilter;
    float mipLodBias;
    DWORD maxMipLevel;
    DWORD maxAnisotropy;
  };

  template <template <typename T> typename ItemType>
  struct D3D9State {
    D3D9State();
    ~D3D9State();

    Com<D3D9VertexDecl,  false>                         vertexDecl;
    Com<D3D9IndexBuffer, false>                         indices;

    ItemType<std::array<DWORD, RenderStateCount>>       renderStates = {};

    ItemType<std::array<
      std::array<DWORD, SamplerStateCount>,
      SamplerCount>>                                    samplerStates = {};

    ItemType<std::array<D3D9VBO, caps::MaxStreams>>     vertexBuffers = {};

    ItemType<std::array<
      IDirect3DBaseTexture9*,
      SamplerCount>>                                    textures = {};

    Com<D3D9VertexShader, false>                        vertexShader;
    Com<D3D9PixelShader,  false>                        pixelShader;

    D3DVIEWPORT9                                        viewport = {};
    RECT                                                scissorRect = {};

    D3DCLIPSTATUS9                                      clipStatus = {0, 0xffffffff};

    ItemType<std::array<
      D3D9ClipPlane,
      caps::MaxClipPlanes>>                             clipPlanes = {};

    ItemType<std::array<
      std::array<DWORD, TextureStageStateCount>,
      caps::TextureStageCount>>                         textureStages = {};

    ItemType<D3D9ShaderConstantsVSSoftware>             vsConsts;
    ItemType<D3D9ShaderConstantsPS>                     psConsts;

    std::array<UINT, caps::MaxStreams>                  streamFreq = {};

    ItemType<std::array<Matrix4, caps::MaxTransforms>>  transforms = {};

    ItemType<D3DMATERIAL9>                              material = {};

    std::vector<std::optional<D3DLIGHT9>>               lights;
    std::array<DWORD, caps::MaxEnabledLights>           enabledLightIndices;

    bool IsLightEnabled(DWORD Index) const {
      const auto& enabledIndices = enabledLightIndices;
      return std::find(enabledIndices.begin(), enabledIndices.end(), Index) != enabledIndices.end();
    }
  };

  using D3D9CapturableState = D3D9State<dynamic_item>;
  using D3D9DeviceState = D3D9State<static_item>;

  template <
    DxsoProgramType  ProgramType,
    D3D9ConstantType ConstantType,
    typename         T,
    typename         StateType>
  HRESULT UpdateStateConstants(
          StateType*           pState,
          UINT                 StartRegister,
    const T*                   pConstantData,
          UINT                 Count,
          bool                 FloatEmu) {
    auto UpdateHelper = [&] (auto& set) {
      if constexpr (ConstantType == D3D9ConstantType::Float) {

        if (!FloatEmu) {
          size_t size = Count * sizeof(Vector4);

          std::memcpy(set->fConsts[StartRegister].data, pConstantData, size);
        }
        else {
          for (UINT i = 0; i < Count; i++)
            set->fConsts[StartRegister + i] = replaceNaN(pConstantData + (i * 4));
        }
      }
      else if constexpr (ConstantType == D3D9ConstantType::Int) {
        size_t size = Count * sizeof(Vector4i);

        std::memcpy(set->iConsts[StartRegister].data, pConstantData, size);
      }
      else {
        for (uint32_t i = 0; i < Count; i++) {
          const uint32_t constantIdx = StartRegister + i;
          const uint32_t arrayIdx    = constantIdx / 32;
          const uint32_t bitIdx      = constantIdx % 32;

          const uint32_t bit = 1u << bitIdx;

          set->bConsts[arrayIdx] &= ~bit;
          if (pConstantData[i])
            set->bConsts[arrayIdx] |= bit;
        }
      }

      return D3D_OK;
    };

    return ProgramType == DxsoProgramTypes::VertexShader
      ? UpdateHelper(pState->vsConsts)
      : UpdateHelper(pState->psConsts);
  }

  struct Direct3DState9 : public D3D9DeviceState {

    std::array<Com<D3D9Surface, false>, caps::MaxSimultaneousRenderTargets> renderTargets;
    Com<D3D9Surface, false> depthStencil;

  };


  struct D3D9InputAssemblyState {
    D3DPRIMITIVETYPE primitiveType = D3DPRIMITIVETYPE(0);
    uint32_t streamsInstanced = 0;
    uint32_t streamsUsed      = 0;
  };

}