#pragma once

#include "d3d9_include.h"

#include "d3d9_caps.h"

#include "../dxvk/dxvk_shader.h"

#include "../dxso/dxso_isgn.h"

#include <unordered_map>
#include <bitset>

namespace dxvk {

  class D3D9DeviceEx;
  class SpirvModule;

  struct D3D9Options;
  class D3D9ShaderSpecConstantManager;

  struct D3D9FogContext {
    // General inputs...
    bool     IsPixel;
    bool     RangeFog;
    uint32_t RenderState;
    uint32_t vPos;
    uint32_t vFog;

    uint32_t oColor;

    bool     HasFogInput;

    bool     IsFixedFunction;
    bool     IsPositionT;
    bool     HasSpecular;
    uint32_t Specular;
    uint32_t SpecUBO;
  };

  struct D3D9AlphaTestContext {
    uint32_t alphaId;
    uint32_t alphaPrecisionId;
    uint32_t alphaFuncId;
    uint32_t alphaRefId;
  };

  struct D3D9FixedFunctionOptions {
    D3D9FixedFunctionOptions(const D3D9Options* options);

    bool    invariantPosition;
    bool    forceSampleRateShading;
    int32_t drefScaling;
  };

  constexpr float GetDrefScaleFactor(int32_t bitDepth) {
    return 1.0f / (float(1 << bitDepth) - 1.0f);
  }

  // Returns new oFog if VS
  // Returns new oColor if PS
  uint32_t DoFixedFunctionFog(D3D9ShaderSpecConstantManager& spec, SpirvModule& spvModule, const D3D9FogContext& fogCtx);

  void DoFixedFunctionAlphaTest(SpirvModule& spvModule, const D3D9AlphaTestContext& ctx);

  // Returns a render state block
  uint32_t SetupRenderStateBlock(SpirvModule& spvModule);

  struct D3D9PointSizeInfoVS {
    uint32_t defaultValue;
    uint32_t min;
    uint32_t max;
  };

  // Default point size and point scale magic!
  D3D9PointSizeInfoVS GetPointSizeInfoVS(D3D9ShaderSpecConstantManager& spec, SpirvModule& spvModule, uint32_t vPos, uint32_t vtx, uint32_t perVertPointSize, uint32_t rsBlock, uint32_t specUbo, bool isFixedFunction);

  struct D3D9PointSizeInfoPS {
    uint32_t isSprite;
  };

  D3D9PointSizeInfoPS GetPointSizeInfoPS(D3D9ShaderSpecConstantManager& spec, SpirvModule& spvModule, uint32_t rsBlock, uint32_t specUbo);

  uint32_t GetPointCoord(SpirvModule& spvModule);

  uint32_t GetSharedConstants(SpirvModule& spvModule);

  uint32_t SetupSpecUBO(SpirvModule& spvModule, std::vector<DxvkBindingInfo>& bindings);

  constexpr uint32_t TCIOffset = 16;
  constexpr uint32_t TCIMask   = 0b111 << TCIOffset;

  enum D3D9FF_VertexBlendMode {
    D3D9FF_VertexBlendMode_Disabled,
    D3D9FF_VertexBlendMode_Normal,
    D3D9FF_VertexBlendMode_Tween,
  };

  struct D3D9FFShaderKeyVSData {
    union {
      struct {
        uint32_t TexcoordIndices : 24;

        uint32_t HasPositionT : 1;

        uint32_t HasColor0 : 1; // Diffuse
        uint32_t HasColor1 : 1; // Specular

        uint32_t HasPointSize : 1;

        uint32_t UseLighting : 1;

        uint32_t NormalizeNormals : 1;
        uint32_t LocalViewer : 1;
        uint32_t RangeFog : 1;

        uint32_t TexcoordFlags : 24;

        uint32_t DiffuseSource : 2;
        uint32_t AmbientSource : 2;
        uint32_t SpecularSource : 2;
        uint32_t EmissiveSource : 2;

        uint32_t TransformFlags : 24;

        uint32_t LightCount : 4;

        uint32_t TexcoordDeclMask : 24;
        uint32_t HasFog : 1;

        uint32_t VertexBlendMode    : 2;
        uint32_t VertexBlendIndexed : 1;
        uint32_t VertexBlendCount   : 3;

        uint32_t VertexClipping     : 1;

        uint32_t Projected : 8;
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

  constexpr uint32_t TextureArgCount = 3;

  struct D3D9FFShaderStage {
    union {
      struct {
        uint32_t     ColorOp   : 5;
        uint32_t     ColorArg0 : 6;
        uint32_t     ColorArg1 : 6;
        uint32_t     ColorArg2 : 6;

        uint32_t     AlphaOp   : 5;
        uint32_t     AlphaArg0 : 6;
        uint32_t     AlphaArg1 : 6;
        uint32_t     AlphaArg2 : 6;

        uint32_t     Type         : 2;
        uint32_t     ResultIsTemp : 1;
        uint32_t     Projected    : 1;

        uint32_t     ProjectedCount : 3;
        uint32_t     SampleDref     : 1;

        uint32_t     TextureBound : 1;

        // Included in here, read from Stage 0 for packing reasons
        // Affects all stages.
        uint32_t     GlobalSpecularEnable : 1;
      } Contents;

      uint32_t Primitive[2];
    };
  };

  struct D3D9FFShaderKeyFS {
    D3D9FFShaderKeyFS() {
      // memcmp safety
      std::memset(Stages, 0, sizeof(Stages));

      // Normalize this. DISABLE != 0.
      for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
        Stages[i].Contents.ColorOp = D3DTOP_DISABLE;
        Stages[i].Contents.AlphaOp = D3DTOP_DISABLE;
      }
    }

    D3D9FFShaderStage Stages[caps::TextureStageCount];
  };

  struct D3D9FFShaderKeyHash {
    size_t operator () (const D3D9FFShaderKeyVS& key) const;
    size_t operator () (const D3D9FFShaderKeyFS& key) const;
  };

  bool operator == (const D3D9FFShaderKeyVS& a, const D3D9FFShaderKeyVS& b);
  bool operator != (const D3D9FFShaderKeyVS& a, const D3D9FFShaderKeyVS& b);
  bool operator == (const D3D9FFShaderKeyFS& a, const D3D9FFShaderKeyFS& b);
  bool operator != (const D3D9FFShaderKeyFS& a, const D3D9FFShaderKeyFS& b);

  struct D3D9FFShaderKeyEq {
    bool operator () (const D3D9FFShaderKeyVS& a, const D3D9FFShaderKeyVS& b) const;
    bool operator () (const D3D9FFShaderKeyFS& a, const D3D9FFShaderKeyFS& b) const;
  };

  class D3D9FFShader {

  public:

    D3D9FFShader(
            D3D9DeviceEx*         pDevice,
      const D3D9FFShaderKeyVS&    Key);

    D3D9FFShader(
            D3D9DeviceEx*         pDevice,
      const D3D9FFShaderKeyFS&    Key);

    template <typename T>
    void Dump(D3D9DeviceEx* pDevice, const T& Key, const std::string& Name);

    Rc<DxvkShader> GetShader() const {
      return m_shader;
    }

  private:

    Rc<DxvkShader> m_shader;

    DxsoIsgn       m_isgn;

  };


  class D3D9FFShaderModuleSet : public RcObject {

  public:

    D3D9FFShader GetShaderModule(
            D3D9DeviceEx*         pDevice,
      const D3D9FFShaderKeyVS&    ShaderKey);

    D3D9FFShader GetShaderModule(
            D3D9DeviceEx*         pDevice,
      const D3D9FFShaderKeyFS&    ShaderKey);

  private:

    std::unordered_map<
      D3D9FFShaderKeyVS,
      D3D9FFShader,
      D3D9FFShaderKeyHash, D3D9FFShaderKeyEq> m_vsModules;

    std::unordered_map<
      D3D9FFShaderKeyFS,
      D3D9FFShader,
      D3D9FFShaderKeyHash, D3D9FFShaderKeyEq> m_fsModules;

  };


  inline const DxsoIsgn& GetFixedFunctionIsgn() {
    extern DxsoIsgn g_ffIsgn;

    return g_ffIsgn;
  }

}