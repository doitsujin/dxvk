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

      setupSharedPushData();

      if (m_shaderStage == ir::ShaderStage::eVertex)
        setupVsPushData();

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

    struct SpecFunction {
      uint8_t specId   = 0u;
      uint8_t bitIndex = 0u;
      uint8_t bitCount = 0u;
      bool    isIndexed = false;
      dxbc_spv::ir::ScalarType type = {};
      dxbc_spv::ir::SsaDef function = {};

      bool matchesKey(const SpecFunction& other) const {
        return specId == other.specId
            && bitIndex == other.bitIndex
            && bitCount == other.bitCount
            && isIndexed == other.isIndexed
            && type == other.type;
      }
    };

    dxbc_spv::ir::Builder&    m_builder;
    D3D9ShaderOptions         m_options;
    const D3D9ShaderAnalysis& m_analysis;

    dxbc_spv::ir::ShaderStage m_shaderStage = {};
    dxbc_spv::ir::SsaDef      m_entryPoint = {};
    dxbc_spv::ir::SsaDef      m_clipPlaneCbv = {};
    dxbc_spv::ir::SsaDef      m_textureStageCbv = {};

    dxbc_spv::ir::SsaDef      m_mergedCbv = {};

    dxbc_spv::ir::SsaDef      m_staticCbv = {};
    dxbc_spv::ir::SsaDef      m_dynamicCbv = {};

    uint32_t                  m_firstConstInt  = 0u;
    uint32_t                  m_firstConstBool = 0u;

    dxbc_spv::ir::SsaDef      m_sharedPushData = {};
    dxbc_spv::ir::SsaDef      m_vsPushData = {};

    std::array<dxbc_spv::ir::SsaDef, DxvkLimits::MaxNumSpecConstants> m_specConstants = { };
    small_vector<SpecFunction, 32u>                                   m_specFunctions;

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

    void setupSharedPushData() {
      using namespace dxbc_spv;

      auto type = ir::Type()
        .addStructMember(ir::ScalarType::eU8, 3u)   // Fog color
        .addStructMember(ir::ScalarType::eU8)       // Alpha ref
        .addStructMember(ir::ScalarType::eF32)      // Fog scale
        .addStructMember(ir::ScalarType::eF32)      // Fog end
        .addStructMember(ir::ScalarType::eF32);     // Fog density

      m_sharedPushData = m_builder.add(ir::Op::DclPushData(type, m_entryPoint,
        D3D9SharedPushData::Offset, getPushDataStage(D3D9SharedPushData::Stages)));

      m_builder.add(ir::Op::DebugName(m_sharedPushData, "global"));
      m_builder.add(ir::Op::DebugMemberName(m_sharedPushData, 0u, "fogColor"));
      m_builder.add(ir::Op::DebugMemberName(m_sharedPushData, 1u, "alphaRef"));
      m_builder.add(ir::Op::DebugMemberName(m_sharedPushData, 2u, "fogDistanceScale"));
      m_builder.add(ir::Op::DebugMemberName(m_sharedPushData, 3u, "fogDistanceEnd"));
      m_builder.add(ir::Op::DebugMemberName(m_sharedPushData, 4u, "fogDensity"));
    }

    void setupVsPushData() {
      using namespace dxbc_spv;

      auto type = ir::Type()
        .addStructMember(ir::ScalarType::eU16)      // Dynamic float count
        .addStructMember(ir::ScalarType::eU16)      // Point size
        .addStructMember(ir::ScalarType::eU16)      // Minimum point size
        .addStructMember(ir::ScalarType::eU16);     // Maximum point size

      m_vsPushData = m_builder.add(ir::Op::DclPushData(type, m_entryPoint,
        D3D9VsPushData::Offset, getPushDataStage(D3D9VsPushData::Stages)));

      m_builder.add(ir::Op::DebugName(m_vsPushData, "vs"));
      m_builder.add(ir::Op::DebugMemberName(m_vsPushData, 0u, "cFSize"));
      m_builder.add(ir::Op::DebugMemberName(m_vsPushData, 1u, "pointSize"));
      m_builder.add(ir::Op::DebugMemberName(m_vsPushData, 2u, "pointSizeMin"));
      m_builder.add(ir::Op::DebugMemberName(m_vsPushData, 3u, "pointSizeMax"));
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

      // Over-declare by one element so that we can index past the
      // last actual constant in order to read zero.
      auto cbvSize = layout.computeConstantCount(-1u) + 1u;
      auto cbvType = ir::Type(ir::ScalarType::eF32, 4u).addArrayDimension(cbvSize);

      m_dynamicCbv = m_builder.add(ir::Op::DclCbv(cbvType, m_entryPoint, 0u,
        D3D9ShaderResourceMapping::CbvIndex::VSDynamicConstants, 1u).setFlags(ir::OpFlag::eInBounds));
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

            // The front-end promises to bind *at least* all statically
            // indexed constants. Only clamp dynamic indices.
            auto addressDef = addressOp.getDef();

            if (!addressOp.isConstant()) {
              auto cursor = m_builder.setCursor(m_builder.getPrev(use));

              auto count = m_builder.add(ir::Op::PushDataLoad(ir::ScalarType::eU16,
                m_vsPushData, m_builder.makeConstant(0u)));
              count = m_builder.add(ir::Op::ConvertItoI(ir::ScalarType::eU32, count));

              auto index = ir::extractFromVector(m_builder, addressDef, 0u);
              index = m_builder.add(ir::Op::UMin(ir::ScalarType::eU32, index, count));
              addressDef = ir::insertIntoVector(m_builder, addressDef, 0u, index);

              m_builder.setCursor(cursor);
            }

            m_builder.rewriteOp(use, ir::Op::BufferLoad(useOp.getType(), cbvDescriptor,
              addressDef, useOp.getType().byteSize()).setFlags(ir::OpFlag::eInBounds));
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

            auto cbvType = m_builder.getOp(m_staticCbv).getType();

            auto memberIndex = dstIndex.value_or(0u) + (isFloat ? 0u : m_firstConstInt);
            auto memberType = cbvType.getBaseType(cbvType.isStructType() ? memberIndex : 0u);

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

      ir::SsaDef loadFn = {};

      if (!layout.computeConstantCount(0u)) {
        // Emit regular indexed spec constant function
        uint32_t offset = m_shaderStage == ir::ShaderStage::eVertex
          ? offsetof(D3D9SpecData, vsBoolConstants)
          : offsetof(D3D9SpecData, psBoolConstants);

        SpecFunction fn = {};
        fn.specId = offset / 4u;
        fn.bitIndex = (offset % 4u) * 8u;
        fn.bitCount = 1u;
        fn.isIndexed = true;
        fn.type = ir::ScalarType::eBool;

        loadFn = getSpecConstantFunction(fn, "loadBoolConstant");
      } else {
        dxbc_spv_assert(m_staticCbv);
        auto ref = m_builder.getCode().first->getDef();

        auto loadArg = m_builder.add(ir::Op::DclParam(ir::ScalarType::eU32));
        m_builder.add(ir::Op::DebugName(loadArg, "index"));

        loadFn = m_builder.addBefore(ref, ir::Op::Function(ir::ScalarType::eBool).addParam(loadArg));
        m_builder.add(ir::Op::DebugName(loadFn, "loadBoolConstant"));

        auto cursor = m_builder.setCursor(loadFn);

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

        // Finalize helper function
        m_builder.add(ir::Op::Return(ir::ScalarType::eBool, result));
        m_builder.add(ir::Op::FunctionEnd());
        m_builder.setCursor(cursor);
      }

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

    dxbc_spv::ir::SsaDef emitSpecConstant(uint32_t specId) {
      using namespace dxbc_spv;

      static const std::array<const char*, 7u> s_specNames = {{
        "commonState",
        "fogState",
        "samplerProjMask",
        "vsSamplerStateAndBoolConstants",
        "psSamplerTypes",
        "psSamplerModes",
        "psBoolConstants",
      }};

      auto& specDef = m_specConstants.at(specId);

      if (!specDef) {
        specDef = m_builder.add(ir::Op::DclSpecConstant(ir::ScalarType::eU32, m_entryPoint, specId, 0u));
        m_builder.add(ir::Op::DebugName(specDef, s_specNames.at(specId)));
      }

      return specDef;
    }

    dxbc_spv::ir::SsaDef buildSpecConstantFunction(SpecFunction fn, const char* name) {
      using namespace dxbc_spv;

      auto codeStart = m_builder.getCode().first->getDef();

      // Build function and load spec constant value
      auto funcArg = ir::SsaDef();
      auto funcOp = ir::Op::Function(fn.type);

      if (fn.isIndexed) {
        funcArg = m_builder.add(ir::Op::DclParam(ir::ScalarType::eU32));
        m_builder.add(ir::Op::DebugName(funcArg, "index"));
        funcOp.addOperand(funcArg);
      }

      fn.function = m_builder.addBefore(codeStart, std::move(funcOp));

      if (name)
        m_builder.add(ir::Op::DebugName(fn.function, name));

      auto cursor = m_builder.setCursor(fn.function);
      auto result = emitSpecConstant(fn.specId);

      // Extract requested bits if this is an indexed bit mask
      if (fn.bitCount < 32u) {
        auto bitIndexDef = m_builder.makeConstant(uint32_t(fn.bitIndex));
        auto bitCountDef = m_builder.makeConstant(uint32_t(fn.bitCount));

        if (fn.isIndexed) {
          auto indexValue = m_builder.add(ir::Op::ParamLoad(ir::ScalarType::eU32, fn.function, funcArg));
          indexValue = m_builder.add(ir::Op::IMul(ir::ScalarType::eU32, indexValue, bitCountDef));
          bitIndexDef = m_builder.add(ir::Op::IAdd(ir::ScalarType::eU32, bitIndexDef, indexValue));
        }

        result = m_builder.add(ir::Op::UBitExtract(ir::ScalarType::eU32, result, bitIndexDef, bitCountDef));
      }

      // Convert to requested return type
      if (fn.type == ir::ScalarType::eBool)
        result = m_builder.add(ir::Op::INe(ir::ScalarType::eBool, result, m_builder.makeConstant(0u)));
      else
        result = m_builder.add(ir::Op::ConsumeAs(fn.type, result));

      // Finalize function
      m_builder.add(ir::Op::Return(fn.type, result));
      m_builder.add(ir::Op::FunctionEnd());
      m_builder.setCursor(cursor);
      return fn.function;
    }

    dxbc_spv::ir::SsaDef getSpecConstantFunction(SpecFunction fn, const char* name) {
      for (auto& e : m_specFunctions) {
        if (e.matchesKey(fn))
          return e.function;
      }

      fn.function = buildSpecConstantFunction(fn, name);
      m_specFunctions.push_back(fn);
      return fn.function;
    }

    dxbc_spv::ir::SsaDef emitSpecConstantLoad(
            uint32_t                    byteOffset,
            uint32_t                    bitIndex,
            uint32_t                    bitCount,
      const char*                       name,
            dxbc_spv::ir::SsaDef        ref,
            dxbc_spv::ir::ScalarType    type,
            dxbc_spv::ir::SsaDef        index) {
      using namespace dxbc_spv;

      SpecFunction fn = {};
      fn.specId = byteOffset / 4u;
      fn.bitIndex = bitIndex + 8u * (byteOffset % 4u);
      fn.bitCount = bitCount;
      fn.isIndexed = bool(index);
      fn.type = type;

      // Build function call op that the caller can add
      auto result = ir::Op::FunctionCall(type, getSpecConstantFunction(fn, name));

      if (index)
        result.addOperand(index);

      return m_builder.addBefore(ref, std::move(result));
    }

    dxbc_spv::ir::SsaDef loadAlphaTestArgs(dxbc_spv::ir::SsaDef ref) {
      using namespace dxbc_spv;

      ir::Op resultOp = ir::Op(ir::OpCode::eCompositeConstruct, ir::makeLegacyAlphaTestType());

      for (uint32_t i = 0u; i < resultOp.getType().getStructMemberCount(); i++) {
        switch (ir::LegacyAlphaTestLayout(i)) {
          case ir::LegacyAlphaTestLayout::eAlphaCompareOp: {
            resultOp.addOperand(emitSpecConstantLoad(offsetof(D3D9SpecData, alphaTest),
              0u, 3u, "alphaCompareOp", ref, ir::ScalarType::eU32, ir::SsaDef()));
          } break;

          case ir::LegacyAlphaTestLayout::eAlphaPrecision: {
            resultOp.addOperand(emitSpecConstantLoad(offsetof(D3D9SpecData, alphaTest),
              4u, 4u, "alphaPrecisionBits", ref, ir::ScalarType::eU32, ir::SsaDef()));
          } break;

          case ir::LegacyAlphaTestLayout::eAlphaRef: {
            auto alphaRef = m_builder.addBefore(ref, ir::Op::PushDataLoad(
              ir::ScalarType::eU8, m_sharedPushData, m_builder.makeConstant(1u)));
            alphaRef = m_builder.addBefore(ref, ir::Op::ConvertItoI(ir::ScalarType::eU32, alphaRef));
            resultOp.addOperand(alphaRef);
          } break;
        }
      }

      return m_builder.addBefore(ref, std::move(resultOp));
    }

    dxbc_spv::ir::SsaDef loadFogArgs(dxbc_spv::ir::SsaDef ref) {
      using namespace dxbc_spv;

      ir::Op resultOp = ir::Op(ir::OpCode::eCompositeConstruct, ir::makeLegacyFogType());

      for (uint32_t i = 0u; i < resultOp.getType().getStructMemberCount(); i++) {
        switch (ir::LegacyFogLayout(i)) {
          case ir::LegacyFogLayout::eFogEnable: {
            resultOp.addOperand(emitSpecConstantLoad(offsetof(D3D9SpecData, fogEnable),
              0u, 1u, "fogEnable", ref, ir::ScalarType::eBool, ir::SsaDef()));
          } break;

          case ir::LegacyFogLayout::eFogUseZ: {
            resultOp.addOperand(emitSpecConstantLoad(offsetof(D3D9SpecData, fogUseZ),
              0u, 1u, "fogUseZ", ref, ir::ScalarType::eBool, ir::SsaDef()));
          } break;

          case ir::LegacyFogLayout::eFogMode: {
            auto offset = m_shaderStage == ir::ShaderStage::eVertex
              ? offsetof(D3D9SpecData, fogModeVertex)
              : offsetof(D3D9SpecData, fogModePixel);

            resultOp.addOperand(emitSpecConstantLoad(offset,
              0u, 2u, "fogMode", ref, ir::ScalarType::eU32, ir::SsaDef()));
          } break;

          case ir::LegacyFogLayout::eFogColor: {
            // We store the raw D3DCOLOR in .bgr order. Convert to float and swizzle.
            auto fogColor = m_builder.addBefore(ref, ir::Op::PushDataLoad(
              ir::BasicType(ir::ScalarType::eU8, 3u), m_sharedPushData, m_builder.makeConstant(0u)));

            ir::Op op(ir::OpCode::eCompositeConstruct, resultOp.getType().getSubType(i));

            for (uint32_t i = 0u; i < 3u; i++) {
              auto scalar = m_builder.addBefore(ref, ir::Op::CompositeExtract(ir::ScalarType::eU8, fogColor, m_builder.makeConstant(2u - i)));
              scalar = m_builder.addBefore(ref, ir::Op::ConvertItoF(ir::ScalarType::eF32, scalar));
              scalar = m_builder.addBefore(ref, ir::Op::FDiv(ir::ScalarType::eF32, scalar, m_builder.makeConstant(255.0f)));
              op.addOperand(scalar);
            }

            resultOp.addOperand(m_builder.addBefore(ref, std::move(op)));
          } break;

          case ir::LegacyFogLayout::eFogScale: {
            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::PushDataLoad(
              resultOp.getType().getSubType(i), m_sharedPushData, m_builder.makeConstant(2u))));
          } break;

          case ir::LegacyFogLayout::eFogEnd: {
            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::PushDataLoad(
              resultOp.getType().getSubType(i), m_sharedPushData, m_builder.makeConstant(3u))));
          } break;

          case ir::LegacyFogLayout::eFogDensity: {
            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::PushDataLoad(
              resultOp.getType().getSubType(i), m_sharedPushData, m_builder.makeConstant(4u))));
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
            resultOp.addOperand(emitSpecConstantLoad(offsetof(D3D9SpecData, clipPlaneCount),
              0u, 3u, "clipPlaneCount", ref, ir::ScalarType::eU32, ir::SsaDef()));
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

      ir::Op resultOp = ir::Op(ir::OpCode::eCompositeConstruct, ir::makeLegacyPointArgsType());

      for (uint32_t i = 0u; i < resultOp.getType().getStructMemberCount(); i++) {
        switch (ir::LegacyPointArgsLayout(i)) {
          case ir::LegacyPointArgsLayout::eIsPointSprite: {
            // Bit 1 of the point mode spec constant
            resultOp.addOperand(emitSpecConstantLoad(offsetof(D3D9SpecData, enablePointSprite),
              0u, 1u, "enablePointSprite", ref, ir::ScalarType::eBool, ir::SsaDef()));
          } break;

          case ir::LegacyPointArgsLayout::ePointSize: {
            auto size = m_builder.addBefore(ref, ir::Op::PushDataLoad(
              ir::ScalarType::eU16, m_vsPushData, m_builder.makeConstant(1u)));
            resultOp.addOperand(emitDecodePointSize(ref, size));
          } break;

          case ir::LegacyPointArgsLayout::ePointSizeMin: {
            auto size = m_builder.addBefore(ref, ir::Op::PushDataLoad(
              ir::ScalarType::eU16, m_vsPushData, m_builder.makeConstant(2u)));
            resultOp.addOperand(emitDecodePointSize(ref, size));
          } break;

          case ir::LegacyPointArgsLayout::ePointSizeMax: {
            auto size = m_builder.addBefore(ref, ir::Op::PushDataLoad(
              ir::ScalarType::eU16, m_vsPushData, m_builder.makeConstant(3u)));
            resultOp.addOperand(emitDecodePointSize(ref, size));
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

      auto samplerTypeOffset = isVs
        ? offsetof(D3D9SpecData, vsSamplerTypes)
        : offsetof(D3D9SpecData, psSamplerTypes);

      auto samplerModeOffset = isVs
        ? offsetof(D3D9SpecData, vsSamplerModes)
        : offsetof(D3D9SpecData, psSamplerModes);

      auto samplerMode = emitSpecConstantLoad(samplerModeOffset,
        0u, 2u, "samplerMode", ref, ir::ScalarType::eU32, index);

      auto samplerType = emitSpecConstantLoad(samplerTypeOffset,
        0u, 2u, "samplerType", ref, ir::ScalarType::eU32, index);

      ir::Op resultOp = ir::Op(ir::OpCode::eCompositeConstruct,
        ir::makeLegacySamplerStateType(0u));

      for (uint32_t i = 0u; i < resultOp.getType().getStructMemberCount(); i++) {
        switch (ir::LegacySamplerStateLayout(i)) {
          case ir::LegacySamplerStateLayout::eTextureType: {
            resultOp.addOperand(samplerType);
          } break;

          case ir::LegacySamplerStateLayout::eUseDepthCompare: {
            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::UGe(ir::ScalarType::eBool,
              samplerMode, m_builder.makeConstant(uint32_t(D3D9SamplerMode::Dref)))));
          } break;

          case ir::LegacySamplerStateLayout::eUseProjection: {
            if (isVs) {
              // Not a thing in VS
              resultOp.addOperand(m_builder.makeConstant(false));
            } else {
              auto proj = emitSpecConstantLoad(offsetof(D3D9SpecData, samplerProjMask),
                0u, 1u, "samplerProjection", ref, ir::ScalarType::eBool, index);
              auto cond = m_builder.addBefore(ref, ir::Op::ULt(ir::ScalarType::eBool, index, m_builder.makeConstant(8u)));
              resultOp.addOperand(m_builder.addBefore(ref, ir::Op::BAnd(ir::ScalarType::eBool, cond, proj)));
            }
          } break;

          case ir::LegacySamplerStateLayout::eIsNull: {
            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::IEq(
              ir::ScalarType::eBool, samplerType, m_builder.makeConstant(3u))));
          } break;

          case ir::LegacySamplerStateLayout::eUseGather: {
            if (isVs) {
              // Not a thing in VS
              resultOp.addOperand(m_builder.makeConstant(false));
            } else {
              resultOp.addOperand(m_builder.addBefore(ref, ir::Op::IEq(ir::ScalarType::eBool,
                samplerMode, m_builder.makeConstant(uint32_t(D3D9SamplerMode::Fetch4)))));
            }
          } break;

          case ir::LegacySamplerStateLayout::eDrefClamp: {
            resultOp.addOperand(m_builder.addBefore(ref, ir::Op::IEq(ir::ScalarType::eBool,
              samplerMode, m_builder.makeConstant(uint32_t(D3D9SamplerMode::DrefClamp)))));
          } break;

          case ir::LegacySamplerStateLayout::eDrefScale: {
            auto drefShift = emitSpecConstantLoad(offsetof(D3D9SpecData, drefScale),
              0u, 8u, "samplerDrefScaleBits", ref, ir::ScalarType::eU32, ir::SsaDef());
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
        .addStructMember(ir::ScalarType::eU32)      // constant color
        .addStructMember(ir::ScalarType::eU32)      // padding
        .addStructMember(ir::ScalarType::eF32, 2u)  // matrix row 0
        .addStructMember(ir::ScalarType::eF32, 2u)  // matrix row 1
        .addStructMember(ir::ScalarType::eF32)      // luminance scale
        .addStructMember(ir::ScalarType::eF32)      // luminance offset
        .addArrayDimension(caps::TextureStageCount);

      if (!m_textureStageCbv) {
        m_textureStageCbv = m_builder.add(ir::Op::DclCbv(textureStageCbvType,
          m_entryPoint, 0u, D3D9ShaderResourceMapping::CbvIndex::PSShared, 1u)
          .setFlags(ir::OpFlag::eInBounds));

        m_builder.add(ir::Op::DebugName(m_textureStageCbv, "textureStages"));
        m_builder.add(ir::Op::DebugMemberName(m_textureStageCbv, 0u, "constant"));
        m_builder.add(ir::Op::DebugMemberName(m_textureStageCbv, 1u, "reserved"));
        m_builder.add(ir::Op::DebugMemberName(m_textureStageCbv, 2u, "bumpMat0"));
        m_builder.add(ir::Op::DebugMemberName(m_textureStageCbv, 3u, "bumpMat1"));
        m_builder.add(ir::Op::DebugMemberName(m_textureStageCbv, 4u, "bumpLScale"));
        m_builder.add(ir::Op::DebugMemberName(m_textureStageCbv, 5u, "bumpLOffset"));
      }

      ir::Op resultOp = ir::Op(ir::OpCode::eCompositeConstruct,
        ir::makeLegacyTextureStageType(0u));

      for (uint32_t i = 0u; i < resultOp.getType().getStructMemberCount(); i++) {
        auto member = [i] {
          switch (ir::LegacyTextureStageLayout(i)) {
            case ir::LegacyTextureStageLayout::eBumpMat0: return 2u;
            case ir::LegacyTextureStageLayout::eBumpMat1: return 3u;
            case ir::LegacyTextureStageLayout::eBumpScale: return 4u;
            case ir::LegacyTextureStageLayout::eBumpOffset: return 5u;
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

    dxbc_spv::ir::SsaDef emitDecodePointSize(dxbc_spv::ir::SsaDef ref, dxbc_spv::ir::SsaDef size) {
      using namespace dxbc_spv;

      auto value = m_builder.addBefore(ref, ir::Op::ConvertItoF(ir::ScalarType::eF32, size));
      return m_builder.addBefore(ref, ir::Op::FDiv(ir::ScalarType::eF32, value, m_builder.makeConstant(8.0f)));
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

    static dxbc_spv::ir::ShaderStageMask getPushDataStage(VkShaderStageFlags flags) {
      using namespace dxbc_spv;

      if (flags == VK_SHADER_STAGE_VERTEX_BIT)
        return ir::ShaderStage::eVertex;

      if (flags == VK_SHADER_STAGE_FRAGMENT_BIT)
        return ir::ShaderStage::ePixel;

      return ir::ShaderStageMask();
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
#
}
