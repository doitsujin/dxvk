#pragma once

#include "d3d9_include.h"

#include "d3d9_caps.h"

#include "d3d9_state.h"

#include "../dxvk/dxvk_shader.h"

#include "../dxso/dxso_isgn.h"

#include <utility>
#include <unordered_map>

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
  };

  constexpr uint32_t GetGlobalSamplerSetIndex() {
    // arbitrary, but must not conflict with bindings
    return 15u;
  }

  constexpr uint32_t GetPushSamplerOffset(uint32_t samplerIndex) {
    // Must not conflict with render state block
    return MaxSharedPushDataSize + sizeof(uint16_t) * samplerIndex;
  }

  // Returns new oFog if VS
  // Returns new oColor if PS
  uint32_t DoFixedFunctionFog(D3D9ShaderSpecConstantManager& spec, SpirvModule& spvModule, const D3D9FogContext& fogCtx);

  void DoFixedFunctionAlphaTest(SpirvModule& spvModule, const D3D9AlphaTestContext& ctx);

  // Returns a render state block, as well as the index of the
  // first sampler member.
  std::pair<uint32_t, uint32_t> SetupRenderStateBlock(SpirvModule& spvModule, uint32_t samplerMask);

  // Returns a global sampler descriptor array
  uint32_t SetupSamplerArray(SpirvModule& spvModule);

  // Common code to load a sampler from the sampler array
  uint32_t LoadSampler(SpirvModule& spvModule, uint32_t descriptorId,
    uint32_t pushBlockId, uint32_t pushMember, uint32_t samplerIndex);

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

  constexpr uint32_t TextureArgCount = 3;

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

    D3D9FFShader(
            D3D9DeviceEx*         pDevice,
            DxsoProgramType       ProgramType);

    template <typename T>
    void Dump(D3D9DeviceEx* pDevice, const T& Key, const std::string& Name);

    Rc<DxvkShader> GetShader() const {
      return m_shader;
    }

  private:

    Rc<DxvkShader> m_shader;

  };


  class D3D9FFShaderModuleSet : public RcObject {

  public:

    D3D9FFShaderModuleSet() = delete;

    explicit D3D9FFShaderModuleSet(D3D9DeviceEx* pDevice);

    D3D9FFShader GetShaderModule(
            D3D9DeviceEx*         pDevice,
      const D3D9FFShaderKeyVS&    ShaderKey);

    D3D9FFShader GetShaderModule(
            D3D9DeviceEx*         pDevice,
      const D3D9FFShaderKeyFS&    ShaderKey);

    const D3D9FFShader& GetVSUbershaderModule() const {
      return m_vsUbershader;
    }

    const D3D9FFShader& GetFSUbershaderModule() const {
      return m_fsUbershader;
    }

    UINT GetVSCount() const {
      return m_vsModules.size();
    }

    UINT GetFSCount() const {
      return m_fsModules.size();
    }

  private:

    std::unordered_map<
      D3D9FFShaderKeyVS,
      D3D9FFShader,
      D3D9FFShaderKeyHash, D3D9FFShaderKeyEq> m_vsModules;

    std::unordered_map<
      D3D9FFShaderKeyFS,
      D3D9FFShader,
      D3D9FFShaderKeyHash, D3D9FFShaderKeyEq> m_fsModules;

    D3D9FFShader m_vsUbershader;
    D3D9FFShader m_fsUbershader;

  };


  inline const DxsoIsgn& GetFixedFunctionIsgn() {
    extern DxsoIsgn g_ffIsgn;

    return g_ffIsgn;
  }

}
