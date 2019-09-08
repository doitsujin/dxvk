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

  struct D3D9FogContext {
    // General inputs...
    bool     IsPixel;
    bool     RangeFog;
    uint32_t RenderState;
    uint32_t vPos;
    uint32_t vFog;

    uint32_t oColor;
  };

  // Returns new oFog if VS
  // Returns new oColor if PS
  uint32_t DoFixedFunctionFog(SpirvModule& spvModule, const D3D9FogContext& fogCtx);

  struct D3D9FFShaderKeyVS {
    D3D9FFShaderKeyVS() {
      // memcmp safety
      std::memset(this, 0, sizeof(*this));
    }

    bool HasPositionT;

    bool HasColor0; // Diffuse
    bool HasColor1; // Specular

    bool UseLighting;

    bool NormalizeNormals;
    bool LocalViewer;
    bool RangeFog;

    D3DMATERIALCOLORSOURCE DiffuseSource;
    D3DMATERIALCOLORSOURCE AmbientSource;
    D3DMATERIALCOLORSOURCE SpecularSource;
    D3DMATERIALCOLORSOURCE EmissiveSource;

    std::array<uint32_t, caps::TextureStageCount> TexcoordIndices;
    std::array<uint32_t, caps::TextureStageCount> TransformFlags;

    uint32_t LightCount;
  };

  constexpr uint32_t TextureArgCount = 3;

  struct D3D9FFShaderStage {
    union {
      struct {
        uint16_t     ColorOp : 5;
        uint16_t     ColorArg0 : 6;
        uint16_t     ColorArg1 : 6;
        uint16_t     ColorArg2 : 6;

        uint16_t     AlphaOp : 5;
        uint16_t     AlphaArg0 : 6;
        uint16_t     AlphaArg1 : 6;
        uint16_t     AlphaArg2 : 6;

        uint16_t     Type : 2;
        uint16_t     ResultIsTemp : 1;
        uint16_t     Projected : 1;
      } data;

      uint64_t uint64[8];
    };
  };

  struct D3D9FFShaderKeyFS {
    D3D9FFShaderKeyFS() {
      // memcmp safety
      std::memset(this, 0, sizeof(*this));

      for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
        auto& stage = Stages[i].data;

        stage.ColorOp = D3DTOP_DISABLE;
        stage.AlphaOp = D3DTOP_DISABLE;
      }
    }

    D3D9FFShaderStage Stages[caps::TextureStageCount];
    bool              SpecularEnable;
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
    void Dump(const T& Key, const std::string& Name);

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

}