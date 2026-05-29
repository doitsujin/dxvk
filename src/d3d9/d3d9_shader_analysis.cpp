#include <sm3/sm3_io_map.h>

#include "../dxvk/dxvk_include.h"

#include "d3d9_shader_analysis.h"
#include "d3d9_util.h"
#include "d3d9_fixed_function.h"

#include <utility>

#include "../util/log/log.h"

using namespace dxbc_spv::sm3;

namespace dxvk {

  D3D9ShaderAnalysis::D3D9ShaderAnalysis(dxbc_spv::util::ByteReader code, bool isSWVP)
    : m_isSWVP(isSWVP) {
    if (!code) {
      Logger::err("No code chunk found in shader.");
      return;
    }

    if (Parser parser = Parser(code)) {
      m_length = 0u;
      RunAnalysis(parser);
    } else {
      Logger::err("Failed to parse code chunk.");
    }
  }

  D3D9ShaderAnalysis::D3D9ShaderAnalysis(D3D9ShaderAnalysis&& other)
    : m_isSWVP(other.m_isSWVP), m_length(other.m_length), m_shaderInfo(other.m_shaderInfo),
      m_constants(other.m_constants), m_immediateConstants(std::move(other.m_immediateConstants)),
      m_usedRTs(other.m_usedRTs), m_usedSamplers(other.m_usedSamplers), m_imageViewTypes(other.m_imageViewTypes),
      m_flatShadingMask(other.m_flatShadingMask), m_inputSignature(std::move(other.m_inputSignature)) {
    other.m_length = 0u;
  }

  D3D9ShaderAnalysis::D3D9ShaderAnalysis(const D3D9ShaderAnalysis& other)
    : m_isSWVP(other.m_isSWVP), m_length(other.m_length), m_shaderInfo(other.m_shaderInfo),
      m_constants(other.m_constants), m_immediateConstants(other.m_immediateConstants), m_usedRTs(other.m_usedRTs),
      m_usedSamplers(other.m_usedSamplers), m_imageViewTypes(other.m_imageViewTypes),
      m_flatShadingMask(other.m_flatShadingMask), m_inputSignature(other.m_inputSignature) { }

  bool D3D9ShaderAnalysis::RunAnalysis(Parser& parser) {
    if (!(m_shaderInfo = parser.getShaderInfo()))
      return false;

    if (m_shaderInfo.getVersion().first == 1u && m_shaderInfo.getType() == dxbc_spv::sm3::ShaderType::ePixel)
      m_usedRTs = 0b1u;

    while (parser) {
      dxbc_spv::sm3::Instruction instruction = parser.parseInstruction();

      if (!instruction || !HandleInstruction(instruction))
        return false;
    }

    m_length = parser.getByteOffset();

    // Shift up these sampler bits so we can just
    // do an or per-draw in the device.
    // We shift by 17 because 16 ps samplers + 1 dmap (tess)
    if (m_shaderInfo.getType() == ShaderType::eVertex)
      m_usedSamplers <<= FirstVSSamplerSlot;

    return true;
  }

  bool D3D9ShaderAnalysis::HandleInstruction(const Instruction& op) {
    auto matrixSize = getMatrixSize(op.getOpCode());

    /* Determine whether we're accessing float constants dynamically
     * because in that case, we'll need to copy the immediate constants
     * into the constant buffer inside DXVK. */
    for (uint32_t i = 0u; i < op.getSrcCount(); i++) {
      const auto& src = op.getSrc(i);
      auto registerType = src.getRegisterType();

      uint32_t index = src.getIndex();

      if (i && matrixSize)
        index += matrixSize->second - 1u;

      switch (registerType) {
        case RegisterType::eConstInt:
          m_constants.intCount = std::max(m_constants.intCount, index + 1u);
          break;

        case RegisterType::eConstBool: {
          m_constants.boolCount = std::max(m_constants.boolCount, index + 1u);

          if (!m_isSWVP) // SWVP raises the constant limit too high to use bit mask.
            m_constants.boolMask |= 1u << index;
        } break;

        case RegisterType::eConst:
        case RegisterType::eConst2:
        case RegisterType::eConst3:
        case RegisterType::eConst4: {
          m_constants.floatCount = std::max(m_constants.floatCount, index + 1u);

          if (src.hasRelativeAddressing()) {
            uint32_t hwvpFloatConstantsCount = GetShaderInfo().getType() == ShaderType::ePixel ? MaxFloatConstantsPS : MaxFloatConstantsVS;

            m_constants.floatCount = std::max(m_constants.floatCount,
              m_isSWVP ? MaxFloatConstantsSoftware : hwvpFloatConstantsCount);

            m_constants.floatsAccessedDynamically = true;
          }
        } break;

        default: break;
      }
    }

    if (GetShaderInfo().getType() == ShaderType::ePixel
      && op.hasDst()
      && op.getDst().getRegisterType() == RegisterType::eColorOut) {
      m_usedRTs |= 1u << op.getDst().getIndex();
    }

    switch (op.getOpCode()) {
      case OpCode::eDef:
      case OpCode::eDefI:
      case OpCode::eDefB:
        if (!HandleDef(op))
          return false;
        break;

      case OpCode::eTexLd:
      case OpCode::eTexBem:
      case OpCode::eTexBemL:
      case OpCode::eTexReg2Ar:
      case OpCode::eTexReg2Gb:
      case OpCode::eTexM3x2Tex:
      case OpCode::eTexM3x3Tex:
      case OpCode::eTexM3x3Spec:
      case OpCode::eTexM3x3VSpec:
      case OpCode::eTexReg2Rgb:
      case OpCode::eTexDp3Tex:
      case OpCode::eTexM3x2Depth:
      case OpCode::eTexDp3:
      case OpCode::eTexM3x3:
      case OpCode::eTexLdd:
      case OpCode::eTexLdl:
        if (!HandleTextureSample(op))
          return false;
        break;

      case OpCode::eDcl:
        if (!HandleDcl(op))
          return false;
        break;

      default: break;
    }

    return true;
  }

  bool D3D9ShaderAnalysis::HandleDef(const Instruction& op) {
    dxbc_spv_assert(op.hasDst());
    uint32_t index = op.getDst().getIndex();

    if (op.getOpCode() == OpCode::eDef) {
      m_immediateConstants.floatCount = std::max(m_immediateConstants.floatCount, index + 1u);

      dxbc_spv_assert(op.hasImm());
      auto imm = op.getImm();

      Vector4 value = {
        imm.getImmediate<float>(0u), imm.getImmediate<float>(1u),
        imm.getImmediate<float>(2u), imm.getImmediate<float>(3u)
      };
      m_immediateConstants.floats.push_back({ index, value });
    } else if (op.getOpCode() == OpCode::eDefI) {
      m_immediateConstants.intCount = std::max(m_immediateConstants.intCount, index + 1u);
    } else if (op.getOpCode() == OpCode::eDefB) {
      m_immediateConstants.boolCount = std::max(m_immediateConstants.boolCount, index + 1u);
    } else {
      return false;
    }

    return true;
  }


  bool D3D9ShaderAnalysis::HandleTextureSample(const Instruction& op) {
    uint32_t samplerIndex;
    auto dst = op.getDst();
    Operand src1;
    if (op.getSrcCount() >= 2u)
      src1 = op.getSrc(1u);

    switch (op.getOpCode()) {
      case OpCode::eTexLd:
        if (GetShaderInfo().getVersion().first <= 1u)
          samplerIndex = dst.getIndex();
        else
          samplerIndex = src1.getIndex();
        break;

      case OpCode::eTexLdl:
      case OpCode::eTexLdd:
        samplerIndex = src1.getIndex();
        break;

      case OpCode::eTexReg2Ar:
      case OpCode::eTexReg2Gb:
      case OpCode::eTexReg2Rgb:
      case OpCode::eTexM3x2Tex:
      case OpCode::eTexM3x3Tex:
      case OpCode::eTexM3x3:
      case OpCode::eTexM3x2Depth:
      case OpCode::eTexM3x3Spec:
      case OpCode::eTexM3x3VSpec:
      case OpCode::eTexDp3Tex:
      case OpCode::eTexDp3:
      case OpCode::eTexBem:
      case OpCode::eTexBemL:
        samplerIndex = dst.getIndex();
        break;

      default: return false;
    }

    m_usedSamplers |= 1u << samplerIndex;

    return true;
  }


  bool D3D9ShaderAnalysis::HandleDcl(const Instruction& op) {
    dxbc_spv_assert(op.hasDcl());
    const auto& dcl = op.getDcl();
    dxbc_spv_assert(op.hasDst());
    const auto& dst = op.getDst();

    RegisterType registerType = dst.getRegisterType();
    uint32_t index = dst.getIndex();

    if (registerType == RegisterType::eSampler) {
      switch (dcl.getTextureType()) {
        case TextureType::eTexture3D:
          m_imageViewTypes[index] = VK_IMAGE_VIEW_TYPE_3D;
          break;
        case TextureType::eTextureCube:
          m_imageViewTypes[index] = VK_IMAGE_VIEW_TYPE_CUBE;
          break;
        case TextureType::eTexture2D:
        default:
          m_imageViewTypes[index] = VK_IMAGE_VIEW_TYPE_2D;
          break;
      }
      m_usedSamplers |= 1u << index;
      return true;
    }

    if (registerType == RegisterType::eInput && GetShaderInfo().getType() == ShaderType::eVertex) {
      m_inputSignature.push_back(Semantic { dcl.getSemanticUsage(), dcl.getSemanticIndex() });
      return true;
    }

    if (GetShaderInfo().getType() == ShaderType::ePixel
      && dcl.getSemanticUsage() == SemanticUsage::eColor
      && dcl.getSemanticIndex() < 2u) {
      Semantic semantic = { dcl.getSemanticUsage(), dcl.getSemanticIndex() };

      auto location = FindLocationInFixedFunctionIO(semantic);

      if (!location.has_value()) {
        // Should never happen. The semantics we're looking for are at indices 9 and 10.
        dxbc_spv_unreachable();
        return false;
      }

      // The flat shading mask is applied before Semantic IO, so it uses the locations that are set by the compiler.
      m_flatShadingMask |= 1u << *location;
    }

    return true;
  }


  std::optional<uint32_t> D3D9ShaderAnalysis::FindLocationInFixedFunctionIO(dxbc_spv::sm3::Semantic semantic) const {
    // Outputs by the FF vertex shader and inputs by the FF pixel shader.
    // The locations of those semantics are reserved to make sure programmable shaders and FF are compatible.
    // The compiler does the same thing when determining IO locations.
    return IoMap::findFixedFunctionLocation(semantic);
  }


  D3D9ShaderAnalysis& D3D9ShaderAnalysis::operator=(const D3D9ShaderAnalysis& other) {
    if (this == &other)
      return *this;

    m_isSWVP = other.m_isSWVP;
    m_length = other.m_length;
    m_shaderInfo = other.m_shaderInfo;
    m_constants = other.m_constants;
    m_immediateConstants = other.m_immediateConstants;
    m_usedRTs = other.m_usedRTs;
    m_usedSamplers = other.m_usedSamplers;
    m_imageViewTypes = other.m_imageViewTypes;
    m_flatShadingMask = other.m_flatShadingMask;
    m_inputSignature = other.m_inputSignature;

    return *this;
  }

  D3D9ShaderAnalysis& D3D9ShaderAnalysis::operator=(D3D9ShaderAnalysis&& other) {
    if (this == &other)
      return *this;

    m_isSWVP = other.m_isSWVP;
    m_length = other.m_length;
    m_shaderInfo = other.m_shaderInfo;
    m_constants = other.m_constants;
    m_immediateConstants = std::move(other.m_immediateConstants);
    m_usedRTs = other.m_usedRTs;
    m_usedSamplers = other.m_usedSamplers;
    m_imageViewTypes = other.m_imageViewTypes;
    m_flatShadingMask = other.m_flatShadingMask;
    m_inputSignature = std::move(other.m_inputSignature);
    other.m_length = 0u;

    return *this;
  }

}
