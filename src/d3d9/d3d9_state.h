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
    static constexpr VkShaderStageFlags Stages = VK_SHADER_STAGE_VERTEX_BIT;
    static constexpr uint32_t           Offset = 0u;

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
    static constexpr uint32_t           Offset = sizeof(D3D9VsPushData);

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

  /// Sampler modes
  enum class D3D9SamplerMode : uint8_t {
    Default   = 0,
    Fetch4    = 1,
    Dref      = 2,
    DrefClamp = 3,
  };

  /// Specialization constant data. Each dword corresponds to one numbered
  /// spec constants inside shaders and in the backend.
  ///
  /// Spec constants are packed in such a way that shaders can use only
  /// those constants that they actually need, with minimal unrelated
  /// state. This helps keep the number of redundant pipeline variants
  /// to a minimum, while requiring little normalization in the front-end.
  struct D3D9SpecData {
    // Spec ID 0: Common parameters used by all pipelines. Alpha compare
    // op is packed into the lower 4 bits of the alpha test byte, alpha
    // precision into the upper 4 bits.
    uint8_t alphaTest = 0u;
    bool enablePointSprite = false;
    uint8_t clipPlaneCount = 0u;
    uint8_t drefScale = 0u;

    // Spec ID 1: Fog. Used in fixed-function pipelines as
    // well as pixel shaders with Shader Model 2 and below.
    bool fogEnable = 0u;
    uint8_t fogModeVertex = 0u;
    uint8_t fogModePixel = 0u;
    bool fogUseZ = 0u;

    // Spec ID 2: Parameters used in fixed-function pipelines. The
    // projection mask is also used in shader model 1 pixel shaders.
    uint8_t samplerProjMask = 0u;
    bool enableGlobalSpecular = false;
    bool enablePointScale = false;
    bool psIsShaderModel3 = false;

    // Spec ID 3: Vertex shader sampler info and bool
    // constants. Used in programmable shaders only.
    uint8_t vsSamplerTypes = 0u;
    uint8_t vsSamplerModes = 0u;
    uint16_t vsBoolConstants = 0u;

    // Spec ID 4: Pixel shader sampler types, two bits per sampler.
    // Fixed-function will only use the lower 16 bits.
    uint32_t psSamplerTypes = 0u;

    // Spec ID 5: Pixel shader sampler modes, two bits per sampler.
    // Fixed-function will only use the lower 16 bits.
    uint32_t psSamplerModes = 0u;

    // Spec ID 6: Pixel shader boolean constants.
    // Programmable shaders only. 16 bits used.
    uint16_t psBoolConstants = 0u;
    uint16_t spec6Pad = 0u;

    // Spec ID 7: Reserved.
    uint32_t spec7Pad = 0u;

    // Spec ID 8..11: Texture stage ops for fixed function.
    // Each stage is packed as follows:
    // - Bits 0..4: Color op
    // - Bits 5..7: Unused
    // - Bits 8..12: Alpha op
    // - Bits 13..14: Unused;
    // - Bit 15: Whether to store result in temp
    std::array<uint16_t, 8u> stageOps = {};

    // Spec ID 12..19: Arguments for each texture stage:
    // - Bits 0..4: Color arg 0, with flags packed into bits 3..4
    // - Bits 5..9: Color arg 1
    // - Bits 10..14: Color arg 2
    // - Bit 15: Unused
    // - Bits 16..20: Alpha arg 0
    // - Bits 21..25: Alpha arg 1
    // - Bits 26..30: Alpha arg 2
    // - Bit 31: Unused
    std::array<uint32_t, 8u> stageArgs = {};

    bool setAlphaCompareOp(VkCompareOp op) {
      return set(alphaTest, op, 0u, 4u);
    }

    bool setAlphaPrecision(uint32_t precision) {
      return set(alphaTest, precision, 4u, 4u);
    }

    bool setDrefScale(uint32_t shift) {
      return set(drefScale, shift);
    }

    bool setClipPlaneCount(uint32_t count) {
      return set(clipPlaneCount, count);
    }

    bool setPointScale(bool enable) {
      return set(enablePointScale, enable);
    }

    bool setPointSprite(bool enable) {
      return set(enablePointSprite, enable);
    }

    bool setGlobalSpecular(bool enable) {
      return set(enableGlobalSpecular, enable);
    }

    bool setFogMode(bool enable, bool zFog, D3DFOGMODE vertexFog, D3DFOGMODE pixelFog) {
      bool dirty = false;
      dirty |= set(fogEnable,     enable);
      dirty |= set(fogModeVertex, enable && !pixelFog ? vertexFog : D3DFOG_NONE);
      dirty |= set(fogModePixel,  enable              ? pixelFog  : D3DFOG_NONE);
      dirty |= set(fogUseZ,       enable && zFog);
      return dirty;
    }

    bool setSamplerProjectionMask(uint32_t mask) {
      return set(samplerProjMask, mask);
    }

    bool setPsSamplers(uint64_t textureTypes, uint32_t nullMask,
        uint32_t fetch4Mask, uint32_t drefMask, uint32_t drefClampMask) {
      return updateSamplers(psSamplerTypes, psSamplerModes, textureTypes,
        nullMask, fetch4Mask, drefMask, drefClampMask);
    }

    bool setVsSamplers(uint32_t nullMask, uint32_t drefMask, uint32_t drefClampMask) {
      return updateSamplers(vsSamplerTypes, vsSamplerModes, 0u,
        nullMask      >> FirstVSSamplerSlot, 0u,
        drefMask      >> FirstVSSamplerSlot,
        drefClampMask >> FirstVSSamplerSlot);
    }

    bool setPsShaderModel(uint32_t majorVersion) {
      return set(psIsShaderModel3, majorVersion >= 3u);
    }

    bool setVsBoolConstants(uint32_t bits) {
      return set(vsBoolConstants, bits);
    }

    bool setPsBoolConstants(uint32_t bits) {
      return set(psBoolConstants, bits);
    }

    bool setTextureStage(uint32_t stage, D3DTEXTUREOP colorOp, D3DTEXTUREOP alphaOp, DWORD resultArg,
        DWORD colorArg0, DWORD colorArg1, DWORD colorArg2,
        DWORD alphaArg0, DWORD alphaArg1, DWORD alphaArg2) {
      uint16_t ops = 0u;
      ops |= getTextureOp(colorOp) << 0u;
      ops |= getTextureOp(alphaOp) << 8u;

      if ((resultArg & D3DTA_SELECTMASK) == D3DTA_TEMP)
        ops |= 1u << 15u;

      uint32_t args = 0u;

      if (colorOp != D3DTOP_DISABLE) {
        args |= getTextureArg(colorArg0) <<  0u;
        args |= getTextureArg(colorArg1) <<  5u;
        args |= getTextureArg(colorArg2) << 10u;
      }

      if (alphaOp != D3DTOP_DISABLE) {
        args |= getTextureArg(alphaArg0) << 16u;
        args |= getTextureArg(alphaArg1) << 21u;
        args |= getTextureArg(alphaArg2) << 26u;
      }

      bool dirty = false;
      dirty |= set(stageOps[stage], ops);
      dirty |= set(stageArgs[stage], args);
      return dirty;
    }

    bool disableTextureStage(uint32_t stage) {
      uint32_t ops = D3DTOP_DISABLE | (D3DTOP_DISABLE << 5u);

      bool dirty = false;
      dirty |= set(stageOps[stage], ops);
      dirty |= set(stageArgs[stage], 0u);
      return dirty;
    }

    template<typename T>
    static bool updateSamplers(T& types, T& modes, uint32_t textureTypes, uint32_t nullMask,
        uint32_t fetch4Mask, uint32_t drefMask, uint32_t drefClampMask) {
      // Forward null texture mask as texture type '3'
      uint32_t nullTypes = bit::interleave(nullMask, nullMask);
      uint32_t newTypes = nullTypes | textureTypes;

      // As for the modes, treat it as a two-bit value:
      // - 0: Default mode
      // - 1: Fetch4 (color only)
      // - 2: Depth-compare
      // - 3: Depth-compare with dref clamp
      uint32_t newFetch4 = fetch4Mask & ~drefMask;
      uint32_t newClamp = drefClampMask & drefMask;

      uint32_t newModes = bit::interleave(newFetch4 | newClamp, drefMask) & ~nullTypes;

      bool dirty = false;
      dirty |= set(types, newTypes);
      dirty |= set(modes, newModes);
      return dirty;
    }

    static uint16_t getTextureOp(D3DTEXTUREOP op) {
      return uint16_t(op) & 0x1fu;
    }

    static uint32_t getTextureArg(DWORD arg) {
      // Shift flags one  to the right to pack everything
      // into 5 bits, only 7 selectors are defined.
      uint32_t select = arg & D3DTA_SELECTMASK;
      uint32_t flags = arg & ~D3DTA_SELECTMASK;
      return uint32_t(select | (flags >> 1u)) & 0x1fu;
    }

    template<typename T, typename U>
    static bool set(T& dst, U data) {
      if (dst == T(data))
        return false;

      dst = T(data);
      return true;
    }

    template<typename T, typename U>
    static bool set(T& dst, U data, uint32_t bitIndex, uint32_t bitCount) {
      T mask = ((T(1u) << bitCount) - 1u) << bitIndex;
      T bits = (T(data) << bitIndex) & mask;

      if ((dst & mask) == bits)
        return false;

      dst &= ~mask;
      dst |= bits;
      return true;
    }
  };

  static_assert(sizeof(D3D9SpecData) <= sizeof(uint32_t) * MaxNumSpecConstants);

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

  struct D3D9LightState {
    bool isValid = false;
    bool isEnabled = false;
    D3DLIGHT9 light = DefaultLight;
  };

  struct D3D9FixedFunctionVS {
    Matrix4 WorldView;
    Matrix4 NormalMatrix;
    Matrix4 InverseView;
    Matrix4 Projection;
    Matrix4 WorldViewProj;

    std::array<Matrix4, 8> TexcoordMatrices;

    D3D9ViewportInfo ViewportInfo;

    std::array<D3D9Light, caps::MaxEnabledLights> Lights;
    D3DMATERIAL9 Material;
    uint32_t GlobalAmbient;
    float TweenFactor;

    // Following part uses uint8 and bool so it's gonna be represented as uint32 in the shader and manually unpacked:
    std::array<uint8_t, caps::MaxTextureBlendStages> TexcoordIndices;
    std::array<uint8_t, caps::MaxTextureBlendStages> TexcoordFlags;
    std::array<uint8_t, caps::MaxTextureBlendStages> TexcoordTransformFlags;

    // How many vector components does each texcoord have
    uint32_t VertexTexcoordDeclMask;

    // Vertex Decl
    bool VertexHasPositionT;
    bool VertexHasColor0; // Diffuse
    bool VertexHasColor1; // Specular
    bool VertexHasPointSize;
    bool VertexHasFog;

    // Blending
    uint8_t VertexBlendMode;
    bool VertexBlendIndexed;
    uint8_t VertexBlendCount;

    // Misc
    bool VertexClipping;
    bool NormalizeNormals;
    bool LocalViewer;
    bool RangeFog;

    // Lighting
    bool UseLighting;
    uint8_t LightCount;
    uint8_t DiffuseSource;
    uint8_t AmbientSource;
    uint8_t SpecularSource;
    uint8_t EmissiveSource;
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
      uint32_t Constant;
      uint32_t Padding;
      float BumpEnvMat[2][2];
      float BumpEnvLScale;
      float BumpEnvLOffset;
    } Stages[8];
  };
  
  struct D3D9VBO {
    Com<D3D9VertexBuffer, false> vertexBuffer;

    UINT              offset = 0;
    UINT              length = 0;
    UINT              stride = 0;
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

    std::vector<D3D9LightState>                         lights;

    float                                               nPatchSegments = 0.0f;
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
