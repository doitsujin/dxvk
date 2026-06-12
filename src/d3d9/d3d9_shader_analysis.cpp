#include <algorithm>
#include <utility>

#include "../dxvk/dxvk_include.h"

#include <sm3/sm3_io_map.h>

#include "d3d9_fixed_function.h"
#include "d3d9_shader_analysis.h"
#include "d3d9_util.h"

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

  bool D3D9ShaderAnalysis::RunAnalysis(Parser& parser) {
    if (!(m_shaderInfo = parser.getShaderInfo()))
      return false;

    if (m_shaderInfo.getVersion().first == 1u && m_shaderInfo.getType() == dxbc_spv::sm3::ShaderType::ePixel)
      m_usedRTs = 0b1u;

    // No real point in gathering bool masks here. For HWVP we have to
    // use a different mechanism anyway, and in SWVP mode we can just
    // use the entire bit array up to the highest accessed constant.
    D3D9ImmediateConstantsData shaderDefs;

    ConstantMask constMaskF;
    ConstantMask constMaskI;

    while (parser) {
      dxbc_spv::sm3::Instruction instruction = parser.parseInstruction();

      if (!instruction || !HandleInstruction(instruction, constMaskF, constMaskI, shaderDefs))
        return false;
    }

    m_length = parser.getByteOffset();

    // The compiler will always constant-fold inline constants that
    // are statically indexed, so there is no need to keep them around
    for (size_t i = 0u; i < shaderDefs.floats.size(); i++)
      clrBit(constMaskF, shaderDefs.floats[i].index);

    for (size_t i = 0u; i < shaderDefs.ints.size(); i++)
      clrBit(constMaskI, shaderDefs.ints[i]);

    D3D9ConstantBufferLayout constLayoutF = m_constants.floatsAccessedDynamically
      ? D3D9ConstantBufferLayout(m_constants.floatCount, constMaskF.size(), constMaskF.data(),
          shaderDefs.floats.size(), shaderDefs.floats.data())
      : D3D9ConstantBufferLayout(constMaskF.size(), constMaskF.data());

    D3D9ConstantBufferLayout constLayoutI(constMaskI.size(), constMaskI.data());
    D3D9ConstantBufferLayout constLayoutB;

    if (m_isSWVP && m_shaderInfo.getType() == ShaderType::eVertex) {
      // Pad bool constants to a multiple of 16 bytes so we
      // can trivially keep all the other constants aligned
      uint32_t dwordCount = align(m_constants.boolCount, 32u) / 32u;
      constLayoutB = D3D9ConstantBufferLayout(align(dwordCount, 4u));
    }

    m_constLayout = D3D9ConstantBufferCopy::getOrCreate(
      std::move(constLayoutF),
      std::move(constLayoutI),
      std::move(constLayoutB));

    // Shift up these sampler bits so we can just
    // do an or per-draw in the device.
    // We shift by 17 because 16 ps samplers + 1 dmap (tess)
    if (m_shaderInfo.getType() == ShaderType::eVertex)
      m_usedSamplers <<= FirstVSSamplerSlot;

    return true;
  }

  bool D3D9ShaderAnalysis::HandleInstruction(
    const dxbc_spv::sm3::Instruction&   op,
          ConstantMask&                 constMaskF,
          ConstantMask&                 constMaskI,
          D3D9ImmediateConstantsData&   shaderDefs) {
    auto matrixSize = getMatrixSize(op.getOpCode());

    /* Determine whether we're accessing float constants dynamically
     * because in that case, we'll need to copy the immediate constants
     * into the constant buffer inside DXVK. */
    for (uint32_t i = 0u; i < op.getSrcCount(); i++) {
      const auto& src = op.getSrc(i);
      auto registerType = src.getRegisterType();

      uint32_t index = src.getIndex();
      uint32_t count = 1u;

      if (i && matrixSize)
        count = matrixSize->second;

      switch (registerType) {
        case RegisterType::eConstInt:
          m_constants.intCount = std::max(m_constants.intCount, index + count);

          for (uint32_t j = 0u; j < count; j++)
            setBit(constMaskI, index + j);
          break;

        case RegisterType::eConstBool: {
          m_constants.boolCount = std::max(m_constants.boolCount, index + count);

          // SWVP raises the constant limit too high to use bit mask.
          if (!m_isSWVP || m_shaderInfo.getType() != ShaderType::eVertex)
            m_constants.boolMask |= 1u << index;
        } break;

        case RegisterType::eConst:
        case RegisterType::eConst2:
        case RegisterType::eConst3:
        case RegisterType::eConst4: {
          m_constants.floatCount = std::max(m_constants.floatCount, index + count);

          if (src.hasRelativeAddressing()) {
            uint32_t hwvpFloatConstantsCount = GetShaderInfo().getType() == ShaderType::ePixel ? MaxFloatConstantsPS : MaxFloatConstantsVS;

            m_constants.floatCount = std::max(m_constants.floatCount,
              m_isSWVP ? MaxFloatConstantsSoftware : hwvpFloatConstantsCount);

            m_constants.floatsAccessedDynamically = true;
          } else {
            // Gather indices of statically accessed constants
            for (uint32_t j = 0u; j < count; j++)
              setBit(constMaskF, index + j);
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
        if (!HandleDef(op, shaderDefs))
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

  bool D3D9ShaderAnalysis::HandleDef(
    const Instruction&                  op,
          D3D9ImmediateConstantsData&   shaderDefs) {
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

      shaderDefs.floats.push_back({ index, value });
    } else if (op.getOpCode() == OpCode::eDefI) {
      m_immediateConstants.intCount = std::max(m_immediateConstants.intCount, index + 1u);
      shaderDefs.ints.push_back(index);
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
      m_inputSignature.add(Semantic { dcl.getSemanticUsage(), dcl.getSemanticIndex() });
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


  void D3D9ShaderAnalysis::setBit(ConstantMask& mask, uint32_t bit) {
    uint32_t index = bit / 32u;
    uint32_t shift = bit % 32u;

    if (index >= mask.size())
      mask.resize(index + 1u);

    mask[index] |= 1u << shift;
  }


  void D3D9ShaderAnalysis::clrBit(ConstantMask& mask, uint32_t bit) {
    uint32_t index = bit / 32u;
    uint32_t shift = bit % 32u;

    if (index < mask.size())
      mask[index] &= ~(1u << shift);
  }

}
