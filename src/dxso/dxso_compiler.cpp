#include "dxso_compiler.h"

#include "../d3d9/d3d9_constant_set.h"
#include "dxso_util.h"

namespace dxvk {

  DxsoCompiler::DxsoCompiler(
    const std::string&     fileName,
    const DxsoModuleInfo&  moduleInfo,
    const DxsoProgramInfo& programInfo)
    : m_moduleInfo{ moduleInfo }
    , m_programInfo{ programInfo } {
    // Declare an entry point ID. We'll need it during the
    // initialization phase where the execution mode is set.
    m_entryPointId = m_module.allocateId();

    // Set the shader name so that we recognize it in renderdoc
    m_module.setDebugSource(
      spv::SourceLanguageUnknown, 0,
      m_module.addDebugString(fileName.c_str()),
      nullptr);

    // Set the memory model. This is the same for all shaders.
    m_module.setMemoryModel(
      spv::AddressingModelLogical,
      spv::MemoryModelGLSL450);


    // Make sure our interface registers are clear
    for (uint32_t i = 0; i < 16; i++) {
      m_vDecls.at(i) = DxsoDeclaration{ };
      m_oDecls.at(i) = DxsoDeclaration{ };
      m_oPtrs.at(i) = 0;
    }

    for (uint32_t i = 0; i < m_samplers.size(); i++)
      m_samplers.at(i) = 0;

    this->emitInit();
  }

  void DxsoCompiler::processInstruction(
    const DxsoInstructionContext& ctx) {
    const DxsoOpcode opcode = ctx.instruction.opcode();

    switch (opcode) {
    case DxsoOpcode::Nop:
      return;

    case DxsoOpcode::Dcl:
      return this->emitDcl(ctx);

    case DxsoOpcode::Def:
    case DxsoOpcode::DefI:
    case DxsoOpcode::DefB:
      return this->emitDef(opcode, ctx);

    case DxsoOpcode::Mov:
    case DxsoOpcode::Mova:
    case DxsoOpcode::Add:
    case DxsoOpcode::Sub:
    case DxsoOpcode::Mad:
    case DxsoOpcode::Mul:
    case DxsoOpcode::Rcp:
    case DxsoOpcode::Rsq:
    case DxsoOpcode::Dp3:
    case DxsoOpcode::Dp4:
    case DxsoOpcode::Slt:
    case DxsoOpcode::Sge:
    case DxsoOpcode::Min:
    case DxsoOpcode::ExpP:
    case DxsoOpcode::Exp:
    case DxsoOpcode::Max:
    case DxsoOpcode::Pow:
    case DxsoOpcode::Abs:
    case DxsoOpcode::Nrm:
    case DxsoOpcode::LogP:
    case DxsoOpcode::Log:
    case DxsoOpcode::Lrp:
    case DxsoOpcode::Frc:
    case DxsoOpcode::Cmp:
    case DxsoOpcode::Dp2Add:
      return this->emitVectorAlu(ctx);

    case DxsoOpcode::Tex:
    case DxsoOpcode::TexLdl:
      return this->emitTextureSample(ctx);

    default:
      Logger::warn(str::format("DxsoCompiler::processInstruction: unhandled opcode: ", opcode));
      break;
    }
  }

  Rc<DxvkShader> DxsoCompiler::finalize() {
    for (uint32_t i = 0; i < 32; i++) {
      if (m_interfaceSlots.outputSlots & (1u << i)) {
        emitDebugName(m_oPtrs[i], m_oDecls[i].id);
        m_module.opStore(m_oPtrs[i], getSpirvRegister(m_oDecls[i].id, false, nullptr).varId);
      }
    }

    if (m_programInfo.type() == DxsoProgramType::VertexShader)
      this->emitVsFinalize();
    else
      this->emitPsFinalize();

    // Declare the entry point, we now have all the
    // information we need, including the interfaces
    m_module.addEntryPoint(m_entryPointId,
      m_programInfo.executionModel(), "main",
      m_entryPointInterfaces.size(),
      m_entryPointInterfaces.data());
    m_module.setDebugName(m_entryPointId, "main");

    DxvkShaderOptions shaderOptions = { };

    DxvkShaderConstData constData = { };

    // Create the shader module object
    return new DxvkShader(
      m_programInfo.shaderStage(),
      m_resourceSlots.size(),
      m_resourceSlots.data(),
      m_interfaceSlots,
      m_module.compile(),
      shaderOptions,
      std::move(constData));
  }

  void DxsoCompiler::emitVsFinalize() {
    this->emitMainFunctionBegin();
    m_module.opFunctionCall(
      m_module.defVoidType(),
      m_vs.functionId, 0, nullptr);
    this->emitFunctionEnd();
  }

  void DxsoCompiler::emitPsFinalize() {
    this->emitMainFunctionBegin();

    m_module.opFunctionCall(
      m_module.defVoidType(),
      m_ps.functionId, 0, nullptr);

    /*if (m_ps.killState != 0) {
      DxbcConditional cond;
      cond.labelIf = m_module.allocateId();
      cond.labelEnd = m_module.allocateId();

      uint32_t killTest = m_module.opLoad(m_module.defBoolType(), m_ps.killState);

      m_module.opSelectionMerge(cond.labelEnd, spv::SelectionControlMaskNone);
      m_module.opBranchConditional(killTest, cond.labelIf, cond.labelEnd);

      m_module.opLabel(cond.labelIf);
      m_module.opKill();

      m_module.opLabel(cond.labelEnd);
    }*/

    //this->emitOutputMapping();
    this->emitOutputDepthClamp();
    this->emitFunctionEnd();
  }

  void DxsoCompiler::emitOutputDepthClamp() {
    // HACK: Some drivers do not clamp FragDepth to [minDepth..maxDepth]
    // before writing to the depth attachment, but we do not have acccess
    // to those. Clamp to [0..1] instead.
    /*if (m_ps.builtinDepth) {
      DxbcRegisterPointer ptr;
      ptr.type = { DxbcScalarType::Float32, 1 };
      ptr.id = m_ps.builtinDepth;

      DxbcRegisterValue value = emitValueLoad(ptr);

      value.id = m_module.opFClamp(
        getVectorTypeId(ptr.type),
        value.id,
        m_module.constf32(0.0f),
        m_module.constf32(1.0f));

      emitValueStore(ptr, value,
        DxbcRegMask::firstN(1));
    }*/
  }

  void DxsoCompiler::emitInit() {
    // Set up common capabilities for all shaders
    m_module.enableCapability(spv::CapabilityShader);
    m_module.enableCapability(spv::CapabilityImageQuery);

    this->emitDclConstantBuffer();

    if (m_programInfo.type() == DxsoProgramType::VertexShader)
      this->emitVsInit();
    else
      this->emitPsInit();
  }

  void DxsoCompiler::emitDclConstantBuffer() {
    const uint32_t floatArray   = m_module.defArrayTypeUnique(getTypeId(DxsoRegisterType::Const),     m_module.constu32(256));
    const uint32_t intArray     = m_module.defArrayTypeUnique(getTypeId(DxsoRegisterType::ConstInt),  m_module.constu32(16));
    const uint32_t boolBitfield = m_module.defIntType(32, false);

    std::array<uint32_t, 3> constantTypes{ floatArray, intArray, boolBitfield };

    m_module.decorateArrayStride(floatArray, 16);
    m_module.decorateArrayStride(intArray,   16);

    const uint32_t structType = m_module.defStructTypeUnique(constantTypes.size(), constantTypes.data());

    const uint32_t floatArraySize = 4 * sizeof(float)    * 256;
    const uint32_t intArraySize   = 4 * sizeof(uint32_t) * 16;

    m_module.decorateBlock(structType);
    m_module.memberDecorateOffset(structType, 0, 0);
    m_module.memberDecorateOffset(structType, 1, floatArraySize);
    m_module.memberDecorateOffset(structType, 2, floatArraySize + intArraySize);

    m_module.setDebugName(structType, "cBuffer");
    m_module.setDebugMemberName(structType, 0, "f");
    m_module.setDebugMemberName(structType, 1, "i");
    m_module.setDebugMemberName(structType, 2, "b");

    m_cBuffer = m_module.newVar(
      m_module.defPointerType(structType, spv::StorageClassUniform),
      spv::StorageClassUniform);

    m_module.setDebugName(m_cBuffer, "c");

    const uint32_t bindingId = computeResourceSlotId(
      m_programInfo.type(), DxsoBindingType::ConstantBuffer,
      0);

    m_module.decorateDescriptorSet(m_cBuffer, 0);
    m_module.decorateBinding(m_cBuffer, bindingId);

    DxvkResourceSlot resource;
    resource.slot   = bindingId;
    resource.type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    resource.view   = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    resource.access = VK_ACCESS_UNIFORM_READ_BIT;
    m_resourceSlots.push_back(resource);
  }

  void DxsoCompiler::emitVsInit() {
    m_module.enableCapability(spv::CapabilityDrawParameters);

    m_module.enableExtension("SPV_KHR_shader_draw_parameters");

    // Main function of the vertex shader
    m_vs.functionId = m_module.allocateId();
    m_module.setDebugName(m_vs.functionId, "vs_main");

    this->emitFunctionBegin(
      m_vs.functionId,
      m_module.defVoidType(),
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr));
    this->emitFunctionLabel();
  }

  void DxsoCompiler::emitPsInit() {
    m_module.enableCapability(spv::CapabilityDerivativeControl);

    m_module.setExecutionMode(m_entryPointId,
      spv::ExecutionModeOriginUpperLeft);

    // Main function of the pixel shader
    m_ps.functionId = m_module.allocateId();
    m_module.setDebugName(m_ps.functionId, "ps_main");

    this->emitFunctionBegin(
      m_ps.functionId,
      m_module.defVoidType(),
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr));
    this->emitFunctionLabel();

    // We may have to defer kill operations to the end of
    // the shader in order to keep derivatives correct.
    /*if (m_analysis->usesKill && m_analysis->usesDerivatives) {
      m_ps.killState = m_module.newVarInit(
        m_module.defPointerType(m_module.defBoolType(), spv::StorageClassPrivate),
        spv::StorageClassPrivate, m_module.constBool(false));

      m_module.setDebugName(m_ps.killState, "ps_kill");

      if (m_moduleInfo.options.useSubgroupOpsForEarlyDiscard) {
        m_module.enableCapability(spv::CapabilityGroupNonUniform);
        m_module.enableCapability(spv::CapabilityGroupNonUniformBallot);

        DxbcRegisterInfo invocationMask;
        invocationMask.type = { DxbcScalarType::Uint32, 4, 0 };
        invocationMask.sclass = spv::StorageClassFunction;

        m_ps.invocationMask = emitNewVariable(invocationMask);
        m_module.setDebugName(m_ps.invocationMask, "fInvocationMask");

        m_module.opStore(m_ps.invocationMask,
          m_module.opGroupNonUniformBallot(
            getVectorTypeId({ DxbcScalarType::Uint32, 4 }),
            m_module.constu32(spv::ScopeSubgroup),
            m_module.constBool(true)));
      }
    }*/
  }

  void DxsoCompiler::emitFunctionBegin(
    uint32_t                entryPoint,
    uint32_t                returnType,
    uint32_t                funcType) {
    this->emitFunctionEnd();

    m_module.functionBegin(
      returnType, entryPoint, funcType,
      spv::FunctionControlMaskNone);

    m_insideFunction = true;
  }

  void DxsoCompiler::emitFunctionEnd() {
    if (m_insideFunction) {
      m_module.opReturn();
      m_module.functionEnd();
    }
    
    m_insideFunction = false;
  }

  void DxsoCompiler::emitFunctionLabel() {
    m_module.opLabel(m_module.allocateId());
  }

  void DxsoCompiler::emitMainFunctionBegin() {
    this->emitFunctionBegin(
      m_entryPointId,
      m_module.defVoidType(),
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr));
    this->emitFunctionLabel();
  }

  uint32_t DxsoCompiler::emitNewVariable(DxsoRegisterType regType, spv::StorageClass storageClass) {
    return m_module.newVar(
      getPointerTypeId(regType, storageClass),
      storageClass);
  }

  uint32_t DxsoCompiler::emitRegisterSwizzle(uint32_t typeId, uint32_t varId, DxsoRegSwizzle swizzle, uint32_t count) {
    if (swizzle == IdentitySwizzle && count == 4)
      return varId;

    std::array<uint32_t, 4> indices = { 0,0,0,0 };

    for (uint32_t i = 0; i < count; i++)
      indices[i] = swizzle[i];

    if (count == 1)
      return m_module.opCompositeExtract(typeId, varId, 1, indices.data());
    else
      return m_module.opVectorShuffle(typeId, varId, varId, count, indices.data());
  }

  uint32_t DxsoCompiler::emitVecTrunc(uint32_t typeId, uint32_t varId, uint32_t count) {
    std::array<uint32_t, 4> identityShuffle = { 0, 1, 2, 3 };

    return m_module.opVectorShuffle(typeId, varId, varId, count, identityShuffle.data());
  }

  uint32_t DxsoCompiler::emitSrcOperandModifier(uint32_t typeId, uint32_t varId, DxsoRegModifier modifier, uint32_t count) {
    uint32_t result = varId;

    // 1 - r
    if (modifier == DxsoRegModifier::Comp) {
      uint32_t vec = m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f);
      vec = emitVecTrunc(typeId, vec, count);

      result = m_module.opFSub(typeId, vec, varId);
    }

    // r * 2
    if (modifier == DxsoRegModifier::X2
     || modifier == DxsoRegModifier::X2Neg) {
      uint32_t vec2 = m_module.constvec4f32(2.0f, 2.0f, 2.0f, 2.0f);
      vec2 = emitVecTrunc(typeId, vec2, count);

      result = m_module.opFMul(typeId, vec2, varId);
    }

    // abs( r )
    if (modifier == DxsoRegModifier::Abs
     || modifier == DxsoRegModifier::AbsNeg) {
      result = m_module.opFAbs(typeId, varId);
    }

    // !r
    if (modifier == DxsoRegModifier::Not) {
      uint32_t one = m_module.constBool(true);
      result = m_module.opBitwiseXor(typeId, varId, one);
    }

    // r / r.z
    // r / r.w
    if (modifier == DxsoRegModifier::Dz
     || modifier == DxsoRegModifier::Dw) {
      const uint32_t index = modifier == DxsoRegModifier::Dz ? 2 : 3;

      std::array<uint32_t, 4> indices = { index, index, index, index };

      uint32_t component = m_module.opVectorShuffle(typeId, result, result, 4, indices.data());
      result = m_module.opFDiv(typeId, result, component);
    }

    // -r
    // Treating as -r
    // Treating as -r
    // -r * 2
    // -abs(r)
    if (modifier == DxsoRegModifier::Neg
     || modifier == DxsoRegModifier::BiasNeg
     || modifier == DxsoRegModifier::SignNeg
     || modifier == DxsoRegModifier::X2Neg
     || modifier == DxsoRegModifier::AbsNeg) {
      result = m_module.opFNegate(typeId, result);
    }

    return result;
  }

  uint32_t DxsoCompiler::emitRegisterLoad(const DxsoRegister& reg, uint32_t count) {
    const uint32_t typeId = spvType(reg, count);

    uint32_t result = spvId(reg);

    result = emitRegisterSwizzle(typeId, result, reg.swizzle(), count);
    result = emitSrcOperandModifier(typeId, result, reg.modifier(), count);

    return result;
  }

  uint32_t DxsoCompiler::emitDstOperandModifier(uint32_t typeId, uint32_t varId, bool saturate, bool partialPrecision) {
    uint32_t result = varId;

    if (saturate) {
      uint32_t vec0 = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);
      uint32_t vec1 = m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f);

      result = m_module.opFClamp(typeId, result, vec0, vec1);
    }

    // Partial precision is currently ignored.
    // I'll wait for issues when some game needs this for some stupid assumption
    // about crap being truncated until this gets implemented.

    return result;
  }

  uint32_t DxsoCompiler::emitWriteMask(uint32_t typeId, uint32_t dst, uint32_t src, DxsoRegMask writeMask) {
    if (writeMask == IdentityWriteMask)
      return src;
    
    std::array<uint32_t, 4> components;
    for (uint32_t i = 0; i < 4; i++)
      components[i] = writeMask[i] ? i + 4 : i;

    return m_module.opVectorShuffle(typeId, dst, src, 4, components.data());
  }

  void DxsoCompiler::emitDebugName(uint32_t varId, DxsoRegisterId id) {
    bool ps = m_programInfo.type() == DxsoProgramType::PixelShader;

    std::string name = "";
    switch (id.type()) {
      case DxsoRegisterType::Temp:          name = str::format("r", id.num()); break;
      case DxsoRegisterType::Input:         name = str::format("v", id.num()); break;
      case DxsoRegisterType::Addr:          name = str::format(ps ? "t" : "a", id.num()); break;
      case DxsoRegisterType::RasterizerOut: {
        std::string suffix = "Unknown";
        if (id.num() == RasterOutPosition)
          suffix = "Pos";
        else if (id.num() == RasterOutFog)
          suffix = "Fog";
        else
          suffix = "Psize";
        name = str::format("o", suffix);
        break;
      }
      case DxsoRegisterType::AttributeOut:  name = str::format("oC", id.num()); break;
      case DxsoRegisterType::Output:        name = str::format("o", id.num()); break;
      case DxsoRegisterType::ColorOut:      name = str::format("oC", id.num()); break;
      case DxsoRegisterType::DepthOut:      name = str::format("oDepth", id.num()); break;
      case DxsoRegisterType::Loop:          name = str::format("l", id.num()); break;
      case DxsoRegisterType::TempFloat16:   name = str::format("h", id.num()); break;
      case DxsoRegisterType::MiscType:      name = str::format("m", id.num()); break;
      case DxsoRegisterType::Predicate:     name = str::format("p", id.num()); break;
    }

    if (!name.empty())
      m_module.setDebugName(varId, name.c_str());
  }

  uint32_t DxsoCompiler::emitScalarReplicant(uint32_t vectorTypeId, uint32_t varId) {
    std::array<uint32_t, 4> replicantIndices = { varId, varId, varId, varId };
    return m_module.opCompositeConstruct(vectorTypeId, replicantIndices.size(), replicantIndices.data());
  }

  void DxsoCompiler::emitVectorAlu(const DxsoInstructionContext& ctx) {
    const auto& dst = ctx.dst;
    const auto& src = ctx.src;

    const uint32_t typeId = spvType(dst);

    const auto opcode = ctx.instruction.opcode();
    uint32_t result;
    switch (opcode) {
      case DxsoOpcode::Mov:
        result = emitRegisterLoad(src[0]);
        break;
      case DxsoOpcode::Mova: {
        if (src[0].registerId().type() != DxsoRegisterType::ConstInt) {
          result = m_module.opRound(
            m_module.defVectorType(m_module.defFloatType(32), 4),
            emitRegisterLoad(src[0]));

          result = m_module.opConvertFtoS(typeId, result);
        }
        else {
          result = emitRegisterLoad(src[0]);
        }

        break;
      }
      case DxsoOpcode::Add:
        result = m_module.opFAdd(typeId, emitRegisterLoad(src[0]), emitRegisterLoad(src[1]));
        break;
      case DxsoOpcode::Sub:
        result = m_module.opFSub(typeId, emitRegisterLoad(src[0]), emitRegisterLoad(src[1]));
        break;
      case DxsoOpcode::Mad:
        result = m_module.opFFma(
          typeId,
          emitRegisterLoad(src[0]),
          emitRegisterLoad(src[1]),
          emitRegisterLoad(src[2]));
        break;
      case DxsoOpcode::Mul:
        result = m_module.opFMul(typeId, emitRegisterLoad(src[0]), emitRegisterLoad(src[1]));
        break;
      case DxsoOpcode::Rcp:
        result = m_module.opFDiv(typeId,
          m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f),
          emitRegisterLoad(src[0]));
        break;
      case DxsoOpcode::Rsq:
        result = m_module.opInverseSqrt(typeId, emitRegisterLoad(src[0]));
        break;
      case DxsoOpcode::Dp3: {
        const uint32_t scalarTypeId = spvType(dst, 1);

        result = m_module.opDot(scalarTypeId, emitRegisterLoad(src[0], 3), emitRegisterLoad(src[1], 3));
        result = this->emitScalarReplicant(typeId, result);
        break;
      }
      case DxsoOpcode::Dp4: {
        const uint32_t scalarTypeId = spvType(dst, 1);

        result = m_module.opDot(scalarTypeId, emitRegisterLoad(src[0]), emitRegisterLoad(src[1]));
        result = this->emitScalarReplicant(typeId, result);
        break;
      }
      case DxsoOpcode::Slt:
      case DxsoOpcode::Sge: {
        const uint32_t boolVec = m_module.defVectorType(m_module.defBoolType(), 4);

        result = opcode == DxsoOpcode::Slt
          ? m_module.opFOrdLessThan   (boolVec, emitRegisterLoad(src[0]), emitRegisterLoad(src[1]))
          : m_module.opFOrdGreaterThan(boolVec, emitRegisterLoad(src[0]), emitRegisterLoad(src[1]));

        result = m_module.opSelect(typeId, result, m_module.constvec4f32(1, 1, 1, 1), m_module.constvec4f32(0, 0, 0, 0));
        break;
      }
      case DxsoOpcode::Min:
        result = m_module.opFMin(typeId, emitRegisterLoad(src[0]), emitRegisterLoad(src[1]));
        break;
      case DxsoOpcode::Max:
        result = m_module.opFMax(typeId, emitRegisterLoad(src[0]), emitRegisterLoad(src[1]));
        break;
      case DxsoOpcode::ExpP:
      case DxsoOpcode::Exp:
        result = m_module.opExp2(typeId, emitRegisterLoad(src[0]));
        break;
      case DxsoOpcode::Pow:
        result = m_module.opPow(typeId, emitRegisterLoad(src[0]), emitRegisterLoad(src[1]));
        break;
      case DxsoOpcode::Abs:
        result = m_module.opFAbs(typeId, emitRegisterLoad(src[0]));
        break;
      case DxsoOpcode::Nrm: {
        // Nrm is 3D...
        const uint32_t scalarTypeId = spvType(dst, 1);

        uint32_t vec3 = emitRegisterLoad(src[0], 3);

        result = m_module.opDot(scalarTypeId, vec3, vec3);
        result = m_module.opInverseSqrt(scalarTypeId, result);
        result = emitScalarReplicant(typeId, result);

        // r * rsq(r . r);
        result = m_module.opFMul(
          typeId,
          emitRegisterLoad(src[0]),
          result);
      } break;
      case DxsoOpcode::LogP:
      case DxsoOpcode::Log:
        result = m_module.opLog2(typeId, emitRegisterLoad(src[0]));
        break;
      case DxsoOpcode::Lrp: {
        uint32_t src0 = emitRegisterLoad(src[0]);
        uint32_t src1 = emitRegisterLoad(src[1]);
        uint32_t src2 = emitRegisterLoad(src[2]);
        // We are implementing like:
        // src2 + src0 * (src1 - src2)

        // X = src1 - src2
        result = m_module.opFSub(typeId, src1, src2);
        // result = src2 + src0 * X
        result = m_module.opFFma(typeId, src0, result, src2);
        break;
      }
      case DxsoOpcode::Frc:
        result = m_module.opFract(typeId, emitRegisterLoad(src[0]));
        break;
      case DxsoOpcode::Cmp: {
        const uint32_t boolVec = m_module.defVectorType(m_module.defBoolType(), 4);

        result = m_module.opFOrdGreaterThanEqual(
          boolVec,
          emitRegisterLoad(src[0]),
          m_module.constvec4f32(0, 0, 0, 0));

        result = m_module.opSelect(
          typeId,
          result,
          emitRegisterLoad(src[1]),
          emitRegisterLoad(src[2]));
        break;
      }
      case DxsoOpcode::Dp2Add: {
        const uint32_t scalarTypeId = spvType(dst, 1);

        result = m_module.opDot(scalarTypeId, emitRegisterLoad(src[0], 2), emitRegisterLoad(src[1], 2));
        result = m_module.opFAdd(scalarTypeId, result, emitRegisterLoad(src[2], 1));
        result = this->emitScalarReplicant(typeId, result);
        break;
      }
      default:
        Logger::warn(str::format("DxsoCompiler::emitVectorAlu: unimplemented op ", opcode));
        return;
    }

    result = emitDstOperandModifier(typeId, result, dst.saturate(), dst.partialPrecision());

    auto& dstSpvReg = getSpirvRegister(dst);
    dstSpvReg.varId = emitWriteMask(typeId, dstSpvReg.varId, result, dst.writeMask());
    emitDebugName(dstSpvReg.varId, dst.registerId());
  }

  void DxsoCompiler::emitTextureSample(const DxsoInstructionContext& ctx) {
    const auto& dst = ctx.dst;

    const uint32_t typeId = spvType(dst);

    uint32_t texcoordVarId;
    uint32_t samplerIdx;

    if (m_programInfo.majorVersion() >= 2) { // SM 2.0+
      texcoordVarId = emitRegisterLoad(ctx.src[0]);
      samplerIdx = ctx.src[1].registerId().num();
    } else if (
      m_programInfo.majorVersion() == 1
   && m_programInfo.minorVersion() == 4) { // SM 1.4
      texcoordVarId = emitRegisterLoad(ctx.src[0]);
      samplerIdx = ctx.dst.registerId().num();
    }
    else { // SM 1.0-1.3
      DxsoRegisterId texcoordId = { DxsoRegisterType::TexcoordOut, ctx.dst.registerId().num() };

      texcoordVarId = getSpirvRegister(texcoordId, ctx.dst.centroid(), nullptr).varId;
      samplerIdx = ctx.dst.registerId().num();
    }

    if (m_samplers.at(samplerIdx) == 0) {
      Logger::warn("DxsoCompiler::emitTextureSample: Adding implicit 2D sampler");
      emitDclSampler(samplerIdx, DxsoTextureType::Texture2D);
    }

    uint32_t imageVarId = m_module.opSampledImage(
      m_module.defSampledImageType(m_textureTypes.at(samplerIdx)),

      m_module.opLoad(m_textureTypes.at(samplerIdx), m_textures.at(samplerIdx)),
      m_module.opLoad(m_module.defSamplerType(),     m_samplers.at(samplerIdx)));

    SpirvImageOperands imageOperands;
    if (m_programInfo.type() == DxsoProgramType::VertexShader) {
      imageOperands.sLod = m_module.constf32(0.0f);
      imageOperands.flags |= spv::ImageOperandsLodMask;
    }

    uint32_t result =
      m_programInfo.type() == DxsoProgramType::PixelShader
      ? m_module.opImageSampleImplicitLod(
        typeId,
        imageVarId,
        texcoordVarId,
        imageOperands)
      : m_module.opImageSampleExplicitLod(
        typeId,
        imageVarId,
        texcoordVarId,
        imageOperands);

    result = emitDstOperandModifier(typeId, result, dst.saturate(), dst.partialPrecision());
    auto& dstSpvReg = getSpirvRegister(dst);
    dstSpvReg.varId = emitWriteMask(typeId, dstSpvReg.varId, result, dst.writeMask());
    emitDebugName(dstSpvReg.varId, dst.registerId());
  }

  void DxsoCompiler::emitDclSampler(uint32_t idx, DxsoTextureType type) {
    // Sampler Setup
    {
      const uint32_t samplerType = m_module.defSamplerType();
      const uint32_t samplerPtrType = m_module.defPointerType(
        samplerType, spv::StorageClassUniformConstant);

      const uint32_t varId = m_module.newVar(samplerPtrType,
        spv::StorageClassUniformConstant);

      m_samplers.at(idx) = varId;

      const uint32_t bindingId = computeResourceSlotId(
        m_programInfo.type(), DxsoBindingType::ImageSampler, idx);

      m_module.decorateDescriptorSet(varId, 0);
      m_module.decorateBinding(varId, bindingId);

      DxvkResourceSlot resource;
      resource.slot   = bindingId;
      resource.type   = VK_DESCRIPTOR_TYPE_SAMPLER;
      resource.view   = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
      resource.access = 0;
      m_resourceSlots.push_back(resource);
    }

    // Resource Setup
    {
      spv::Dim dimensionality;
      VkImageViewType viewType;

      switch (type) {
        default:
        case DxsoTextureType::Texture2D:
          dimensionality = spv::Dim2D;
          viewType = VK_IMAGE_VIEW_TYPE_2D;
          break;
        case DxsoTextureType::TextureCube:
          dimensionality = spv::DimCube;
          viewType = VK_IMAGE_VIEW_TYPE_CUBE;
          m_module.enableCapability(
            spv::CapabilitySampledCubeArray);
          break;
        case DxsoTextureType::Texture3D:
          dimensionality = spv::Dim3D;
          viewType = VK_IMAGE_VIEW_TYPE_3D;
          break;
      }

      const uint32_t imageTypeId = m_module.defImageType(m_module.defFloatType(32),
        dimensionality, 0, 0, 0, 1,
        spv::ImageFormatR32f);

      const uint32_t resourcePtrType = m_module.defPointerType(
        imageTypeId, spv::StorageClassUniformConstant);

      const uint32_t varId = m_module.newVar(resourcePtrType,
        spv::StorageClassUniformConstant);

      m_textures.at(idx) = varId;
      m_textureTypes.at(idx) = imageTypeId;

      const uint32_t bindingId = computeResourceSlotId(
        m_programInfo.type(), DxsoBindingType::Image, idx);

      m_module.decorateDescriptorSet(varId, 0);
      m_module.decorateBinding(varId, bindingId);

      // Store descriptor info for the shader interface
      DxvkResourceSlot resource;
      resource.slot   = bindingId;
      resource.type   = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      resource.view   = viewType;
      resource.access = VK_ACCESS_SHADER_READ_BIT;
      m_resourceSlots.push_back(resource);
    }
  }

  void DxsoCompiler::emitDcl(const DxsoInstructionContext& ctx) {
    const auto type    = ctx.dst.registerId().type();

    const bool input   = type == DxsoRegisterType::Input
                      || type == DxsoRegisterType::Texture;

    auto dcl = ctx.dcl;
    dcl.id = ctx.dst.registerId();

    if (type == DxsoRegisterType::Input) {
      if (m_programInfo.majorVersion() != 3 && m_programInfo.type() == DxsoProgramType::PixelShader)
        return;
    }
    else if (type == DxsoRegisterType::Texture) {
      dcl.semantic.usage      = DxsoUsage::Texcoord;
      dcl.semantic.usageIndex = ctx.dst.registerId().num();
    }

    const bool sampler = type == DxsoRegisterType::Sampler;

    if (sampler) {
      this->emitDclSampler(ctx.dst.registerId().num(), ctx.dcl.textureType);
      return;
    }

    mapSpirvRegister(ctx.dst.registerId(), ctx.dst.centroid(), nullptr, &dcl);
  }

  void DxsoCompiler::emitDef(DxsoOpcode opcode, const DxsoInstructionContext& ctx) {
    switch (opcode) {
      case DxsoOpcode::Def:  this->emitDefF(ctx); break;
      case DxsoOpcode::DefI: this->emitDefI(ctx); break;
      case DxsoOpcode::DefB: this->emitDefB(ctx); break;
      default:
        throw DxvkError("DxsoCompiler::emitDef: Invalid definition opcode");
        break;
    }
  }

  void DxsoCompiler::emitDefF(const DxsoInstructionContext& ctx) {
    DxsoSpirvRegister reg;
    reg.regId = ctx.dst.registerId();

    const float* data = reinterpret_cast<const float*>(ctx.def.data());
    reg.varId = m_module.constvec4f32(data[0], data[1], data[2], data[3]);

    m_regs.push_back(reg);
  }

  void DxsoCompiler::emitDefI(const DxsoInstructionContext& ctx) {
    DxsoSpirvRegister reg;
    reg.regId = ctx.dst.registerId();

    const auto& data = ctx.def;
    reg.varId = m_module.constvec4i32(data[0], data[1], data[2], data[3]);

    m_regs.push_back(reg);
  }

  void DxsoCompiler::emitDefB(const DxsoInstructionContext& ctx) {
    DxsoSpirvRegister reg;
    reg.regId = ctx.dst.registerId();

    bool data = ctx.def[0] != 0;
    reg.varId = m_module.constBool(data);

    m_regs.push_back(reg);
  }

  DxsoSpirvRegister& DxsoCompiler::getSpirvRegister(DxsoRegisterId id, bool centroid, DxsoRegister* relative) {
    if (!id.constant() || (id.constant() && relative == nullptr)) {
      for (auto& regMapping : m_regs) {
        if (regMapping.regId == id)
          return regMapping;
      }
    }

    return this->mapSpirvRegister(id, centroid, relative, nullptr);
  }

  DxsoSpirvRegister& DxsoCompiler::getSpirvRegister(const DxsoRegister& reg) {
    DxsoRegister relativeReg = reg.relativeRegister();

    return this->getSpirvRegister(
      reg.registerId(),
      reg.centroid(),
      reg.isRelative()
        ? &relativeReg
        : nullptr);
  }

  DxsoSpirvRegister& DxsoCompiler::mapSpirvRegister(DxsoRegisterId id, bool centroid, DxsoRegister* relative, const DxsoDeclaration* optionalPremadeDecl) {
    DxsoSpirvRegister spirvRegister;
    spirvRegister.regId = id;

    uint32_t inputSlot  = InvalidInputSlot;
    uint32_t outputSlot = InvalidOutputSlot;

    spv::BuiltIn builtIn = spv::BuiltInMax;

    if (optionalPremadeDecl != nullptr) {
      const bool input = id.type() == DxsoRegisterType::Input
                      || id.type() == DxsoRegisterType::Texture;

      auto& decl = *optionalPremadeDecl;
      auto& semantic = decl.semantic;

      if (input)
        m_vDecls[inputSlot = allocateSlot(true, id, semantic)]   = decl;
      else {
        m_oDecls[outputSlot = allocateSlot(false, id, semantic)] = decl;

        if (decl.semantic.usage == DxsoUsage::Position)
          builtIn = spv::BuiltInPosition;
        else if (decl.semantic.usage == DxsoUsage::PointSize)
          builtIn = spv::BuiltInPointSize;
      }
    }
    else {
      if (id.type() == DxsoRegisterType::Input) {
        if (m_programInfo.majorVersion() != 3 && m_programInfo.type() == DxsoProgramType::PixelShader) {
          DxsoSemantic semantic = { DxsoUsage::Color, id.num() };

          auto& dcl = m_vDecls[inputSlot = allocateSlot(true, id, semantic)];
          dcl.id = id;
          dcl.semantic = semantic;
        }
      }
      else if (id.type() == DxsoRegisterType::RasterizerOut) {
        DxsoSemantic semantic;

        semantic.usageIndex = 0;
        if (id.num() == RasterOutPosition) {
          semantic.usage = DxsoUsage::Position;
          builtIn = spv::BuiltInPosition;
        }
        else if (id.num() == RasterOutFog)
          semantic.usage = DxsoUsage::Fog;
        else {
          semantic.usage = DxsoUsage::PointSize;
          builtIn = spv::BuiltInPointSize;
        }

        auto& dcl = m_oDecls[outputSlot = allocateSlot(false, id, semantic)];
        dcl.id = id;
        dcl.semantic = semantic;
      }
      else if (id.type() == DxsoRegisterType::Output) { // TexcoordOut
        DxsoSemantic semantic = { DxsoUsage::Texcoord , id.num() };

        auto& dcl = m_oDecls[outputSlot = allocateSlot(false, id, semantic)];
        dcl.id = id;
        dcl.semantic = semantic;
      }
      else if (id.type() == DxsoRegisterType::AttributeOut) {
        DxsoSemantic semantic = { DxsoUsage::Color, id.num() };

        auto& dcl = m_oDecls[outputSlot = allocateSlot(false, id, semantic)];
        dcl.id = id;
        dcl.semantic = semantic;
      }
      else if (id.type() == DxsoRegisterType::Texture) {
        if (m_programInfo.type() == DxsoProgramType::PixelShader) {

          // SM 2+ or 1.4
          if (m_programInfo.majorVersion() >= 2
            || (m_programInfo.majorVersion() == 1
             && m_programInfo.minorVersion() == 4)) {
            DxsoSemantic semantic = { DxsoUsage::Texcoord, id.num() };

            auto& dcl = m_vDecls[inputSlot = allocateSlot(true, id, semantic)];
            dcl.id = id;
            dcl.semantic = semantic;
          }
        }
      }
      else if (id.type() == DxsoRegisterType::ColorOut) {
        DxsoSemantic semantic = { DxsoUsage::Color, id.num() };

        auto& dcl = m_oDecls[outputSlot = allocateSlot(false, id, semantic)];
        dcl.id = id;
        dcl.semantic = semantic;
      }
    }

    const bool input = inputSlot != InvalidInputSlot;
    const bool output = outputSlot != InvalidOutputSlot;

    uint32_t varId = 0;

    if (id.constant()) {
      uint32_t member;
      switch (id.type()) {
        default:
          //Logger::warn(str::format("Unhandled register type: ", regId.type()));
        case DxsoRegisterType::Const:     member = 0;    break;
        case DxsoRegisterType::ConstInt:  member = 1;  break;
        case DxsoRegisterType::ConstBool: member = 2;  break;
      }

      const uint32_t memberId = m_module.consti32(member);

      uint32_t constantIdx = m_module.consti32(id.num());

      uint32_t typeId = id.type() != DxsoRegisterType::ConstBool
        ? getTypeId(id.type())
        : m_module.defIntType(32, false);

      if (relative != nullptr) {
        DxsoRegisterId id = { DxsoRegisterType::Addr, relative->registerId().num() };
        uint32_t r = getSpirvRegister(id, false, nullptr).varId;

        r = emitRegisterSwizzle(m_module.defIntType(32, 1), r, relative->swizzle(), 1);

        constantIdx = m_module.opIAdd(
          m_module.defIntType(32, 1),
          constantIdx, r);
      }

      uint32_t idx = id.type() != DxsoRegisterType::ConstBool
        ? constantIdx
        : m_module.consti32(0);

      const std::array<uint32_t, 2> indices =
      { { memberId, idx } };

      const uint32_t ptrType = m_module.defPointerType(typeId, spv::StorageClassUniform);
      uint32_t regPtr = m_module.opAccessChain(ptrType,
        m_cBuffer,
        indices.size(), indices.data());

      varId = m_module.opLoad(typeId, regPtr);

      if (id.type() == DxsoRegisterType::ConstBool) {
        varId = m_module.opBitFieldUExtract(getTypeId(id.type()), varId, constantIdx, 1);
      }
    } else if (input || output) {
      uint32_t ptrId = this->emitNewVariable(
        id.type(),
        inputSlot != InvalidInputSlot
        ? spv::StorageClassInput
        : spv::StorageClassOutput);

      emitDebugName(ptrId, id);

      if (input) {
        m_module.decorateLocation(ptrId, inputSlot);
        m_entryPointInterfaces.push_back(ptrId);

        if (centroid)
          m_module.decorate(ptrId, spv::DecorationCentroid);

        varId = m_module.opLoad(getTypeId(id.type()), ptrId);
      }
      else {
        m_oPtrs[outputSlot] = ptrId;

        if (builtIn == spv::BuiltInMax) {
          m_module.decorateLocation(ptrId, outputSlot);

          if (m_programInfo.type() == DxsoProgramType::PixelShader)
            m_module.decorateIndex(ptrId, 0);
        }
        m_entryPointInterfaces.push_back(ptrId);
      }

      if (builtIn != spv::BuiltInMax)
        m_module.decorateBuiltIn(ptrId, builtIn);
    }

    if (varId == 0) {
      if ((m_programInfo.type() == DxsoProgramType::VertexShader && id.type() == DxsoRegisterType::Addr)
        || id.type() == DxsoRegisterType::ConstInt)
        varId = m_module.constvec4i32(0, 0, 0, 0);
      else if (id.type() == DxsoRegisterType::Loop)
        varId = m_module.consti32(0);
      else if (id.type() == DxsoRegisterType::ConstBool)
        varId = m_module.constBool(0);
      else
        varId = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);
    }
    else
      emitDebugName(varId, id);

    spirvRegister.varId = varId;

    if (id.constant() && relative != nullptr)
    {
      m_relativeRegs.push_back(spirvRegister);
      return m_relativeRegs[m_relativeRegs.size() - 1];
    }

    m_regs.push_back(spirvRegister);
    return m_regs[m_regs.size() - 1];
  }

  uint32_t DxsoCompiler::getTypeId(DxsoRegisterType regType, uint32_t count) {
    switch (regType) {
    case DxsoRegisterType::Addr: {
      if (m_programInfo.type() == DxsoProgramType::VertexShader) {
        uint32_t intType = m_module.defIntType(32, 1);
        return count > 1 ? m_module.defVectorType(intType, count) : intType;
      }
    }
    case DxsoRegisterType::Temp:
    case DxsoRegisterType::Input:
    case DxsoRegisterType::Const:
    case DxsoRegisterType::RasterizerOut:
    case DxsoRegisterType::AttributeOut:
    case DxsoRegisterType::Output:
    //case DxsoRegisterType::TexcoordOut:
    case DxsoRegisterType::ColorOut:
    case DxsoRegisterType::DepthOut:
    case DxsoRegisterType::Const2:
    case DxsoRegisterType::Const3:
    case DxsoRegisterType::Const4:
    case DxsoRegisterType::TempFloat16:
    case DxsoRegisterType::MiscType: {
      uint32_t floatType = m_module.defFloatType(32);
      return count > 1 ? m_module.defVectorType(floatType, count) : floatType;
    }

    case DxsoRegisterType::ConstInt: {
      uint32_t intType = m_module.defIntType(32, 1);
      return count > 1 ? m_module.defVectorType(intType, count) : intType;
    }

    case DxsoRegisterType::ConstBool:
      return m_module.defBoolType();
    case DxsoRegisterType::Loop:
      return m_module.defIntType(32, true);

    case DxsoRegisterType::Predicate: {
      uint32_t boolType = m_module.defBoolType();
      return count > 1 ? m_module.defVectorType(boolType, count) : boolType;
    }

    case DxsoRegisterType::Label:
    case DxsoRegisterType::Sampler:
      throw DxvkError("DxsoCompiler::getTypeId: Spirv type requested for Label or Sampler");

    default:
      throw DxvkError("DxsoCompiler::getTypeId: Unknown register type");
    }
  }

  // Maps a Usage and Usage Index to an I/O slot for Shader Models less than 3 that don't have general purpose IO registers
  static std::unordered_map<
    DxsoSemantic,
    uint32_t,
    DxsoSemanticHash,
    DxsoSemanticEq> g_transientMappings = {
      {{DxsoUsage::Position,   0}, 0},

      {{DxsoUsage::Texcoord,   0}, 1},
      {{DxsoUsage::Texcoord,   1}, 2},
      {{DxsoUsage::Texcoord,   2}, 3},
      {{DxsoUsage::Texcoord,   3}, 4},
      {{DxsoUsage::Texcoord,   4}, 5},
      {{DxsoUsage::Texcoord,   5}, 6},
      {{DxsoUsage::Texcoord,   6}, 7},
      {{DxsoUsage::Texcoord,   7}, 8},

      {{DxsoUsage::Color,      0}, 9},
      {{DxsoUsage::Color,      1}, 10},

      {{DxsoUsage::Fog,        0}, 11},
      {{DxsoUsage::PointSize,  0}, 12},
  };

  uint32_t DxsoCompiler::allocateSlot(bool input, DxsoRegisterId id, DxsoSemantic semantic) {
    uint32_t slot;

    bool transient = (input  && m_programInfo.type() == DxsoProgramType::PixelShader)
                  || (!input && m_programInfo.type() == DxsoProgramType::VertexShader);

    transient = transient && m_programInfo.majorVersion() < 3;

    if (!transient)
      slot = id.num();
    else {
      auto idx = g_transientMappings.find(semantic);
      if (idx != g_transientMappings.end()) {
        slot = idx->second;
      } else {
        slot = 0;
        //Logger::warn(str::format("Could not find transient mapping for DxsoSemantic{", semantic.usage, ", ", semantic.usageIndex, "}"));
      }
    }

    input
    ? m_interfaceSlots.inputSlots  |= 1u << slot
    : m_interfaceSlots.outputSlots |= 1u << slot;

    return slot;
  }

}