#include "d3d9_shader.h"

#include "d3d9_caps.h"
#include "d3d9_device.h"
#include "d3d9_util.h"

#include <sm3/sm3_parser.h>
#include <sm3/sm3_converter.h>
#include <sm3/sm3_types.h>

namespace dxvk {

  enum class D3D9IrCbvIndex : uint32_t {
    SpecData        = 0u,
    VsClipping      = 1u,
    PsTextureStages = 2u,
    ConstFloat      = 3u,
    ConstInt        = 4u,
    ConstBool       = 5u,
  };

  class D3D9ShaderLowerLegacyInputPass {

  public:

    D3D9ShaderLowerLegacyInputPass(
            dxbc_spv::ir::Builder&  builder,
      const D3D9ShaderOptions&      options,
      const D3D9ShaderAnalysis&     analysis)
    : m_builder(builder), m_options(options), m_analysis(analysis) { }

    void run() {
      using namespace dxbc_spv;

      std::tie(m_entryPoint, m_shaderStage) = findEntryPoint();
      dxbc_spv_assert(m_entryPoint);

      dxvk::small_vector<ir::SsaDef, 32u> inputs;

      for (auto iter = m_builder.getDeclarations().first;
                iter != m_builder.getDeclarations().second; iter++) {
        if (iter->getOpCode() == ir::OpCode::eDclInputBuiltIn)
          inputs.push_back(iter->getDef());
      }

      for (auto input : inputs) {
        const auto& inputOp = m_builder.getOp(input);

        auto builtIn = ir::BuiltIn(inputOp.getOperand(inputOp.getFirstLiteralOperandIndex()));

        switch (builtIn) {
          case ir::BuiltIn::eLegacyConstFloat:
          case ir::BuiltIn::eLegacyConstInt:
            lowerLegacyConstInput(inputOp, builtIn);
            break;

          case ir::BuiltIn::eLegacyConstBool:
            lowerLegacyConstBoolInput(inputOp);
            break;

          case ir::BuiltIn::eLegacyAlphaTest:
          case ir::BuiltIn::eLegacyFog:
          case ir::BuiltIn::eLegacyClipPlanes:
          case ir::BuiltIn::eLegacyPointArgs:
          case ir::BuiltIn::eLegacySamplerState:
          case ir::BuiltIn::eLegacyTextureStage:
            lowerLegacyRenderStateInput(inputOp, builtIn);
            break;

          default:
            break;
        }
      }
    }

  private:

    dxbc_spv::ir::Builder&    m_builder;
    D3D9ShaderOptions         m_options;
    const D3D9ShaderAnalysis& m_analysis;

    dxbc_spv::ir::ShaderStage m_shaderStage = {};
    dxbc_spv::ir::SsaDef      m_entryPoint = {};
    dxbc_spv::ir::SsaDef      m_specSelector = {};
    dxbc_spv::ir::SsaDef      m_specCbv = {};
    dxbc_spv::ir::SsaDef      m_mergedCbv = {};
    dxbc_spv::ir::SsaDef      m_clipPlaneCbv = {};
    dxbc_spv::ir::SsaDef      m_textureStageCbv = {};

    dxbc_spv::ir::SsaDef      m_fogPushData = {};
    dxbc_spv::ir::SsaDef      m_alphaTestPushData = {};
    dxbc_spv::ir::SsaDef      m_pointSizePushData = {};

    std::array<dxbc_spv::ir::SsaDef, DxvkLimits::MaxNumSpecConstants> m_specConstants = { };
    std::array<dxbc_spv::ir::SsaDef, D3D9SpecConstantId::SpecConstantCount> m_specFunctions = { };

    std::pair<dxbc_spv::ir::SsaDef, dxbc_spv::ir::ShaderStage> findEntryPoint() {
      using namespace dxbc_spv;

      auto [a, b] = m_builder.getDeclarations();

      for (auto iter = a; iter != b; iter++) {
        if (iter->getOpCode() == ir::OpCode::eEntryPoint)
          return std::make_pair(iter->getDef(), ir::ShaderStage(iter->getOperand(iter->getFirstLiteralOperandIndex())));
      }

      return {};
    }

    void lowerLegacyConstInput(const dxbc_spv::ir::Op& decl, dxbc_spv::ir::BuiltIn builtIn) {
      using namespace dxbc_spv;

      // Declare constant buffer for given constant type
      ir::SsaDef cbv = { };

      uint32_t indexOffset = 0u;

      if (m_options.isSWVP && m_shaderStage == ir::ShaderStage::eVertex) {
        cbv = emitConstantCbv(decl, builtIn);
      } else {
        if (!m_mergedCbv)
          m_mergedCbv = emitMergedConstantCbv();

        cbv = m_mergedCbv;

        if (builtIn == ir::BuiltIn::eLegacyConstFloat)
          indexOffset = caps::MaxOtherConstants;
      }

      if (!cbv)
        return;

      // Replace all input loads with equivalent constant buffer loads,
      // and apply the constant index offset when using the merged buffer.
      dxvk::small_vector<ir::SsaDef, 256u> uses;
      m_builder.getUses(decl.getDef(), uses);

      for (auto use : uses) {
        const auto& useOp = m_builder.getOp(use);

        switch (useOp.getOpCode()) {
          case ir::OpCode::eInputLoad: {
            const auto& addressOp = m_builder.getOpForOperand(useOp, 1u);
            auto address = addressOp.getDef();

            if (indexOffset) {
              // Adjust index offset
              if (addressOp.isConstant()) {
                address = m_builder.add(ir::Op(addressOp).setOperand(0u,
                  uint32_t(addressOp.getOperand(0u)) + indexOffset));
              } else {
                auto srcTy = addressOp.getType().getBaseType(0u);
                auto dstTy = ir::BasicType(ir::ScalarType::eU32, srcTy.getVectorSize());

                auto offsetConst = ir::Op(ir::OpCode::eConstant, dstTy).addOperand(indexOffset);

                for (uint32_t i = 1u; i < dstTy.getVectorSize(); i++)
                  offsetConst.addOperand(0u);

                address = m_builder.addBefore(use, ir::Op::ConsumeAs(dstTy, address));
                address = m_builder.addBefore(use, ir::Op::IAdd(dstTy, address, m_builder.add(offsetConst)));
              }
            }

            // Emit actual buffer load using the adjusted address
            auto descriptor = m_builder.addBefore(use, ir::Op::DescriptorLoad(
              ir::ScalarType::eCbv, cbv, m_builder.makeConstant(0u)));

            // Merged CBV is annoying and may force us to load the
            // wrong type if the CBV itself is arrayed.
            auto loadType = useOp.getType();

            if (m_builder.getOp(cbv).getType().isArrayType())
              loadType = m_builder.getOp(cbv).getType().getBaseType(0u);

            auto load = m_builder.addBefore(use, ir::Op::BufferLoad(
              loadType, descriptor, address, 16u));

            if (useOp.getType() != loadType)
              load = m_builder.addBefore(use, ir::Op::ConsumeAs(useOp.getType(), load));

            m_builder.rewriteDef(use, load);
          } break;

          case ir::OpCode::eDebugName:
          case ir::OpCode::eDebugMemberName: {
            m_builder.remove(use);
          } break;

          default:
            dxbc_spv_unreachable();
            break;
        }
      }
    }

    void lowerLegacyConstBoolInput(const dxbc_spv::ir::Op& decl) {
      using namespace dxbc_spv;

      // Bools are special: In HWVP, we simply map them tp 16 bits of spec
      // data, in SWVP we need to load them from an actual constant buffer.
      // Build helper function to actually fetch a single boolean from the
      // source.
      auto ref = m_builder.getCode().first->getDef();

      auto loadArg = m_builder.add(ir::Op::DclParam(ir::ScalarType::eU32));
      m_builder.add(ir::Op::DebugName(loadArg, "index"));

      auto loadFn = m_builder.addBefore(ref, ir::Op::Function(ir::ScalarType::eBool).addParam(loadArg));
      m_builder.add(ir::Op::DebugName(loadFn, "loadBoolConstant"));

      auto cursor = m_builder.setCursor(loadFn);

      if (!m_options.isSWVP) {
        // Build helper function to extract boolean from spec constant
        auto constIndex = m_builder.add(ir::Op::ParamLoad(ir::ScalarType::eU32, loadFn, loadArg));

        auto constBits = emitSpecConstantLoadRaw(m_shaderStage == ir::ShaderStage::eVertex
          ? D3D9SpecConstantId::SpecVertexShaderBools
          : D3D9SpecConstantId::SpecPixelShaderBools);

        auto constMask = m_builder.add(ir::Op::IShl(ir::ScalarType::eU32, m_builder.makeConstant(1u), constIndex));

        auto result = m_builder.add(ir::Op::IAnd(ir::ScalarType::eU32, constBits, constMask));
        result = m_builder.add(ir::Op::INe(ir::ScalarType::eBool, result, m_builder.makeConstant(0u)));
        m_builder.add(ir::Op::Return(ir::ScalarType::eBool, result));
      } else {
        uint32_t bitCount = getUsedConstantCount(ir::BuiltIn::eLegacyConstBool)
          .value_or(caps::MaxOtherConstantsSoftware);
        uint32_t dwordCount = (bitCount + 31u) / 32u;

        // Declare constant buffer as a dword array, one bit per constant
        auto cbvType = ir::Type(ir::ScalarType::eU32).addArrayDimension(dwordCount);
        auto cbvDef = m_builder.add(ir::Op::DclCbv(cbvType, m_entryPoint, 0u, uint32_t(D3D9IrCbvIndex::ConstBool), 1u));
        m_builder.add(ir::Op::DebugName(cbvDef, "cB"));

        auto constIndex = m_builder.add(ir::Op::ParamLoad(ir::ScalarType::eU32, loadFn, loadArg));
        auto dwordIndex = m_builder.add(ir::Op::UShr(ir::ScalarType::eU32, constIndex, m_builder.makeConstant(5u)));
        auto dwordShift = m_builder.add(ir::Op::IAnd(ir::ScalarType::eU32, constIndex, m_builder.makeConstant(0x1fu)));

        auto descriptor = m_builder.add(ir::Op::DescriptorLoad(ir::ScalarType::eCbv, cbvDef, m_builder.makeConstant(0u)));
        auto dwordLoad = m_builder.add(ir::Op::BufferLoad(ir::ScalarType::eU32, descriptor, dwordIndex, 4u));
        auto dwordMask = m_builder.add(ir::Op::IShl(ir::ScalarType::eU32, m_builder.makeConstant(1u), dwordShift));

        auto result = m_builder.add(ir::Op::IAnd(ir::ScalarType::eU32, dwordLoad, dwordMask));
        result = m_builder.add(ir::Op::INe(ir::ScalarType::eBool, result, m_builder.makeConstant(0u)));
        m_builder.add(ir::Op::Return(ir::ScalarType::eBool, result));
      }

      // Finalize helper function
      m_builder.add(ir::Op::FunctionEnd());
      m_builder.setCursor(cursor);

      // Replace all loads with function calls
      dxvk::small_vector<ir::SsaDef, 256u> uses;
      m_builder.getUses(decl.getDef(), uses);

      for (auto use : uses) {
        const auto& useOp = m_builder.getOp(use);

        switch (useOp.getOpCode()) {
          case ir::OpCode::eInputLoad: {
            const auto& addressOp = m_builder.getOpForOperand(useOp, 1u);
            dxbc_spv_assert(addressOp.getType().isScalarType());

            auto address = m_builder.addBefore(use, ir::Op::ConsumeAs(ir::ScalarType::eU32, addressOp.getDef()));
            m_builder.rewriteOp(use, ir::Op::FunctionCall(useOp.getType(), loadFn).addOperand(address));
          } break;

          case ir::OpCode::eDebugName:
          case ir::OpCode::eDebugMemberName: {
            m_builder.remove(use);
          } break;

          default:
            dxbc_spv_unreachable();
            break;
        }
      }
    }

    void lowerLegacyRenderStateInput(
      const dxbc_spv::ir::Op&           decl,
            dxbc_spv::ir::BuiltIn       builtIn) {
      using namespace dxbc_spv;

      // The basic idea for all render states is to source data from
      // a mixture of push data, spec constants and constant buffers.
      // Replace all input loads with the full composite, and extract
      // the required components.
      dxvk::small_vector<ir::SsaDef, 256u> uses;
      m_builder.getUses(decl.getDef(), uses);

      for (auto use : uses) {
        const auto& useOp = m_builder.getOp(use);

        switch (useOp.getOpCode()) {
          case ir::OpCode::eInputLoad: {
            const auto& addressOp = m_builder.getOpForOperand(useOp, 1u);
            auto addressType = addressOp.getType().getBaseType(0u);

            // If the input is arrayed, extract the first
            // address component as the array index.
            ir::SsaDef index = { };
            ir::SsaDef member = addressOp.getDef();

            if (decl.getType().isArrayType()) {
              if (addressOp.getType().isScalarType()) {
                index = addressOp.getDef();
                member = ir::SsaDef();
              } else {
                index = m_builder.addBefore(use, ir::Op::CompositeExtract(
                  addressType.getBaseType(), addressOp.getDef(), m_builder.makeConstant(0u)));

                ir::Op composite(ir::OpCode::eCompositeConstruct,
                  ir::BasicType(addressType.getBaseType(), addressType.getVectorSize() - 1u));

                for (uint32_t i = 1u; i < addressType.getVectorSize(); i++) {
                  composite.addOperand(m_builder.addBefore(use, ir::Op::CompositeExtract(
                    addressType.getBaseType(), addressOp.getDef(), m_builder.makeConstant(i))));
                }

                if (composite.getOperandCount() > 1u)
                  member = m_builder.addBefore(use, std::move(composite));
                else
                  member = ir::SsaDef(composite.getOperand(0u));
              }
            }

            // Load the full input structure
            auto result = [this, builtIn, use, index] {
              switch (builtIn) {
                case dxbc_spv::ir::BuiltIn::eLegacyAlphaTest:
                  return loadAlphaTestArgs(use);
                case dxbc_spv::ir::BuiltIn::eLegacyFog:
                  return loadFogArgs(use);
                case dxbc_spv::ir::BuiltIn::eLegacyClipPlanes:
                  return loadClippingArgs(use);
                case dxbc_spv::ir::BuiltIn::eLegacyPointArgs:
                  return loadPointArgs(use);
                case dxbc_spv::ir::BuiltIn::eLegacySamplerState:
                  return loadSamplerStateArgs(use, index);
                case dxbc_spv::ir::BuiltIn::eLegacyTextureStage:
                  return loadTextureStageArgs(use, index);
                default:
                  dxbc_spv_unreachable();
                  return ir::SsaDef();
              }
            } ();

            // Extract requested member. Dead code elimination
            // will remove any unused loads.
            if (member) {
              result = m_builder.addBefore(use, ir::Op::CompositeExtract(
                useOp.getType(), result, member));
            }

            m_builder.rewriteDef(use, result);
          } break;

          case ir::OpCode::eDebugName:
          case ir::OpCode::eDebugMemberName: {
            m_builder.remove(use);
          } break;

          default:
            dxbc_spv_unreachable();
            break;
        }
      }

    }

    std::optional<uint32_t> getUsedConstantCount(dxbc_spv::ir::BuiltIn builtIn) {
      using namespace dxbc_spv;

      auto constants = m_analysis.GetConstantsInfo();

      if (builtIn == ir::BuiltIn::eLegacyConstBool)
        return std::make_optional(constants.boolCount);

      if (builtIn == ir::BuiltIn::eLegacyConstInt)
        return std::make_optional(constants.intCount);

      if (builtIn == ir::BuiltIn::eLegacyConstFloat && !constants.floatsAccessedDynamically)
        return std::make_optional(constants.floatCount);

      return std::nullopt;
    }

    dxbc_spv::ir::SsaDef emitSpecConstantSelector() {
      using namespace dxbc_spv;

      auto result = m_builder.add(ir::Op::DclSpecConstant(ir::ScalarType::eBool,
        m_entryPoint, DxvkLimits::MaxNumSpecConstants, false));
      m_builder.add(ir::Op::DebugName(result, "IsOptimized"));

      return result;
    }

    dxbc_spv::ir::SsaDef emitSpecConstantCbv() {
      using namespace dxbc_spv;

      auto result = m_builder.add(ir::Op::DclCbv(
        ir::Type(ir::ScalarType::eU32).addArrayDimension(DxvkLimits::MaxNumSpecConstants),
        m_entryPoint, 0u, uint32_t(D3D9IrCbvIndex::SpecData), 1u));
      m_builder.add(ir::Op::DebugName(result, "spec_data"));

      return result;
    }

    dxbc_spv::ir::SsaDef emitSpecConstantLoadRaw(
            D3D9SpecConstantId          specConstant) {
      using namespace dxbc_spv;

      auto& layout = D3D9SpecializationInfo::Layout[specConstant];
      auto& specDef = m_specConstants.at(layout.dwordOffset);

      if (!specDef) {
        specDef = m_builder.add(ir::Op::DclSpecConstant(ir::ScalarType::eU32, m_entryPoint, layout.dwordOffset, 0u));
        m_builder.add(ir::Op::DebugName(specDef, str::format("SpecConst", layout.dwordOffset).c_str()));
      }

      if (!m_specSelector)
        m_specSelector = emitSpecConstantSelector();

      if (!m_specCbv)
        m_specCbv = emitSpecConstantCbv();

      auto cbvDescriptor = m_builder.add(ir::Op::DescriptorLoad(ir::ScalarType::eCbv, m_specCbv, m_builder.makeConstant(0u)));
      auto cbvLoad = m_builder.add(ir::Op::BufferLoad(ir::ScalarType::eU32, cbvDescriptor, m_builder.makeConstant(layout.dwordOffset), 4u));

      auto dword = m_builder.add(ir::Op::Select(ir::ScalarType::eU32, m_specSelector, specDef, cbvLoad));

      return m_builder.add(ir::Op::UBitExtract(ir::ScalarType::eU32, dword,
        m_builder.makeConstant(layout.bitOffset),
        m_builder.makeConstant(layout.sizeInBits)));
    }

    dxbc_spv::ir::SsaDef emitSpecConstantLoadIndexed(
            D3D9SpecConstantId          specConstant,
      const char*                       name,
            dxbc_spv::ir::SsaDef        ref,
            dxbc_spv::ir::ScalarType    type,
            uint32_t                    indexBits,
            uint32_t                    indexBase,
            dxbc_spv::ir::SsaDef        index) {
      using namespace dxbc_spv;

      if (!m_specFunctions.at(specConstant)) {
        auto codeStart = m_builder.getCode().first->getDef();

        // Build function and load spec constant value
        auto funcArg = ir::SsaDef();
        auto funcOp = ir::Op::Function(type);

        if (indexBits) {
          funcArg = m_builder.add(ir::Op::DclParam(ir::ScalarType::eU32));
          m_builder.add(ir::Op::DebugName(funcArg, "index"));
          funcOp.addOperand(funcArg);
        }

        auto funcDef =  m_builder.addBefore(codeStart, std::move(funcOp));

        if (name)
          m_builder.add(ir::Op::DebugName(funcDef, name));

        auto cursor = m_builder.setCursor(funcDef);
        auto result = emitSpecConstantLoadRaw(specConstant);

        // Extract requested bits if this is an indexed bit mask
        if (indexBits) {
          auto indexValue = m_builder.add(ir::Op::ParamLoad(ir::ScalarType::eU32, funcDef, funcArg));
          indexValue = m_builder.add(ir::Op::IAdd(ir::ScalarType::eU32, indexValue, m_builder.makeConstant(indexBase)));
          indexValue = m_builder.add(ir::Op::IMul(ir::ScalarType::eU32, indexValue, m_builder.makeConstant(indexBits)));
          result = m_builder.add(ir::Op::UBitExtract(ir::ScalarType::eU32, result, indexValue, m_builder.makeConstant(indexBits)));
        }

        // Convert to requested return type
        if (type == ir::ScalarType::eBool)
          result = m_builder.add(ir::Op::INe(ir::ScalarType::eBool, result, m_builder.makeConstant(0u)));
        else
          result = m_builder.add(ir::Op::ConsumeAs(type, result));

        // Finalize function
        m_builder.add(ir::Op::Return(type, result));
        m_builder.add(ir::Op::FunctionEnd());
        m_builder.setCursor(cursor);

        m_specFunctions.at(specConstant) = funcDef;
      }

      // Build function call op that the caller can add
      auto result = ir::Op::FunctionCall(type, m_specFunctions.at(specConstant));

      if (indexBits)
        result.addOperand(index);

      return m_builder.addBefore(ref, std::move(result));
    }

    dxbc_spv::ir::SsaDef emitSpecConstantLoad(
            D3D9SpecConstantId          specConstant,
      const char*                       name,
            dxbc_spv::ir::SsaDef        ref,
            dxbc_spv::ir::ScalarType    type) {
      using namespace dxbc_spv;

      return emitSpecConstantLoadIndexed(specConstant, name, ref, type, 0u, 0u, ir::SsaDef());
    }

    dxbc_spv::ir::SsaDef emitConstantCbv(const dxbc_spv::ir::Op& decl, dxbc_spv::ir::BuiltIn builtIn) {
      using namespace dxbc_spv;

      // Declare CBV as array if dynamically indexed, or as an actual
      // structure if statically indexed so we can use debug names.
      auto constantsUsed = getUsedConstantCount(builtIn);
      auto constantCount = constantsUsed.value_or(decl.getType().getArraySize(0u));

      if (!constantCount)
        return ir::SsaDef();

      ir::Type cbvType;

      if (constantsUsed && constantCount < ir::Type::MaxStructMembers) {
        auto baseType = decl.getType().getBaseType(0u);

        for (uint32_t i = 0u; i < constantCount; i++)
          cbvType.addStructMember(baseType);
      } else {
        cbvType = decl.getType().getBaseType(0u);
        cbvType.addArrayDimension(constantCount);
      }

      auto cbvIndex = builtIn == ir::BuiltIn::eLegacyConstFloat
        ? D3D9IrCbvIndex::ConstFloat
        : D3D9IrCbvIndex::ConstInt;
      auto cbv = m_builder.add(ir::Op::DclCbv(std::move(cbvType), m_entryPoint, 0u, uint32_t(cbvIndex), 1u));

      if (constantsUsed && constantCount < ir::Type::MaxStructMembers)
        copyConstantNames(cbv, 0u, constantCount, decl.getDef(), "c");

      m_builder.add(ir::Op::DebugName(cbv, builtIn == ir::BuiltIn::eLegacyConstFloat ? "cF" : "cI"));
      return cbv;
    }

    dxbc_spv::ir::SsaDef emitMergedConstantCbv() {
      using namespace dxbc_spv;

      // Count number of constants used by type. If any dynamic
      // indexing is involved, either number will be undefined.
      ir::SsaDef constInt = {};
      ir::SsaDef constFloat = {};

      auto numIntConstants = getUsedConstantCount(ir::BuiltIn::eLegacyConstInt);
      auto numFloatConstants = getUsedConstantCount(ir::BuiltIn::eLegacyConstFloat);

      auto [a, b] = m_builder.getDeclarations();

      for (auto iter = a; iter != b; iter++) {
        if (iter->getOpCode() == ir::OpCode::eDclInputBuiltIn) {
          auto builtIn = ir::BuiltIn(iter->getOperand(iter->getFirstLiteralOperandIndex()));

          if (builtIn == ir::BuiltIn::eLegacyConstFloat)
            constFloat = iter->getDef();
          else if (builtIn == ir::BuiltIn::eLegacyConstInt)
            constInt = iter->getDef();
        }
      }

      // No constants declared at all
      if (!constInt && !constFloat)
        return ir::SsaDef();

      // Declare the merged buffer with a float type since that will be the
      // most common access type. The merged buffer is not used in SWVP mode,
      // so using the regular constant counts is fine here.
      // TODO rework CBV binding to always keep int constants separate.
      auto cbvInts = caps::MaxOtherConstants;
      auto cbvFloats = numFloatConstants.value_or(m_shaderStage == ir::ShaderStage::eVertex
        ? caps::MaxFloatConstantsVS : caps::MaxSM3FloatConstantsPS);

      auto cbvConstants = cbvFloats + cbvInts;

      // If there is no dynamic indexing going on, declare constants as
      // individual members.
      ir::Type cbvType = ir::Type(ir::ScalarType::eF32, 4u).addArrayDimension(cbvConstants);

      if (numIntConstants && numFloatConstants) {
        cbvType = ir::Type();

        for (uint32_t i = 0u; i < caps::MaxOtherConstants; i++)
          cbvType.addStructMember(ir::ScalarType::eI32, 4u);

        for (uint32_t i = 0u; i < cbvFloats; i++)
          cbvType.addStructMember(ir::ScalarType::eF32, 4u);
      }

      auto cbv = m_builder.add(ir::Op::DclCbv(std::move(cbvType),
        m_entryPoint, 0u, uint32_t(D3D9IrCbvIndex::ConstFloat), 1u));
      m_builder.add(ir::Op::DebugName(cbv, "c"));

      // Copy debug names for individual constants if possible
      if (numIntConstants && numFloatConstants) {
        copyConstantNames(cbv, 0u, cbvInts, constInt, "i");
        copyConstantNames(cbv, cbvInts, cbvFloats, constFloat, "c");
      }

      return cbv;
    }

    dxbc_spv::ir::SsaDef loadAlphaTestArgs(dxbc_spv::ir::SsaDef ref) {
      using namespace dxbc_spv;

      if (!m_alphaTestPushData) {
        m_alphaTestPushData = m_builder.add(ir::Op::DclPushData(ir::ScalarType::eU32,
          m_entryPoint, offsetof(D3D9RenderStateInfo, alphaRef),
          ir::ShaderStage::eVertex | ir::ShaderStage::ePixel));
        m_builder.add(ir::Op::DebugName(m_alphaTestPushData, "alphaRef"));
      }

      ir::Op resultOp = ir::Op(ir::OpCode::eCompositeConstruct, ir::makeLegacyAlphaTestType());

      for (uint32_t i = 0u; i < resultOp.getType().getStructMemberCount(); i++) {
        switch (ir::LegacyAlphaTestLayout(i)) {
          case ir::LegacyAlphaTestLayout::eAlphaCompareOp: {
            resultOp.addOperand(emitSpecConstantLoad(
              D3D9SpecConstantId::SpecAlphaCompareOp,
              "alphaCompareOp", ref, ir::ScalarType::eU32));
          } break;

          case ir::LegacyAlphaTestLayout::eAlphaPrecision: {
            resultOp.addOperand(emitSpecConstantLoad(
              D3D9SpecConstantId::SpecAlphaPrecisionBits,
              "alphaPrecisionBits", ref, ir::ScalarType::eU32));
          } break;

          case ir::LegacyAlphaTestLayout::eAlphaRef: {
            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::PushDataLoad(
              ir::ScalarType::eU32, m_alphaTestPushData, ir::SsaDef())));
          } break;
        }
      }

      return m_builder.addBefore(ref, std::move(resultOp));
    }

    dxbc_spv::ir::SsaDef loadFogArgs(dxbc_spv::ir::SsaDef ref) {
      using namespace dxbc_spv;

      if (!m_fogPushData) {
        auto fogDataType = ir::Type()
          .addStructMember(ir::ScalarType::eF32, 3u)  // color
          .addStructMember(ir::ScalarType::eF32)      // scale
          .addStructMember(ir::ScalarType::eF32)      // end
          .addStructMember(ir::ScalarType::eF32);     // density

        m_fogPushData = m_builder.add(ir::Op::DclPushData(fogDataType,
          m_entryPoint, offsetof(D3D9RenderStateInfo, fogColor),
          ir::ShaderStage::eVertex | ir::ShaderStage::ePixel));

        m_builder.add(ir::Op::DebugMemberName(m_fogPushData, 0u, "fogColor"));
        m_builder.add(ir::Op::DebugMemberName(m_fogPushData, 1u, "fogDistScale"));
        m_builder.add(ir::Op::DebugMemberName(m_fogPushData, 2u, "fogDistEnd"));
        m_builder.add(ir::Op::DebugMemberName(m_fogPushData, 3u, "fogDensity"));
      }

      ir::Op resultOp = ir::Op(ir::OpCode::eCompositeConstruct, ir::makeLegacyFogType());

      for (uint32_t i = 0u; i < resultOp.getType().getStructMemberCount(); i++) {
        switch (ir::LegacyFogLayout(i)) {
          case ir::LegacyFogLayout::eFogEnable: {
            resultOp.addOperand(emitSpecConstantLoad(D3D9SpecConstantId::SpecFogEnabled,
              "fogEnable", ref, ir::ScalarType::eBool));
          } break;

          case ir::LegacyFogLayout::eFogMode: {
            auto spec = m_shaderStage == ir::ShaderStage::eVertex
              ? D3D9SpecConstantId::SpecVertexFogMode
              : D3D9SpecConstantId::SpecPixelFogMode;

            resultOp.addOperand(emitSpecConstantLoad(spec,
              "fogMode", ref, ir::ScalarType::eU32));
          } break;

          case ir::LegacyFogLayout::eFogColor: {
            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::PushDataLoad(
              resultOp.getType().getSubType(i), m_fogPushData, m_builder.makeConstant(0u))));
          } break;

          case ir::LegacyFogLayout::eFogScale: {
            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::PushDataLoad(
              resultOp.getType().getSubType(i), m_fogPushData, m_builder.makeConstant(1u))));
          } break;

          case ir::LegacyFogLayout::eFogEnd: {
            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::PushDataLoad(
              resultOp.getType().getSubType(i), m_fogPushData, m_builder.makeConstant(2u))));
          } break;

          case ir::LegacyFogLayout::eFogDensity: {
            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::PushDataLoad(
              resultOp.getType().getSubType(i), m_fogPushData, m_builder.makeConstant(3u))));
          } break;
        }
      }

      return m_builder.addBefore(ref, std::move(resultOp));
    }

    dxbc_spv::ir::SsaDef loadClippingArgs(dxbc_spv::ir::SsaDef ref) {
      using namespace dxbc_spv;

      auto clipCbvType = ir::Type(ir::ScalarType::eF32, 4u)
        .addArrayDimension(caps::MaxClipPlanes);

      if (!m_clipPlaneCbv) {
        m_clipPlaneCbv = m_builder.add(ir::Op::DclCbv(clipCbvType,
          m_entryPoint, 0u, uint32_t(D3D9IrCbvIndex::VsClipping), 1u));
        m_builder.add(ir::Op::DebugName(m_clipPlaneCbv, "clipPlanes"));
      }

      ir::Op resultOp = ir::Op(ir::OpCode::eCompositeConstruct, ir::makeLegacyClipPlaneType());

      for (uint32_t i = 0u; i < resultOp.getType().getStructMemberCount(); i++) {
        switch (ir::LegacyClipPlaneLayout(i)) {
          case ir::LegacyClipPlaneLayout::eClipPlaneCount: {
            resultOp.addOperand(emitSpecConstantLoad(
              D3D9SpecConstantId::SpecClipPlaneCount,
              "clipPlaneCount", ref, ir::ScalarType::eU32));
          } break;

          case ir::LegacyClipPlaneLayout::eClipPlane0:
          case ir::LegacyClipPlaneLayout::eClipPlane1:
          case ir::LegacyClipPlaneLayout::eClipPlane2:
          case ir::LegacyClipPlaneLayout::eClipPlane3:
          case ir::LegacyClipPlaneLayout::eClipPlane4:
          case ir::LegacyClipPlaneLayout::eClipPlane5: {
            auto descriptor = m_builder.addBefore(ref, ir::Op::DescriptorLoad(
              ir::ScalarType::eCbv, m_clipPlaneCbv, m_builder.makeConstant(0u)));
            auto index = m_builder.makeConstant(i - uint32_t(ir::LegacyClipPlaneLayout::eClipPlane0));

            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::BufferLoad(
              clipCbvType.getSubType(0u), descriptor, index, 16u)));
          } break;
        }
      }

      return m_builder.addBefore(ref, std::move(resultOp));
    }

    dxbc_spv::ir::SsaDef loadPointArgs(dxbc_spv::ir::SsaDef ref) {
      using namespace dxbc_spv;

      if (!m_pointSizePushData) {
        auto pointSizeType = ir::Type()
          .addStructMember(ir::ScalarType::eF32)  // size
          .addStructMember(ir::ScalarType::eF32)  // min
          .addStructMember(ir::ScalarType::eF32); // max

        m_pointSizePushData = m_builder.add(ir::Op::DclPushData(pointSizeType,
          m_entryPoint, offsetof(D3D9RenderStateInfo, pointSize),
          ir::ShaderStage::eVertex | ir::ShaderStage::ePixel));

        m_builder.add(ir::Op::DebugMemberName(m_pointSizePushData, 0u, "pointSize"));
        m_builder.add(ir::Op::DebugMemberName(m_pointSizePushData, 1u, "pointSizeMin"));
        m_builder.add(ir::Op::DebugMemberName(m_pointSizePushData, 2u, "pointSizeMax"));
      }

      ir::Op resultOp = ir::Op(ir::OpCode::eCompositeConstruct, ir::makeLegacyPointArgsType());

      for (uint32_t i = 0u; i < resultOp.getType().getStructMemberCount(); i++) {
        switch (ir::LegacyPointArgsLayout(i)) {
          case ir::LegacyPointArgsLayout::eIsPointSprite: {
            // Bit 1 of the point mode spec constant
            resultOp.addOperand(emitSpecConstantLoadIndexed(
              D3D9SpecConstantId::SpecPointMode, "pointMode", ref,
              ir::ScalarType::eBool, 1u, 0u, m_builder.makeConstant(1u)));
          } break;

          case ir::LegacyPointArgsLayout::ePointSize: {
            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::PushDataLoad(
              ir::ScalarType::eF32, m_pointSizePushData, m_builder.makeConstant(0u))));
          } break;

          case ir::LegacyPointArgsLayout::ePointSizeMin: {
            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::PushDataLoad(
              ir::ScalarType::eF32, m_pointSizePushData, m_builder.makeConstant(1u))));
          } break;

          case ir::LegacyPointArgsLayout::ePointSizeMax: {
            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::PushDataLoad(
              ir::ScalarType::eF32, m_pointSizePushData, m_builder.makeConstant(2u))));
          } break;
        }
      }

      return m_builder.addBefore(ref, std::move(resultOp));
    }

    dxbc_spv::ir::SsaDef loadSamplerStateArgs(dxbc_spv::ir::SsaDef ref, dxbc_spv::ir::SsaDef index) {
      using namespace dxbc_spv;
      dxbc_spv_assert(index);

      // Some state only applies to PS, and we need to offset sampler
      // indices in vertex shaders to read the proper state bits.
      // *Everything* here is based on spec constants.
      bool isVs = m_shaderStage == ir::ShaderStage::eVertex;
      uint32_t baseSampler = isVs ? FirstVSSamplerSlot : 0u;

      ir::Op resultOp = ir::Op(ir::OpCode::eCompositeConstruct,
        ir::makeLegacySamplerStateType(0u));

      for (uint32_t i = 0u; i < resultOp.getType().getStructMemberCount(); i++) {
        switch (ir::LegacySamplerStateLayout(i)) {
          case ir::LegacySamplerStateLayout::eTextureType: {
            if (isVs) {
              // Not used in VS since type must be declared explicitly
              resultOp.addOperand(m_builder.makeConstant(0u));
            } else {
              resultOp.addOperand(emitSpecConstantLoadIndexed(
                D3D9SpecConstantId::SpecSamplerType, "samplerType", ref,
                ir::ScalarType::eU32, 2u, baseSampler, index));
            }
          } break;

          case ir::LegacySamplerStateLayout::eUseDepthCompare: {
            resultOp.addOperand(emitSpecConstantLoadIndexed(
              D3D9SpecConstantId::SpecSamplerDepthMode, "samplerDepthCompare", ref,
              ir::ScalarType::eBool, 1u, baseSampler, index));
          } break;

          case ir::LegacySamplerStateLayout::eUseProjection: {
            if (isVs) {
              // Not a thing in VS
              resultOp.addOperand(m_builder.makeConstant(false));
            } else {
              resultOp.addOperand(emitSpecConstantLoadIndexed(
                D3D9SpecConstantId::SpecSamplerProjected, "samplerProjection", ref,
                ir::ScalarType::eBool, 1u, baseSampler, index));
            }
          } break;

          case ir::LegacySamplerStateLayout::eIsNull: {
            resultOp.addOperand(emitSpecConstantLoadIndexed(
              D3D9SpecConstantId::SpecSamplerNull, "samplerNull", ref,
              ir::ScalarType::eBool, 1u, baseSampler, index));
          } break;

          case ir::LegacySamplerStateLayout::eUseGather: {
            if (isVs) {
              // Not a thing in VS
              resultOp.addOperand(m_builder.makeConstant(false));
            } else {
              resultOp.addOperand(emitSpecConstantLoadIndexed(
                D3D9SpecConstantId::SpecSamplerFetch4, "samplerFetch4", ref,
                ir::ScalarType::eBool, 1u, baseSampler, index));
            }
          } break;

          case ir::LegacySamplerStateLayout::eDrefClamp: {
            resultOp.addOperand(emitSpecConstantLoadIndexed(
              D3D9SpecConstantId::SpecSamplerDrefClamp, "samplerDrefClamp", ref,
              ir::ScalarType::eBool, 1u, baseSampler, index));
          } break;

          case ir::LegacySamplerStateLayout::eDrefScale: {
            auto drefShift = emitSpecConstantLoad(D3D9SpecConstantId::SpecDrefScaling,
              "samplerDrefScaleBits", ref, ir::ScalarType::eU32);
            drefShift = m_builder.addBefore(ref, ir::Op::UMax(ir::ScalarType::eU32, drefShift, m_builder.makeConstant(1u)));

            // We need to divide dref by (1 << shift) - 1
            auto scale = m_builder.addBefore(ref, ir::Op::IBitInsert(ir::ScalarType::eU32,
              m_builder.makeConstant(0u), m_builder.makeConstant(-1u),
              m_builder.makeConstant(0u), drefShift));

            scale = m_builder.addBefore(ref, ir::Op::ConvertItoF(ir::ScalarType::eF32, scale));
            scale = m_builder.addBefore(ref, ir::Op::FRcp(ir::ScalarType::eF32, scale));

            resultOp.addOperand(scale);
          } break;
        }
      }

      return m_builder.addBefore(ref, std::move(resultOp));
    }

    dxbc_spv::ir::SsaDef loadTextureStageArgs(dxbc_spv::ir::SsaDef ref, dxbc_spv::ir::SsaDef index) {
      using namespace dxbc_spv;
      dxbc_spv_assert(index);

      auto textureStageCbvType = ir::Type()
        .addStructMember(ir::ScalarType::eF32, 4u)  // constant
        .addStructMember(ir::ScalarType::eF32, 2u)  // matrix row 0
        .addStructMember(ir::ScalarType::eF32, 2u)  // matrix row 1
        .addStructMember(ir::ScalarType::eF32)      // luminance scale
        .addStructMember(ir::ScalarType::eF32)      // luminance offset
        .addStructMember(ir::ScalarType::eF32, 2u)  // padding
        .addArrayDimension(caps::TextureStageCount);

      if (!m_textureStageCbv) {
        m_textureStageCbv = m_builder.add(ir::Op::DclCbv(textureStageCbvType,
          m_entryPoint, 0u, uint32_t(D3D9IrCbvIndex::PsTextureStages), 1u));

        m_builder.add(ir::Op::DebugName(m_textureStageCbv, "textureStages"));
        m_builder.add(ir::Op::DebugMemberName(m_textureStageCbv, 0u, "constant"));
        m_builder.add(ir::Op::DebugMemberName(m_textureStageCbv, 1u, "bumpMat0"));
        m_builder.add(ir::Op::DebugMemberName(m_textureStageCbv, 2u, "bumpMat1"));
        m_builder.add(ir::Op::DebugMemberName(m_textureStageCbv, 3u, "bumpLScale"));
        m_builder.add(ir::Op::DebugMemberName(m_textureStageCbv, 4u, "bumpLOffset"));
        m_builder.add(ir::Op::DebugMemberName(m_textureStageCbv, 5u, "reserved"));
      }

      ir::Op resultOp = ir::Op(ir::OpCode::eCompositeConstruct,
        ir::makeLegacyTextureStageType(0u));

      for (uint32_t i = 0u; i < resultOp.getType().getStructMemberCount(); i++) {
        auto member = [i] {
          switch (ir::LegacyTextureStageLayout(i)) {
            case ir::LegacyTextureStageLayout::eBumpMat0: return 1u;
            case ir::LegacyTextureStageLayout::eBumpMat1: return 2u;
            case ir::LegacyTextureStageLayout::eBumpScale: return 3u;
            case ir::LegacyTextureStageLayout::eBumpOffset: return 4u;
          }

          dxbc_spv_unreachable();
          return -1u;
        } ();

        // Emit actual load
        auto descriptor = m_builder.addBefore(ref, ir::Op::DescriptorLoad(
          ir::ScalarType::eCbv, m_textureStageCbv, m_builder.makeConstant(0u)));
        auto address = m_builder.addBefore(ref, ir::Op::CompositeConstruct(
          ir::BasicType(ir::ScalarType::eU32, 2u), index, m_builder.makeConstant(member)));

        // Members in this struct are naturally aligned
        resultOp.addOperand(m_builder.addBefore(ref, ir::Op::BufferLoad(
          textureStageCbvType.getBaseType(member), descriptor, address,
          textureStageCbvType.getBaseType(member).byteSize())));
      }

      return m_builder.addBefore(ref, std::move(resultOp));
    }

    dxbc_spv::ir::SsaDef getDebugName(const dxbc_spv::ir::Op& decl, uint32_t index) {
      using namespace dxbc_spv;

      auto [a, b] = m_builder.getUses(decl.getDef());

      for (auto iter = a; iter != b; iter++) {
        if (iter->getOpCode() == ir::OpCode::eDebugMemberName &&
            uint32_t(iter->getOperand(1u)) == index)
          return iter->getDef();
      }

      return ir::SsaDef();
    }

    void copyConstantNames(
            dxbc_spv::ir::SsaDef    dstDecl,
            uint32_t                dstIndex,
            uint32_t                dstCount,
            dxbc_spv::ir::SsaDef    srcDecl,
      const char*                   prefix) {
      using namespace dxbc_spv;

      auto memberCount = m_builder.getOp(dstDecl).getType().getStructMemberCount();

      if (memberCount == 1u)
        return;

      dxvk::small_vector<ir::SsaDef, 272u> names;
      dxvk::small_vector<ir::SsaDef, 256u> uses;
      m_builder.getUses(srcDecl, uses);

      // Assign dummy IDs to avoid assigning debug names for unused
      // constants. Simply assign a 0 ID to mark constants as used.
      names.resize(dstCount, dstDecl);

      for (auto use : uses) {
        const auto& useOp = m_builder.getOp(use);

        if (useOp.getOpCode() == ir::OpCode::eInputLoad) {
          const auto& addressOp = m_builder.getOpForOperand(useOp, 1u);
          dxbc_spv_assert(addressOp.isConstant());

          uint32_t index = uint32_t(addressOp.getOperand(0u));
          dxbc_spv_assert(index < dstCount);

          names[index] = ir::SsaDef();
        }
      }

      // Copy existing debug names for indexed constants
      for (auto use : uses) {
        const auto& useOp = m_builder.getOp(use);

        if (useOp.getOpCode() == ir::OpCode::eDebugMemberName) {
          auto index = uint32_t(useOp.getOperand(useOp.getFirstLiteralOperandIndex()));

          if (index < dstCount && index + dstIndex < memberCount && !names[index]) {
            names[index] = m_builder.add(ir::Op(useOp)
              .setOperand(0u, dstDecl)
              .setOperand(1u, dstIndex + index));
          }
        }
      }

      // Assign fallback names for used constants that have no name
      for (uint32_t i = 0u; i < dstCount && dstIndex + i < memberCount; i++) {
        if (!names[i]) {
          names[i] = m_builder.add(ir::Op::DebugMemberName(
            dstDecl, dstIndex + i, str::format(prefix, i).c_str()));
        }
      }
    }

  };

  class D3D9ShaderConverter : public DxvkIrShaderConverter {

  public:

    D3D9ShaderConverter(
      const DxvkShaderHash&           ShaderKey,
      const D3D9ShaderOptions&        Options,
      const void*                     pShaderBytecode,
      const D3D9ShaderAnalysis&       ShaderAnalysis)
    : m_key(ShaderKey), m_options(Options), m_analysis(ShaderAnalysis) {
      m_dxbc.resize(m_analysis.GetLength());
      std::memcpy(m_dxbc.data(), pShaderBytecode, m_dxbc.size());
    }

    ~D3D9ShaderConverter() { }

    void convertShader(
            dxbc_spv::ir::Builder&    builder) {
      auto debugName = m_key.toString();

      bool isVs = m_key.stage() == VK_SHADER_STAGE_VERTEX_BIT;

      dxbc_spv::sm3::Converter::Options options = { };
      options.name = debugName.c_str();
      options.includeDebugNames = true;
      options.fastFloatEmulation = m_options.d3d9FloatEmulation == D3D9FloatEmulation::Enabled;
      options.isSWVP = m_options.isSWVP && isVs;
      options.forceDynamicTextureType = m_options.forceSamplerTypeSpecConstants && !isVs;

      dxbc_spv::util::ByteReader reader(m_dxbc.data(), m_dxbc.size());

      dxbc_spv::sm3::Converter converter(reader, options);

      if (!converter.convertShader(builder))
        throw DxvkError(str::format("Failed to convert shader: ", m_key.toString()));

      D3D9ShaderLowerLegacyInputPass lowerLegacyPass(builder, m_options, m_analysis);
      lowerLegacyPass.run();
    }

    uint32_t determineResourceIndex(
            dxbc_spv::ir::ShaderStage stage,
            dxbc_spv::ir::ScalarType  type,
            uint32_t                  regSpace,
            uint32_t                  regIndex) const {
      // D3D9ShaderType has pixel shaders at 1, dxbc_spv has vertex shaders at 1.
      D3D9ShaderType shaderType = stage == dxbc_spv::ir::ShaderStage::ePixel
        ? D3D9ShaderType::PixelShader
        : D3D9ShaderType::VertexShader;

      switch (type) {
        case dxbc_spv::ir::ScalarType::eCbv:
          switch (D3D9IrCbvIndex(regIndex)) {
            case D3D9IrCbvIndex::SpecData:
              return D3D9ShaderResourceMapping::getSpecConstantBufferSlot();

            case D3D9IrCbvIndex::VsClipping:
              return D3D9ShaderResourceMapping::computeCbvBinding(shaderType,
                D3D9ShaderResourceMapping::ConstantBuffers::VSClipPlanes);

            case D3D9IrCbvIndex::PsTextureStages:
              return D3D9ShaderResourceMapping::computeCbvBinding(shaderType,
                D3D9ShaderResourceMapping::ConstantBuffers::PSShared);

            case D3D9IrCbvIndex::ConstFloat:
              return D3D9ShaderResourceMapping::computeCbvBinding(shaderType,
                D3D9ShaderResourceMapping::ConstantBuffers::VSFloatConstantBuffer);

            case D3D9IrCbvIndex::ConstInt:
              return D3D9ShaderResourceMapping::computeCbvBinding(shaderType,
                D3D9ShaderResourceMapping::ConstantBuffers::VSIntConstantBuffer);

            case D3D9IrCbvIndex::ConstBool:
              return D3D9ShaderResourceMapping::computeCbvBinding(shaderType,
                D3D9ShaderResourceMapping::ConstantBuffers::VSBoolConstantBuffer);
          } break;

        case dxbc_spv::ir::ScalarType::eSrv:
        case dxbc_spv::ir::ScalarType::eSampler:
          return D3D9ShaderResourceMapping::computeTextureBinding(shaderType, regIndex);

        default:
          break;
      }

      Logger::err(str::format("Missing Resource index. Stage: ", stage, ", regSpace: ", regSpace, ", regIndex: ", regIndex));
      return -1u;
    }

    void dumpSource(const std::string& path) const {
      std::ofstream file(str::topath(str::format(path, "/", m_key.toString(), ".sm3_dxbc").c_str()).c_str(), std::ios_base::trunc | std::ios_base::binary);
      file.write(reinterpret_cast<const char*>(m_dxbc.data()), m_dxbc.size());
    }

    std::string getDebugName() const {
      return m_key.toString();
    }

  private:

    std::vector<uint8_t> m_dxbc;

    DxvkShaderHash       m_key;
    D3D9ShaderOptions    m_options;
    D3D9ShaderAnalysis   m_analysis;

  };

  D3D9CommonShader::D3D9CommonShader(
            D3D9DeviceEx*         pDevice,
      const DxvkShaderHash&       ShaderKey,
            D3D9ShaderAnalysis&&  ShaderAnalysis,
      const D3D9ShaderCreateInfo& ModuleInfo,
      const void*                 pShaderBytecode)
  : m_analysis(std::move(ShaderAnalysis)) {
    const std::string name = ShaderKey.toString();
    Logger::debug(str::format("Compiling shader ", name));
    
    // If requested by the user, dump both the raw DXBC
    // shader and the compiled SPIR-V module to a file.
    const std::string& dumpPath = pDevice->GetOptions()->shaderDumpPath;
    
    if (dumpPath.size() != 0) {
      const uint32_t bytecodeLength = m_analysis.GetLength();

      std::ofstream file(str::topath(str::format(dumpPath, "/", name, ".sm3_dxbc").c_str()).c_str(), std::ios_base::binary | std::ios_base::trunc);
      file.write(reinterpret_cast<const char*>(pShaderBytecode), bytecodeLength);
    }

    if (pDevice->GetOptions()->useDxbcSpirv)
      CreateIrShader(pDevice, ShaderKey, ModuleInfo, pShaderBytecode, m_analysis);
    else
      CreateLegacyShader(pDevice, ShaderKey, ModuleInfo, pShaderBytecode);

    if (!dumpPath.empty()) {
      std::ofstream dumpStream(
        str::topath(str::format(dumpPath, "/", name, ".spv").c_str()).c_str(),
        std::ios_base::binary | std::ios_base::trunc);
      m_shader->dump(dumpStream);
    }

    pDevice->GetDXVKDevice()->registerShader(m_shader);
  }


  void D3D9CommonShader::CreateIrShader(
          D3D9DeviceEx*           pDevice,
    const DxvkShaderHash&         ShaderKey,
    const D3D9ShaderCreateInfo&   ModuleInfo,
    const void*                   pShaderBytecode,
    const D3D9ShaderAnalysis&     ShaderAnalysis) {
    m_shader = pDevice->GetDXVKDevice()->createCachedShader(
      ShaderKey.toString(), ModuleInfo.irCreateInfo, nullptr);

    if (!m_shader) {
      Rc<D3D9ShaderConverter> converter = new D3D9ShaderConverter(ShaderKey,
        ModuleInfo.shaderOptions, pShaderBytecode, ShaderAnalysis);

      m_shader = pDevice->GetDXVKDevice()->createCachedShader(
        ShaderKey.toString(), ModuleInfo.irCreateInfo, std::move(converter));
    }
  }


  void D3D9CommonShader::CreateLegacyShader(
            D3D9DeviceEx*         pDevice,
      const DxvkShaderHash&       ShaderKey,
      const D3D9ShaderCreateInfo& ModuleInfo,
      const void*                 pShaderBytecode) {

    DxsoReader reader(
      reinterpret_cast<const char*>(pShaderBytecode));
    DxsoModule module(reader);

    const D3D9ConstantLayout& constantLayout = module.info().shaderStage() == VK_SHADER_STAGE_VERTEX_BIT
      ? pDevice->GetVertexConstantLayout()
      : pDevice->GetPixelConstantLayout();

    DxsoModuleInfo moduleInfo;
    moduleInfo.options.d3d9FloatEmulation = ModuleInfo.shaderOptions.d3d9FloatEmulation;
    moduleInfo.options.forceSamplerTypeSpecConstants = ModuleInfo.shaderOptions.forceSamplerTypeSpecConstants;
    moduleInfo.options.sincosEmulation = ModuleInfo.irCreateInfo.options.flags.test(DxvkShaderCompileFlag::LowerSinCos);
    moduleInfo.options.forceSampleRateShading = ModuleInfo.irCreateInfo.options.flags.test(DxvkShaderCompileFlag::EnableSampleRateShading);
    moduleInfo.options.vertexFloatConstantBufferAsSSBO = ModuleInfo.irCreateInfo.options.maxUniformBufferSize < constantLayout.totalSize();

    m_shader       = module.compile(moduleInfo, ShaderKey.toString(), module.analyze(), constantLayout);
  }


  HRESULT D3D9ShaderModuleSet::GetShaderModule(
          D3D9DeviceEx*           pDevice,
    const DxvkShaderHash&         ShaderKey,
          D3D9ShaderAnalysis&&    ShaderAnalysis,
    const D3D9ShaderCreateInfo&   ModuleInfo,
    const void*                   pShaderBytecode,
          D3D9CommonShader*       pShader) {
    std::unique_lock<dxvk::mutex> lock(m_mutex);

    auto entry = m_modules.find(ShaderKey);
    if (entry != m_modules.end()) {
      *pShader = entry->second;
      return D3D_OK;
    }

    *pShader = D3D9CommonShader(pDevice, ShaderKey, std::move(ShaderAnalysis), ModuleInfo, pShaderBytecode);

    m_modules.insert({ ShaderKey, *pShader });
    return D3D_OK;
  }


}
