#include "d3d9_shader.h"

#include "d3d9_caps.h"
#include "d3d9_device.h"
#include "d3d9_util.h"

#include <sm3/sm3_parser.h>
#include <sm3/sm3_converter.h>
#include <sm3/sm3_types.h>

namespace dxvk {

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

      setupStaticCbv();
      setupDynamicCbv();

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
    dxbc_spv::ir::SsaDef      m_clipPlaneCbv = {};
    dxbc_spv::ir::SsaDef      m_textureStageCbv = {};

    dxbc_spv::ir::SsaDef      m_mergedCbv = {};

    dxbc_spv::ir::SsaDef      m_staticCbv = {};
    dxbc_spv::ir::SsaDef      m_dynamicCbv = {};

    uint32_t                  m_firstConstInt  = 0u;
    uint32_t                  m_firstConstBool = 0u;

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

    dxbc_spv::ir::SsaDef findDeclarationForBuiltIn(dxbc_spv::ir::BuiltIn builtIn) {
      using namespace dxbc_spv;

      for (auto iter = m_builder.getDeclarations().first;
                iter != m_builder.getDeclarations().second; iter++) {
        if (iter->getOpCode() == ir::OpCode::eDclInputBuiltIn
         && builtIn == ir::BuiltIn(iter->getOperand(iter->getFirstLiteralOperandIndex())))
          return iter->getDef();
      }

      return ir::SsaDef();
    }

    void emitStaticConstantRanges(
            dxbc_spv::ir::BuiltIn     builtIn,
            dxbc_spv::ir::SsaDef      cbv,
            dxbc_spv::ir::Type&       cbvType,
      const D3D9ConstantBufferLayout& layout) {
      using namespace dxbc_spv;

      auto decl = m_builder.getOp(findDeclarationForBuiltIn(builtIn));

      if (!decl)
        return;

      auto type = decl.getType().getBaseType(0u);

      for (uint32_t i = 0u; i < layout.getRangeCount(); i++) {
        const auto& range = layout.getRange(i);

        for (uint32_t n = 0u; n < range.count; n++) {
          uint32_t memberIndex = cbvType.getStructMemberCount();
          cbvType.addStructMember(type);

          const auto& debugName = m_builder.getOp(getDebugName(decl, range.srcIndex + n));

          std::stringstream name;

          switch (builtIn) {
            case ir::BuiltIn::eLegacyConstBool: name << "b"; break;
            case ir::BuiltIn::eLegacyConstInt: name << "i"; break;
            default: name << "c";
          }

          name << range.srcIndex + n;

          if (debugName)
            name << "_" << debugName.getLiteralString(2u);

          m_builder.add(ir::Op::DebugMemberName(cbv, memberIndex, name.str().c_str()));
        }
      }
    }

    void fixupStaticCbvDebugName() {
      // If the static buffer only has one struct member, any OpDebugMemberName
      // instruction will not apply properly, so just set it as the debug name
      // for the constant buffer itself.
      using namespace dxbc_spv;

      const auto& op = m_builder.getOp(m_staticCbv);

      if (op.getType().getStructMemberCount() != 1u)
        return;

      ir::Op debugName = {};

      small_vector<ir::SsaDef, 4u> uses;
      m_builder.getUses(op.getDef(), uses);

      for (auto use : uses) {
        const auto& useOp = m_builder.getOp(use);

        switch (useOp.getOpCode()) {
          case ir::OpCode::eDebugName:
            m_builder.remove(use);
            break;

          case ir::OpCode::eDebugMemberName: {
            if (!uint32_t(useOp.getOperand(1u)))
              debugName = useOp;

            m_builder.remove(use);
          } break;

          default:
            break;
        }
      }

      if (debugName) {
        auto name = debugName.getLiteralString(2u);
        m_builder.add(ir::Op::DebugName(m_staticCbv, name.c_str()));
      }
    }

    void setupStaticCbv() {
      using namespace dxbc_spv;

      const auto& layoutB = m_analysis.GetConstantLayout()->getLayout(D3D9ConstantType::Bool);
      const auto& layoutI = m_analysis.GetConstantLayout()->getLayout(D3D9ConstantType::Int);
      const auto& layoutF = m_analysis.GetConstantLayout()->getLayout(D3D9ConstantType::Float);

      auto constantCountF = 0u;
      auto constantCountI = layoutI.computeConstantCount(0u);
      auto constantCountB = layoutB.computeConstantCount(0u);

      if (!layoutF.isDynamicallyIndexed())
        constantCountF = layoutF.computeConstantCount(0u);

      uint32_t totalCount = constantCountF + constantCountI + constantCountB;

      if (!totalCount)
        return;

      uint32_t regIndex = m_shaderStage == ir::ShaderStage::eVertex
        ? D3D9ShaderResourceMapping::CbvIndex::VSStaticConstants
        : D3D9ShaderResourceMapping::CbvIndex::PSStaticConstants;

      m_firstConstInt = constantCountF;
      m_firstConstBool = constantCountF + constantCountI;

      if (totalCount <= ir::Type::MaxStructMembers && !constantCountB) {
        // Put floats first, then integer constants, then bools. Emit a dummy CBV
        // declaration first so we can accumulate type info and add debug names,
        // then fix it up later with the proper type.
        auto cbvType = ir::Type();

        m_staticCbv = m_builder.add(ir::Op::DclCbv(cbvType, m_entryPoint,
          0u, regIndex, 1u).setFlags(ir::OpFlag::eInBounds));

        if (!layoutF.isDynamicallyIndexed())
          emitStaticConstantRanges(ir::BuiltIn::eLegacyConstFloat, m_staticCbv, cbvType, layoutF);

        emitStaticConstantRanges(ir::BuiltIn::eLegacyConstInt, m_staticCbv, cbvType, layoutI);
        emitStaticConstantRanges(ir::BuiltIn::eLegacyConstBool, m_staticCbv, cbvType, layoutB);

        m_builder.rewriteOp(m_staticCbv, ir::Op(m_builder.getOp(m_staticCbv)).setType(std::move(cbvType)));
        m_builder.add(ir::Op::DebugName(m_staticCbv, "c"));

        fixupStaticCbvDebugName();
      } else {
        // SWVP fallback. Just emit everything as a dword array that we can
        // dynamically index into, which will be needed for booleans.
        uint32_t boolCount = layoutB.computeConstantCount(0u);
        uint32_t vec4Count = layoutI.computeConstantCount(0u) + align(boolCount, 4u) / 4u;

        if (!layoutF.isDynamicallyIndexed())
          vec4Count += layoutF.computeConstantCount(0u);

        auto cbvType = ir::Type(ir::ScalarType::eU32, 4u).addArrayDimension(vec4Count);

        m_staticCbv = m_builder.add(ir::Op::DclCbv(cbvType, m_entryPoint,
          0u, regIndex, 1u).setFlags(ir::OpFlag::eInBounds));
        m_builder.add(ir::Op::DebugName(m_staticCbv, "c"));
      }
    }

    void setupDynamicCbv() {
      using namespace dxbc_spv;

      const auto& layout = m_analysis.GetConstantLayout()->getLayout(D3D9ConstantType::Float);

      if (!layout.isDynamicallyIndexed())
        return;

      // Trivial, just emit a float vector array with the maximum required size.
      auto cbvSize = layout.computeConstantCount(-1u);
      auto cbvType = ir::Type(ir::ScalarType::eF32, 4u).addArrayDimension(cbvSize);

      m_dynamicCbv = m_builder.add(ir::Op::DclCbv(cbvType, m_entryPoint,
        0u, D3D9ShaderResourceMapping::CbvIndex::VSDynamicConstants, 1u));
      m_builder.add(ir::Op::DebugName(m_dynamicCbv, "cF"));
    }

    void lowerLegacyConstInput(const dxbc_spv::ir::Op& decl, dxbc_spv::ir::BuiltIn builtIn) {
      using namespace dxbc_spv;

      // Check which layout to use for constant mappings
      bool isFloat = builtIn == ir::BuiltIn::eLegacyConstFloat;

      const auto& layout = m_analysis.GetConstantLayout()->getLayout(
        isFloat ? D3D9ConstantType::Float : D3D9ConstantType::Int);

      // We already emitted all the constant buffers that we might need.
      // Replace all input loads with loads from the relevant buffer.
      dxvk::small_vector<ir::SsaDef, 256u> uses;
      m_builder.getUses(decl.getDef(), uses);

      for (auto use : uses) {
        const auto& useOp = m_builder.getOp(use);

        if (useOp.getOpCode() == ir::OpCode::eInputLoad) {
          const auto& addressOp = m_builder.getOpForOperand(useOp, 1u);

          if (layout.isDynamicallyIndexed()) {
            dxbc_spv_assert(isFloat);
            dxbc_spv_assert(m_dynamicCbv);

            // Trivially forward the load to the dynamically indexed buffer
            auto cbvDescriptor = m_builder.addBefore(use, ir::Op::DescriptorLoad(
              ir::ScalarType::eCbv, m_dynamicCbv, m_builder.makeConstant(0u)));

            m_builder.rewriteOp(use, ir::Op::BufferLoad(useOp.getType(),
              cbvDescriptor, addressOp.getDef(), useOp.getType().byteSize()));
          } else {
            // For compacted constants, we need to map the source constant
            // to a member index in the static constant buffer struct.
            dxbc_spv_assert(addressOp.isConstant());
            dxbc_spv_assert(m_staticCbv);

            auto cbvDescriptor = m_builder.addBefore(use, ir::Op::DescriptorLoad(
              ir::ScalarType::eCbv, m_staticCbv, m_builder.makeConstant(0u)));

            auto srcIndex = uint32_t(addressOp.getOperand(0u));
            auto dstIndex = layout.findConstant(srcIndex);

            dxbc_spv_assert(dstIndex);

            auto memberIndex = dstIndex.value_or(0u) + (isFloat ? 0u : m_firstConstInt);
            auto memberType = m_builder.getOp(m_staticCbv).getType().getBaseType(memberIndex);

            if (addressOp.getOperandCount() > 1u)
              memberType = memberType.getBaseType();

            auto newAddress = m_builder.add(ir::Op(addressOp).setOperand(0u, memberIndex));

            auto bufferLoad = m_builder.addBefore(use, ir::Op::BufferLoad(memberType,
              cbvDescriptor, newAddress, useOp.getType().byteSize()).setFlags(ir::OpFlag::eInBounds));
            m_builder.rewriteOp(use, ir::Op::ConsumeAs(useOp.getType(), bufferLoad));
          }
        }
      }
    }

    void lowerLegacyConstBoolInput(const dxbc_spv::ir::Op& decl) {
      using namespace dxbc_spv;

      // Bools are special: In HWVP, we simply map them tp 16 bits of spec data,
      // in SWVP we need to load them from an actual constant buffer. Build helper
      // function to actually fetch a single boolean from the source.
      const auto& layout = m_analysis.GetConstantLayout()->getLayout(D3D9ConstantType::Bool);

      auto ref = m_builder.getCode().first->getDef();

      auto loadArg = m_builder.add(ir::Op::DclParam(ir::ScalarType::eU32));
      m_builder.add(ir::Op::DebugName(loadArg, "index"));

      auto loadFn = m_builder.addBefore(ref, ir::Op::Function(ir::ScalarType::eBool).addParam(loadArg));
      m_builder.add(ir::Op::DebugName(loadFn, "loadBoolConstant"));

      auto cursor = m_builder.setCursor(loadFn);

      if (!layout.computeConstantCount(0u)) {
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
        dxbc_spv_assert(m_staticCbv);

        // This is all extremely awful, but compilers should be able to constant-fold
        // all the indices and optimize. We should basically never hit this, anyway.
        auto constIndex = m_builder.add(ir::Op::ParamLoad(ir::ScalarType::eU32, loadFn, loadArg));

        auto cbvType = m_builder.getOp(m_staticCbv).getType().getBaseType(0u);
        auto cbvDescriptor = m_builder.add(ir::Op::DescriptorLoad(ir::ScalarType::eCbv, m_staticCbv, m_builder.makeConstant(0u)));

        auto vectorIndex = m_builder.add(ir::Op::UShr(ir::ScalarType::eU32, constIndex, m_builder.makeConstant(7u)));
        vectorIndex = m_builder.add(ir::Op::IAdd(ir::ScalarType::eU32, vectorIndex, m_builder.makeConstant(m_firstConstBool)));

        auto vectorLoad = m_builder.add(ir::Op::BufferLoad(cbvType,
          cbvDescriptor, vectorIndex, 16u).setFlags(ir::OpFlag::eInBounds));
        vectorLoad = m_builder.add(ir::Op::ConsumeAs(ir::BasicType(ir::ScalarType::eU32, 4u), vectorLoad));

        auto dwordIndex = m_builder.add(ir::Op::UBitExtract(ir::ScalarType::eU32,
          constIndex, m_builder.makeConstant(5u), m_builder.makeConstant(2u)));

        // Select correct dword from vector load
        auto dword = m_builder.add(ir::Op::CompositeExtract(ir::ScalarType::eU32, vectorLoad, m_builder.makeConstant(0u)));

        for (uint32_t i = 1u; i < 4u; i++) {
          dword = m_builder.add(ir::Op::Select(ir::ScalarType::eU32,
            m_builder.add(ir::Op::IEq(ir::ScalarType::eBool, dwordIndex, m_builder.makeConstant(i))),
            m_builder.add(ir::Op::CompositeExtract(ir::ScalarType::eU32, vectorLoad, m_builder.makeConstant(i))),
            dword));
        }

        // Test whether the requested bit is set in the bit mask
        auto bitIndex = m_builder.add(ir::Op::UBitExtract(ir::ScalarType::eU32,
          constIndex, m_builder.makeConstant(0u), m_builder.makeConstant(5u)));

        auto dwordMask = m_builder.add(ir::Op::IShl(ir::ScalarType::eU32, m_builder.makeConstant(1u), bitIndex));

        auto result = m_builder.add(ir::Op::IAnd(ir::ScalarType::eU32, dword, dwordMask));
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

        if (useOp.getOpCode() == ir::OpCode::eInputLoad) {
          const auto& addressOp = m_builder.getOpForOperand(useOp, 1u);
          dxbc_spv_assert(addressOp.getType().isScalarType());

          auto address = m_builder.addBefore(use, ir::Op::ConsumeAs(ir::ScalarType::eU32, addressOp.getDef()));
          m_builder.rewriteOp(use, ir::Op::FunctionCall(useOp.getType(), loadFn).addOperand(address));
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

      auto specDataType = ir::Type(ir::ScalarType::eU32).addArrayDimension(DxvkLimits::MaxNumSpecConstants);
      auto result = m_builder.add(ir::Op::DclCbv(specDataType, m_entryPoint,
        0u, D3D9ShaderResourceMapping::CbvIndex::SpecData, 1u).setFlags(ir::OpFlag::eInBounds));
      m_builder.add(ir::Op::DebugName(result, "specData"));
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
      auto cbvLoad = m_builder.add(ir::Op::BufferLoad(ir::ScalarType::eU32, cbvDescriptor,
        m_builder.makeConstant(layout.dwordOffset), 4u).setFlags(ir::OpFlag::eInBounds));

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
          m_entryPoint, 0u, D3D9ShaderResourceMapping::CbvIndex::VSClipPlanes, 1u)
          .setFlags(ir::OpFlag::eInBounds));
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

            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::BufferLoad(clipCbvType.getSubType(0u),
              descriptor, index, 16u).setFlags(ir::OpFlag::eInBounds)));
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
          m_entryPoint, 0u, D3D9ShaderResourceMapping::CbvIndex::PSShared, 1u)
          .setFlags(ir::OpFlag::eInBounds));

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
          textureStageCbvType.getBaseType(member).byteSize())
          .setFlags(ir::OpFlag::eInBounds)));
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
          return regIndex;

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
    
    m_shader = pDevice->GetDXVKDevice()->createCachedShader(
      ShaderKey.toString(), ModuleInfo.irCreateInfo, nullptr);

    if (!m_shader) {
      Rc<D3D9ShaderConverter> converter = new D3D9ShaderConverter(ShaderKey,
        ModuleInfo.shaderOptions, pShaderBytecode, m_analysis);

      m_shader = pDevice->GetDXVKDevice()->createCachedShader(
        ShaderKey.toString(), ModuleInfo.irCreateInfo, std::move(converter));
    }

    pDevice->GetDXVKDevice()->registerShader(m_shader);
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
