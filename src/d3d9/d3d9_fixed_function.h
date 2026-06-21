#pragma once

#include "d3d9_include.h"

#include "d3d9_caps.h"

#include "d3d9_state.h"

#include "d3d9_shader_analysis.h"

#include "../dxvk/dxvk_shader.h"

#include <utility>
#include <unordered_map>

namespace dxvk {

  class D3D9DeviceEx;

  struct D3D9Options;

  enum D3D9FF_VertexBlendMode {
    D3D9FF_VertexBlendMode_Disabled,
    D3D9FF_VertexBlendMode_Normal,
    D3D9FF_VertexBlendMode_Tween,
  };

  // Texture stage properties
  enum class D3D9TextureStageStateFlag {
    UsesTexture = 0u,
    UsesCurrent = 1u,
    UsesTemp    = 2u,
  };

  using D3D9TextureStageStateFlags = Flags<D3D9TextureStageStateFlag>;

  // No idea what this is
  constexpr uint32_t TCIOffset = 16;
  constexpr uint32_t TCIMask   = 0b111 << TCIOffset;

  class D3D9FFShaderModuleSet : public RcObject {
    static constexpr uint32_t SamplerSet = 0u;
    static constexpr uint32_t SrvSet = 1u;
    static constexpr uint32_t CbvSet = 2u;
    static constexpr uint32_t SpecDataSet = 3u;
  public:

    D3D9FFShaderModuleSet() = delete;

    explicit D3D9FFShaderModuleSet(D3D9DeviceEx* pDevice);

    template<D3D9ShaderType Stage>
    Rc<DxvkShader> GetShader() {
      return Stage == D3D9ShaderType::VertexShader ? m_vs : m_fs;
    }

  private:

    Rc<DxvkShader> m_vs;
    Rc<DxvkShader> m_fs;

    static Rc<DxvkShader> buildVs();
    static Rc<DxvkShader> buildFs(D3D9DeviceEx* pDevice);

    constexpr static uint32_t GetPushSamplerOffset(uint32_t samplerIndex) {
      // Located directly after the PS push data block.
      return MaxSharedPushDataSize +
        sizeof(D3D9FfpsPushData) +
        sizeof(uint16_t) * samplerIndex;
    }

  };


  inline const D3D9InputSignature& GetFixedFunctionIsgn() {
    extern D3D9InputSignature g_ffIsgn;

    return g_ffIsgn;
  }

}
