#pragma once

#include "d3d9_caps.h"
#include "d3d9_constant_set.h"
#include "d3d9_surface.h"
#include "d3d9_shader.h"
#include "d3d9_vertex_declaration.h"
#include "d3d9_buffer.h"

#include "../util/util_matrix.h"

#include <array>
#include <bitset>
#include <optional>

namespace dxvk {

  static constexpr uint32_t RenderStateCount  = 256;
  static constexpr uint32_t SamplerStateCount = D3DSAMP_DMAPOFFSET + 1;
  static constexpr uint32_t SamplerCount      = caps::MaxTexturesPS + caps::MaxTexturesVS + 1;
  static constexpr uint32_t TextureStageStateCount = DXVK_TSS_COUNT;
  static constexpr uint32_t PaletteEntryCount = 256;

  /// Helper function to update bit-packed spec constants
  /// and check if the value has actually changed.
  template<typename T, typename U>
  bool updateSpecData(T& data, U value, uint32_t bitIndex, uint32_t bitCount) {
    T mask = ((2u << (bitCount - 1u)) - 1u) << bitIndex;
    T bits = (T(value) << bitIndex) & mask;

    if ((data & mask) == bits)
      return false;

    data &= ~mask;
    data |= bits;
    return true;
  }

  template<typename T, typename U>
  bool updateSpecData(T& data, U value) {
    if (data == T(value))
      return false;

    data = T(value);
    return true;
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

  /// Shared push data
  struct D3D9SharedPushData {
    static constexpr VkShaderStageFlags Stages = VK_SHADER_STAGE_ALL_GRAPHICS;
    static constexpr uint32_t           Offset = 0u;

    uint8_t fogColor[3] = {};
    uint8_t alphaRef = 0u;

    float fogDistanceScale = 0.0f;
    float fogDistanceEnd = 0.0f;
    float fogDensity = 0.0f;
  };

  /// Vertex shader push data
  struct D3D9VsPushData {
    // We don't have enough VS-only push data space available to fit all
    // possible samplers, constant buffers and data in there, so put VS
    // data into the shared block.
    static constexpr VkShaderStageFlags Stages = VK_SHADER_STAGE_ALL_GRAPHICS;
    static constexpr uint32_t           Offset = sizeof(D3D9SharedPushData);

    // Dynamically indexed float count
    uint16_t floatCount = 0u;
    // Point size, as 13.3 fixed-point
    uint16_t pointSize = 0u;
    uint16_t pointSizeMin = 0u;
    uint16_t pointSizeMax = 0u;
  };

  /// Fixed-function vertex shader push data.
  /// Can theoretically use up to 32 bytes.
  struct D3D9FfvsPushData {
    static constexpr VkShaderStageFlags Stages = VK_SHADER_STAGE_VERTEX_BIT;
    static constexpr uint32_t           Offset = 0u;

    float pointScaleA = 0.0f;
    float pointScaleB = 0.0f;
    float pointScaleC = 0.0f;
  };

  /// Fixed-function pixel shader push data.
  struct D3D9FfpsPushData {
    static constexpr VkShaderStageFlags Stages = VK_SHADER_STAGE_FRAGMENT_BIT;
    static constexpr uint32_t           Offset = 0u;

    uint32_t textureFactor = 0u;
  };

  /// Complete push data state. Note that the data layout inside
  /// this struct is different from what it is in shaders.
  struct D3D9PushData {
    D3D9SharedPushData shared;
    D3D9VsPushData vs;
    D3D9FfvsPushData ffvs;
    D3D9FfpsPushData ffps;
  };

  /// Shared specialization constant data
  struct D3D9SharedSpecData {
    uint8_t fogMode     = 0u;
    uint8_t pointMode   = 0u;
    uint8_t drefScale   = 0u;
    uint8_t clipPlanes  = 0u;

    bool setFogEnabled(bool enable) {
      return updateSpecData(fogMode, enable, 0u, 1u);
    }

    bool setVertexFogMode(uint32_t mode) {
      return updateSpecData(fogMode, mode, 1u, 2u);
    }

    bool setPixelFogMode(uint32_t mode) {
      return updateSpecData(fogMode, mode, 3u, 2u);
    }

    bool setPointScaleEnable(bool enable) {
      return updateSpecData(pointMode, enable, 0u, 1u);
    }

    bool setPointSpriteEnable(bool enable) {
      return updateSpecData(pointMode, enable, 1u, 1u);
    }

    bool setDrefScale(uint32_t shift) {
      return updateSpecData(drefScale, shift);
    }

    bool setClipPlaneCount(uint32_t count) {
      return updateSpecData(clipPlanes, count);
    }
  };

  /// Vertex shader spec constants
  struct D3D9VsSpecData {
    uint8_t samplerTypes = 0u;
    uint8_t samplerModes = 0u;
    uint16_t boolConstants = 0u;

    bool setSamplerType(uint32_t idx, uint32_t type) {
      return updateSpecData(samplerTypes, type, idx * 2u, 2u);
    }

    bool setSamplerMode(uint32_t idx, uint32_t mode) {
      return updateSpecData(samplerModes, mode, idx * 2u, 2u);
    }

    uint16_t setBoolConstants(uint32_t bits) {
      return updateSpecData(boolConstants, bits);
    }
  };

  /// Pixel shader spec constants
  struct D3D9PsSpecData {
    uint32_t samplerTypes = 0u;
    uint32_t samplerModes = 0u;
    uint8_t samplerProjection = 0u;
    uint8_t alphaTest = 0u;
    uint16_t boolConstants = 0u;

    bool setSamplerType(uint32_t idx, uint32_t type) {
      return updateSpecData(samplerTypes, type, idx * 2u, 2u);
    }

    bool setSamplerMode(uint32_t idx, uint32_t mode) {
      return updateSpecData(samplerModes, mode, idx * 2u, 2u);
    }

    bool setSamplerProjection(uint32_t idx, bool enable) {
      return updateSpecData(samplerProjection, enable, idx, 1u);
    }

    bool setAlphaCompareOp(uint32_t op) {
      return updateSpecData(alphaTest, op, 0u, 4u);
    }

    bool setAlphaPrecision(uint32_t bits) {
      return updateSpecData(alphaTest, bits, 4u, 4u);
    }

    uint16_t setBoolConstants(uint32_t bits) {
      return updateSpecData(boolConstants, bits);
    }
  };

  /// Fixed-function spec constants
  struct D3D9FfpsSpecData {
    uint16_t stageOps[8u];
    uint16_t colorArgs[8u];
    uint16_t alphaArgs[8u];

    bool resetStage(uint32_t index) {
      bool dirty = false;
      dirty |= updateSpecData(stageOps[index], 0u, 0u, 12u);
      dirty |= updateSpecData(colorArgs[index], 0u);
      dirty |= updateSpecData(alphaArgs[index], 0u);
      return dirty;
    }

    bool setHighetActiveStageIndex(uint32_t index) {
      return updateSpecData(stageOps[0u], index, 12u, 3u);
    }

    bool setSpecularEnable(bool enable) {
      return updateSpecData(stageOps[0u], enable, 15u, 1u);
    }

    bool setColorOp(uint32_t stage, D3DTEXTUREOP op) {
      return updateSpecData(stageOps[stage], op, 0u, 5u);
    }

    bool setAlphaOp(uint32_t stage, D3DTEXTUREOP op) {
      return updateSpecData(stageOps[stage], op, 5u, 5u);
    }

    bool setStoreTemp(uint32_t stage, bool enable) {
      return updateSpecData(stageOps[stage], enable, 10u, 1u);
    }

    bool setColorArg(uint32_t stage, uint32_t index, uint32_t arg) {
      return setArg(colorArgs[stage], index, arg);
    }

    bool setAlphaArg(uint32_t stage, uint32_t index, uint32_t arg) {
      return setArg(alphaArgs[stage], index, arg);
    }

    static bool setArg(uint16_t& bitfield, uint32_t index, uint32_t arg) {
      // Only 7 selectors are defined, so squeeze them into 3
      // bits and pack the flags into the upper two bits.
      uint32_t selector = arg &  D3DTA_SELECTMASK;
      uint32_t modifier = arg & ~D3DTA_SELECTMASK;

      bool dirty = false;
      dirty |= updateSpecData(bitfield, selector, index * 5u + 0u, 3u);
      dirty |= updateSpecData(bitfield, modifier, index * 5u + 3u, 2u);
      return dirty;
    }
  };

  /// All specialization constants combined
  struct D3D9SpecData {
    D3D9SharedSpecData shared;
    D3D9VsSpecData vs;
    D3D9PsSpecData ps;
    D3D9FfpsSpecData ffps;
  };

  // This is needed in fixed function for POSITION_T support.
  // These are constants we need to * and add to move
  // Window Coords -> Real Coords w/ respect to the viewport.
  struct D3D9ViewportInfo {
    Vector4 inverseOffset;
    Vector4 inverseExtent;
  };

  struct D3D9Light {
    D3D9Light(const D3DLIGHT9& light, Matrix4 viewMtx)
      : Diffuse      ( Vector4(light.Diffuse.r,  light.Diffuse.g,  light.Diffuse.b,  light.Diffuse.a) )
      , Specular     ( Vector4(light.Specular.r, light.Specular.g, light.Specular.b, light.Specular.a) )
      , Ambient      ( Vector4(light.Ambient.r,  light.Ambient.g,  light.Ambient.b,  light.Ambient.a) )
      , Position     ( viewMtx * Vector4(light.Position.x,  light.Position.y,  light.Position.z,  1.0f) )
      , Direction    ( normalize(viewMtx * Vector4(light.Direction.x, light.Direction.y, light.Direction.z, 0.0f)) )
      , Type         ( light.Type )
      , Range        ( light.Range )
      , Falloff      ( light.Falloff )
      , Attenuation0 ( light.Attenuation0 )
      , Attenuation1 ( light.Attenuation1 )
      , Attenuation2 ( light.Attenuation2 )
      , Theta        ( cosf(light.Theta / 2.0f) )
      , Phi          ( cosf(light.Phi / 2.0f) ) { }

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

  struct D3D9FFShaderKeyVSData {
    union {
      struct {
        uint32_t TexcoordIndices : 24;

        uint32_t VertexHasPositionT : 1;

        uint32_t VertexHasColor0 : 1; // Diffuse
        uint32_t VertexHasColor1 : 1; // Specular

        uint32_t VertexHasPointSize : 1;

        uint32_t UseLighting : 1;

        uint32_t NormalizeNormals : 1;
        uint32_t LocalViewer : 1;
        uint32_t RangeFog : 1;

        // End of uint32_t

        uint32_t TexcoordFlags : 24;

        uint32_t DiffuseSource : 2;
        uint32_t AmbientSource : 2;
        uint32_t SpecularSource : 2;
        uint32_t EmissiveSource : 2;

        // Next uint32_t

        uint32_t TransformFlags : 24;

        uint32_t LightCount : 4;

        // End of uint32_t

        uint32_t VertexTexcoordDeclMask : 24;
        uint32_t VertexHasFog : 1;

        uint32_t VertexBlendMode    : 2;
        uint32_t VertexBlendIndexed : 1;
        uint32_t VertexBlendCount   : 2;

        uint32_t VertexClipping     : 1;

        // End of uint32_t
      } Contents;

      uint32_t Primitive[5];
    };
  };

  struct D3D9FFShaderKeyVS {
    D3D9FFShaderKeyVS() {
      // memcmp safety
      std::memset(&Data, 0, sizeof(Data));
    }

    D3D9FFShaderKeyVSData Data;
  };

  struct D3D9FixedFunctionVS {
    Matrix4 WorldView;
    Matrix4 NormalMatrix;
    Matrix4 InverseView;
    Matrix4 Projection;

    std::array<Matrix4, 8> TexcoordMatrices;

    D3D9ViewportInfo ViewportInfo;

    Vector4 GlobalAmbient;
    std::array<D3D9Light, caps::MaxEnabledLights> Lights;
    D3DMATERIAL9 Material;
    float TweenFactor;

    D3D9FFShaderKeyVSData Key;
  };

  static constexpr uint32_t D3D9MaxVertexBlendTransformsHw = 8u;
  static constexpr uint32_t D3D9MaxVertexBlendTransformsSw = 256u;

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

    std::unordered_map<
       UINT,
       std::array<PALETTEENTRY, PaletteEntryCount>>     texturePalettes;
    UINT                                                texturePaletteNumber = 0u;

    ItemType<D3D9ShaderConstantsVSSoftware>             vsConsts;
    ItemType<D3D9ShaderConstantsPS>                     psConsts;

    std::array<UINT, caps::MaxStreams>                  streamFreq = {};

    ItemType<std::array<Matrix4, caps::MaxTransforms>>  transforms = {};

    ItemType<D3DMATERIAL9>                              material = {};

    std::vector<std::optional<D3DLIGHT9>>               lights;
    std::array<DWORD, caps::MaxEnabledLights>           enabledLightIndices;

    float                                               nPatchSegments = 0.0f;

    bool IsLightEnabled(DWORD Index) const {
      const auto& enabledIndices = enabledLightIndices;
      return std::find(enabledIndices.begin(), enabledIndices.end(), Index) != enabledIndices.end();
    }
  };

  using D3D9CapturableState = D3D9State<dynamic_item>;
  using D3D9DeviceState = D3D9State<static_item>;

  template<D3D9ShaderType ShaderType, D3D9ConstantType ConstantType, typename T, typename StateType>
  bool UpdateStateConstants(
          StateType*           pState,
          UINT                 StartRegister,
    const T*                   pConstantData,
          UINT                 Count) {
    if constexpr (ConstantType == D3D9ConstantType::Bool) {
      uint32_t* dstData = ShaderType == D3D9ShaderType::VertexShader
        ? pState->vsConsts->bConsts
        : pState->psConsts->bConsts;

      for (uint32_t i = 0; i < Count; i++) {
        const uint32_t constantIdx = StartRegister + i;
        const uint32_t arrayIdx    = constantIdx / 32;
        const uint32_t bitIdx      = constantIdx % 32;

        const uint32_t bit = 1u << bitIdx;

        dstData[arrayIdx] &= ~bit;

        if (pConstantData[i])
          dstData[arrayIdx] |= bit;
      }

      return true;
    } else {
      static_assert(sizeof(T) == 4u);

      Vector4Base<T>* dstData = nullptr;

      if constexpr (ConstantType == D3D9ConstantType::Float) {
        dstData = ShaderType == D3D9ShaderType::VertexShader
          ? pState->vsConsts->fConsts
          : pState->psConsts->fConsts;
      } else if constexpr (ConstantType == D3D9ConstantType::Int) {
        dstData = ShaderType == D3D9ShaderType::VertexShader
          ? pState->vsConsts->iConsts
          : pState->psConsts->iConsts;
      }

      dstData += StartRegister;

      #if defined(DXVK_ARCH_X86) && (defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER))
      auto* dstPtr = reinterpret_cast<      __m128i*>(dstData);
      auto* srcPtr = reinterpret_cast<const __m128i*>(pConstantData);

      // In the first loop, find the first contant that has changed, if any, and
      // only copy that. Basically a glorified memcmp. The idea is to give feedback
      // to the caller on whether any constant values have changed.
      bool dirty = false;

      uint32_t index = 0u;

      while (index < Count) {
        __m128i srcData = _mm_loadu_si128(srcPtr + index);
        __m128i dstData = _mm_loadu_si128(dstPtr + index);

        __m128i eqMask = _mm_cmpeq_epi32(srcData, dstData);

        dirty = _mm_movemask_epi8(eqMask) != 0xffff;
        index += 1u;

        if (dirty) {
          _mm_storeu_si128(dstPtr + index - 1u, srcData);
          break;
        }
      }

      if (unlikely(!dirty))
        return false;

      // Once we know constants have changed, just copy the rest.
      while (index + 2u <= Count) {
        __m128i src0 = _mm_loadu_si128(srcPtr + index + 0u);
        __m128i src1 = _mm_loadu_si128(srcPtr + index + 1u);

        _mm_storeu_si128(dstPtr + index + 0u, src0);
        _mm_storeu_si128(dstPtr + index + 1u, src1);

        index += 2u;
      }

      if (index < Count) {
        __m128i srcData = _mm_loadu_si128(srcPtr + index);
        _mm_storeu_si128(dstPtr + index, srcData);
      }

      return true;
      // If any mask bit is 0, a constant has changed
      #else
      size_t dataSize = Count * sizeof(*dstData);

      if (!std::memcmp(&dstData->data, pConstantData, dataSize))
        return false;

      std::memcpy(&dstData->data, pConstantData, dataSize);
      return true;
      #endif
    }
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
