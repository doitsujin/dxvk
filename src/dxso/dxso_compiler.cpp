#include "dxso_compiler.h"

#include "../d3d9/d3d9_caps.h"
#include "../d3d9/d3d9_constant_set.h"
#include "../d3d9/d3d9_state.h"
#include "dxso_util.h"
#include <cfloat>

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
      m_vDecls.at(i) = DxsoDeclaration();
      m_oDecls.at(i) = DxsoDeclaration();
      m_oPtrs.at(i)  = 0;
    }

    for (uint32_t i = 0; i < m_samplers.size(); i++) {
      m_samplers.at(i).imageTypeId = 0;
      m_samplers.at(i).imagePtrId  = 0;
      m_samplers.at(i).type        = DxsoTextureType::Texture2D;
    }

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
    case DxsoOpcode::SinCos:
    case DxsoOpcode::Lit:
    case DxsoOpcode::LogP:
    case DxsoOpcode::Log:
    case DxsoOpcode::Lrp:
    case DxsoOpcode::Frc:
    case DxsoOpcode::Cmp:
    case DxsoOpcode::Dp2Add:
      return this->emitVectorAlu(ctx);

    case DxsoOpcode::If:
    case DxsoOpcode::Ifc:
      return this->emitControlFlowIf(ctx);
    case DxsoOpcode::Else:
      return this->emitControlFlowElse(ctx);
    case DxsoOpcode::EndIf:
      return this->emitControlFlowEndIf(ctx);

    case DxsoOpcode::Tex:
    case DxsoOpcode::TexLdl:
    case DxsoOpcode::TexKill:
      return this->emitTextureSample(ctx);

    case DxsoOpcode::Comment:
      break;

    default:
      Logger::warn(str::format("DxsoCompiler::processInstruction: unhandled opcode: ", opcode));
      break;
    }
  }

  Rc<DxvkShader> DxsoCompiler::finalize() {
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
    this->emitVsClipping();
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
    this->emitPsProcessing();
    this->emitOutputDepthClamp();
    this->emitFunctionEnd();
  }

  void DxsoCompiler::emitVsClipping() {
    uint32_t clipPlaneCountId = m_module.constu32(caps::MaxClipPlanes);
    
    uint32_t floatType = m_module.defFloatType(32);
    uint32_t vec4Type  = m_module.defVectorType(floatType, 4);
    
    // Declare uniform buffer containing clip planes
    uint32_t clipPlaneArray  = m_module.defArrayTypeUnique(vec4Type, clipPlaneCountId);
    uint32_t clipPlaneStruct = m_module.defStructTypeUnique(1, &clipPlaneArray);
    uint32_t clipPlaneBlock  = m_module.newVar(
      m_module.defPointerType(clipPlaneStruct, spv::StorageClassUniform),
      spv::StorageClassUniform);
    
    m_module.decorateArrayStride  (clipPlaneArray, 16);
    
    m_module.setDebugName         (clipPlaneStruct, "clip_info_t");
    m_module.setDebugMemberName   (clipPlaneStruct, 0, "clip_planes");
    m_module.decorate             (clipPlaneStruct, spv::DecorationBlock);
    m_module.memberDecorateOffset (clipPlaneStruct, 0, 0);
    
    uint32_t bindingId = computeResourceSlotId(
      m_programInfo.type(), DxsoBindingType::ConstantBuffer,
      DxsoConstantBuffers::VSClipPlanes);
    
    m_module.setDebugName         (clipPlaneBlock, "clip_info");
    m_module.decorateDescriptorSet(clipPlaneBlock, 0);
    m_module.decorateBinding      (clipPlaneBlock, bindingId);
    
    DxvkResourceSlot resource;
    resource.slot   = bindingId;
    resource.type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    resource.view   = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    resource.access = VK_ACCESS_UNIFORM_READ_BIT;
    m_resourceSlots.push_back(resource);
    
    // Declare output array for clip distances
    uint32_t clipDistArray = m_module.newVar(
      m_module.defPointerType(
        m_module.defArrayType(floatType, clipPlaneCountId),
        spv::StorageClassOutput),
      spv::StorageClassOutput);

    m_module.decorateBuiltIn(clipDistArray, spv::BuiltInClipDistance);
    m_entryPointInterfaces.push_back(clipDistArray);
    
    // Compute clip distances
    uint32_t positionId = m_module.opLoad(vec4Type,
      findBuiltInOutputPtr(DxsoUsage::Position, 0).ptrId);
    
    for (uint32_t i = 0; i < caps::MaxClipPlanes; i++) {
      std::array<uint32_t, 2> blockMembers = {{
        m_module.constu32(0),
        m_module.constu32(i),
      }};
      
      uint32_t planeId = m_module.opLoad(vec4Type,
        m_module.opAccessChain(
          m_module.defPointerType(vec4Type, spv::StorageClassUniform),
          clipPlaneBlock, blockMembers.size(), blockMembers.data()));
      
      uint32_t distId = m_module.opDot(floatType, positionId, planeId);
      
      m_module.opStore(
        m_module.opAccessChain(
          m_module.defPointerType(floatType, spv::StorageClassOutput),
          clipDistArray, 1, &blockMembers[1]),
        distId);
    }
  }
  
  void DxsoCompiler::emitPsProcessing() {
    uint32_t boolType  = m_module.defBoolType();
    uint32_t floatType = m_module.defFloatType(32);
    uint32_t floatPtr  = m_module.defPointerType(floatType, spv::StorageClassUniform);
    
    // Declare uniform buffer containing render states
    enum RenderStateMember : uint32_t {
      RsAlphaRef = 0,
    };
    
    std::array<uint32_t, 1> rsMembers = {{
      floatType,
    }};
    
    uint32_t rsStruct = m_module.defStructTypeUnique(rsMembers.size(), rsMembers.data());
    uint32_t rsBlock  = m_module.newVar(
      m_module.defPointerType(rsStruct, spv::StorageClassUniform),
      spv::StorageClassUniform);
    
    m_module.setDebugName         (rsStruct, "render_state_t");
    m_module.decorate             (rsStruct, spv::DecorationBlock);
    m_module.setDebugMemberName   (rsStruct, 0, "alpha_ref");
    m_module.memberDecorateOffset (rsStruct, 0, offsetof(D3D9RenderStateInfo, alphaRef));
    
    uint32_t bindingId = computeResourceSlotId(
      m_programInfo.type(), DxsoBindingType::ConstantBuffer,
      DxsoConstantBuffers::PSRenderStates);
    
    m_module.setDebugName         (rsBlock, "render_state");
    m_module.decorateDescriptorSet(rsBlock, 0);
    m_module.decorateBinding      (rsBlock, bindingId);
    
    DxvkResourceSlot resource;
    resource.slot   = bindingId;
    resource.type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    resource.view   = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    resource.access = VK_ACCESS_UNIFORM_READ_BIT;
    m_resourceSlots.push_back(resource);
    
    // Declare spec constants for render states
    uint32_t alphaTestId = m_module.specConstBool(false);
    uint32_t alphaFuncId = m_module.specConst32(m_module.defIntType(32, 0), uint32_t(VK_COMPARE_OP_ALWAYS));
    
    m_module.setDebugName   (alphaTestId, "alpha_test");
    m_module.decorateSpecId (alphaTestId, uint32_t(DxvkSpecConstantId::AlphaTestEnable));
    
    m_module.setDebugName   (alphaFuncId, "alpha_func");
    m_module.decorateSpecId (alphaFuncId, uint32_t(DxvkSpecConstantId::AlphaCompareOp));
    
    // Implement alpha test
    auto oC0 = findBuiltInOutputPtr(DxsoUsage::Color, 0);
    
    if (oC0.ptrId) {
      // Labels for the alpha test
      std::array<SpirvSwitchCaseLabel, 8> atestCaseLabels = {{
        { uint32_t(VK_COMPARE_OP_NEVER),            m_module.allocateId() },
        { uint32_t(VK_COMPARE_OP_LESS),             m_module.allocateId() },
        { uint32_t(VK_COMPARE_OP_EQUAL),            m_module.allocateId() },
        { uint32_t(VK_COMPARE_OP_LESS_OR_EQUAL),    m_module.allocateId() },
        { uint32_t(VK_COMPARE_OP_GREATER),          m_module.allocateId() },
        { uint32_t(VK_COMPARE_OP_NOT_EQUAL),        m_module.allocateId() },
        { uint32_t(VK_COMPARE_OP_GREATER_OR_EQUAL), m_module.allocateId() },
        { uint32_t(VK_COMPARE_OP_ALWAYS),           m_module.allocateId() },
      }};
      
      uint32_t atestBeginLabel   = m_module.allocateId();
      uint32_t atestTestLabel    = m_module.allocateId();
      uint32_t atestDiscardLabel = m_module.allocateId();
      uint32_t atestKeepLabel    = m_module.allocateId();
      uint32_t atestSkipLabel    = m_module.allocateId();
      
      // if (alpha_test) { ... }
      m_module.opSelectionMerge(atestSkipLabel, spv::SelectionControlMaskNone);
      m_module.opBranchConditional(alphaTestId, atestBeginLabel, atestSkipLabel);
      m_module.opLabel(atestBeginLabel);
      
      // Load alpha component
      uint32_t alphaComponentId = 3;
      uint32_t alphaId = m_module.opCompositeExtract(floatType,
        m_module.opLoad(m_module.defVectorType(floatType, 4), oC0.ptrId),
        1, &alphaComponentId);
      
      // Load alpha reference
      uint32_t alphaRefMember = m_module.constu32(RsAlphaRef);
      uint32_t alphaRefId = m_module.opLoad(floatType,
        m_module.opAccessChain(floatPtr, rsBlock, 1, &alphaRefMember));
      
      // switch (alpha_func) { ... }
      m_module.opSelectionMerge(atestTestLabel, spv::SelectionControlMaskNone);
      m_module.opSwitch(alphaFuncId,
        atestCaseLabels[uint32_t(VK_COMPARE_OP_ALWAYS)].labelId,
        atestCaseLabels.size(),
        atestCaseLabels.data());
      
      std::array<SpirvPhiLabel, 8> atestVariables;
      
      for (uint32_t i = 0; i < atestCaseLabels.size(); i++) {
        m_module.opLabel(atestCaseLabels[i].labelId);
        
        atestVariables[i].labelId = atestCaseLabels[i].labelId;
        atestVariables[i].varId   = [&] {
          switch (VkCompareOp(atestCaseLabels[i].literal)) {
            case VK_COMPARE_OP_NEVER:            return m_module.constBool(false);
            case VK_COMPARE_OP_LESS:             return m_module.opFOrdLessThan        (boolType, alphaId, alphaRefId);
            case VK_COMPARE_OP_EQUAL:            return m_module.opFOrdEqual           (boolType, alphaId, alphaRefId);
            case VK_COMPARE_OP_LESS_OR_EQUAL:    return m_module.opFOrdLessThanEqual   (boolType, alphaId, alphaRefId);
            case VK_COMPARE_OP_GREATER:          return m_module.opFOrdGreaterThan     (boolType, alphaId, alphaRefId);
            case VK_COMPARE_OP_NOT_EQUAL:        return m_module.opFOrdNotEqual        (boolType, alphaId, alphaRefId);
            case VK_COMPARE_OP_GREATER_OR_EQUAL: return m_module.opFOrdGreaterThanEqual(boolType, alphaId, alphaRefId);
            default:
            case VK_COMPARE_OP_ALWAYS:           return m_module.constBool(true);
          }
        }();
        
        m_module.opBranch(atestTestLabel);
      }
      
      // end switch
      m_module.opLabel(atestTestLabel);
      
      uint32_t atestResult = m_module.opPhi(boolType,
        atestVariables.size(),
        atestVariables.data());
      uint32_t atestDiscard = m_module.opLogicalNot(boolType, atestResult);
      
      atestResult = m_module.opLogicalNot(boolType, atestResult);
      
      // if (do_discard) { ... }
      m_module.opSelectionMerge(atestKeepLabel, spv::SelectionControlMaskNone);
      m_module.opBranchConditional(atestDiscard, atestDiscardLabel, atestKeepLabel);
      
      m_module.opLabel(atestDiscardLabel);
      m_module.opKill();
      
      // end if (do_discard)
      m_module.opLabel(atestKeepLabel);
      m_module.opBranch(atestSkipLabel);
      
      // end if (alpha_test)
      m_module.opLabel(atestSkipLabel);
    }
  }

  void DxsoCompiler::emitOutputDepthClamp() {
    // HACK: Some drivers do not clamp FragDepth to [minDepth..maxDepth]
    // before writing to the depth attachment, but we do not have acccess
    // to those. Clamp to [0..1] instead.

    auto reg = findBuiltInOutputPtr(DxsoUsage::Depth, 0);

    if (reg.ptrId != 0) {
      uint32_t typeId = spvTypeVar(DxsoRegisterType::DepthOut);
      uint32_t result = spvLoad(DxsoRegisterId(DxsoRegisterType::DepthOut, 0));

      result = m_module.opFClamp(
        typeId,
        result,
        m_module.constf32(0.0f),
        m_module.constf32(1.0f));

      m_module.opStore(
        reg.ptrId,
        result);
    }
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

    m_dclInsertionPtr = m_module.getInsertionPtr();
  }

  void DxsoCompiler::emitDclConstantBuffer() {
    const uint32_t floatArray   = m_module.defArrayTypeUnique(spvTypeVar(DxsoRegisterType::Const),     m_module.constu32(256));
    const uint32_t intArray     = m_module.defArrayTypeUnique(spvTypeVar(DxsoRegisterType::ConstInt),  m_module.constu32(16));
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

  uint32_t DxsoCompiler::emitNewVariable(DxsoRegisterType regType, uint32_t value) {
    uint32_t ptrId = m_module.newVar(
      spvTypePtr(regType),
      spvStorage(regType));

    if (value != 0) {
      m_module.opStore(
        ptrId,
        value);
    }

    return ptrId;
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

    if (count == 1)
      return m_module.opCompositeExtract(typeId, varId, 1, identityShuffle.data());
    else
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
      result = m_module.opNot(typeId, varId);
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

  bool DxsoCompiler::isVectorReg(DxsoRegisterType type) {
    return type != DxsoRegisterType::ConstBool
        && type != DxsoRegisterType::Loop
        && type != DxsoRegisterType::DepthOut;
  }

  uint32_t DxsoCompiler::emitRegisterLoad(const DxsoRegister& reg, uint32_t count) {
    bool vector = isVectorReg(reg.registerId().type());

    const uint32_t typeId = spvTypeVar(reg, count);

    uint32_t result = spvLoad(reg);

    // These three are scalar types so we don't want to swizzle them.
    if (vector)
      result = emitRegisterSwizzle(typeId, result, reg.swizzle(), count);
    else
      count = 1;

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

  uint32_t DxsoCompiler::emitWriteMask(bool vector, uint32_t typeId, uint32_t dst, uint32_t src, DxsoRegMask writeMask) {
    if (writeMask == IdentityWriteMask && vector)
      return src;
    
    std::array<uint32_t, 4> components;
    for (uint32_t i = 0; i < 4; i++)
      components[i] = writeMask[i] ? i + 4 : i;

    if (vector)
      return m_module.opVectorShuffle(typeId, dst, src, 4, components.data());
    else {
      uint32_t idx = components[0] - 4;
      return m_module.opCompositeExtract(typeId, src, 1, &idx);
    }
  }

  void DxsoCompiler::emitDebugName(uint32_t varId, DxsoRegisterId id, bool deffed) {
    bool ps = m_programInfo.type() == DxsoProgramType::PixelShader;

    std::string name = "";
    switch (id.type()) {
      case DxsoRegisterType::Temp:          name = str::format("r", id.num()); break;
      case DxsoRegisterType::Input:         name = str::format("v", id.num()); break;
      case DxsoRegisterType::Addr:          name = ps ? str::format("t", id.num()) : "a"; break;
      case DxsoRegisterType::RasterizerOut: {
        std::string suffix = "Unknown";
        if (id.num() == RasterOutPosition)
          suffix = "Pos";
        else if (id.num() == RasterOutFog)
          suffix = "Fog";
        else
          suffix = "Pts";
        name = str::format("o", suffix);
        break;
      }
      case DxsoRegisterType::AttributeOut:  name = str::format("oC", id.num()); break;
      case DxsoRegisterType::Output:        name = str::format("o", id.num()); break;
      case DxsoRegisterType::ColorOut:      name = str::format("oC", id.num()); break;
      case DxsoRegisterType::DepthOut:      name = "oDepth"; break;
      case DxsoRegisterType::Loop:          name = str::format("l", id.num()); break;
      case DxsoRegisterType::TempFloat16:   name = str::format("h", id.num()); break;
      case DxsoRegisterType::MiscType: {
        if (id.num() == MiscTypePosition)
          name = "vPos";
        else
          name = "vFace";
      }
      case DxsoRegisterType::Predicate:     name = str::format("p", id.num()); break;

      case DxsoRegisterType::Const:         name = str::format("cf", id.num()); break;
      case DxsoRegisterType::ConstInt:      name = str::format("ci", id.num()); break;
      case DxsoRegisterType::ConstBool:     name = str::format("cb", id.num()); break;

      default: break;
    }

    if (deffed)
      name += "_def";

    if (!name.empty())
      m_module.setDebugName(varId, name.c_str());
  }

  uint32_t DxsoCompiler::emitScalarReplicant(uint32_t vectorTypeId, uint32_t varId) {
    std::array<uint32_t, 4> replicantIndices = { varId, varId, varId, varId };
    return m_module.opCompositeConstruct(vectorTypeId, replicantIndices.size(), replicantIndices.data());
  }

  void DxsoCompiler::emitControlFlowIf(const DxsoInstructionContext& ctx) {
    const auto opcode = ctx.instruction.opcode();

    uint32_t result;

    if (opcode == DxsoOpcode::Ifc) {
      const uint32_t typeId = m_module.defBoolType();

      uint32_t a = emitRegisterLoad(ctx.src[0], 1);
      uint32_t b = emitRegisterLoad(ctx.src[1], 1);

      switch (ctx.instruction.comparison()) {
        default:
        case DxsoComparison::Never:        result = m_module.constBool             (false); break;
        case DxsoComparison::GreaterThan:  result = m_module.opFOrdGreaterThan     (typeId, a, b); break;
        case DxsoComparison::Equal:        result = m_module.opFOrdEqual           (typeId, a, b); break;
        case DxsoComparison::GreaterEqual: result = m_module.opFOrdGreaterThanEqual(typeId, a, b); break;
        case DxsoComparison::LessThan:     result = m_module.opFOrdLessThan        (typeId, a, b); break;
        case DxsoComparison::NotEqual:     result = m_module.opFOrdNotEqual        (typeId, a, b); break;
        case DxsoComparison::LessEqual:    result = m_module.opFOrdLessThanEqual   (typeId, a, b); break;
        case DxsoComparison::Always:       result = m_module.constBool             (true); break;
      }
    } else
      result = emitRegisterLoad(ctx.src[0]);

    // Declare the 'if' block. We do not know if there
    // will be an 'else' block or not, so we'll assume
    // that there is one and leave it empty otherwise.
    DxsoCfgBlock block;
    block.type = DxsoCfgBlockType::If;
    block.b_if.ztestId   = result;
    block.b_if.labelIf   = m_module.allocateId();
    block.b_if.labelElse = 0;
    block.b_if.labelEnd  = m_module.allocateId();
    block.b_if.headerPtr = m_module.getInsertionPtr();
    m_controlFlowBlocks.push_back(block);

    // We'll insert the branch instruction when closing
    // the block, since we don't know whether or not an
    // else block is needed right now.
    m_module.opLabel(block.b_if.labelIf);
  }

  void DxsoCompiler::emitControlFlowElse(const DxsoInstructionContext& ctx) {
    if (m_controlFlowBlocks.size() == 0
     || m_controlFlowBlocks.back().type != DxsoCfgBlockType::If
     || m_controlFlowBlocks.back().b_if.labelElse != 0)
      throw DxvkError("DxsoCompiler: 'Else' without 'If' found");
    
    // Set the 'Else' flag so that we do
    // not insert a dummy block on 'EndIf'
    DxsoCfgBlock& block = m_controlFlowBlocks.back();
    block.b_if.labelElse = m_module.allocateId();
    
    // Close the 'If' block by branching to
    // the merge block we declared earlier
    m_module.opBranch(block.b_if.labelEnd);
    m_module.opLabel (block.b_if.labelElse);
  }

  void DxsoCompiler::emitControlFlowEndIf(const DxsoInstructionContext& ctx) {
    if (m_controlFlowBlocks.size() == 0
     || m_controlFlowBlocks.back().type != DxsoCfgBlockType::If)
      throw DxvkError("DxsoCompiler: 'EndIf' without 'If' found");
    
    // Remove the block from the stack, it's closed
    DxsoCfgBlock block = m_controlFlowBlocks.back();
    m_controlFlowBlocks.pop_back();
    
    // Write out the 'if' header
    m_module.beginInsertion(block.b_if.headerPtr);
    
    m_module.opSelectionMerge(
      block.b_if.labelEnd,
      spv::SelectionControlMaskNone);
    
    m_module.opBranchConditional(
      block.b_if.ztestId,
      block.b_if.labelIf,
      block.b_if.labelElse != 0
        ? block.b_if.labelElse
        : block.b_if.labelEnd);
    
    m_module.endInsertion();
    
    // End the active 'if' or 'else' block
    m_module.opBranch(block.b_if.labelEnd);
    m_module.opLabel (block.b_if.labelEnd);
  }

  void DxsoCompiler::emitVectorAlu(const DxsoInstructionContext& ctx) {
    const auto& dst = ctx.dst;
    const auto& src = ctx.src;

    const uint32_t typeId = spvTypeVar(dst);

    const auto opcode = ctx.instruction.opcode();
    uint32_t result;
    switch (opcode) {
      case DxsoOpcode::Mov:
      case DxsoOpcode::Mova: {
        if (dst.registerId().type() == DxsoRegisterType::Addr
         && m_programInfo.type() == DxsoProgramType::VertexShader
         && src[0].registerId().type() != DxsoRegisterType::ConstInt) {
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

        result = this->emitInfinityClamp(typeId, result);
        break;
      case DxsoOpcode::Rsq:
        result = m_module.opInverseSqrt(typeId, emitRegisterLoad(src[0]));
        result = this->emitInfinityClamp(typeId, result);
        break;
      case DxsoOpcode::Dp3: {
        const uint32_t scalarTypeId = spvTypeVar(dst, 1);

        result = m_module.opDot(scalarTypeId, emitRegisterLoad(src[0], 3), emitRegisterLoad(src[1], 3));
        result = this->emitScalarReplicant(typeId, result);
        break;
      }
      case DxsoOpcode::Dp4: {
        const uint32_t scalarTypeId = spvTypeVar(dst, 1);

        result = m_module.opDot(scalarTypeId, emitRegisterLoad(src[0]), emitRegisterLoad(src[1]));
        result = this->emitScalarReplicant(typeId, result);
        break;
      }
      case DxsoOpcode::Slt:
      case DxsoOpcode::Sge: {
        const uint32_t boolVec = m_module.defVectorType(m_module.defBoolType(), 4);

        result = opcode == DxsoOpcode::Slt
          ? m_module.opFOrdLessThan        (boolVec, emitRegisterLoad(src[0]), emitRegisterLoad(src[1]))
          : m_module.opFOrdGreaterThanEqual(boolVec, emitRegisterLoad(src[0]), emitRegisterLoad(src[1]));

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
        const uint32_t scalarTypeId = spvTypeVar(dst, 1);

        uint32_t vec3 = emitRegisterLoad(src[0], 3);

        result = m_module.opDot(scalarTypeId, vec3, vec3);
        result = m_module.opInverseSqrt(scalarTypeId, result);
        result = this->emitInfinityClamp(scalarTypeId, result, false);

        // r * rsq(r . r);
        result = m_module.opVectorTimesScalar(
          typeId,
          emitRegisterLoad(src[0]),
          result);
      } break;
      case DxsoOpcode::SinCos: {
        const uint32_t scalarTypeId = spvTypeVar(dst, 1);

        std::array<uint32_t, 4> sincosVectorIndices = {
          m_module.opSin(scalarTypeId, emitRegisterLoad(src[0], 1)),
          m_module.opCos(scalarTypeId, emitRegisterLoad(src[0], 1)),
          m_module.constf32(0.0f),
          m_module.constf32(0.0f)
        };

        result = m_module.opCompositeConstruct(typeId, sincosVectorIndices.size(), sincosVectorIndices.data());

        break;
      }
      case DxsoOpcode::Lit: {
        const uint32_t scalarTypeId = spvTypeVar(dst, 1);

        uint32_t srcOp = emitRegisterLoad(src[0]);

        const uint32_t x = 0;
        const uint32_t y = 1;
        const uint32_t z = 2;
        const uint32_t w = 3;
        
        uint32_t srcX = m_module.opCompositeExtract(scalarTypeId, srcOp, 1, &x);
        uint32_t srcY = m_module.opCompositeExtract(scalarTypeId, srcOp, 1, &y);
        uint32_t srcZ = m_module.opCompositeExtract(scalarTypeId, srcOp, 1, &z);
        uint32_t srcW = m_module.opCompositeExtract(scalarTypeId, srcOp, 1, &w);

        uint32_t power = m_module.opFClamp(
          scalarTypeId, srcW,
          m_module.constf32(-127.9961f), m_module.constf32(127.9961f));

        std::array<uint32_t, 4> resultIndices;

        resultIndices[0] = m_module.constf32(1.0f);
        resultIndices[1] = m_module.opFMax(scalarTypeId, srcX, m_module.constf32(0));
        resultIndices[2] = m_module.opPow(scalarTypeId, srcY, power);
        resultIndices[3] = m_module.constf32(1.0f);

        const uint32_t boolType = m_module.defBoolType();
        uint32_t zTestX = m_module.opFOrdGreaterThanEqual(boolType, srcX, m_module.constf32(0));
        uint32_t zTestY = m_module.opFOrdGreaterThanEqual(boolType, srcY, m_module.constf32(0));
        uint32_t zTest = m_module.opLogicalAnd(boolType, zTestX, zTestY);

        resultIndices[2] = m_module.opSelect(
          scalarTypeId,
          zTest,
          resultIndices[2],
          m_module.constf32(0.0f));

        result = m_module.opCompositeConstruct(typeId, resultIndices.size(), resultIndices.data());
        break;
      }
      case DxsoOpcode::LogP:
      case DxsoOpcode::Log:
        result = m_module.opFAbs(typeId, emitRegisterLoad(src[0]));
        result = m_module.opLog2(typeId, result);
        result = this->emitInfinityClamp(typeId, result);
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
        const uint32_t scalarTypeId = spvTypeVar(dst, 1);

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

    m_module.opStore(
      spvPtr(dst),
      emitWriteMask(isVectorReg(dst.registerId().type()), typeId, spvLoad(dst), result, dst.writeMask()));
  }

  uint32_t DxsoCompiler::emitInfinityClamp(uint32_t typeId, uint32_t varId, bool vector) {
    return m_module.opFClamp(typeId, varId,
     !vector ? m_module.constf32(-FLT_MAX) : m_module.constvec4f32(-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX),
     !vector ? m_module.constf32( FLT_MAX) : m_module.constvec4f32( FLT_MAX,  FLT_MAX,  FLT_MAX,  FLT_MAX));
  }

  void DxsoCompiler::emitTextureSample(const DxsoInstructionContext& ctx) {
    const auto& dst = ctx.dst;

    bool sample = ctx.instruction.opcode() != DxsoOpcode::TexKill;

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

      texcoordVarId = spvLoad(texcoordId);
      samplerIdx = ctx.dst.registerId().num();
    }

    if (sample) {
      const uint32_t typeId = spvTypeVar(dst);

      DxsoSamplerDesc sampler = m_samplers.at(samplerIdx);

      if (sampler.imagePtrId == 0) {
        Logger::warn("DxsoCompiler::emitTextureSample: Adding implicit 2D sampler");
        emitDclSampler(samplerIdx, DxsoTextureType::Texture2D);
        sampler = m_samplers.at(samplerIdx);
      }

      const uint32_t imageVarId = m_module.opLoad(sampler.imageTypeId, sampler.imagePtrId);

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

      m_module.opStore(
        spvPtr(dst),
        emitWriteMask(isVectorReg(dst.registerId().type()), typeId, spvLoad(dst), result, dst.writeMask()));
    }
    else {
      texcoordVarId = this->emitVecTrunc(
        m_module.defVectorType(m_module.defFloatType(32), 3), texcoordVarId, 3);

      uint32_t boolType = m_module.defBoolType();

      uint32_t result = m_module.opFOrdLessThan(
        m_module.defVectorType(boolType, 3),
        texcoordVarId,
        m_module.constvec3f32(0.0f, 0.0f, 0.0f));

      result = m_module.opAny(boolType, result);

      uint32_t discardLabel = m_module.allocateId();
      uint32_t skipLabel    = m_module.allocateId();

      m_module.opSelectionMerge(skipLabel, spv::SelectionControlMaskNone);
      m_module.opBranchConditional(result, discardLabel, skipLabel);

      m_module.opLabel(discardLabel);
      m_module.opKill();

      m_module.opLabel(skipLabel);
    }
  }

  void DxsoCompiler::emitDclSampler(uint32_t idx, DxsoTextureType type) {
    m_module.beginInsertion(m_dclInsertionPtr);

    // Combined Sampler Setup
    DxsoSamplerDesc sampler;
    sampler.type = type;

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

      uint32_t imageTypeId = m_module.defImageType(m_module.defFloatType(32),
        dimensionality, 0, 0, 0, 1,
        spv::ImageFormatR32f);

      imageTypeId = m_module.defSampledImageType(imageTypeId);

      const uint32_t resourcePtrType = m_module.defPointerType(
        imageTypeId, spv::StorageClassUniformConstant);

      const uint32_t ptrId = m_module.newVar(resourcePtrType,
        spv::StorageClassUniformConstant);

      const uint32_t bindingId = computeResourceSlotId(
        m_programInfo.type(), DxsoBindingType::Image, idx);

      m_module.decorateDescriptorSet(ptrId, 0);
      m_module.decorateBinding(ptrId, bindingId);

      sampler.imagePtrId  = ptrId;
      sampler.imageTypeId = imageTypeId;

      // Store descriptor info for the shader interface
      DxvkResourceSlot resource;
      resource.slot   = bindingId;
      resource.type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      resource.view   = viewType;
      resource.access = VK_ACCESS_SHADER_READ_BIT;
      m_resourceSlots.push_back(resource);
    }

    m_samplers.at(idx) = sampler;

    m_dclInsertionPtr = m_module.getInsertionPtr();
    m_module.endInsertion();
  }

  void DxsoCompiler::emitDcl(const DxsoInstructionContext& ctx) {
    const auto type    = ctx.dst.registerId().type();

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
    const float* data = reinterpret_cast<const float*>(ctx.def.data());

    DxsoSpirvRegister reg;
    reg.regId = ctx.dst.registerId();
    reg.ptrId = this->emitNewVariable(
      reg.regId.type(),
      m_module.constvec4f32(data[0], data[1], data[2], data[3]));

    this->emitDebugName(reg.ptrId, reg.regId, true);

    m_regs.push_back(reg);
  }

  void DxsoCompiler::emitDefI(const DxsoInstructionContext& ctx) {
    const int* data = reinterpret_cast<const int*>(ctx.def.data());

    DxsoSpirvRegister reg;
    reg.regId = ctx.dst.registerId();
    reg.ptrId = this->emitNewVariable(
      reg.regId.type(),
      m_module.constvec4i32(data[0], data[1], data[2], data[3]));

    this->emitDebugName(reg.ptrId, reg.regId, true);

    m_regs.push_back(reg);
  }

  void DxsoCompiler::emitDefB(const DxsoInstructionContext& ctx) {
    const int* data = reinterpret_cast<const int*>(ctx.def.data());

    DxsoSpirvRegister reg;
    reg.regId = ctx.dst.registerId();
    reg.ptrId = this->emitNewVariable(
      reg.regId.type(),
      m_module.constBool(*data != 0));

    this->emitDebugName(reg.ptrId, reg.regId, true);

    m_regs.push_back(reg);
  }

  DxsoSpirvRegister DxsoCompiler::getSpirvRegister(DxsoRegisterId id, bool centroid, DxsoRegister* relative) {
    if (!id.constant() || (id.constant() && relative == nullptr)) {
      for (const auto& regMapping : m_regs) {
        if (regMapping.regId == id)
          return regMapping;
      }
    }

    return this->mapSpirvRegister(id, centroid, relative, nullptr);
  }

  DxsoSpirvRegister DxsoCompiler::getSpirvRegister(const DxsoRegister& reg) {
    DxsoRegister relativeReg = reg.relativeRegister();

    return this->getSpirvRegister(
      reg.registerId(),
      reg.centroid(),
      reg.isRelative()
        ? &relativeReg
        : nullptr);
  }

  DxsoSpirvRegister DxsoCompiler::mapSpirvRegister(DxsoRegisterId id, bool centroid, DxsoRegister* relative, const DxsoDeclaration* optionalPremadeDecl) {

    m_module.beginInsertion(m_dclInsertionPtr);

    DxsoSpirvRegister spirvRegister;
    spirvRegister.regId = id;

    uint32_t inputSlot  = InvalidInputSlot;
    uint32_t outputSlot = InvalidOutputSlot;

    spv::BuiltIn builtIn = spv::BuiltInMax;
    uint32_t ptrId = 0;

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
      else if (id.type() == DxsoRegisterType::DepthOut) {
        m_module.setExecutionMode(m_entryPointId,
          spv::ExecutionModeDepthReplacing);

        DxsoSemantic semantic = { DxsoUsage::Depth, id.num() };

        builtIn = spv::BuiltInFragDepth;
        auto& dcl = m_oDecls[outputSlot = allocateSlot(false, id, semantic)];
        dcl.id = id;
        dcl.semantic = semantic;
      }
      else if (id.type() == DxsoRegisterType::MiscType) {
        if (id.num() == MiscTypePosition) {
          Logger::warn("DxsoCompiler::mapSpirvRegister: unimplemented vPos used.");
        }
        else { // MiscTypeFace
          Logger::warn("DxsoCompiler::mapSpirvRegister: unimplemented vFace used.");
        }
      }
    }

    const bool input = inputSlot != InvalidInputSlot;
    const bool output = outputSlot != InvalidOutputSlot;

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

      uint32_t uniformTypeId = id.type() != DxsoRegisterType::ConstBool
        ? spvTypeVar(id.type())
        : m_module.defIntType(32, false);

      if (relative != nullptr) {
        DxsoRegisterId id;

        if (relative->hasRelativeToken())
          id = relative->registerId();
        else
          id = { DxsoRegisterType::Addr, relative->registerId().num() };

        uint32_t r = spvLoad(id);

        r = emitRegisterSwizzle(m_module.defIntType(32, 1), r, relative->swizzle(), 1);

        constantIdx = m_module.opIAdd(
          m_module.defIntType(32, 1),
          constantIdx, r);
      }

      const std::array<uint32_t, 2> indices =
      { { memberId, constantIdx } };

      const uint32_t ptrType = m_module.defPointerType(uniformTypeId, spv::StorageClassUniform);
      ptrId = m_module.opAccessChain(ptrType,
        m_cBuffer,
        id.type() != DxsoRegisterType::ConstBool ? 2 : 1, indices.data());

      if (id.type() == DxsoRegisterType::ConstBool) {
        uint32_t varId = m_module.opLoad(uniformTypeId, ptrId);

        uint32_t boolTypeId = spvTypeVar(id.type());
        varId = m_module.opBitFieldUExtract(
          uniformTypeId,
          varId,
          constantIdx,
          m_module.consti32(1));

        varId = m_module.opINotEqual(boolTypeId, varId, m_module.constu32(0));

        uint32_t boolPtrTypeId = m_module.defPointerType(boolTypeId, spv::StorageClassPrivate);
        ptrId = m_module.newVar(boolPtrTypeId, spv::StorageClassPrivate);
        m_module.opStore(ptrId, varId);
      }
    } else if (input || output) {
      ptrId = this->emitNewVariable(id.type());

      if (input) {
        m_module.decorateLocation(ptrId, inputSlot);
        m_entryPointInterfaces.push_back(ptrId);

        if (centroid)
          m_module.decorate(ptrId, spv::DecorationCentroid);
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

    if (ptrId == 0) {
      ptrId = this->emitNewVariable(id.type());

      if ((m_programInfo.type() == DxsoProgramType::VertexShader && id.type() == DxsoRegisterType::Addr)
        || id.type() == DxsoRegisterType::ConstInt)
        m_module.opStore(ptrId, m_module.constvec4i32(0, 0, 0, 0));
      else if (id.type() == DxsoRegisterType::Loop)
        m_module.opStore(ptrId, m_module.consti32(0));
      else if (id.type() == DxsoRegisterType::ConstBool)
        m_module.opStore(ptrId, m_module.constBool(0));
      else
        m_module.opStore(ptrId, m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f));
    }

    this->emitDebugName(ptrId, id);
    spirvRegister.ptrId = ptrId;

    m_dclInsertionPtr = m_module.getInsertionPtr();
    m_module.endInsertion();

    if (id.constant() && relative != nullptr)
      return spirvRegister;

    m_regs.push_back(spirvRegister);
    return spirvRegister;
  }

  spv::StorageClass DxsoCompiler::spvStorage(DxsoRegisterType regType) {
    if (m_programInfo.type() == DxsoProgramType::VertexShader
     && regType == DxsoRegisterType::Addr)
      return spv::StorageClassPrivate;

    bool texture = m_programInfo.type() == DxsoProgramType::PixelShader
                && regType == DxsoRegisterType::Texture;

    // SM 2+ or 1.4
    texture &= m_programInfo.majorVersion() >= 2
           || (m_programInfo.majorVersion() == 1
            && m_programInfo.minorVersion() == 4);

    if (regType == DxsoRegisterType::Input
     || texture)
      return spv::StorageClassInput;

    if (regType == DxsoRegisterType::RasterizerOut
     || regType == DxsoRegisterType::Output
     || regType == DxsoRegisterType::AttributeOut
     || regType == DxsoRegisterType::ColorOut
     || regType == DxsoRegisterType::DepthOut)
      return spv::StorageClassOutput;

    return spv::StorageClassPrivate;
  }

  DxsoSpirvRegister DxsoCompiler::findBuiltInOutputPtr(DxsoUsage usage, uint32_t index) {
    for (uint32_t i = 0; i < m_oDecls.size(); i++) {
      if (m_oDecls[i].semantic.usage == usage
        && m_oDecls[i].semantic.usageIndex == index)
        return DxsoSpirvRegister{ m_oDecls[i].id, m_oPtrs[i] };
    }

    return DxsoSpirvRegister();
  }

  uint32_t DxsoCompiler::spvTypeVar(DxsoRegisterType regType, uint32_t count) {
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
    case DxsoRegisterType::Const2:
    case DxsoRegisterType::Const3:
    case DxsoRegisterType::Const4:
    case DxsoRegisterType::TempFloat16:
    case DxsoRegisterType::MiscType: {
      uint32_t floatType = m_module.defFloatType(32);
      return count > 1 ? m_module.defVectorType(floatType, count) : floatType;
    }

    case DxsoRegisterType::DepthOut:
      return m_module.defFloatType(32);

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