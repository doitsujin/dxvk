#pragma once

#include "../util/util_vector.h"
#include "../util/util_small_vector.h"

#include <util/util_byte_stream.h>

#include <sm3/sm3_types.h>
#include <sm3/sm3_parser.h>

#include <vulkan/vulkan.h>

#include <vector>

namespace dxvk {

struct D3D9ImmediateFloatConstant {
  uint32_t index;
  Vector4 value;
};

using D3D9ImmediateFloatConstants = small_vector<D3D9ImmediateFloatConstant, 16u>;

struct D3D9ImmediateConstants {
  uint32_t floatCount = 0u;
  uint32_t intCount   = 0u;
  uint32_t boolCount  = 0u;

  D3D9ImmediateFloatConstants floats;
};

struct D3D9ShaderConstantsInfo {
  bool floatsAccessedDynamically = false;

  uint32_t floatCount = 0u;
  uint32_t intCount   = 0u;
  uint32_t boolCount  = 0u;

  uint32_t boolMask = 0u;
};

using D3D9RenderTargetMask = uint8_t;
using D3D9SamplerMask      = uint32_t;
using D3D9InputSignature   = small_vector<dxbc_spv::sm3::Semantic, 16u>;

class D3D9ShaderAnalysis {

public:

  D3D9ShaderAnalysis() = default;

  D3D9ShaderAnalysis(dxbc_spv::util::ByteReader code, bool isSWVP);

  D3D9ShaderAnalysis(D3D9ShaderAnalysis&& other);

  D3D9ShaderAnalysis(const D3D9ShaderAnalysis& other);

  D3D9ShaderAnalysis& operator=(const D3D9ShaderAnalysis& other);

  D3D9ShaderAnalysis& operator=(D3D9ShaderAnalysis&& other);

  dxbc_spv::sm3::ShaderInfo GetShaderInfo() const {
    return m_shaderInfo;
  }

  size_t GetLength() const {
    return m_length;
  }

  const D3D9ShaderConstantsInfo& GetConstantsInfo() const {
    return m_constants;
  }

  const D3D9ImmediateConstants& GetImmediateConstants() const {
    return m_immediateConstants;
  }

  D3D9RenderTargetMask GetRenderTargetMask() const {
    return m_usedRTs;
  }

  D3D9SamplerMask GetSamplerMask() const {
    return m_usedSamplers;
  }

  VkImageViewType GetImageViewType(uint32_t index) const {
    return m_imageViewTypes[index];
  }

  bool IsSWVP() const {
    return m_isSWVP;
  }

  const D3D9InputSignature& GetInputSignature() const { return m_inputSignature; }

  uint32_t GetFlatShadingMask() const {
    return m_flatShadingMask;
  }

  explicit operator bool() const {
    return m_length != 0u;
  }

private:

  bool RunAnalysis(dxbc_spv::sm3::Parser& parser);

  bool HandleInstruction(const dxbc_spv::sm3::Instruction& op);

  bool HandleDef(const dxbc_spv::sm3::Instruction& op);

  bool HandleTextureSample(const dxbc_spv::sm3::Instruction& op);

  bool HandleDcl(const dxbc_spv::sm3::Instruction& op);

  std::optional<uint32_t> FindLocationInFixedFunctionIO(dxbc_spv::sm3::Semantic semantic) const;

  bool m_isSWVP = false;

  uint32_t m_length = 0u;

  dxbc_spv::sm3::ShaderInfo m_shaderInfo;

  D3D9ShaderConstantsInfo m_constants;

  D3D9ImmediateConstants m_immediateConstants;

  D3D9RenderTargetMask m_usedRTs = 0u;

  D3D9SamplerMask m_usedSamplers = 0u;

  std::array<VkImageViewType, 16u> m_imageViewTypes = {};

  uint32_t m_flatShadingMask = 0u;

  D3D9InputSignature m_inputSignature    = {};

};

}
