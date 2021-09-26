#include "dxso_compiler.h"

#include "dxso_analysis.h"

#include "../d3d9/d3d9_caps.h"
#include "../d3d9/d3d9_constant_set.h"
#include "../d3d9/d3d9_state.h"
#include "../d3d9/d3d9_spec_constants.h"
#include "../d3d9/d3d9_fixed_function.h"
#include "dxso_util.h"

#include "../dxvk/dxvk_spec_const.h"

#include <cfloat>

namespace dxvk {

  DxsoCompiler::DxsoCompiler(
    const std::string&        fileName,
    const DxsoModuleInfo&     moduleInfo,
    const DxsoProgramInfo&    programInfo,
    const DxsoAnalysisInfo&   analysis,
    const D3D9ConstantLayout& layout)
    : m_moduleInfo ( moduleInfo )
    , m_programInfo( programInfo )
    , m_analysis   ( &analysis )
    , m_layout     ( &layout )
    , m_module     ( spvVersion(1, 3) ) {
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

    m_usedSamplers = 0;
    m_usedRTs      = 0;

    for (uint32_t i = 0; i < m_rRegs.size(); i++)
      m_rRegs.at(i)  = DxsoRegisterPointer{ };

    for (uint32_t i = 0; i < m_cFloat.size(); i++)
      m_cFloat.at(i) = 0;

    for (uint32_t i = 0; i < m_cInt.size(); i++)
      m_cInt.at(i)   = 0;

    for (uint32_t i = 0; i < m_cBool.size(); i++)
      m_cBool.at(i)  = 0;

    m_vs.addr        = DxsoRegisterPointer{ };
    m_vs.oPos        = DxsoRegisterPointer{ };
    m_fog            = DxsoRegisterPointer{ };
    m_vs.oPSize      = DxsoRegisterPointer{ };

    for (uint32_t i = 0; i < m_ps.oColor.size(); i++)
      m_ps.oColor.at(i) = DxsoRegisterPointer{ };
    m_ps.oDepth      = DxsoRegisterPointer{ };
    m_ps.vFace       = DxsoRegisterPointer{ };
    m_ps.vPos        = DxsoRegisterPointer{ };

    m_loopCounter = DxsoRegisterPointer{ };

    this->emitInit();
  }


  void DxsoCompiler::processInstruction(
    const DxsoInstructionContext& ctx,
          uint32_t                currentCoissueIdx) {
    const DxsoOpcode opcode = ctx.instruction.opcode;

    for (const auto& coissue : m_analysis->coissues) {
      if (coissue.instructionIdx == ctx.instructionIdx &&
          coissue.instructionIdx != currentCoissueIdx)
        return;

      if (coissue.instructionIdx == ctx.instructionIdx + 1)
        processInstruction(coissue, coissue.instructionIdx);
    }

    switch (opcode) {
    case DxsoOpcode::Nop:
      return;

    case DxsoOpcode::Dcl:
      return this->emitDcl(ctx);

    case DxsoOpcode::Def:
    case DxsoOpcode::DefI:
    case DxsoOpcode::DefB:
      return this->emitDef(ctx);

    case DxsoOpcode::Mov:
    case DxsoOpcode::Mova:
      return this->emitMov(ctx);

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
    case DxsoOpcode::Crs:
    case DxsoOpcode::Abs:
    case DxsoOpcode::Sgn:
    case DxsoOpcode::Nrm:
    case DxsoOpcode::SinCos:
    case DxsoOpcode::Lit:
    case DxsoOpcode::Dst:
    case DxsoOpcode::LogP:
    case DxsoOpcode::Log:
    case DxsoOpcode::Lrp:
    case DxsoOpcode::Frc:
    case DxsoOpcode::Cmp:
    case DxsoOpcode::Cnd:
    case DxsoOpcode::Dp2Add:
    case DxsoOpcode::DsX:
    case DxsoOpcode::DsY:
      return this->emitVectorAlu(ctx);

    case DxsoOpcode::SetP:
      return this->emitPredicateOp(ctx);

    case DxsoOpcode::M3x2:
    case DxsoOpcode::M3x3:
    case DxsoOpcode::M3x4:
    case DxsoOpcode::M4x3:
    case DxsoOpcode::M4x4:
      return this->emitMatrixAlu(ctx);

    case DxsoOpcode::Loop:
      return this->emitControlFlowLoop(ctx);
    case DxsoOpcode::EndLoop:
      return this->emitControlFlowEndLoop(ctx);

    case DxsoOpcode::Rep:
      return this->emitControlFlowRep(ctx);
    case DxsoOpcode::EndRep:
      return this->emitControlFlowEndRep(ctx);

    case DxsoOpcode::Break:
      return this->emitControlFlowBreak(ctx);
    case DxsoOpcode::BreakC:
      return this->emitControlFlowBreakC(ctx);

    case DxsoOpcode::If:
    case DxsoOpcode::Ifc:
      return this->emitControlFlowIf(ctx);
    case DxsoOpcode::Else:
      return this->emitControlFlowElse(ctx);
    case DxsoOpcode::EndIf:
      return this->emitControlFlowEndIf(ctx);

    case DxsoOpcode::TexCoord:
      return this->emitTexCoord(ctx);

    case DxsoOpcode::Tex:
    case DxsoOpcode::TexLdl:
    case DxsoOpcode::TexLdd:
    case DxsoOpcode::TexDp3Tex:
    case DxsoOpcode::TexReg2Ar:
    case DxsoOpcode::TexReg2Gb:
    case DxsoOpcode::TexReg2Rgb:
    case DxsoOpcode::TexBem:
    case DxsoOpcode::TexBemL:
    case DxsoOpcode::TexM3x2Tex:
    case DxsoOpcode::TexM3x3Tex:
    case DxsoOpcode::TexM3x3Spec:
    case DxsoOpcode::TexM3x3VSpec:
      return this->emitTextureSample(ctx);
    case DxsoOpcode::TexKill:
      return this->emitTextureKill(ctx);
    case DxsoOpcode::TexDepth:
      return this->emitTextureDepth(ctx);

    case DxsoOpcode::TexM3x3Pad:
    case DxsoOpcode::TexM3x2Pad:
      // We don't need to do anything here, these are just padding instructions
      break;

    case DxsoOpcode::End:
    case DxsoOpcode::Comment:
    case DxsoOpcode::Phase:
      break;

    default:
      Logger::warn(str::format("DxsoCompiler::processInstruction: unhandled opcode: ", opcode));
      break;
    }
  }

  void DxsoCompiler::finalize() {
    if (m_programInfo.type() == DxsoProgramTypes::VertexShader)
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
  }


  DxsoPermutations DxsoCompiler::compile() {
    DxsoPermutations permutations = { };

    // Create the shader module object
    permutations[D3D9ShaderPermutations::None] = compileShader();

    // If we need to add more permuations, might be worth making a copy of module
    // before we do anything more. :-)
    if (m_programInfo.type() == DxsoProgramType::PixelShader) {
      if (m_ps.diffuseColorIn)
        m_module.decorate(m_ps.diffuseColorIn, spv::DecorationFlat);

      if (m_ps.specularColorIn)
        m_module.decorate(m_ps.specularColorIn, spv::DecorationFlat);

      permutations[D3D9ShaderPermutations::FlatShade] = compileShader();
    }

    return permutations;
  }


  Rc<DxvkShader> DxsoCompiler::compileShader() {
    DxvkShaderOptions shaderOptions = { };
    DxvkShaderConstData constData = { };

    return new DxvkShader(
      m_programInfo.shaderStage(),
      m_resourceSlots.size(),
      m_resourceSlots.data(),
      m_interfaceSlots,
      m_module.compile(),
      shaderOptions,
      std::move(constData));
  }

  void DxsoCompiler::emitInit() {
    // Set up common capabilities for all shaders
    m_module.enableCapability(spv::CapabilityShader);
    m_module.enableCapability(spv::CapabilityImageQuery);

    this->emitDclConstantBuffer();
    this->emitDclInputArray();

    // Initialize the shader module with capabilities
    // etc. Each shader type has its own peculiarities.
    switch (m_programInfo.type()) {
      case DxsoProgramTypes::VertexShader: return this->emitVsInit();
      case DxsoProgramTypes::PixelShader:  return this->emitPsInit();
      default: break;
    }
  }


  void DxsoCompiler::emitDclConstantBuffer() {
    const bool asSsbo = m_moduleInfo.options.vertexConstantBufferAsSSBO &&
      m_programInfo.type() == DxsoProgramType::VertexShader;

    std::array<uint32_t, 3> members = {
      // float f[256 or 224 or 8192]
      m_module.defArrayTypeUnique(
        getVectorTypeId({ DxsoScalarType::Float32, 4 }),
        m_module.constu32(m_layout->floatCount)),

      // int i[16 or 2048]
      m_module.defArrayTypeUnique(
        getVectorTypeId({ DxsoScalarType::Sint32, 4 }),
        m_module.constu32(m_layout->intCount)),

      //    uint32_t boolBitmask
      // or uvec4    boolBitmask[512]
      // Defined later...
      0
    };

    // Decorate array strides, this is required.
    m_module.decorateArrayStride(members[0], 16);
    m_module.decorateArrayStride(members[1], 16);

    const bool swvp = m_layout->bitmaskCount != 1;

    if (swvp) {
      // Must be a multiple of 4 otherwise.
      members[2] = m_module.defArrayTypeUnique(
        getVectorTypeId({ DxsoScalarType::Uint32, 4 }),
        m_module.constu32(m_layout->bitmaskCount / 4));

      m_module.decorateArrayStride(members[2], 16);
    }

    const uint32_t structType =
      m_module.defStructType(swvp ? 3 : 2, members.data());

    m_module.decorate(structType, asSsbo
      ? spv::DecorationBufferBlock
      : spv::DecorationBlock);

    m_module.memberDecorateOffset(structType, 0, m_layout->floatOffset());
    m_module.memberDecorateOffset(structType, 1, m_layout->intOffset());

    if (swvp)
      m_module.memberDecorateOffset(structType, 2, m_layout->bitmaskOffset());

    m_module.setDebugName(structType, "cbuffer_t");
    m_module.setDebugMemberName(structType, 0, "f");
    m_module.setDebugMemberName(structType, 1, "i");

    if (swvp)
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

    if (asSsbo)
      m_module.decorate(m_cBuffer, spv::DecorationNonWritable);

    DxvkResourceSlot resource;
    resource.slot   = bindingId;
    resource.type   = asSsbo
      ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
      : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    resource.view   = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    resource.access = VK_ACCESS_UNIFORM_READ_BIT;
    m_resourceSlots.push_back(resource);

    m_boolSpecConstant = m_module.specConst32(m_module.defIntType(32, 0), 0);
    m_module.decorateSpecId(m_boolSpecConstant, getSpecId(
      m_programInfo.type() == DxsoProgramType::VertexShader
      ? D3D9SpecConstantId::VertexShaderBools
      : D3D9SpecConstantId::PixelShaderBools));
    m_module.setDebugName(m_boolSpecConstant, "boolConstants");

    m_depthSpecConstant = m_module.specConst32(m_module.defIntType(32, 0), 0);
    m_module.decorateSpecId(m_depthSpecConstant, getSpecId(D3D9SpecConstantId::SamplerDepthMode));
    m_module.setDebugName(m_depthSpecConstant, "depthSamplers");
  }


  void DxsoCompiler::emitDclInputArray() {
    DxsoArrayType info;
    info.ctype   = DxsoScalarType::Float32;
    info.ccount  = 4;
    info.alength = DxsoMaxInterfaceRegs;

    uint32_t arrayTypeId = getArrayTypeId(info);

    // Define the actual variable. Note that this is private
    // because we will copy input registers
    // to the array during the setup phase.
    const uint32_t ptrTypeId = m_module.defPointerType(
      arrayTypeId, spv::StorageClassPrivate);

    m_vArray = m_module.newVar(
      ptrTypeId, spv::StorageClassPrivate);
    m_module.setDebugName(m_vArray, "v");
  }

  void DxsoCompiler::emitDclOutputArray() {
    DxsoArrayType info;
    info.ctype   = DxsoScalarType::Float32;
    info.ccount  = 4;
    info.alength = m_programInfo.type() == DxsoProgramTypes::VertexShader
      ? DxsoMaxInterfaceRegs
      : caps::MaxSimultaneousRenderTargets;

    uint32_t arrayTypeId = getArrayTypeId(info);

    // Define the actual variable. Note that this is private
    // because we will copy input registers
    // to the array during the setup phase.
    const uint32_t ptrTypeId = m_module.defPointerType(
      arrayTypeId, spv::StorageClassPrivate);

    m_oArray = m_module.newVar(
      ptrTypeId, spv::StorageClassPrivate);
    m_module.setDebugName(m_oArray, "o");
  }


  void DxsoCompiler::emitVsInit() {
    m_module.enableCapability(spv::CapabilityClipDistance);

    // Only VS needs this, because PS has
    // non-indexable specialized output regs
    this->emitDclOutputArray();

    // Main function of the vertex shader
    m_vs.functionId = m_module.allocateId();
    m_module.setDebugName(m_vs.functionId, "vs_main");

    this->setupRenderStateInfo();

    this->emitFunctionBegin(
      m_vs.functionId,
      m_module.defVoidType(),
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr));
    this->emitFunctionLabel();
  }


  void DxsoCompiler::emitPsSharedConstants() {
    m_ps.sharedState = GetSharedConstants(m_module);

    const uint32_t bindingId = computeResourceSlotId(
      m_programInfo.type(), DxsoBindingType::ConstantBuffer,
      PSShared);

    m_module.decorateDescriptorSet(m_ps.sharedState, 0);
    m_module.decorateBinding(m_ps.sharedState, bindingId);

    DxvkResourceSlot resource;
    resource.slot   = bindingId;
    resource.type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    resource.view   = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    resource.access = VK_ACCESS_UNIFORM_READ_BIT;
    m_resourceSlots.push_back(resource);
  }


  void DxsoCompiler::emitPsInit() {
    m_module.enableCapability(spv::CapabilityDerivativeControl);

    m_module.setExecutionMode(m_entryPointId,
      spv::ExecutionModeOriginUpperLeft);


    // Main function of the pixel shader
    m_ps.functionId = m_module.allocateId();
    m_module.setDebugName(m_ps.functionId, "ps_main");

    if (m_programInfo.majorVersion() < 2 || m_moduleInfo.options.forceSamplerTypeSpecConstants) {
      m_ps.samplerTypeSpec = m_module.specConst32(m_module.defIntType(32, 0), 0);
      m_module.decorateSpecId(m_ps.samplerTypeSpec, getSpecId(D3D9SpecConstantId::SamplerType));
      m_module.setDebugName(m_ps.samplerTypeSpec, "s_sampler_types");

      if (m_programInfo.majorVersion() < 2) {
        m_ps.projectionSpec = m_module.specConst32(m_module.defIntType(32, 0), 0);
        m_module.decorateSpecId(m_ps.projectionSpec, getSpecId(D3D9SpecConstantId::ProjectionType));
        m_module.setDebugName(m_ps.projectionSpec, "s_projections");
      }
    }

    m_ps.fetch4Spec = m_module.specConst32(m_module.defIntType(32, 0), 0);
    m_module.decorateSpecId(m_ps.fetch4Spec, getSpecId(D3D9SpecConstantId::Fetch4));
    m_module.setDebugName(m_ps.fetch4Spec, "s_fetch4");

    this->setupRenderStateInfo();
    this->emitPsSharedConstants();

    this->emitFunctionBegin(
      m_ps.functionId,
      m_module.defVoidType(),
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr));
    this->emitFunctionLabel();

    // We may have to defer kill operations to the end of
    // the shader in order to keep derivatives correct.
    if (m_analysis->usesKill && m_moduleInfo.options.useDemoteToHelperInvocation) {
      // This extension basically implements D3D-style discard
      m_module.enableExtension("SPV_EXT_demote_to_helper_invocation");
      m_module.enableCapability(spv::CapabilityDemoteToHelperInvocationEXT);
    }
    else if (m_analysis->usesKill && m_analysis->usesDerivatives) {
      m_ps.killState = m_module.newVarInit(
        m_module.defPointerType(m_module.defBoolType(), spv::StorageClassPrivate),
        spv::StorageClassPrivate, m_module.constBool(false));

      m_module.setDebugName(m_ps.killState, "ps_kill");

      if (m_moduleInfo.options.useSubgroupOpsForEarlyDiscard) {
        m_module.enableCapability(spv::CapabilityGroupNonUniform);
        m_module.enableCapability(spv::CapabilityGroupNonUniformBallot);

        DxsoRegisterInfo laneId;
        laneId.type = { DxsoScalarType::Uint32, 1, 0 };
        laneId.sclass = spv::StorageClassInput;

        m_ps.builtinLaneId = emitNewBuiltinVariable(
          laneId, spv::BuiltInSubgroupLocalInvocationId,
          "fLaneId", 0);
      }
    }
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


  uint32_t DxsoCompiler::emitFunctionLabel() {
    uint32_t labelId = m_module.allocateId();
    m_module.opLabel(labelId);
    return labelId;
  }


  void DxsoCompiler::emitMainFunctionBegin() {
    this->emitFunctionBegin(
      m_entryPointId,
      m_module.defVoidType(),
      m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr));
    m_mainFuncLabel = this->emitFunctionLabel();
  }


  uint32_t DxsoCompiler::emitNewVariable(const DxsoRegisterInfo& info) {
    const uint32_t ptrTypeId = this->getPointerTypeId(info);
    return m_module.newVar(ptrTypeId, info.sclass);
  }


  uint32_t DxsoCompiler::emitNewVariableDefault(
    const DxsoRegisterInfo& info,
          uint32_t          value) {
    const uint32_t ptrTypeId = this->getPointerTypeId(info);
    if (value == 0)
      return m_module.newVar(ptrTypeId, info.sclass);
    else
      return m_module.newVarInit(ptrTypeId, info.sclass, value);
  }


  uint32_t DxsoCompiler::emitNewBuiltinVariable(
    const DxsoRegisterInfo& info,
          spv::BuiltIn      builtIn,
    const char*             name,
          uint32_t          value) {
    const uint32_t varId = emitNewVariableDefault(info, value);

    m_module.setDebugName(varId, name);
    m_module.decorateBuiltIn(varId, builtIn);

    if (m_programInfo.type() == DxsoProgramTypes::PixelShader
     && info.type.ctype != DxsoScalarType::Float32
     && info.type.ctype != DxsoScalarType::Bool
     && info.sclass == spv::StorageClassInput)
      m_module.decorate(varId, spv::DecorationFlat);

    m_entryPointInterfaces.push_back(varId);
    return varId;
  }

  DxsoCfgBlock* DxsoCompiler::cfgFindBlock(
    const std::initializer_list<DxsoCfgBlockType>& types) {
    for (auto cur =  m_controlFlowBlocks.rbegin();
              cur != m_controlFlowBlocks.rend(); cur++) {
      for (auto type : types) {
        if (cur->type == type)
          return &(*cur);
      }
    }

    return nullptr;
  }

  spv::BuiltIn semanticToBuiltIn(bool input, DxsoSemantic semantic) {
    if (input)
      return spv::BuiltInMax;

    if (semantic == DxsoSemantic{ DxsoUsage::Position, 0 })
      return spv::BuiltInPosition;

    if (semantic == DxsoSemantic{ DxsoUsage::PointSize, 0 })
      return spv::BuiltInPointSize;

    return spv::BuiltInMax;
  }

  void DxsoCompiler::emitDclInterface(
            bool         input,
            uint32_t     regNumber,
            DxsoSemantic semantic,
            DxsoRegMask  mask,
            bool         centroid) {
    auto& sgn = input
      ? m_isgn : m_osgn;

    const bool pixel  = m_programInfo.type() == DxsoProgramTypes::PixelShader;
    const bool vertex = !pixel;

    if (pixel && input && semantic.usage == DxsoUsage::Color && m_programInfo.majorVersion() < 3)
      centroid = true;

    uint32_t slot = 0;

    uint32_t& slots = input
      ? m_interfaceSlots.inputSlots
      : m_interfaceSlots.outputSlots;

    uint16_t& explicits = input
      ? m_explicitInputs
      : m_explicitOutputs;

    // Some things we consider builtins could be packed in an output reg.
    bool builtin = semanticToBuiltIn(input, semantic) != spv::BuiltInMax;

    uint32_t i = sgn.elemCount++;

    if (input && vertex) {
      // Any slot will do! Let's chose the next one
      slot = i;
    }
    else if ( (!input && vertex)
           || (input  && pixel ) ) {
      // Don't register the slot if it belongs to a builtin
      if (!builtin)
        slot = RegisterLinkerSlot(semantic);
    }
    else { //if (!input && pixel)
      // We want to make the output slot the same as the
      // output register for pixel shaders so they go to
      // the right render target.
      slot = regNumber;
    }

    // Don't want to mark down any of these builtins.
    if (!builtin)
      slots   |= 1u << slot;
    explicits |= 1u << regNumber;

    auto& elem = sgn.elems[i];
    elem.slot      = slot;
    elem.regNumber = regNumber;
    elem.semantic  = semantic;
    elem.mask      = mask;
    elem.centroid  = centroid;
  }

  void DxsoCompiler::emitDclSampler(
          uint32_t        idx,
          DxsoTextureType type) {
    m_usedSamplers |= (1u << idx);

    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;

    auto DclSampler = [this, &viewType](
      uint32_t        idx,
      uint32_t        bindingId,
      DxsoSamplerType type,
      bool            depth,
      bool            implicit) {
      // Setup our combines sampler.
      DxsoSamplerInfo& sampler = !depth
        ? m_samplers[idx].color[type]
        : m_samplers[idx].depth[type];

      spv::Dim dimensionality;

      const char* suffix = "_2d";

      switch (type) {
        default:
        case SamplerTypeTexture2D:
          sampler.dimensions = 2;
          dimensionality = spv::Dim2D;
          viewType = VK_IMAGE_VIEW_TYPE_2D;
          break;

        case SamplerTypeTextureCube:
          suffix = "_cube";
          sampler.dimensions = 3;
          dimensionality = spv::DimCube;
          viewType = VK_IMAGE_VIEW_TYPE_CUBE;
          break;

        case SamplerTypeTexture3D:
          suffix = "_3d";
          sampler.dimensions = 3;
          dimensionality = spv::Dim3D;
          viewType = VK_IMAGE_VIEW_TYPE_3D;
          break;
      }

      sampler.imageTypeId = m_module.defImageType(
        m_module.defFloatType(32),
        dimensionality, depth ? 1 : 0, 0, 0, 1,
        spv::ImageFormatUnknown);

      sampler.typeId = m_module.defSampledImageType(sampler.imageTypeId);

      sampler.varId = m_module.newVar(
        m_module.defPointerType(
          sampler.typeId, spv::StorageClassUniformConstant),
        spv::StorageClassUniformConstant);

      std::string name = str::format("s", idx, suffix, depth ? "_shadow" : "");
      m_module.setDebugName(sampler.varId, name.c_str());

      m_module.decorateDescriptorSet(sampler.varId, 0);
      m_module.decorateBinding      (sampler.varId, bindingId);
    };

    const uint32_t binding = computeResourceSlotId(m_programInfo.type(),
      DxsoBindingType::Image,
      idx);

    const bool implicit = m_programInfo.majorVersion() < 2 || m_moduleInfo.options.forceSamplerTypeSpecConstants;

    if (!implicit) {
      DxsoSamplerType samplerType = 
        SamplerTypeFromTextureType(type);

      DclSampler(idx, binding, samplerType, false, implicit);

      if (samplerType != SamplerTypeTexture3D) {
        // We could also be depth compared!
        DclSampler(idx, binding, samplerType, true, implicit);
      }
    }
    else {
      // Could be any of these!
      // We will check with the spec constant at sample time.
      for (uint32_t i = 0; i < SamplerTypeCount; i++) {
        auto samplerType = static_cast<DxsoSamplerType>(i);

        DclSampler(idx, binding, samplerType, false, implicit);

        if (samplerType != SamplerTypeTexture3D)
          DclSampler(idx, binding, samplerType, true, implicit);
      }
    }

    DxsoSampler& sampler = m_samplers[idx];
    sampler.boundConst = m_module.specConstBool(true);
    sampler.type = type;
    m_module.decorateSpecId(sampler.boundConst, binding);
    m_module.setDebugName(sampler.boundConst,
      str::format("s", idx, "_bound").c_str());

    // Store descriptor info for the shader interface
    DxvkResourceSlot resource;
    resource.slot   = binding;
    resource.type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    resource.view   = implicit ? VK_IMAGE_VIEW_TYPE_MAX_ENUM : viewType;
    resource.access = VK_ACCESS_SHADER_READ_BIT;
    m_resourceSlots.push_back(resource);
  }


  uint32_t DxsoCompiler::emitArrayIndex(
            uint32_t          idx,
      const DxsoBaseRegister* relative) {
    uint32_t result = m_module.consti32(idx);

    if (relative != nullptr) {
      DxsoRegisterValue offset = emitRegisterLoad(*relative, DxsoRegMask(true, false, false, false), nullptr);

      result = m_module.opIAdd(
        getVectorTypeId(offset.type),
        result, offset.id);
    }

    return result;
  }


  DxsoRegisterPointer DxsoCompiler::emitInputPtr(
            bool              texture,
      const DxsoBaseRegister& reg,
      const DxsoBaseRegister* relative) {
    uint32_t idx = reg.id.num;

    // Account for the two color regs.
    if (texture)
      idx += 2;

    DxsoRegisterPointer input;

    input.type = DxsoVectorType{ DxsoScalarType::Float32, 4 };

    uint32_t index = this->emitArrayIndex(idx, relative);

    const uint32_t typeId = getVectorTypeId(input.type);
    input.id = m_module.opAccessChain(
      m_module.defPointerType(typeId, spv::StorageClassPrivate),
      m_vArray,
      1, &index);

    return input;
  }

  DxsoRegisterPointer DxsoCompiler::emitRegisterPtr(
      const char*             name,
            DxsoScalarType    ctype,
            uint32_t          ccount,
            uint32_t          defaultVal,
            spv::StorageClass storageClass,
            spv::BuiltIn      builtIn) {
    DxsoRegisterPointer result;

    DxsoRegisterInfo info;
    info.type.ctype    = ctype;
    info.type.ccount   = ccount;
    info.type.alength  = 1;
    info.sclass        = storageClass;

    result.type = DxsoVectorType{ ctype, ccount };
    if (builtIn == spv::BuiltInMax) {
      result.id = this->emitNewVariableDefault(info, defaultVal);
      m_module.setDebugName(result.id, name);
    }
    else {
      result.id = this->emitNewBuiltinVariable(
        info, builtIn, name, defaultVal);
    }

    return result;
  }


  DxsoRegisterValue DxsoCompiler::emitLoadConstant(
      const DxsoBaseRegister& reg,
      const DxsoBaseRegister* relative) {
    // struct cBuffer_t {
    //
    //   Type     Member        Index
    //
    //   float    f[256 or 224];       0
    //   int32_t  i[16];        1
    //   uint32_t boolBitmask;  2
    // }
    DxsoRegisterValue result = { };

    switch (reg.id.type) {
      case DxsoRegisterType::Const:
        result.type = { DxsoScalarType::Float32, 4 };

        if (!relative)
          result.id = m_cFloat.at(reg.id.num);
        break;
      
      case DxsoRegisterType::ConstInt:
        result.type = { DxsoScalarType::Sint32, 4 };
        result.id = m_cInt.at(reg.id.num);
        break;
      
      case DxsoRegisterType::ConstBool:
        result.type = { DxsoScalarType::Bool, 1 };
        result.id = m_cBool.at(reg.id.num);
        break;
      
      default: break;
    }

    if (result.id)
      return result;

    switch (reg.id.type) {
      case DxsoRegisterType::Const:
        if (!relative) {
          m_meta.maxConstIndexF = std::max(m_meta.maxConstIndexF, reg.id.num + 1);
          m_meta.maxConstIndexF = std::min(m_meta.maxConstIndexF, m_layout->floatCount);
        } else {
          m_meta.maxConstIndexF = m_layout->floatCount;
          m_meta.needsConstantCopies |= m_moduleInfo.options.strictConstantCopies
                                     || m_cFloat.at(reg.id.num) != 0;
        }
        break;
      
      case DxsoRegisterType::ConstInt:
        m_meta.maxConstIndexI = std::max(m_meta.maxConstIndexI, reg.id.num + 1);
        m_meta.maxConstIndexI = std::min(m_meta.maxConstIndexI, m_layout->intCount);
        break;
      
      case DxsoRegisterType::ConstBool:
        m_meta.maxConstIndexB = std::max(m_meta.maxConstIndexB, reg.id.num + 1);
        m_meta.maxConstIndexB = std::min(m_meta.maxConstIndexB, m_layout->boolCount);
        m_meta.boolConstantMask |= 1 << reg.id.num;
        break;
      
      default: break;
    }

    uint32_t relativeIdx = this->emitArrayIndex(reg.id.num, relative);

    if (reg.id.type != DxsoRegisterType::ConstBool) {
      uint32_t structIdx = reg.id.type == DxsoRegisterType::Const
        ? m_module.constu32(0)
        : m_module.constu32(1);

      std::array<uint32_t, 2> indices = { structIdx, relativeIdx };

      uint32_t typeId = getVectorTypeId(result.type);
      uint32_t ptrId = m_module.opAccessChain(
        m_module.defPointerType(typeId, spv::StorageClassUniform),
        m_cBuffer, indices.size(), indices.data());

      result.id = m_module.opLoad(typeId, ptrId);

      if (relative) {
        uint32_t constCount = m_module.constu32(m_layout->floatCount);

        // Expand condition to bvec4 since the result has four components
        uint32_t cond = m_module.opULessThan(m_module.defBoolType(), relativeIdx, constCount);
        std::array<uint32_t, 4> condIds = { cond, cond, cond, cond };

        cond = m_module.opCompositeConstruct(
          m_module.defVectorType(m_module.defBoolType(), 4),
          condIds.size(), condIds.data());

        result.id = m_module.opSelect(typeId, cond, result.id,
          m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f));
      }
    } else {
      // Bool constants have no relative indexing, so we can do the bitfield
      // magic for SWVP at compile time.

      uint32_t uintType  = getScalarTypeId(DxsoScalarType::Uint32);
      uint32_t uvec4Type = getVectorTypeId({ DxsoScalarType::Uint32, 4 });

      // If not SWVP, spec const this
      uint32_t bitfield;
      if (m_layout->bitmaskCount != 1) {
        std::array<uint32_t, 2> indices = { m_module.constu32(2), m_module.constu32(reg.id.num / 128) };

        uint32_t indexCount = m_layout->bitmaskCount == 1 ? 1 : 2;
        uint32_t accessType = m_layout->bitmaskCount == 1 ? uintType : uvec4Type;

        uint32_t ptrId = m_module.opAccessChain(
          m_module.defPointerType(accessType, spv::StorageClassUniform),
          m_cBuffer, indexCount, indices.data());

        bitfield = m_module.opLoad(accessType, ptrId);
      }
      else
        bitfield = m_boolSpecConstant;

      uint32_t bitIdx = m_module.consti32(reg.id.num % 32);

      if (m_layout->bitmaskCount != 1) {
        uint32_t index = (reg.id.num % 128) / 32;
        bitfield = m_module.opCompositeExtract(uintType, bitfield, 1, &index);
      }
      uint32_t bit = m_module.opBitFieldUExtract(
        uintType, bitfield, bitIdx, m_module.consti32(1));

      result.id = m_module.opINotEqual(
        getVectorTypeId(result.type),
        bit, m_module.constu32(0));
    }

    return result;
  }


  DxsoRegisterPointer DxsoCompiler::emitOutputPtr(
            bool              texcrdOut,
      const DxsoBaseRegister& reg,
      const DxsoBaseRegister* relative) {
    uint32_t idx = reg.id.num;

    // Account for the two color regs.
    if (texcrdOut)
      idx += 2;

    DxsoRegisterPointer input;

    input.type = DxsoVectorType{ DxsoScalarType::Float32, 4 };

    uint32_t index = this->emitArrayIndex(idx, relative);

    const uint32_t typeId = getVectorTypeId(input.type);
    input.id = m_module.opAccessChain(
      m_module.defPointerType(typeId, spv::StorageClassPrivate),
      m_oArray,
      1, &index);

    return input;
  }


  DxsoRegisterPointer DxsoCompiler::emitGetOperandPtr(
      const DxsoBaseRegister& reg,
      const DxsoBaseRegister* relative) {
    switch (reg.id.type) {
      case DxsoRegisterType::Temp: {
        DxsoRegisterPointer& ptr = m_rRegs.at(reg.id.num);
        if (ptr.id == 0) {
          std::string name = str::format("r", reg.id.num);
          ptr = this->emitRegisterPtr(
            name.c_str(), DxsoScalarType::Float32, 4,
            m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f));
        }
        return ptr;
      }

      case DxsoRegisterType::Input: {
        if (!(m_explicitInputs & 1u << reg.id.num)) {
          this->emitDclInterface(
            true, reg.id.num,
            DxsoSemantic{ DxsoUsage::Color, reg.id.num },
            IdentityWriteMask, false);
        }

        return this->emitInputPtr(false, reg, relative);
      }

      case DxsoRegisterType::PixelTexcoord:
      case DxsoRegisterType::Texture: {
        if (m_programInfo.type() == DxsoProgramTypes::PixelShader) {
          // Texture register

          // SM2, or SM 1.4
          if (reg.id.type == DxsoRegisterType::PixelTexcoord
          ||  m_programInfo.majorVersion() >= 2
          || (m_programInfo.majorVersion() == 1
           && m_programInfo.minorVersion() == 4)) {
            uint32_t adjustedNumber = reg.id.num + 2;
            if (!(m_explicitInputs & 1u << adjustedNumber)) {
              this->emitDclInterface(
                true, adjustedNumber,
                DxsoSemantic{ DxsoUsage::Texcoord, reg.id.num },
                IdentityWriteMask, false);
            }

            return this->emitInputPtr(true, reg, relative);
          }
          else {
            // User must use tex/texcoord to put data in this private register.
            // We use the an oob id which fxc never generates for the texcoord data.
            DxsoRegisterPointer& ptr = m_tRegs.at(reg.id.num);
            if (ptr.id == 0) {
              std::string name = str::format("t", reg.id.num);
              ptr = this->emitRegisterPtr(
                name.c_str(), DxsoScalarType::Float32, 4,
                m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f));
            }
            return ptr;
          }
        }
        else {
          // Address register
          if (m_vs.addr.id == 0) {
            m_vs.addr = this->emitRegisterPtr(
              "a0", DxsoScalarType::Sint32, 4,
              m_module.constvec4i32(0, 0, 0, 0));
          }
          return m_vs.addr;
        }
      }

      case DxsoRegisterType::RasterizerOut:
        switch (reg.id.num) {
          case RasterOutPosition:
            if (m_vs.oPos.id == 0) {
              m_vs.oPos = this->emitRegisterPtr(
                "oPos", DxsoScalarType::Float32, 4,
                m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f),
                spv::StorageClassOutput, spv::BuiltInPosition);
            }
            return m_vs.oPos;

          case RasterOutFog:
            if (m_fog.id == 0) {
              bool input = m_programInfo.type() == DxsoProgramType::PixelShader;
              DxsoSemantic semantic = DxsoSemantic{ DxsoUsage::Fog, 0 };

              uint32_t slot = RegisterLinkerSlot(semantic);

              uint32_t& slots = input
                ? m_interfaceSlots.inputSlots
                : m_interfaceSlots.outputSlots;

              slots |= 1u << slot;

              m_fog = this->emitRegisterPtr(
                input ? "vFog" : "oFog",
                DxsoScalarType::Float32, 1,
                input ? 0 : m_module.constf32(1.0f),
                input ? spv::StorageClassInput : spv::StorageClassOutput);

              m_entryPointInterfaces.push_back(m_fog.id);

              m_module.decorateLocation(m_fog.id, slot);
            }
            return m_fog;

          case RasterOutPointSize:
            if (m_vs.oPSize.id == 0) {
              m_vs.oPSize = this->emitRegisterPtr(
                "oPSize", DxsoScalarType::Float32, 1,
                m_module.constf32(0.0f),
                spv::StorageClassOutput, spv::BuiltInPointSize);
            }
            return m_vs.oPSize;
        }

      case DxsoRegisterType::ColorOut: {
        uint32_t idx = std::min(reg.id.num, 4u);

        if (m_ps.oColor[idx].id == 0) {
          std::string name = str::format("oC", idx);
          m_ps.oColor[idx] = this->emitRegisterPtr(
            name.c_str(), DxsoScalarType::Float32, 4,
            m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f),
            spv::StorageClassOutput);

          m_interfaceSlots.outputSlots |= 1u << idx;
          m_module.decorateLocation(m_ps.oColor[idx].id, idx);
          m_module.decorateIndex(m_ps.oColor[idx].id, 0);

          m_entryPointInterfaces.push_back(m_ps.oColor[idx].id);
          m_usedRTs |= (1u << idx);
        }
        return m_ps.oColor[idx];
      }

      case DxsoRegisterType::AttributeOut: {
        auto ptr = this->emitOutputPtr(false, reg, nullptr);

        if (!(m_explicitOutputs & 1u << reg.id.num)) {
          this->emitDclInterface(
            false, reg.id.num,
            DxsoSemantic{ DxsoUsage::Color, reg.id.num },
            IdentityWriteMask, false);

          m_module.opStore(ptr.id, m_module.constfReplicant(0, ptr.type.ccount));
        }

        return ptr;
      }

      case DxsoRegisterType::Output: {
        bool texcrdOut = m_programInfo.type() == DxsoProgramTypes::VertexShader
                      && m_programInfo.majorVersion() != 3;

        auto ptr = this->emitOutputPtr(texcrdOut, reg, !texcrdOut ? relative : nullptr);

        if (texcrdOut) {
          uint32_t adjustedNumber = reg.id.num + 2;
          if (!(m_explicitOutputs & 1u << adjustedNumber)) {
            this->emitDclInterface(
              false, adjustedNumber,
              DxsoSemantic{ DxsoUsage::Texcoord, reg.id.num },
              IdentityWriteMask, false);

            m_module.opStore(ptr.id, m_module.constfReplicant(0, ptr.type.ccount));
          }
        }

        return ptr;
      }

      case DxsoRegisterType::DepthOut:
        if (m_ps.oDepth.id == 0) {
          m_module.setExecutionMode(m_entryPointId,
            spv::ExecutionModeDepthReplacing);

          m_ps.oDepth = this->emitRegisterPtr(
            "oDepth", DxsoScalarType::Float32, 1,
            m_module.constf32(0.0f),
            spv::StorageClassOutput, spv::BuiltInFragDepth);
        }
        return m_ps.oDepth;

      case DxsoRegisterType::Loop:
        if (m_loopCounter.id == 0) {
          m_loopCounter = this->emitRegisterPtr(
            "aL", DxsoScalarType::Sint32, 1,
            m_module.consti32(0));
        }
        return m_loopCounter;

      case DxsoRegisterType::MiscType:
        if (reg.id.num == MiscTypePosition) {
          if (m_ps.vPos.id == 0) {
            m_ps.vPos = this->emitRegisterPtr(
              "vPos", DxsoScalarType::Float32, 4, 0);
          }
          return m_ps.vPos;
        }
        else { // MiscTypeFace
          if (m_ps.vFace.id == 0) {
            m_ps.vFace = this->emitRegisterPtr(
              "vFace", DxsoScalarType::Float32, 4, 0);
          }
          return m_ps.vFace;
        }

      case DxsoRegisterType::Predicate: {
        DxsoRegisterPointer& ptr = m_pRegs.at(reg.id.num);
        if (ptr.id == 0) {
          std::string name = str::format("p", reg.id.num);
          ptr = this->emitRegisterPtr(
            name.c_str(), DxsoScalarType::Bool, 4,
            m_module.constvec4b32(false, false, false, false));
        }
        return ptr;
      }

      default: {
        //Logger::warn(str::format("emitGetOperandPtr: unhandled reg type: ", reg.id.type));

        DxsoRegisterPointer nullPointer;
        nullPointer.id = 0;
        return nullPointer;
      }
    }
  }


  uint32_t DxsoCompiler::emitBoolComparison(DxsoVectorType type, DxsoComparison cmp, uint32_t a, uint32_t b) {
    const uint32_t typeId = getVectorTypeId(type);
    switch (cmp) {
      default:
      case DxsoComparison::Never:        return m_module.constbReplicant(false, type.ccount);  break;
      case DxsoComparison::GreaterThan:  return m_module.opFOrdGreaterThan     (typeId, a, b); break;
      case DxsoComparison::Equal:        return m_module.opFOrdEqual           (typeId, a, b); break;
      case DxsoComparison::GreaterEqual: return m_module.opFOrdGreaterThanEqual(typeId, a, b); break;
      case DxsoComparison::LessThan:     return m_module.opFOrdLessThan        (typeId, a, b); break;
      case DxsoComparison::NotEqual:     return m_module.opFOrdNotEqual        (typeId, a, b); break;
      case DxsoComparison::LessEqual:    return m_module.opFOrdLessThanEqual   (typeId, a, b); break;
      case DxsoComparison::Always:       return m_module.constbReplicant(true, type.ccount);   break;
    }
}


  DxsoRegisterValue DxsoCompiler::emitValueLoad(
            DxsoRegisterPointer ptr) {
    DxsoRegisterValue result;
    result.type = ptr.type;
    result.id   = m_module.opLoad(
      getVectorTypeId(result.type),
      ptr.id);
    return result;
  }


  DxsoRegisterValue DxsoCompiler::applyPredicate(DxsoRegisterValue pred, DxsoRegisterValue dst, DxsoRegisterValue src) {
    if (dst.type.ccount != pred.type.ccount) {
      DxsoRegMask mask = DxsoRegMask(
        pred.type.ccount > 0,
        pred.type.ccount > 1,
        pred.type.ccount > 2,
        pred.type.ccount > 3);

      pred = emitRegisterSwizzle(pred, IdentitySwizzle, mask);
    }

    dst.id = m_module.opSelect(
      getVectorTypeId(dst.type),
      pred.id,
      src.id, dst.id);

    return dst;
  }


  void DxsoCompiler::emitValueStore(
          DxsoRegisterPointer     ptr,
          DxsoRegisterValue       value,
          DxsoRegMask             writeMask,
          DxsoRegisterValue       predicate) {
    // If the source value consists of only one component,
    // it is stored in all components of the destination.
    if (value.type.ccount == 1)
      value = emitRegisterExtend(value, writeMask.popCount());

    if (ptr.type.ccount == writeMask.popCount()) {
      if (predicate.id)
        value = applyPredicate(predicate, emitValueLoad(ptr), value);

      // Simple case: We write to the entire register
      m_module.opStore(ptr.id, value.id);
    } else {
      // We only write to part of the destination
      // register, so we need to load and modify it
      DxsoRegisterValue tmp = emitValueLoad(ptr);
      tmp = emitRegisterInsert(tmp, value, writeMask);

      if (predicate.id)
        value = applyPredicate(predicate, emitValueLoad(ptr), tmp);

      m_module.opStore(ptr.id, tmp.id);
    }
  }


  DxsoRegisterValue DxsoCompiler::emitClampBoundReplicant(
            DxsoRegisterValue       srcValue,
            float                   lb,
            float                   ub) {
    srcValue.id = m_module.opFClamp(getVectorTypeId(srcValue.type), srcValue.id,
      m_module.constfReplicant(lb, srcValue.type.ccount),
      m_module.constfReplicant(ub, srcValue.type.ccount));

    return srcValue;
  }


  DxsoRegisterValue DxsoCompiler::emitSaturate(
            DxsoRegisterValue       srcValue) {
    return emitClampBoundReplicant(srcValue, 0.0f, 1.0f);
  }


  DxsoRegisterValue DxsoCompiler::emitDot(
            DxsoRegisterValue       a,
            DxsoRegisterValue       b) {
    DxsoRegisterValue dot;
    dot.type        = a.type;
    dot.type.ccount = 1;

    dot.id = m_module.opDot(getVectorTypeId(dot.type), a.id, b.id);

    return dot;
  }


  DxsoRegisterValue DxsoCompiler::emitRegisterInsert(
            DxsoRegisterValue       dstValue,
            DxsoRegisterValue       srcValue,
            DxsoRegMask             srcMask) {
    DxsoRegisterValue result;
    result.type = dstValue.type;

    const uint32_t typeId = getVectorTypeId(result.type);

    if (srcMask.popCount() == 0) {
      // Nothing to do if the insertion mask is empty
      result.id = dstValue.id;
    } else if (dstValue.type.ccount == 1) {
      // Both values are scalar, so the first component
      // of the write mask decides which one to take.
      result.id = srcMask[0] ? srcValue.id : dstValue.id;
    } else if (srcValue.type.ccount == 1) {
      // The source value is scalar. Since OpVectorShuffle
      // requires both arguments to be vectors, we have to
      // use OpCompositeInsert to modify the vector instead.
      const uint32_t componentId = srcMask.firstSet();

      result.id = m_module.opCompositeInsert(typeId,
        srcValue.id, dstValue.id, 1, &componentId);
    } else {
      // Both arguments are vectors. We can determine which
      // components to take from which vector and use the
      // OpVectorShuffle instruction.
      std::array<uint32_t, 4> components;
      uint32_t srcComponentId = dstValue.type.ccount;

      for (uint32_t i = 0; i < dstValue.type.ccount; i++)
        components.at(i) = srcMask[i] ? srcComponentId++ : i;

      result.id = m_module.opVectorShuffle(
        typeId, dstValue.id, srcValue.id,
        dstValue.type.ccount, components.data());
    }

    return result;
  }


  DxsoRegisterValue DxsoCompiler::emitRegisterLoadRaw(
      const DxsoBaseRegister& reg,
      const DxsoBaseRegister* relative) {
    switch (reg.id.type) {
      case DxsoRegisterType::Const:
      case DxsoRegisterType::ConstInt:
      case DxsoRegisterType::ConstBool:
        return emitLoadConstant(reg, relative);
      
      default:
        return emitValueLoad(emitGetOperandPtr(reg, relative));
    }
  }


  DxsoRegisterValue DxsoCompiler::emitRegisterExtend(
            DxsoRegisterValue       value,
            uint32_t                size) {
    if (size == 1)
      return value;

    std::array<uint32_t, 4> ids = {{
      value.id, value.id,
      value.id, value.id,
    }};

    DxsoRegisterValue result;
    result.type.ctype  = value.type.ctype;
    result.type.ccount = size;
    result.id = m_module.opCompositeConstruct(
      getVectorTypeId(result.type),
      size, ids.data());
    return result;
  }


  DxsoRegisterValue DxsoCompiler::emitRegisterSwizzle(
            DxsoRegisterValue       value,
            DxsoRegSwizzle          swizzle,
            DxsoRegMask             writeMask) {
    if (value.type.ccount == 1)
      return emitRegisterExtend(value, writeMask.popCount());

    std::array<uint32_t, 4> indices;

    uint32_t dstIndex = 0;

    for (uint32_t i = 0; i < 4; i++) {
      if (writeMask[i])
        indices[dstIndex++] = swizzle[i];
    }

    // If the swizzle combined with the mask can be reduced
    // to a no-op, we don't need to insert any instructions.
    bool isIdentitySwizzle = dstIndex == value.type.ccount;

    for (uint32_t i = 0; i < dstIndex && isIdentitySwizzle; i++)
      isIdentitySwizzle &= indices[i] == i;

    if (isIdentitySwizzle)
      return value;

    // Use OpCompositeExtract if the resulting vector contains
    // only one component, and OpVectorShuffle if it is a vector.
    DxsoRegisterValue result;
    result.type.ctype  = value.type.ctype;
    result.type.ccount = dstIndex;

    const uint32_t typeId = getVectorTypeId(result.type);

    if (dstIndex == 1) {
      result.id = m_module.opCompositeExtract(
        typeId, value.id, 1, indices.data());
    } else {
      result.id = m_module.opVectorShuffle(
        typeId, value.id, value.id,
        dstIndex, indices.data());
    }

    return result;
  }


  DxsoRegisterValue DxsoCompiler::emitSrcOperandPreSwizzleModifiers(
            DxsoRegisterValue       value,
            DxsoRegModifier         modifier) {
    // r / r.z
    // r / r.w
    if (modifier == DxsoRegModifier::Dz
     || modifier == DxsoRegModifier::Dw) {
      const uint32_t index = modifier == DxsoRegModifier::Dz ? 2 : 3;

      std::array<uint32_t, 4> indices = { index, index, index, index };

      uint32_t component = m_module.opVectorShuffle(
        getVectorTypeId(value.type), value.id, value.id, value.type.ccount, indices.data());

      value.id = m_module.opFDiv(
        getVectorTypeId(value.type), value.id, component);
    }

    return value;
  }


  DxsoRegisterValue DxsoCompiler::emitSrcOperandPostSwizzleModifiers(
            DxsoRegisterValue       value,
            DxsoRegModifier         modifier) {
    // r - 0.5
    if (modifier == DxsoRegModifier::Bias
     || modifier == DxsoRegModifier::BiasNeg) {
      uint32_t halfVec = m_module.constfReplicant(
        0.5f, value.type.ccount);

      value.id = m_module.opFSub(
        getVectorTypeId(value.type), value.id, halfVec);
    }

    // fma(r, 2.0f, -1.0f)
    if (modifier == DxsoRegModifier::Sign
     || modifier == DxsoRegModifier::SignNeg) {
      uint32_t twoVec = m_module.constfReplicant(
        2.0f, value.type.ccount);

      uint32_t minusOneVec = m_module.constfReplicant(
        -1.0f, value.type.ccount);

      value.id = m_module.opFFma(
        getVectorTypeId(value.type), value.id, twoVec, minusOneVec);
    }

    // 1 - r
    if (modifier == DxsoRegModifier::Comp) {
      uint32_t oneVec = m_module.constfReplicant(
        1.0f, value.type.ccount);

      value.id = m_module.opFSub(
        getVectorTypeId(value.type), oneVec, value.id);
    }

    // r * 2
    if (modifier == DxsoRegModifier::X2
     || modifier == DxsoRegModifier::X2Neg) {
      uint32_t twoVec = m_module.constfReplicant(
        2.0f, value.type.ccount);

      value.id = m_module.opFMul(
        getVectorTypeId(value.type), value.id, twoVec);
    }

    // abs( r )
    if (modifier == DxsoRegModifier::Abs
     || modifier == DxsoRegModifier::AbsNeg) {
      value.id = m_module.opFAbs(
        getVectorTypeId(value.type), value.id);
    }

    // !r
    if (modifier == DxsoRegModifier::Not) {
      value.id =
        m_module.opLogicalNot(getVectorTypeId(value.type), value.id);
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
      value.id = m_module.opFNegate(
        getVectorTypeId(value.type), value.id);
    }

    return value;
  }

  DxsoRegisterValue DxsoCompiler::emitRegisterLoad(
      const DxsoBaseRegister& reg,
            DxsoRegMask       writeMask,
      const DxsoBaseRegister* relative) {
    // Load operand from the operand pointer
    DxsoRegisterValue result = emitRegisterLoadRaw(reg, relative);

    // PS 1.x clamps float constants
    if (m_programInfo.type() == DxsoProgramType::PixelShader && m_programInfo.majorVersion() == 1
      && reg.id.type == DxsoRegisterType::Const)
      result = emitClampBoundReplicant(result, -1.0f, 1.0f);

    // Apply operand modifiers
    result = emitSrcOperandPreSwizzleModifiers(result, reg.modifier);

    // Apply operand swizzle to the operand value
    result = emitRegisterSwizzle(result, reg.swizzle, writeMask);

    // Apply operand modifiers
    result = emitSrcOperandPostSwizzleModifiers(result, reg.modifier);
    return result;
  }

  void DxsoCompiler::emitDcl(const DxsoInstructionContext& ctx) {
    auto id = ctx.dst.id;

    if (id.type == DxsoRegisterType::Sampler) {
      this->emitDclSampler(
        ctx.dst.id.num,
        ctx.dcl.textureType);
    }
    else if (id.type == DxsoRegisterType::Input
          || id.type == DxsoRegisterType::Texture
          || id.type == DxsoRegisterType::Output) {
      DxsoSemantic semantic = ctx.dcl.semantic;

      uint32_t vIndex = id.num;

      if (m_programInfo.type() == DxsoProgramTypes::PixelShader) {
        // Semantic in PS < 3 is based upon id.
        if (m_programInfo.majorVersion() < 3) {
          // Account for the two color registers.
          if (id.type == DxsoRegisterType::Texture)
            vIndex += 2;

          semantic = DxsoSemantic{
            id.type == DxsoRegisterType::Texture ? DxsoUsage::Texcoord : DxsoUsage::Color,
            id.num };
        }
      }

      this->emitDclInterface(
        id.type != DxsoRegisterType::Output,
        vIndex,
        semantic,
        ctx.dst.mask,
        ctx.dst.centroid);
    }
    else {
      //Logger::warn(str::format("DxsoCompiler::emitDcl: unhandled register type ", id.type));
    }
  }

  void DxsoCompiler::emitDef(const DxsoInstructionContext& ctx) {
    switch (ctx.instruction.opcode) {
      case DxsoOpcode::Def:  emitDefF(ctx); break;
      case DxsoOpcode::DefI: emitDefI(ctx); break;
      case DxsoOpcode::DefB: emitDefB(ctx); break;
      default:
        throw DxvkError("DxsoCompiler::emitDef: Invalid definition opcode");
        break;
    }
  }

  void DxsoCompiler::emitDefF(const DxsoInstructionContext& ctx) {
    const float* data = ctx.def.float32;

    uint32_t constId = m_module.constvec4f32(data[0], data[1], data[2], data[3]);
    m_cFloat.at(ctx.dst.id.num) = constId;

    std::string name = str::format("cF", ctx.dst.id.num, "_def");
    m_module.setDebugName(constId, name.c_str());

    DxsoDefinedConstant constant;
    constant.uboIdx = ctx.dst.id.num;
    for (uint32_t i = 0; i < 4; i++)
      constant.float32[i] = data[i];
    m_constants.push_back(constant);
  }

  void DxsoCompiler::emitDefI(const DxsoInstructionContext& ctx) {
    const int32_t* data = ctx.def.int32;

    uint32_t constId = m_module.constvec4i32(data[0], data[1], data[2], data[3]);
    m_cInt.at(ctx.dst.id.num) = constId;

    std::string name = str::format("cI", ctx.dst.id.num, "_def");
    m_module.setDebugName(constId, name.c_str());
  }

  void DxsoCompiler::emitDefB(const DxsoInstructionContext& ctx) {
    const int32_t* data = ctx.def.int32;

    uint32_t constId = m_module.constBool(data[0] != 0);
    m_cBool.at(ctx.dst.id.num) = constId;

    std::string name = str::format("cB", ctx.dst.id.num, "_def");
    m_module.setDebugName(constId, name.c_str());
  }


  bool DxsoCompiler::isScalarRegister(DxsoRegisterId id) {
    return id == DxsoRegisterId{DxsoRegisterType::DepthOut, 0}
        || id == DxsoRegisterId{DxsoRegisterType::RasterizerOut, RasterOutPointSize}
        || id == DxsoRegisterId{DxsoRegisterType::RasterizerOut, RasterOutFog};
  }


  void DxsoCompiler::emitMov(const DxsoInstructionContext& ctx) {
    DxsoRegisterPointer dst = emitGetOperandPtr(ctx.dst);

    DxsoRegMask mask = ctx.dst.mask;

    if (isScalarRegister(ctx.dst.id))
      mask = DxsoRegMask(true, false, false, false);

    DxsoRegisterValue src0 = emitRegisterLoad(ctx.src[0], mask);

    DxsoRegisterValue result;
    result.type.ctype  = dst.type.ctype;
    result.type.ccount = mask.popCount();

    const uint32_t typeId = getVectorTypeId(result.type);

    if (dst.type.ctype != src0.type.ctype) {
      // We have Mova for this... but it turns out Mov has the same behaviour in d3d9!

      // Convert float -> int32_t
      // and vice versa
      if (dst.type.ctype == DxsoScalarType::Sint32) {
        // We need to floor for VS 1.1 and below, the documentation is a dirty stinking liar.
        if (m_programInfo.majorVersion() < 2 && m_programInfo.minorVersion() < 2)
          result.id = m_module.opFloor(getVectorTypeId(src0.type), src0.id);
        else
          result.id = m_module.opRound(getVectorTypeId(src0.type), src0.id);

        result.id = m_module.opConvertFtoS(typeId, result.id);
      }
      else // Float32
        result.id = m_module.opConvertStoF(typeId, src0.id);
    }
    else // No special stuff needed!
      result.id = src0.id;

    this->emitDstStore(dst, result, mask, ctx.dst.saturate, emitPredicateLoad(ctx), ctx.dst.shift, ctx.dst.id);
  }


  void DxsoCompiler::emitVectorAlu(const DxsoInstructionContext& ctx) {
    const auto& src = ctx.src;

    DxsoRegMask mask = ctx.dst.mask;

    DxsoRegisterPointer dst = emitGetOperandPtr(ctx.dst);

    if (isScalarRegister(ctx.dst.id))
      mask = DxsoRegMask(true, false, false, false);

    DxsoRegisterValue result;
    result.type.ctype  = dst.type.ctype;
    result.type.ccount = mask.popCount();

    DxsoVectorType scalarType = result.type;
    scalarType.ccount = 1;

    const uint32_t typeId       = getVectorTypeId(result.type);
    const uint32_t scalarTypeId = getVectorTypeId(scalarType);

    const DxsoOpcode opcode = ctx.instruction.opcode;
    switch (opcode) {
      case DxsoOpcode::Add:
        result.id = m_module.opFAdd(typeId,
          emitRegisterLoad(src[0], mask).id,
          emitRegisterLoad(src[1], mask).id);
        break;
      case DxsoOpcode::Sub:
        result.id = m_module.opFSub(typeId,
          emitRegisterLoad(src[0], mask).id,
          emitRegisterLoad(src[1], mask).id);
        break;
      case DxsoOpcode::Mad:
        if (!m_moduleInfo.options.longMad) {
          result.id = m_module.opFFma(typeId,
            emitRegisterLoad(src[0], mask).id,
            emitRegisterLoad(src[1], mask).id,
            emitRegisterLoad(src[2], mask).id);
        }
        else {
          result.id = m_module.opFMul(typeId,
            emitRegisterLoad(src[0], mask).id,
            emitRegisterLoad(src[1], mask).id);

          result.id = m_module.opFAdd(typeId,
            result.id,
            emitRegisterLoad(src[2], mask).id);
        }
        break;
      case DxsoOpcode::Mul:
        result.id = m_module.opFMul(typeId,
          emitRegisterLoad(src[0], mask).id,
          emitRegisterLoad(src[1], mask).id);
        break;
      case DxsoOpcode::Rcp:
        result.id = m_module.opFDiv(typeId,
          m_module.constfReplicant(1.0f, result.type.ccount),
          emitRegisterLoad(src[0], mask).id);

        if (m_moduleInfo.options.d3d9FloatEmulation) {
          result.id = m_module.opNMin(typeId, result.id,
            m_module.constfReplicant(FLT_MAX, result.type.ccount));
        }
        break;
      case DxsoOpcode::Rsq: 
        result.id = m_module.opFAbs(typeId,
          emitRegisterLoad(src[0], mask).id);

        result.id = m_module.opInverseSqrt(typeId,
          result.id);

        if (m_moduleInfo.options.d3d9FloatEmulation) {
          result.id = m_module.opNMin(typeId, result.id,
            m_module.constfReplicant(FLT_MAX, result.type.ccount));
        }
        break;
      case DxsoOpcode::Dp3: {
        DxsoRegMask srcMask(true, true, true, false);
        result = emitDot(
          emitRegisterLoad(src[0], srcMask),
          emitRegisterLoad(src[1], srcMask));
        break;
      }
      case DxsoOpcode::Dp4:
        result = emitDot(
          emitRegisterLoad(src[0], IdentityWriteMask),
          emitRegisterLoad(src[1], IdentityWriteMask));
        break;
      case DxsoOpcode::Slt:
      case DxsoOpcode::Sge: {
        const uint32_t boolTypeId =
          getVectorTypeId({ DxsoScalarType::Bool, result.type.ccount });

        uint32_t cmpResult = opcode == DxsoOpcode::Slt
          ? m_module.opFOrdLessThan        (boolTypeId, emitRegisterLoad(src[0], mask).id, emitRegisterLoad(src[1], mask).id)
          : m_module.opFOrdGreaterThanEqual(boolTypeId, emitRegisterLoad(src[0], mask).id, emitRegisterLoad(src[1], mask).id);

        result.id = m_module.opSelect(typeId, cmpResult,
          m_module.constfReplicant(1.0f, result.type.ccount),
          m_module.constfReplicant(0.0f, result.type.ccount));
        break;
      }
      case DxsoOpcode::Min:
        result.id = m_module.opFMin(typeId,
          emitRegisterLoad(src[0], mask).id,
          emitRegisterLoad(src[1], mask).id);
        break;
      case DxsoOpcode::Max:
        result.id = m_module.opFMax(typeId,
          emitRegisterLoad(src[0], mask).id,
          emitRegisterLoad(src[1], mask).id);
        break;
      case DxsoOpcode::ExpP:
        if (m_programInfo.majorVersion() < 2) {
          DxsoRegMask srcMask(true, false, false, false);
          uint32_t src0 = emitRegisterLoad(src[0], srcMask).id;

          uint32_t index = 0;

          std::array<uint32_t, 4> resultIndices;

          if (mask[0]) resultIndices[index++] = m_module.opExp2(scalarTypeId, m_module.opFloor(scalarTypeId, src0));
          if (mask[1]) resultIndices[index++] = m_module.opFSub(scalarTypeId, src0, m_module.opFloor(scalarTypeId, src0));
          if (mask[2]) resultIndices[index++] = m_module.opExp2(scalarTypeId, src0);
          if (mask[3]) resultIndices[index++] = m_module.constf32(1.0f);

          if (result.type.ccount == 1)
            result.id = resultIndices[0];
          else
            result.id = m_module.opCompositeConstruct(typeId, result.type.ccount, resultIndices.data());

          break;
        }
      case DxsoOpcode::Exp:
        result.id = m_module.opExp2(typeId,
          emitRegisterLoad(src[0], mask).id);
        break;
      case DxsoOpcode::Pow: {
        uint32_t base = emitRegisterLoad(src[0], mask).id;
        base = m_module.opFAbs(typeId, base);

        uint32_t exponent = emitRegisterLoad(src[1], mask).id;

        result.id = m_module.opPow(typeId, base, exponent);

        if (m_moduleInfo.options.strictPow && m_moduleInfo.options.d3d9FloatEmulation) {
          DxsoRegisterValue cmp;
          cmp.type  = { DxsoScalarType::Bool, result.type.ccount };
          cmp.id    = m_module.opFOrdEqual(getVectorTypeId(cmp.type),
            exponent, m_module.constfReplicant(0.0f, cmp.type.ccount));

          result.id = m_module.opSelect(typeId, cmp.id,
            m_module.constfReplicant(1.0f, cmp.type.ccount), result.id);
        }
        break;
      }
      case DxsoOpcode::Crs: {
        DxsoRegMask vec3Mask(true, true, true, false);
        
        DxsoRegisterValue crossValue;
        crossValue.type = { DxsoScalarType::Float32, 3 };
        crossValue.id = m_module.opCross(getVectorTypeId(crossValue.type),
          emitRegisterLoad(src[0], vec3Mask).id,
          emitRegisterLoad(src[1], vec3Mask).id);

        std::array<uint32_t, 3> indices = { 0, 0, 0 };

        uint32_t index = 0;
        for (uint32_t i = 0; i < indices.size(); i++) {
          if (mask[i])
            indices[index++] = m_module.opCompositeExtract(m_module.defFloatType(32), crossValue.id, 1, &i);
        }

        result.id = m_module.opCompositeConstruct(getVectorTypeId(result.type), result.type.ccount, indices.data());

        break;
      }
      case DxsoOpcode::Abs:
        result.id = m_module.opFAbs(typeId,
          emitRegisterLoad(src[0], mask).id);
        break;
      case DxsoOpcode::Sgn:
        result.id = m_module.opFSign(typeId,
          emitRegisterLoad(src[0], mask).id);
        break;
      case DxsoOpcode::Nrm: {
        // Nrm is 3D...
        DxsoRegMask srcMask(true, true, true, false);
        auto vec3 = emitRegisterLoad(src[0], srcMask);

        DxsoRegisterValue dot = emitDot(vec3, vec3);
        dot.id = m_module.opInverseSqrt (scalarTypeId, dot.id);
        if (m_moduleInfo.options.d3d9FloatEmulation) {
          dot.id = m_module.opNMin        (scalarTypeId, dot.id,
            m_module.constf32(FLT_MAX));
        }

        // r * rsq(r . r);
        result.id = m_module.opVectorTimesScalar(
          typeId,
          emitRegisterLoad(src[0], mask).id,
          dot.id);
        break;
      }
      case DxsoOpcode::SinCos: {
        DxsoRegMask srcMask(true, false, false, false);
        uint32_t src0 = emitRegisterLoad(src[0], srcMask).id;

        std::array<uint32_t, 4> sincosVectorIndices = { 0, 0, 0, 0 };

        uint32_t index = 0;
        if (mask[0])
          sincosVectorIndices[index++] = m_module.opCos(scalarTypeId, src0);

        if (mask[1])
          sincosVectorIndices[index++] = m_module.opSin(scalarTypeId, src0);

        for (; index < result.type.ccount; index++) {
          if (sincosVectorIndices[index] == 0)
            sincosVectorIndices[index] = m_module.constf32(0.0f);
        }
            
        if (result.type.ccount == 1)
          result.id = sincosVectorIndices[0];
        else
          result.id = m_module.opCompositeConstruct(typeId, result.type.ccount, sincosVectorIndices.data());

        break;
      }
      case DxsoOpcode::Lit: {
        DxsoRegMask srcMask(true, true, true, true);
        uint32_t srcOp = emitRegisterLoad(src[0], srcMask).id;

        const uint32_t x = 0;
        const uint32_t y = 1;
        const uint32_t w = 3;

        uint32_t srcX = m_module.opCompositeExtract(scalarTypeId, srcOp, 1, &x);
        uint32_t srcY = m_module.opCompositeExtract(scalarTypeId, srcOp, 1, &y);
        uint32_t srcW = m_module.opCompositeExtract(scalarTypeId, srcOp, 1, &w);

        uint32_t power = m_module.opFClamp(
          scalarTypeId, srcW,
          m_module.constf32(-127.9961f), m_module.constf32(127.9961f));

        std::array<uint32_t, 4> resultIndices;

        uint32_t index = 0;

        if (mask[0]) resultIndices[index++] = m_module.constf32(1.0f);
        if (mask[1]) resultIndices[index++] = m_module.opFMax(scalarTypeId, srcX, m_module.constf32(0));
        if (mask[2]) resultIndices[index++] = m_module.opPow (scalarTypeId, m_module.opFMax(scalarTypeId, srcY, m_module.constf32(0)), power);
        if (mask[3]) resultIndices[index++] = m_module.constf32(1.0f);

        const uint32_t boolType = m_module.defBoolType();
        uint32_t zTestX = m_module.opFOrdGreaterThanEqual(boolType, srcX, m_module.constf32(0));
        uint32_t zTestY = m_module.opFOrdGreaterThanEqual(boolType, srcY, m_module.constf32(0));
        uint32_t zTest  = m_module.opLogicalAnd(boolType, zTestX, zTestY);

        if (result.type.ccount > 2)
          resultIndices[2] = m_module.opSelect(
            scalarTypeId,
            zTest,
            resultIndices[2],
            m_module.constf32(0.0f));

        if (result.type.ccount == 1)
          result.id = resultIndices[0];
        else
          result.id = m_module.opCompositeConstruct(typeId, result.type.ccount, resultIndices.data());
        break;
      }
      case DxsoOpcode::Dst: {
        //dest.x = 1;
        //dest.y = src0.y * src1.y;
        //dest.z = src0.z;
        //dest.w = src1.w;

        DxsoRegMask srcMask(true, true, true, true);

        uint32_t src0 = emitRegisterLoad(src[0], srcMask).id;
        uint32_t src1 = emitRegisterLoad(src[1], srcMask).id;

        const uint32_t y = 1;
        const uint32_t z = 2;
        const uint32_t w = 3;

        uint32_t src0Y = m_module.opCompositeExtract(scalarTypeId, src0, 1, &y);
        uint32_t src1Y = m_module.opCompositeExtract(scalarTypeId, src1, 1, &y);

        uint32_t src0Z = m_module.opCompositeExtract(scalarTypeId, src0, 1, &z);
        uint32_t src1W = m_module.opCompositeExtract(scalarTypeId, src1, 1, &w);

        std::array<uint32_t, 4> resultIndices;
        resultIndices[0] = m_module.constf32(1.0f);
        resultIndices[1] = m_module.opFMul(scalarTypeId, src0Y, src1Y);
        resultIndices[2] = src0Z;
        resultIndices[3] = src1W;

        if (result.type.ccount == 1)
          result.id = resultIndices[0];
        else
          result.id = m_module.opCompositeConstruct(typeId, result.type.ccount, resultIndices.data());
        break;
      }
      case DxsoOpcode::LogP:
      case DxsoOpcode::Log:
        result.id = m_module.opFAbs(typeId, emitRegisterLoad(src[0], mask).id);
        result.id = m_module.opLog2(typeId, result.id);
        if (m_moduleInfo.options.d3d9FloatEmulation) {
          result.id = m_module.opNMax(typeId, result.id,
            m_module.constfReplicant(-FLT_MAX, result.type.ccount));
        }
        break;
      case DxsoOpcode::Lrp:
        result.id = m_module.opFMix(typeId,
          emitRegisterLoad(src[2], mask).id,
          emitRegisterLoad(src[1], mask).id,
          emitRegisterLoad(src[0], mask).id);
        break;
      case DxsoOpcode::Frc:
        result.id = m_module.opFract(typeId,
          emitRegisterLoad(src[0], mask).id);
        break;
      case DxsoOpcode::Cmp: {
        const uint32_t boolTypeId =
          getVectorTypeId({ DxsoScalarType::Bool, result.type.ccount });

        uint32_t cmp = m_module.opFOrdGreaterThanEqual(
          boolTypeId,
          emitRegisterLoad(src[0], mask).id,
          m_module.constfReplicant(0.0f, result.type.ccount));

        result.id = m_module.opSelect(
          typeId, cmp,
          emitRegisterLoad(src[1], mask).id,
          emitRegisterLoad(src[2], mask).id);
        break;
      }
      case DxsoOpcode::Cnd: {
        const uint32_t boolTypeId =
          getVectorTypeId({ DxsoScalarType::Bool, result.type.ccount });

        uint32_t cmp = m_module.opFOrdGreaterThan(
          boolTypeId,
          emitRegisterLoad(src[0], mask).id,
          m_module.constfReplicant(0.5f, result.type.ccount));

        result.id = m_module.opSelect(
          typeId, cmp,
          emitRegisterLoad(src[1], mask).id,
          emitRegisterLoad(src[2], mask).id);
        break;
      }
      case DxsoOpcode::Dp2Add: {
        DxsoRegMask dotSrcMask(true, true, false, false);
        DxsoRegMask addSrcMask(true, false, false, false);

        DxsoRegisterValue dot = emitDot(
          emitRegisterLoad(src[0], dotSrcMask),
          emitRegisterLoad(src[1], dotSrcMask));

        dot.id = m_module.opFAdd(scalarTypeId,
          dot.id, emitRegisterLoad(src[2], addSrcMask).id);

        result.id   = dot.id;
        result.type = scalarType;
        break;
      }
      case DxsoOpcode::DsX:
        result.id = m_module.opDpdx(
          typeId, emitRegisterLoad(src[0], mask).id);
        break;
      case DxsoOpcode::DsY:
        result.id = m_module.opDpdy(
          typeId, emitRegisterLoad(src[0], mask).id);
        break;
      default:
        Logger::warn(str::format("DxsoCompiler::emitVectorAlu: unimplemented op ", opcode));
        return;
    }

    this->emitDstStore(dst, result, mask, ctx.dst.saturate, emitPredicateLoad(ctx), ctx.dst.shift, ctx.dst.id);
  }


  void DxsoCompiler::emitPredicateOp(const DxsoInstructionContext& ctx) {
    const auto& src = ctx.src;

    DxsoRegMask mask = ctx.dst.mask;

    DxsoRegisterPointer dst = emitGetOperandPtr(ctx.dst);

    DxsoRegisterValue result;
    result.type.ctype  = dst.type.ctype;
    result.type.ccount = mask.popCount();

    result.id = emitBoolComparison(
      result.type, ctx.instruction.specificData.comparison,
      emitRegisterLoad(src[0], mask).id, emitRegisterLoad(src[1], mask).id);

    this->emitValueStore(dst, result, mask, emitPredicateLoad(ctx));
  }


  void DxsoCompiler::emitMatrixAlu(const DxsoInstructionContext& ctx) {
    const DxsoOpcode opcode = ctx.instruction.opcode;

    uint32_t dotCount;
    uint32_t componentCount;

    switch (opcode) {
      case DxsoOpcode::M3x2:
        dotCount       = 3;
        componentCount = 2;
        break;
      case DxsoOpcode::M3x3:
        dotCount       = 3;
        componentCount = 3;
        break;
      case DxsoOpcode::M3x4:
        dotCount       = 3;
        componentCount = 4;
        break;
      case DxsoOpcode::M4x3:
        dotCount       = 4;
        componentCount = 3;
        break;
      case DxsoOpcode::M4x4:
        dotCount       = 4;
        componentCount = 4;
        break;
      default:
        Logger::warn(str::format("DxsoCompiler::emitMatrixAlu: unimplemented op ", opcode));
        return;
    }

    DxsoRegisterPointer dst = emitGetOperandPtr(ctx.dst);

    // Fix the dst mask if componentCount != maskCount
    // ie. M4x3 on .xyzw.
    uint32_t maskCnt = 0;
    uint8_t mask = 0;
    for (uint32_t i = 0; i < 4 && maskCnt < componentCount; i++) {
      if (ctx.dst.mask[i]) {
        mask |= 1 << i;
        maskCnt++;
      }
    }
    DxsoRegMask dstMask = DxsoRegMask(mask);

    DxsoRegisterValue result;
    result.type.ctype  = dst.type.ctype;
    result.type.ccount = componentCount;

    DxsoVectorType scalarType;
    scalarType.ctype = result.type.ctype;
    scalarType.ccount = 1;

    const uint32_t typeId = getVectorTypeId(result.type);
    const uint32_t scalarTypeId = getVectorTypeId(scalarType);

    DxsoRegMask srcMask(true, true, true, dotCount == 4);
    std::array<uint32_t, 4> indices;

    DxsoRegister src0 = ctx.src[0];
    DxsoRegister src1 = ctx.src[1];

    for (uint32_t i = 0; i < componentCount; i++) {
      indices[i] = m_module.opDot(scalarTypeId,
        emitRegisterLoad(src0, srcMask).id,
        emitRegisterLoad(src1, srcMask).id);

      src1.id.num++;
    }

    result.id = m_module.opCompositeConstruct(
      typeId, componentCount, indices.data());

    this->emitDstStore(dst, result, dstMask, ctx.dst.saturate, emitPredicateLoad(ctx), ctx.dst.shift, ctx.dst.id);
  }


void DxsoCompiler::emitControlFlowGenericLoop(
          bool     count,
          uint32_t initialVar,
          uint32_t strideVar,
          uint32_t iterationCountVar) {
    const uint32_t itType = m_module.defIntType(32, 1);

    DxsoCfgBlock block;
    block.type = DxsoCfgBlockType::Loop;
    block.b_loop.labelHeader   = m_module.allocateId();
    block.b_loop.labelBegin    = m_module.allocateId();
    block.b_loop.labelContinue = m_module.allocateId();
    block.b_loop.labelBreak    = m_module.allocateId();
    block.b_loop.iteratorPtr   = m_module.newVar(
      m_module.defPointerType(itType, spv::StorageClassPrivate), spv::StorageClassPrivate);
    block.b_loop.strideVar     = strideVar;
    block.b_loop.countBackup   = 0;

    if (count) {
      DxsoBaseRegister loop;
      loop.id = { DxsoRegisterType::Loop, 0 };

      DxsoRegisterPointer loopPtr = emitGetOperandPtr(loop, nullptr);
      uint32_t loopVal = m_module.opLoad(
        getVectorTypeId(loopPtr.type), loopPtr.id);

      block.b_loop.countBackup = loopVal;

      m_module.opStore(loopPtr.id, initialVar);
    }

    m_module.setDebugName(block.b_loop.iteratorPtr, "iter");

    m_module.opStore(block.b_loop.iteratorPtr, iterationCountVar);

    m_module.opBranch(block.b_loop.labelHeader);
    m_module.opLabel (block.b_loop.labelHeader);

    m_module.opLoopMerge(
      block.b_loop.labelBreak,
      block.b_loop.labelContinue,
      spv::LoopControlMaskNone);

    m_module.opBranch(block.b_loop.labelBegin);
    m_module.opLabel (block.b_loop.labelBegin);

    uint32_t iterator = m_module.opLoad(itType, block.b_loop.iteratorPtr);
    uint32_t complete = m_module.opIEqual(m_module.defBoolType(), iterator, m_module.consti32(0));

    const uint32_t breakBlock = m_module.allocateId();
    const uint32_t mergeBlock = m_module.allocateId();

    m_module.opSelectionMerge(mergeBlock,
      spv::SelectionControlMaskNone);

    m_module.opBranchConditional(
      complete, breakBlock, mergeBlock);

    m_module.opLabel(breakBlock);

    m_module.opBranch(block.b_loop.labelBreak);

    m_module.opLabel(mergeBlock);

    iterator = m_module.opISub(itType, iterator, m_module.consti32(1));
    m_module.opStore(block.b_loop.iteratorPtr, iterator);

    m_controlFlowBlocks.push_back(block);
  }

  void DxsoCompiler::emitControlFlowGenericLoopEnd() {
    if (m_controlFlowBlocks.size() == 0
      || m_controlFlowBlocks.back().type != DxsoCfgBlockType::Loop)
      throw DxvkError("DxsoCompiler: 'EndRep' without 'Rep' or 'Loop' found");

    // Remove the block from the stack, it's closed
    const DxsoCfgBlock block = m_controlFlowBlocks.back();
    m_controlFlowBlocks.pop_back();

    if (block.b_loop.strideVar) {
      DxsoBaseRegister loop;
      loop.id = { DxsoRegisterType::Loop, 0 };

      DxsoRegisterPointer loopPtr = emitGetOperandPtr(loop, nullptr);
      uint32_t val = m_module.opLoad(
        getVectorTypeId(loopPtr.type), loopPtr.id);

      val = m_module.opIAdd(
        getVectorTypeId(loopPtr.type),
        val, block.b_loop.strideVar);

      m_module.opStore(loopPtr.id, val);
    }

    // Declare the continue block
    m_module.opBranch(block.b_loop.labelContinue);
    m_module.opLabel(block.b_loop.labelContinue);

    // Declare the merge block
    m_module.opBranch(block.b_loop.labelHeader);
    m_module.opLabel(block.b_loop.labelBreak);

    if (block.b_loop.countBackup) {
      DxsoBaseRegister loop;
      loop.id = { DxsoRegisterType::Loop, 0 };

      DxsoRegisterPointer loopPtr = emitGetOperandPtr(loop, nullptr);

      m_module.opStore(loopPtr.id, block.b_loop.countBackup);
    }
  }

  void DxsoCompiler::emitControlFlowRep(const DxsoInstructionContext& ctx) {
    DxsoRegMask srcMask(true, false, false, false);
    this->emitControlFlowGenericLoop(
      false, 0, 0,
      emitRegisterLoad(ctx.src[0], srcMask).id);
  }

  void DxsoCompiler::emitControlFlowEndRep(const DxsoInstructionContext& ctx) {
    emitControlFlowGenericLoopEnd();
  }

  void DxsoCompiler::emitControlFlowLoop(const DxsoInstructionContext& ctx) {
    const uint32_t itType = m_module.defIntType(32, 1);

    DxsoRegMask srcMask(true, true, true, false);
    uint32_t integerRegister = emitRegisterLoad(ctx.src[1], srcMask).id;
    uint32_t x = 0;
    uint32_t y = 1;
    uint32_t z = 2;

    uint32_t iterCount    = m_module.opCompositeExtract(itType, integerRegister, 1, &x);
    uint32_t initialValue = m_module.opCompositeExtract(itType, integerRegister, 1, &y);
    uint32_t strideSize   = m_module.opCompositeExtract(itType, integerRegister, 1, &z);

    this->emitControlFlowGenericLoop(
      true,
      initialValue,
      strideSize,
      iterCount);
  }

  void DxsoCompiler::emitControlFlowEndLoop(const DxsoInstructionContext& ctx) {
    this->emitControlFlowGenericLoopEnd();
  }

  void DxsoCompiler::emitControlFlowBreak(const DxsoInstructionContext& ctx) {
    DxsoCfgBlock* cfgBlock =
      cfgFindBlock({ DxsoCfgBlockType::Loop });

    if (cfgBlock == nullptr)
      throw DxvkError("DxbcCompiler: 'Break' outside 'Rep' or 'Loop' found");

    m_module.opBranch(cfgBlock->b_loop.labelBreak);

    // Subsequent instructions assume that there is an open block
    const uint32_t labelId = m_module.allocateId();
    m_module.opLabel(labelId);
  }

  void DxsoCompiler::emitControlFlowBreakC(const DxsoInstructionContext& ctx) {
    DxsoCfgBlock* cfgBlock =
      cfgFindBlock({ DxsoCfgBlockType::Loop });

    if (cfgBlock == nullptr)
      throw DxvkError("DxbcCompiler: 'BreakC' outside 'Rep' or 'Loop' found");

    DxsoRegMask srcMask(true, false, false, false);
    auto a = emitRegisterLoad(ctx.src[0], srcMask);
    auto b = emitRegisterLoad(ctx.src[1], srcMask);

    uint32_t result = this->emitBoolComparison(
      { DxsoScalarType::Bool, a.type.ccount },
      ctx.instruction.specificData.comparison,
      a.id, b.id);

    // We basically have to wrap this into an 'if' block
    const uint32_t breakBlock = m_module.allocateId();
    const uint32_t mergeBlock = m_module.allocateId();

    m_module.opSelectionMerge(mergeBlock,
      spv::SelectionControlMaskNone);

    m_module.opBranchConditional(
      result, breakBlock, mergeBlock);

    m_module.opLabel(breakBlock);

    m_module.opBranch(cfgBlock->b_loop.labelBreak);

    m_module.opLabel(mergeBlock);
  }

  void DxsoCompiler::emitControlFlowIf(const DxsoInstructionContext& ctx) {
    const auto opcode = ctx.instruction.opcode;

    uint32_t result;

    DxsoRegMask srcMask(true, false, false, false);
    if (opcode == DxsoOpcode::Ifc) {
      auto a = emitRegisterLoad(ctx.src[0], srcMask);
      auto b = emitRegisterLoad(ctx.src[1], srcMask);

      result = this->emitBoolComparison(
        { DxsoScalarType::Bool, a.type.ccount },
        ctx.instruction.specificData.comparison,
        a.id, b.id);
    } else
      result = emitRegisterLoad(ctx.src[0], srcMask).id;

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


  void DxsoCompiler::emitTexCoord(const DxsoInstructionContext& ctx) {
    DxsoRegisterValue result;

    if (m_programInfo.majorVersion() == 1 && m_programInfo.minorVersion() == 4) {
      // TexCrd Op (PS 1.4)
      result = emitRegisterLoad(ctx.src[0], ctx.dst.mask);
    } else {
      // TexCoord Op (PS 1.0 - PS 1.3)
      DxsoRegister texcoord;
      texcoord.id.type = DxsoRegisterType::PixelTexcoord;
      texcoord.id.num  = ctx.dst.id.num;

      result = emitRegisterLoadRaw(texcoord, nullptr);
      // Saturate
      result = emitSaturate(result);
      // w = 1.0f
      uint32_t wIndex = 3;
      result.id = m_module.opCompositeInsert(getVectorTypeId(result.type),
        m_module.constf32(1.0f),
        result.id,
        1, &wIndex);
    }

    DxsoRegisterPointer dst = emitGetOperandPtr(ctx.dst);

    this->emitDstStore(dst, result, ctx.dst.mask, ctx.dst.saturate, emitPredicateLoad(ctx), ctx.dst.shift, ctx.dst.id);
  }

  void DxsoCompiler::emitTextureSample(const DxsoInstructionContext& ctx) {
    DxsoRegisterPointer dst = emitGetOperandPtr(ctx.dst);

    const DxsoOpcode opcode = ctx.instruction.opcode;

    DxsoRegisterValue texcoordVar;
    uint32_t samplerIdx;

    DxsoRegMask vec3Mask(true, true, true,  false);
    DxsoRegMask srcMask (true, true, true,  true);

    auto GetProjectionValue = [&]() {
      uint32_t w = 3;
      return m_module.opCompositeExtract(
        m_module.defFloatType(32), texcoordVar.id, 1, &w);
    };

    if (opcode == DxsoOpcode::TexM3x2Tex || opcode == DxsoOpcode::TexM3x3Tex || opcode == DxsoOpcode::TexM3x3Spec || opcode == DxsoOpcode::TexM3x3VSpec) {
      const uint32_t count = opcode == DxsoOpcode::TexM3x2Tex ? 2 : 3;

      auto n = emitRegisterLoad(ctx.src[0], vec3Mask);

      std::array<uint32_t, 4> indices = { 0, 0, m_module.constf32(0.0f), m_module.constf32(0.0f) };
      for (uint32_t i = 0; i < count; i++) {
        auto reg = ctx.dst;
        reg.id.num -= (count - 1) - i;
        auto m = emitRegisterLoadTexcoord(reg, vec3Mask);

        indices[i] = m_module.opDot(getScalarTypeId(DxsoScalarType::Float32), m.id, n.id);
      }

      if (opcode == DxsoOpcode::TexM3x3Spec || opcode == DxsoOpcode::TexM3x3VSpec) {
        uint32_t vec3Type = getVectorTypeId({ DxsoScalarType::Float32, 3 });
        uint32_t normal = m_module.opCompositeConstruct(vec3Type, 3, indices.data());

        uint32_t eyeRay;
        // VSpec -> Create eye ray from .w of last 3 tex coords (m, m-1, m-2)
        // Spec -> Get eye ray from src[1]
        if (opcode == DxsoOpcode::TexM3x3VSpec) {
          DxsoRegMask wMask(false, false, false, true);

          std::array<uint32_t, 3> eyeRayIndices;
          for (uint32_t i = 0; i < 3; i++) {
            auto reg = ctx.dst;
            reg.id.num -= (count - 1) - i;
            eyeRayIndices[i] = emitRegisterLoadTexcoord(reg, wMask).id;
          }

          eyeRay = m_module.opCompositeConstruct(vec3Type, eyeRayIndices.size(), eyeRayIndices.data());
        }
        else
          eyeRay = emitRegisterLoad(ctx.src[1], vec3Mask).id;

        eyeRay = m_module.opNormalize(vec3Type, eyeRay);
        normal = m_module.opNormalize(vec3Type, normal);
        uint32_t reflection = m_module.opReflect(vec3Type, eyeRay, normal);
        reflection = m_module.opFNegate(vec3Type, reflection);

        for (uint32_t i = 0; i < 3; i++)
          indices[i] = m_module.opCompositeExtract(m_module.defFloatType(32), reflection, 1, &i);
      }

      texcoordVar.type = { DxsoScalarType::Float32, 4 };
      texcoordVar.id   = m_module.opCompositeConstruct(getVectorTypeId(texcoordVar.type), indices.size(), indices.data());
      
      samplerIdx = ctx.dst.id.num;
    }
    else if (opcode == DxsoOpcode::TexBem || opcode == DxsoOpcode::TexBemL) {
      auto m = emitRegisterLoadTexcoord(ctx.dst, srcMask);
      auto n = emitRegisterLoad(ctx.src[0], srcMask);

      texcoordVar = m;
      samplerIdx = ctx.dst.id.num;

      uint32_t texcoord_t = getVectorTypeId(texcoordVar.type);

      // The projection (/.w) happens before this...
      // Of course it does...
      uint32_t bool_t = m_module.defBoolType();

      uint32_t shouldProj = m_module.opBitFieldUExtract(
        m_module.defIntType(32, 0), m_ps.projectionSpec,
        m_module.consti32(samplerIdx), m_module.consti32(1));

      shouldProj = m_module.opIEqual(bool_t, shouldProj, m_module.constu32(1));

      uint32_t bvec4_t = m_module.defVectorType(bool_t, 4);
      std::array<uint32_t, 4> indices = { shouldProj, shouldProj, shouldProj, shouldProj };
      shouldProj = m_module.opCompositeConstruct(bvec4_t, indices.size(), indices.data());

      uint32_t projScalar = m_module.opFDiv(m_module.defFloatType(32), m_module.constf32(1.0), GetProjectionValue());
      uint32_t projResult = m_module.opVectorTimesScalar(texcoord_t, texcoordVar.id, projScalar);

      texcoordVar.id = m_module.opSelect(texcoord_t, shouldProj, projResult, texcoordVar.id);

      // u' = tc(m).x + [bm00(m) * t(n).x + bm10(m) * t(n).y]
      // v' = tc(m).y + [bm01(m) * t(n).x + bm11(m) * t(n).y]

      // But we flipped the bm indices so we can use dot here...

      // u' = tc(m).x + dot(bm0, tn)
      // v' = tc(m).y + dot(bm1, tn)

      for (uint32_t i = 0; i < 2; i++) {
        uint32_t fl_t   = getScalarTypeId(DxsoScalarType::Float32);
        uint32_t vec2_t = getVectorTypeId({ DxsoScalarType::Float32, 2 });
        std::array<uint32_t, 4> indices = { 0, 1, 2, 3 };

        uint32_t tc_m_n = m_module.opCompositeExtract(fl_t, texcoordVar.id, 1, &i);

        uint32_t offset = m_module.constu32(D3D9SharedPSStages_Count * ctx.dst.id.num + D3D9SharedPSStages_BumpEnvMat0 + i);
        uint32_t bm     = m_module.opAccessChain(m_module.defPointerType(vec2_t, spv::StorageClassUniform),
                                                 m_ps.sharedState, 1, &offset);
                 bm     = m_module.opLoad(vec2_t, bm);

        uint32_t t      = m_module.opVectorShuffle(vec2_t, n.id, n.id, 2, indices.data());

        uint32_t dot    = m_module.opDot(fl_t, bm, t);

        uint32_t result = m_module.opFAdd(fl_t, tc_m_n, dot);
        texcoordVar.id  = m_module.opCompositeInsert(getVectorTypeId(texcoordVar.type), result, texcoordVar.id, 1, &i);
      }
    }
    else if (opcode == DxsoOpcode::TexReg2Ar) {
      texcoordVar = emitRegisterLoad(ctx.src[0], srcMask);
      texcoordVar = emitRegisterSwizzle(texcoordVar, DxsoRegSwizzle(3, 0, 0, 0), srcMask);

      samplerIdx = ctx.dst.id.num;
    }
    else if (opcode == DxsoOpcode::TexReg2Gb) {
      texcoordVar = emitRegisterLoad(ctx.src[0], srcMask);
      texcoordVar = emitRegisterSwizzle(texcoordVar, DxsoRegSwizzle(1, 2, 2, 2), srcMask);

      samplerIdx = ctx.dst.id.num;
    }
    else if (opcode == DxsoOpcode::TexReg2Rgb) {
      texcoordVar = emitRegisterLoad(ctx.src[0], srcMask);
      texcoordVar = emitRegisterSwizzle(texcoordVar, DxsoRegSwizzle(0, 1, 2, 2), srcMask);

      samplerIdx = ctx.dst.id.num;
    }
    else if (opcode == DxsoOpcode::TexDp3Tex) {
      auto m = emitRegisterLoadTexcoord(ctx.dst,    vec3Mask);
      auto n = emitRegisterLoad(ctx.src[0], vec3Mask);

      auto dot = emitDot(m, n);

      std::array<uint32_t, 4> indices = { dot.id, m_module.constf32(0.0f), m_module.constf32(0.0f), m_module.constf32(0.0f) };

      texcoordVar.type = { DxsoScalarType::Float32, 4 };
      texcoordVar.id   = m_module.opCompositeConstruct(getVectorTypeId(texcoordVar.type),
        indices.size(), indices.data());

      samplerIdx  = ctx.dst.id.num;
    }
    else {
      if (m_programInfo.majorVersion() >= 2) { // SM 2.0+
        texcoordVar = emitRegisterLoad(ctx.src[0], srcMask);
        samplerIdx  = ctx.src[1].id.num;
      } else if (
        m_programInfo.majorVersion() == 1
     && m_programInfo.minorVersion() == 4) { // SM 1.4
        texcoordVar = emitRegisterLoad(ctx.src[0], srcMask);
        samplerIdx  = ctx.dst.id.num;
      }
      else { // SM 1.0-1.3
        texcoordVar = emitRegisterLoadTexcoord(ctx.dst, srcMask);
        samplerIdx  = ctx.dst.id.num;
      }
    }

    // SM < 1.x does not have dcl sampler type.
    if (m_programInfo.majorVersion() < 2 && m_samplers[samplerIdx].color[SamplerTypeTexture2D].varId == 0)
      emitDclSampler(samplerIdx, DxsoTextureType::Texture2D);

    DxsoSampler sampler = m_samplers.at(samplerIdx);

    auto SampleImage = [this, opcode, dst, ctx, samplerIdx, GetProjectionValue](DxsoRegisterValue texcoordVar, DxsoSamplerInfo& sampler, bool depth, DxsoSamplerType samplerType, uint32_t specConst) {
      DxsoRegisterValue result;
      result.type.ctype  = dst.type.ctype;
      result.type.ccount = depth ? 1 : 4;

      const uint32_t typeId = getVectorTypeId(result.type);

      SpirvImageOperands imageOperands;
      if (m_programInfo.type() == DxsoProgramTypes::VertexShader) {
        imageOperands.sLod = m_module.constf32(0.0f);
        imageOperands.flags |= spv::ImageOperandsLodMask;
      }

      if (opcode == DxsoOpcode::TexLdl) {
        uint32_t w = 3;
        imageOperands.sLod = m_module.opCompositeExtract(
          m_module.defFloatType(32), texcoordVar.id, 1, &w);
        imageOperands.flags |= spv::ImageOperandsLodMask;
      }

      if (opcode == DxsoOpcode::TexLdd) {
        DxsoRegMask gradMask(true, true, sampler.dimensions == 3, false);
        imageOperands.flags |= spv::ImageOperandsGradMask;
        imageOperands.sGradX = emitRegisterLoad(ctx.src[2], gradMask).id;
        imageOperands.sGradY = emitRegisterLoad(ctx.src[3], gradMask).id;
      }

      uint32_t projDivider = 0;

      if (opcode == DxsoOpcode::Tex
        && m_programInfo.majorVersion() >= 2) {
        if (ctx.instruction.specificData.texld == DxsoTexLdMode::Project) {
          projDivider = GetProjectionValue();
        }
        else if (ctx.instruction.specificData.texld == DxsoTexLdMode::Bias) {
          uint32_t w = 3;
          imageOperands.sLodBias = m_module.opCompositeExtract(
            m_module.defFloatType(32), texcoordVar.id, 1, &w);
          imageOperands.flags |= spv::ImageOperandsBiasMask;
        }
      }

      bool switchProjResult = m_programInfo.majorVersion() < 2 && samplerType != SamplerTypeTextureCube;

      if (switchProjResult)
        projDivider = GetProjectionValue();

      // We already handled this...
      if (opcode == DxsoOpcode::TexBem) {
        switchProjResult = false;
        projDivider = 0;
      }

      uint32_t reference = 0;

      if (depth) {
        uint32_t component = sampler.dimensions;
        reference = m_module.opCompositeExtract(
          m_module.defFloatType(32), texcoordVar.id, 1, &component);
      }

      if (projDivider != 0) {
        for (uint32_t i = sampler.dimensions; i < 4; i++) {
          texcoordVar.id = m_module.opCompositeInsert(getVectorTypeId(texcoordVar.type),
            projDivider, texcoordVar.id, 1, &i);
        }
      }

      uint32_t fetch4 = 0;
      if (m_programInfo.type() == DxsoProgramType::PixelShader && samplerType != SamplerTypeTexture3D) {
        fetch4 = m_module.opBitFieldUExtract(
          m_module.defIntType(32, 0), m_ps.fetch4Spec,
          m_module.consti32(samplerIdx), m_module.consti32(1));

        uint32_t bool_t = m_module.defBoolType();
        fetch4 = m_module.opIEqual(bool_t, fetch4, m_module.constu32(1));

        uint32_t bvec4_t = m_module.defVectorType(bool_t, 4);
        std::array<uint32_t, 4> indices = { fetch4, fetch4, fetch4, fetch4 };
        fetch4 = m_module.opCompositeConstruct(bvec4_t, indices.size(), indices.data());
      }

      result.id = this->emitSample(
        projDivider != 0,
        typeId,
        sampler,
        texcoordVar,
        reference,
        fetch4,
        imageOperands);

      if (switchProjResult) {
        uint32_t bool_t = m_module.defBoolType();

        uint32_t nonProjResult = this->emitSample(
          0,
          typeId,
          sampler,
          texcoordVar,
          reference,
          fetch4,
          imageOperands);

        uint32_t shouldProj = m_module.opBitFieldUExtract(
          m_module.defIntType(32, 0), m_ps.projectionSpec,
          m_module.consti32(samplerIdx), m_module.consti32(1));

        shouldProj = m_module.opIEqual(m_module.defBoolType(), shouldProj, m_module.constu32(1));

        // Depth  -> .x
        // Colour -> .xyzw
        // Need to replicate the bool for the opSelect.
        if (!depth) {
          uint32_t bvec4_t = m_module.defVectorType(bool_t, 4);
          std::array<uint32_t, 4> indices = { shouldProj, shouldProj, shouldProj, shouldProj };
          shouldProj = m_module.opCompositeConstruct(bvec4_t, indices.size(), indices.data());
        }

        result.id = m_module.opSelect(typeId, shouldProj, result.id, nonProjResult);
      }

      // If we are sampling depth we've already specc'ed this!
      // This path is always size 4 because it only hits on color.
      if (specConst != 0) {
        uint32_t bool_t = m_module.defBoolType();
        uint32_t bvec4_t = m_module.defVectorType(bool_t, 4);
        std::array<uint32_t, 4> indices = { specConst, specConst, specConst, specConst };
        specConst = m_module.opCompositeConstruct(bvec4_t, indices.size(), indices.data());
        result.id = m_module.opSelect(typeId, specConst, result.id, m_module.constvec4f32(0.0f, 0.0f, 0.0f, 1.0f));
      }

      // Apply operand swizzle to the operand value
      result = emitRegisterSwizzle(result, IdentitySwizzle, ctx.dst.mask);

      if (opcode == DxsoOpcode::TexBemL) {
        uint32_t float_t = m_module.defFloatType(32);

        uint32_t index = m_module.constu32(D3D9SharedPSStages_Count * ctx.dst.id.num + D3D9SharedPSStages_BumpEnvLScale);
        uint32_t lScale = m_module.opAccessChain(m_module.defPointerType(float_t, spv::StorageClassUniform),
                                                 m_ps.sharedState, 1, &index);
                 lScale = m_module.opLoad(float_t, lScale);

                 index = m_module.constu32(D3D9SharedPSStages_Count * ctx.dst.id.num + D3D9SharedPSStages_BumpEnvLOffset);
        uint32_t lOffset = m_module.opAccessChain(m_module.defPointerType(float_t, spv::StorageClassUniform),
                                                  m_ps.sharedState, 1, &index);
                 lOffset = m_module.opLoad(float_t, lOffset);
            
        uint32_t zIndex = 2;
        uint32_t scale = m_module.opCompositeExtract(float_t, result.id, 1, &zIndex);
                 scale = m_module.opFMul(float_t, scale, lScale);
                 scale = m_module.opFAdd(float_t, scale, lOffset);
                 scale = m_module.opFClamp(float_t, scale, m_module.constf32(0.0f), m_module.constf32(1.0));

        result.id = m_module.opVectorTimesScalar(getVectorTypeId(result.type), result.id, scale);
      }

      this->emitDstStore(dst, result, ctx.dst.mask, ctx.dst.saturate, emitPredicateLoad(ctx), ctx.dst.shift, ctx.dst.id);
    };

    auto SampleType = [&](DxsoSamplerType samplerType) {
      // Only do the check for depth comp. samplers
      // if we aren't a 3D texture
      if (samplerType != SamplerTypeTexture3D) {
        uint32_t colorLabel  = m_module.allocateId();
        uint32_t depthLabel  = m_module.allocateId();
        uint32_t endLabel    = m_module.allocateId();

        uint32_t typeId = m_module.defIntType(32, 0);
        uint32_t offset  = m_module.consti32(m_programInfo.type() == DxsoProgramTypes::VertexShader ? samplerIdx + 17 : samplerIdx);
        uint32_t bitCnt  = m_module.consti32(1);
        uint32_t isDepth = m_module.opBitFieldUExtract(typeId, m_depthSpecConstant, offset, bitCnt);
        isDepth = m_module.opIEqual(m_module.defBoolType(), isDepth, m_module.constu32(1));

        m_module.opSelectionMerge(endLabel, spv::SelectionControlMaskNone);
        m_module.opBranchConditional(isDepth, depthLabel, colorLabel);

        m_module.opLabel(colorLabel);
        SampleImage(texcoordVar, sampler.color[samplerType], false, samplerType, sampler.boundConst);
        m_module.opBranch(endLabel);

        m_module.opLabel(depthLabel);
        // No spec constant as if we are unbound we always fall down the color path.
        SampleImage(texcoordVar, sampler.depth[samplerType], true, samplerType, 0);
        m_module.opBranch(endLabel);

        m_module.opLabel(endLabel);
      }
      else
        SampleImage(texcoordVar, sampler.color[samplerType], false, samplerType, sampler.boundConst);
    };

    if (m_programInfo.majorVersion() >= 2 && !m_moduleInfo.options.forceSamplerTypeSpecConstants) {
      DxsoSamplerType samplerType =
        SamplerTypeFromTextureType(sampler.type);

      SampleType(samplerType);
    }
    else {
      std::array<SpirvSwitchCaseLabel, 3> typeCaseLabels = {{
        { uint32_t(SamplerTypeTexture2D),           m_module.allocateId() },
        { uint32_t(SamplerTypeTexture3D),           m_module.allocateId() },
        { uint32_t(SamplerTypeTextureCube),         m_module.allocateId() },
      }};

      uint32_t switchEndLabel = m_module.allocateId();

      uint32_t typeId = m_module.defIntType(32, 0);

      uint32_t offset  = m_module.consti32(samplerIdx * 2);
      uint32_t bitCnt  = m_module.consti32(2);
      uint32_t type    = m_module.opBitFieldUExtract(typeId, m_ps.samplerTypeSpec, offset, bitCnt);

      m_module.opSelectionMerge(switchEndLabel, spv::SelectionControlMaskNone);
      m_module.opSwitch(type,
        typeCaseLabels[uint32_t(SamplerTypeTexture2D)].labelId,
        typeCaseLabels.size(),
        typeCaseLabels.data());

      for (const auto& label : typeCaseLabels) {
        m_module.opLabel(label.labelId);

        SampleType(DxsoSamplerType(label.literal));

        m_module.opBranch(switchEndLabel);
      }

      m_module.opLabel(switchEndLabel);
    }
  }

  void DxsoCompiler::emitTextureKill(const DxsoInstructionContext& ctx) {
    DxsoRegisterValue texReg;

    if (m_programInfo.majorVersion() >= 2 ||
       (m_programInfo.majorVersion() == 1
     && m_programInfo.minorVersion() == 4)) // SM 2.0+ or 1.4
      texReg = emitRegisterLoadRaw(ctx.dst, ctx.dst.hasRelative ? &ctx.dst.relative : nullptr);
    else { // SM 1.0-1.3
      DxsoRegister texcoord;
      texcoord.id = { DxsoRegisterType::PixelTexcoord, ctx.dst.id.num };

      texReg = emitRegisterLoadRaw(texcoord, nullptr);
    }

    std::array<uint32_t, 4> indices = { 0, 1, 2, 3 };

    // On SM1 it only works on the first 
    if (m_programInfo.majorVersion() < 2) {
      texReg.type.ccount = 3;

      texReg.id = m_module.opVectorShuffle(
        getVectorTypeId(texReg.type),
        texReg.id, texReg.id,
        texReg.type.ccount, indices.data());
    }
    else {
      // The writemask actually applies and works here...
      // (FXC doesn't generate this but it fixes broken ENB shaders)
      texReg = emitRegisterSwizzle(texReg, IdentitySwizzle, ctx.dst.mask);
    }

    const uint32_t boolVecTypeId =
      getVectorTypeId({ DxsoScalarType::Bool, texReg.type.ccount });

    uint32_t result = m_module.opFOrdLessThan(
      boolVecTypeId, texReg.id,
      m_module.constfReplicant(0.0f, texReg.type.ccount));

    if (texReg.type.ccount != 1)
      result = m_module.opAny(m_module.defBoolType(), result);

    if (m_ps.killState == 0) {
      uint32_t labelIf = m_module.allocateId();
      uint32_t labelEnd = m_module.allocateId();

      m_module.opSelectionMerge(labelEnd, spv::SelectionControlMaskNone);
      m_module.opBranchConditional(result, labelIf, labelEnd);

      m_module.opLabel(labelIf);

      if (m_moduleInfo.options.useDemoteToHelperInvocation) {
        m_module.opDemoteToHelperInvocation();
        m_module.opBranch(labelEnd);
      } else {
        // OpKill terminates the block
        m_module.opKill();
      }

      m_module.opLabel(labelEnd);
    }
    else {
      uint32_t typeId = m_module.defBoolType();
      
      uint32_t killState = m_module.opLoad     (typeId, m_ps.killState);
               killState = m_module.opLogicalOr(typeId, killState, result);
      m_module.opStore(m_ps.killState, killState);

      if (m_moduleInfo.options.useSubgroupOpsForEarlyDiscard) {
        uint32_t ballot = m_module.opGroupNonUniformBallot(
          getVectorTypeId({ DxsoScalarType::Uint32, 4 }),
          m_module.constu32(spv::ScopeSubgroup),
          killState);
        
        uint32_t laneId = m_module.opLoad(
          getScalarTypeId(DxsoScalarType::Uint32),
          m_ps.builtinLaneId);

        uint32_t laneIdPart = m_module.opShiftRightLogical(
          getScalarTypeId(DxsoScalarType::Uint32),
          laneId, m_module.constu32(5));

        uint32_t laneMask = m_module.opVectorExtractDynamic(
          getScalarTypeId(DxsoScalarType::Uint32),
          ballot, laneIdPart);

        uint32_t laneIdQuad = m_module.opBitwiseAnd(
          getScalarTypeId(DxsoScalarType::Uint32),
          laneId, m_module.constu32(0x1c));

        laneMask = m_module.opShiftRightLogical(
          getScalarTypeId(DxsoScalarType::Uint32),
          laneMask, laneIdQuad);

        laneMask = m_module.opBitwiseAnd(
          getScalarTypeId(DxsoScalarType::Uint32),
          laneMask, m_module.constu32(0xf));
        
        uint32_t killSubgroup = m_module.opIEqual(
          m_module.defBoolType(),
          laneMask, m_module.constu32(0xf));
        
        uint32_t labelIf  = m_module.allocateId();
        uint32_t labelEnd = m_module.allocateId();
        
        m_module.opSelectionMerge(labelEnd, spv::SelectionControlMaskNone);
        m_module.opBranchConditional(killSubgroup, labelIf, labelEnd);
        
        // OpKill terminates the block
        m_module.opLabel(labelIf);
        m_module.opKill();
        
        m_module.opLabel(labelEnd);
      }
    }
  }

  void DxsoCompiler::emitTextureDepth(const DxsoInstructionContext& ctx) {
    const uint32_t fType = m_module.defFloatType(32);

    DxsoRegMask srcMask(true, true, false, false);
    uint32_t r5 = emitRegisterLoad(ctx.src[0], srcMask).id;
    uint32_t x = 0;
    uint32_t y = 1;

    uint32_t xValue = m_module.opCompositeExtract(fType, r5, 1, &x);
    uint32_t yValue = m_module.opCompositeExtract(fType, r5, 1, &y);

    // The docs say if yValue is 0 the result is 1.0 but native drivers return
    // 0 for xValue <= 0. So we don't have to do anything special since -INF and
    // NAN get clamped to 0 at the end of the shader.
    uint32_t result = m_module.opFDiv(fType, xValue, yValue);

    DxsoBaseRegister depth;
    depth.id = { DxsoRegisterType::DepthOut, 0 };

    DxsoRegisterPointer depthPtr = emitGetOperandPtr(depth, nullptr);

    m_module.opStore(depthPtr.id, result);
  }


  uint32_t DxsoCompiler::emitSample(
          bool                    projected,
          uint32_t                resultType,
          DxsoSamplerInfo&        samplerInfo,
          DxsoRegisterValue       coordinates,
          uint32_t                reference,
          uint32_t                fetch4,
    const SpirvImageOperands&     operands) {
    const bool depthCompare = reference != 0;
    const bool explicitLod  =
       (operands.flags & spv::ImageOperandsLodMask)
    || (operands.flags & spv::ImageOperandsGradMask);

    const uint32_t sampledImage = m_module.opLoad(samplerInfo.typeId, samplerInfo.varId);

    uint32_t val;

    // No Fetch 4
    if (projected) {
      if (depthCompare) {
        if (explicitLod)
          val = m_module.opImageSampleProjDrefExplicitLod(resultType, sampledImage, coordinates.id, reference, operands);
        else
          val = m_module.opImageSampleProjDrefImplicitLod(resultType, sampledImage, coordinates.id, reference, operands);
      }
      else {
        if (explicitLod)
          val = m_module.opImageSampleProjExplicitLod(resultType, sampledImage, coordinates.id, operands);
        else
          val = m_module.opImageSampleProjImplicitLod(resultType, sampledImage, coordinates.id, operands);
      }
    }
    else {
      if (depthCompare) {
        if (explicitLod)
          val = m_module.opImageSampleDrefExplicitLod(resultType, sampledImage, coordinates.id, reference, operands);
        else
          val = m_module.opImageSampleDrefImplicitLod(resultType, sampledImage, coordinates.id, reference, operands);
      }
      else {
        if (explicitLod)
          val = m_module.opImageSampleExplicitLod(resultType, sampledImage, coordinates.id, operands);
        else
          val = m_module.opImageSampleImplicitLod(resultType, sampledImage, coordinates.id, operands);
      }
    }


    if (fetch4 && !depthCompare) {
      SpirvImageOperands fetch4Operands = operands;
      fetch4Operands.flags &= ~spv::ImageOperandsLodMask;
      fetch4Operands.flags &= ~spv::ImageOperandsGradMask;
      fetch4Operands.flags &= ~spv::ImageOperandsBiasMask;

      // Doesn't really work for cubes...
      // D3D9 does support gather on 3D but we cannot :<
      // Nothing probably relies on that though.
      // If we come back to this ever, make sure to handle cube/3d differences.
      if (samplerInfo.dimensions == 2) {
        uint32_t image = m_module.opImage(samplerInfo.imageTypeId, sampledImage);

        // Account for half texel offset...
        // textureSize = 1.0f / float(2 * textureSize(sampler, 0))
        DxsoRegisterValue textureSize;
        textureSize.type = { DxsoScalarType::Sint32, samplerInfo.dimensions };
        textureSize.id = m_module.opImageQuerySizeLod(getVectorTypeId(textureSize.type), image, m_module.consti32(0));
        textureSize.id = m_module.opIMul(getVectorTypeId(textureSize.type), textureSize.id, m_module.constiReplicant(2, samplerInfo.dimensions));

        textureSize.type = { DxsoScalarType::Float32, samplerInfo.dimensions };
        textureSize.id = m_module.opConvertStoF(getVectorTypeId(textureSize.type), textureSize.id);
        // HACK: Bias fetch4 half-texel offset to avoid a "grid" effect.
        // Technically we should only do that for non-powers of two
        // as only then does the imprecision need to be biased
        // towards infinity -- but that's not really worth doing...
        float numerator = 1.0f - 1.0f / 256.0f;
        textureSize.id = m_module.opFDiv(getVectorTypeId(textureSize.type), m_module.constfReplicant(numerator, samplerInfo.dimensions), textureSize.id);

        // coord => same dimensions as texture size (no cube here !)
        const std::array<uint32_t, 4> naturalIndices = { 0, 1, 2, 3 };
        coordinates.type.ccount = samplerInfo.dimensions;
        coordinates.id = m_module.opVectorShuffle(getVectorTypeId(coordinates.type), coordinates.id, coordinates.id, coordinates.type.ccount, naturalIndices.data());
        // coord += textureSize;
        coordinates.id = m_module.opFAdd(getVectorTypeId(coordinates.type), coordinates.id, textureSize.id);
      }

      uint32_t fetch4Val = m_module.opImageGather(resultType, sampledImage, coordinates.id, m_module.consti32(0), fetch4Operands);
      // B R G A swizzle... Funny D3D9 order.
      const std::array<uint32_t, 4> indices = { 2, 0, 1, 3 };
      fetch4Val = m_module.opVectorShuffle(resultType, fetch4Val, fetch4Val, indices.size(), indices.data());

      val = m_module.opSelect(resultType, fetch4, fetch4Val, val);
    }

    return val;
  }


  void DxsoCompiler::emitInputSetup() {
    uint32_t pointCoord = 0;
    D3D9PointSizeInfoPS pointInfo;

    if (m_programInfo.type() == DxsoProgramType::PixelShader) {
      pointCoord = GetPointCoord(m_module, m_entryPointInterfaces);
      pointInfo  = GetPointSizeInfoPS(m_module, m_rsBlock);
    }

    for (uint32_t i = 0; i < m_isgn.elemCount; i++) {
      const auto& elem = m_isgn.elems[i];
      const uint32_t slot = elem.slot;
      
      DxsoRegisterInfo info;
      info.type.ctype   = DxsoScalarType::Float32;
      info.type.ccount  = 4;
      info.type.alength = 1;
      info.sclass       = spv::StorageClassInput;

      DxsoRegisterPointer inputPtr;
      inputPtr.id          = emitNewVariable(info);
      inputPtr.type.ctype  = DxsoScalarType::Float32;
      inputPtr.type.ccount = info.type.ccount;

      m_module.decorateLocation(inputPtr.id, slot);

      std::string name =
        str::format("in_", elem.semantic.usage, elem.semantic.usageIndex);
      m_module.setDebugName(inputPtr.id, name.c_str());

      if (elem.centroid)
        m_module.decorate(inputPtr.id, spv::DecorationCentroid);

      m_entryPointInterfaces.push_back(inputPtr.id);

      uint32_t typeId    = this->getVectorTypeId({ DxsoScalarType::Float32, 4 });
      uint32_t ptrTypeId = m_module.defPointerType(typeId, spv::StorageClassPrivate);

      uint32_t regNumVar = m_module.constu32(elem.regNumber);

      DxsoRegisterPointer indexPtr;
      indexPtr.id   = m_module.opAccessChain(ptrTypeId, m_vArray, 1, &regNumVar);
      indexPtr.type = inputPtr.type;
      indexPtr.type.ccount = 4;

      DxsoRegisterValue indexVal = this->emitValueLoad(inputPtr);

      DxsoRegisterValue workingReg;
      workingReg.type = indexVal.type;

      workingReg.id = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);

      DxsoRegMask mask = elem.mask;
      if (mask.popCount() == 0)
        mask = DxsoRegMask(true, true, true, true);

      std::array<uint32_t, 4> indices = { 0, 1, 2, 3 };
      uint32_t count = 0;
      for (uint32_t i = 0; i < 4; i++) {
        if (mask[i]) {
          indices[i] = i + 4;
          count++;
        }
      }

      workingReg.id = m_module.opVectorShuffle(getVectorTypeId(workingReg.type),
        workingReg.id, indexVal.id, 4, indices.data());

      // We need to replace TEXCOORD inputs with gl_PointCoord
      // if D3DRS_POINTSPRITEENABLE is set.
      if (m_programInfo.type() == DxsoProgramType::PixelShader && elem.semantic.usage == DxsoUsage::Texcoord)
        workingReg.id = m_module.opSelect(getVectorTypeId(workingReg.type), pointInfo.isSprite, pointCoord, workingReg.id);

      if (m_programInfo.type() == DxsoProgramType::PixelShader && elem.semantic.usage == DxsoUsage::Color) {
        if (elem.semantic.usageIndex == 0)
          m_ps.diffuseColorIn = inputPtr.id;
        else if (elem.semantic.usageIndex == 1)
          m_ps.specularColorIn = inputPtr.id;
      }

      m_module.opStore(indexPtr.id, workingReg.id);
    }
  }


  void DxsoCompiler::emitLinkerOutputSetup() {
    bool outputtedColor0 = false;
    bool outputtedColor1 = false;

    for (uint32_t i = 0; i < m_osgn.elemCount; i++) {
      const auto& elem = m_osgn.elems[i];
      const uint32_t slot = elem.slot;

      if (elem.semantic.usage == DxsoUsage::Color) {
        if (elem.semantic.usageIndex == 0)
          outputtedColor0 = true;
        else
          outputtedColor1 = true;
      }
      
      DxsoRegisterInfo info;
      info.type.ctype   = DxsoScalarType::Float32;
      info.type.ccount  = 4;
      info.type.alength = 1;
      info.sclass       = spv::StorageClassOutput;

      spv::BuiltIn builtIn =
        semanticToBuiltIn(false, elem.semantic);

      DxsoRegisterPointer outputPtr;
      outputPtr.type.ctype  = DxsoScalarType::Float32;
      outputPtr.type.ccount = 4;

      DxsoRegMask mask = elem.mask;

      bool scalar = false;

      if (builtIn == spv::BuiltInMax) {
        outputPtr.id = emitNewVariableDefault(info,
          m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f));
        m_module.decorateLocation(outputPtr.id, slot);

        std::string name =
          str::format("out_", elem.semantic.usage, elem.semantic.usageIndex);
        m_module.setDebugName(outputPtr.id, name.c_str());
      }
      else {
        const char* name = "unknown_builtin";
        if (builtIn == spv::BuiltInPosition)
          name = "oPos";
        else if (builtIn == spv::BuiltInPointSize) {
          outputPtr.type.ccount = 1;
          info.type.ccount = 1;
          name = "oPSize";
          bool maskValues[4];
          for (uint32_t i = 0; i < 4; i++)
            maskValues[i] = i == elem.mask.firstSet();
          mask = DxsoRegMask(maskValues[0], maskValues[1], maskValues[2], maskValues[3]);
        }

        outputPtr.id = emitNewVariableDefault(info,
          m_module.constfReplicant(0.0f, info.type.ccount));

        m_module.setDebugName(outputPtr.id, name);
        m_module.decorateBuiltIn(outputPtr.id, builtIn);

        if (builtIn == spv::BuiltInPosition)
          m_vs.oPos = outputPtr;
        else if (builtIn == spv::BuiltInPointSize) {
          scalar = true;
          m_vs.oPSize = outputPtr;
        }
      }

      m_entryPointInterfaces.push_back(outputPtr.id);

      uint32_t typeId    = this->getVectorTypeId({ DxsoScalarType::Float32, 4 });
      uint32_t ptrTypeId = m_module.defPointerType(typeId, spv::StorageClassPrivate);

      uint32_t regNumVar = m_module.constu32(elem.regNumber);

      DxsoRegisterPointer indexPtr;
      indexPtr.id   = m_module.opAccessChain(ptrTypeId, m_oArray, 1, &regNumVar);
      indexPtr.type = outputPtr.type;
      indexPtr.type.ccount = 4;

      DxsoRegisterValue indexVal = this->emitValueLoad(indexPtr);

      DxsoRegisterValue workingReg;
      workingReg.type.ctype  = indexVal.type.ctype;
      workingReg.type.ccount = scalar ? 1 : 4;

      workingReg.id = scalar
        ? m_module.constf32(0.0f)
        : m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);

      std::array<uint32_t, 4> indices = { 0, 1, 2, 3 };

      if (scalar) {
        workingReg.id = m_module.opCompositeExtract(getVectorTypeId(workingReg.type),
          indexVal.id, 1, indices.data());
      } else {
        if (mask.popCount() == 0)
          mask = DxsoRegMask(true, true, true, true);

        uint32_t count = 0;
        for (uint32_t i = 0; i < 4; i++) {
          if (mask[i])
            indices[count++] = i + 4;
        }


        workingReg.id = m_module.opVectorShuffle(getVectorTypeId(workingReg.type),
          workingReg.id, indexVal.id, 4, indices.data());
      }

      // Ie. 0 or 1 for diffuse and specular color
      // and for Shader Model 1 or 2
      // (because those have dedicated color registers
      // where this rule applies)
      if (elem.semantic.usage == DxsoUsage::Color &&
          elem.semantic.usageIndex < 2 &&
          m_programInfo.majorVersion() < 3)
        workingReg = emitSaturate(workingReg);

      m_module.opStore(outputPtr.id, workingReg.id);
    }

    auto OutputDefault = [&](DxsoSemantic semantic) {
      DxsoRegisterInfo info;
      info.type.ctype   = DxsoScalarType::Float32;
      info.type.ccount  = 4;
      info.type.alength = 1;
      info.sclass       = spv::StorageClassOutput;

      uint32_t slot = RegisterLinkerSlot(semantic);

      uint32_t value = semantic == DxsoSemantic{ DxsoUsage::Color, 0 }
        ? m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f)
        : m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);


      uint32_t outputPtr = emitNewVariableDefault(info, value);

      m_module.decorateLocation(outputPtr, slot);

      std::string name =
        str::format("out_", semantic.usage, semantic.usageIndex, "_default");

      m_module.setDebugName(outputPtr, name.c_str());

      m_interfaceSlots.outputSlots |= 1u << slot;
      m_entryPointInterfaces.push_back(outputPtr);
    };

    if (!outputtedColor0)
      OutputDefault(DxsoSemantic{ DxsoUsage::Color, 0 });

    if (!outputtedColor1)
      OutputDefault(DxsoSemantic{ DxsoUsage::Color, 1 });

    auto pointInfo = GetPointSizeInfoVS(m_module, m_vs.oPos.id, 0, 0, m_rsBlock, false);

    if (m_vs.oPSize.id == 0) {
      m_vs.oPSize = this->emitRegisterPtr(
        "oPSize", DxsoScalarType::Float32, 1, 0,
        spv::StorageClassOutput, spv::BuiltInPointSize);

      uint32_t pointSize = m_module.opFClamp(m_module.defFloatType(32), pointInfo.defaultValue, pointInfo.min, pointInfo.max);

      m_module.opStore(m_vs.oPSize.id, pointSize);
    }
    else {
      uint32_t float_t = m_module.defFloatType(32);
      uint32_t pointSize = m_module.opFClamp(m_module.defFloatType(32), m_module.opLoad(float_t, m_vs.oPSize.id), pointInfo.min, pointInfo.max);
      m_module.opStore(m_vs.oPSize.id, pointSize);
    }
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

    if (m_moduleInfo.options.invariantPosition)
      m_module.decorate(m_vs.oPos.id, spv::DecorationInvariant);
    
    const uint32_t positionPtr = m_vs.oPos.id;

    // We generated a bad shader, let's not make it even worse.
    if (positionPtr == 0) {
      Logger::warn("Shader without Position output. Something is likely wrong here.");
      return;
    }

    // Compute clip distances
    uint32_t positionId = m_module.opLoad(vec4Type, positionPtr);
    
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


  void DxsoCompiler::setupRenderStateInfo() {
    uint32_t count;

    // Only need alpha ref for PS 3.
    // No FF fog component.
    if (m_programInfo.type() == DxsoProgramType::PixelShader) {
      if (m_programInfo.majorVersion() == 3) {
        m_interfaceSlots.pushConstOffset = offsetof(D3D9RenderStateInfo, alphaRef);
        m_interfaceSlots.pushConstSize   = sizeof(float);
      }
      else {
        m_interfaceSlots.pushConstOffset = 0;
        m_interfaceSlots.pushConstSize   = offsetof(D3D9RenderStateInfo, pointSize);
      }

      count = 5;
    }
    else {
      m_interfaceSlots.pushConstOffset = offsetof(D3D9RenderStateInfo, pointSize);
      // Point scale never triggers on programmable
      m_interfaceSlots.pushConstSize   = sizeof(float) * 3;
      count = 8;
    }

    m_rsBlock = SetupRenderStateBlock(m_module, count);
  }


  void DxsoCompiler::emitFog() {
    DxsoRegister color0;
    color0.id = DxsoRegisterId{ DxsoRegisterType::ColorOut, 0 };
    auto oColor0Ptr = this->emitGetOperandPtr(color0);

    DxsoRegister vFog;
    vFog.id = DxsoRegisterId{ DxsoRegisterType::RasterizerOut, RasterOutFog };
    auto vFogPtr = this->emitGetOperandPtr(vFog);

    DxsoRegister vPos;
    vPos.id = DxsoRegisterId{ DxsoRegisterType::MiscType, DxsoMiscTypeIndices::MiscTypePosition };
    auto vPosPtr = this->emitGetOperandPtr(vPos);

    D3D9FogContext fogCtx;
    fogCtx.IsPixel     = true;
    fogCtx.RangeFog    = false;
    fogCtx.RenderState = m_rsBlock;
    fogCtx.vPos        = m_module.opLoad(getVectorTypeId(vPosPtr.type),    vPosPtr.id);
    fogCtx.vFog        = m_module.opLoad(getVectorTypeId(vFogPtr.type),    vFogPtr.id);
    fogCtx.oColor      = m_module.opLoad(getVectorTypeId(oColor0Ptr.type), oColor0Ptr.id);
    fogCtx.IsFixedFunction = false;
    fogCtx.IsPositionT = false;
    fogCtx.HasSpecular = false;
    fogCtx.Specular    = 0;

    m_module.opStore(oColor0Ptr.id, DoFixedFunctionFog(m_module, fogCtx));
  }

  
  void DxsoCompiler::emitPsProcessing() {
    uint32_t boolType  = m_module.defBoolType();
    uint32_t floatType = m_module.defFloatType(32);
    uint32_t floatPtr  = m_module.defPointerType(floatType, spv::StorageClassPushConstant);
    
    uint32_t alphaFuncId = m_module.specConst32(m_module.defIntType(32, 0), 0);
    m_module.setDebugName   (alphaFuncId, "alpha_func");
    m_module.decorateSpecId (alphaFuncId, getSpecId(D3D9SpecConstantId::AlphaCompareOp));

    // Implement alpha test and fog
    DxsoRegister color0;
    color0.id = DxsoRegisterId{ DxsoRegisterType::ColorOut, 0 };
    auto oC0 = this->emitGetOperandPtr(color0);
    
    if (oC0.id) {
      if (m_programInfo.majorVersion() < 3)
        emitFog();

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
      
      // if (alpha_func != ALWAYS) { ... }
      uint32_t isNotAlways = m_module.opINotEqual(boolType, alphaFuncId, m_module.constu32(VK_COMPARE_OP_ALWAYS));
      m_module.opSelectionMerge(atestSkipLabel, spv::SelectionControlMaskNone);
      m_module.opBranchConditional(isNotAlways, atestBeginLabel, atestSkipLabel);
      m_module.opLabel(atestBeginLabel);
      
      // Load alpha component
      uint32_t alphaComponentId = 3;
      uint32_t alphaId = m_module.opCompositeExtract(floatType,
        m_module.opLoad(m_module.defVectorType(floatType, 4), oC0.id),
        1, &alphaComponentId);

      if (m_moduleInfo.options.alphaTestWiggleRoom) {
        // NV has wonky interpolation of all 1's in a VS -> PS going to 0.999999...
        // This causes garbage-looking graphics on people's clothing in EverQuest 2 as it does alpha == 1.0.

        // My testing shows the alpha test has a precision of 1/256 for all A8 and below formats,
        // and around 1 / 2048 for A32F formats and 1 / 4096 for A16F formats (It makes no sense to me too)
        // so anyway, we're just going to round this to a precision of 1 / 4096 and hopefully this should make things happy
        // everywhere.
        const uint32_t alphaSizeId = m_module.constf32(4096.0f);

        alphaId = m_module.opFMul(floatType, alphaId, alphaSizeId);
        alphaId = m_module.opRound(floatType, alphaId);
        alphaId = m_module.opFDiv(floatType, alphaId, alphaSizeId);
      }
      
      // Load alpha reference
      uint32_t alphaRefMember = m_module.constu32(uint32_t(D3D9RenderStateItem::AlphaRef));
      uint32_t alphaRefId = m_module.opLoad(floatType,
        m_module.opAccessChain(floatPtr, m_rsBlock, 1, &alphaRefMember));
      
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

    if (m_ps.oDepth.id != 0) {
      auto result = emitValueLoad(m_ps.oDepth);

      result = emitSaturate(result);

      m_module.opStore(
        m_ps.oDepth.id,
        result.id);
    }
}


  void DxsoCompiler::emitVsFinalize() {
    this->emitMainFunctionBegin();

    this->emitInputSetup();
    m_module.opFunctionCall(
      m_module.defVoidType(),
      m_vs.functionId, 0, nullptr);
    this->emitLinkerOutputSetup();

    this->emitVsClipping();

    this->emitFunctionEnd();
  }

  void DxsoCompiler::emitPsFinalize() {
    this->emitMainFunctionBegin();

    this->emitInputSetup();

    bool canUsePixelFog = m_programInfo.majorVersion() < 3;

    if (canUsePixelFog) {
      // Look up vPos so it gets initted.
      DxsoRegister vPos;
      vPos.id = DxsoRegisterId{ DxsoRegisterType::MiscType, DxsoMiscTypeIndices::MiscTypePosition };
      this->emitGetOperandPtr(vPos);
    }

    if (m_ps.vPos.id != 0) {
      DxsoRegisterPointer fragCoord = this->emitRegisterPtr(
        "ps_frag_coord", DxsoScalarType::Float32, 4, 0,
        spv::StorageClassInput, spv::BuiltInFragCoord);

      DxsoRegisterValue val = this->emitValueLoad(fragCoord);
      val.id = m_module.opFSub(
        getVectorTypeId(val.type), val.id,
        m_module.constvec4f32(0.5f, 0.5f, 0.0f, 0.0f));

      m_module.opStore(m_ps.vPos.id, val.id);
    }

    if (m_ps.vFace.id != 0) {
      DxsoRegisterPointer faceBool = this->emitRegisterPtr(
        "ps_is_front_face", DxsoScalarType::Bool, 1, 0,
        spv::StorageClassInput, spv::BuiltInFrontFacing);

      DxsoRegisterValue frontFace = emitValueLoad(faceBool);
      DxsoRegisterValue selectOp = emitRegisterExtend(frontFace, 4);

      m_module.opStore(
        m_ps.vFace.id,
        m_module.opSelect(getVectorTypeId(m_ps.vFace.type), selectOp.id,
          m_module.constvec4f32( 1.0f,  1.0f,  1.0f,  1.0f),
          m_module.constvec4f32(-1.0f, -1.0f, -1.0f, -1.0f)));
    }

    m_module.opFunctionCall(
      m_module.defVoidType(),
      m_ps.functionId, 0, nullptr);

    if (m_ps.killState != 0) {
      uint32_t labelIf  = m_module.allocateId();
      uint32_t labelEnd = m_module.allocateId();
      
      uint32_t killTest = m_module.opLoad(m_module.defBoolType(), m_ps.killState);
      
      m_module.opSelectionMerge(labelEnd, spv::SelectionControlMaskNone);
      m_module.opBranchConditional(killTest, labelIf, labelEnd);
      
      m_module.opLabel(labelIf);
      m_module.opKill();
      
      m_module.opLabel(labelEnd);
    }

    // r0 in PS1 is the colour output register. Move r0 -> cO0 here.
    if (m_programInfo.majorVersion() == 1
    && m_programInfo.type() == DxsoProgramTypes::PixelShader) {
      DxsoRegister r0;
      r0.id = { DxsoRegisterType::Temp, 0 };

      DxsoRegister c0;
      c0.id = { DxsoRegisterType::ColorOut, 0 };

      DxsoRegisterValue val   = emitRegisterLoadRaw(r0, nullptr);
      DxsoRegisterPointer out = emitGetOperandPtr(c0);
      m_module.opStore(out.id, val.id);
    }

    // No need to setup output here as it's non-indexable
    // everything has already gone to the right place!

    this->emitPsProcessing();
    this->emitOutputDepthClamp();
    this->emitFunctionEnd();
  }



  uint32_t DxsoCompiler::getScalarTypeId(DxsoScalarType type) {
    switch (type) {
      case DxsoScalarType::Uint32:  return m_module.defIntType(32, 0);
      case DxsoScalarType::Sint32:  return m_module.defIntType(32, 1);
      case DxsoScalarType::Float32: return m_module.defFloatType(32);
      case DxsoScalarType::Bool:    return m_module.defBoolType();
    }

    throw DxvkError("DxsoCompiler: Invalid scalar type");
  }


  uint32_t DxsoCompiler::getVectorTypeId(const DxsoVectorType& type) {
    uint32_t typeId = this->getScalarTypeId(type.ctype);

    if (type.ccount > 1)
      typeId = m_module.defVectorType(typeId, type.ccount);

    return typeId;
  }


  uint32_t DxsoCompiler::getArrayTypeId(const DxsoArrayType& type) {
    DxsoVectorType vtype;
    vtype.ctype  = type.ctype;
    vtype.ccount = type.ccount;

    uint32_t typeId = this->getVectorTypeId(vtype);

    if (type.alength > 1) {
      typeId = m_module.defArrayType(typeId,
        m_module.constu32(type.alength));
    }

    return typeId;
  }


  uint32_t DxsoCompiler::getPointerTypeId(const DxsoRegisterInfo& type) {
    return m_module.defPointerType(
      this->getArrayTypeId(type.type),
      type.sclass);
  }

}