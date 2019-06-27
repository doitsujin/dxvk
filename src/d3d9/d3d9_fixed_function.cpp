#include "d3d9_fixed_function.h"

#include "d3d9_device.h"
#include "d3d9_spec_constants.h"

#include "../dxvk/dxvk_hash.h"
#include "../dxvk/dxvk_spec_const.h"

#include "../spirv/spirv_module.h"

namespace dxvk {

  class D3D9FFShaderCompiler {

  public:

    D3D9FFShaderCompiler(
            Rc<DxvkDevice>     Device,
      const D3D9FFShaderKeyVS& Key);

    D3D9FFShaderCompiler(
            Rc<DxvkDevice>     Device,
      const D3D9FFShaderKeyFS& Key);

    Rc<DxvkShader> compile();

    DxsoIsgn isgn() { return m_isgn; }

  private:

    // Returns value for inputs
    // Returns ptr for outputs
    uint32_t declareIO(bool input, DxsoSemantic semantic, spv::BuiltIn builtin = spv::BuiltInMax);

    void compileVS();

    void compilePS();

    bool isVS() { return m_programType == DxsoProgramType::VertexShader; }
    bool isPS() { return !isVS(); }

    std::string           m_filename;

    SpirvModule           m_module;
    std::vector
      <DxvkResourceSlot>  m_resourceSlots;
    DxvkInterfaceSlots    m_interfaceSlots;
    std::vector<uint32_t> m_entryPointInterfaces;

    DxsoProgramType       m_programType;
    D3D9FFShaderKeyVS     m_vsKey;
    D3D9FFShaderKeyFS     m_fsKey;

    DxsoIsgn              m_isgn;
    DxsoIsgn              m_osgn;

    uint32_t              m_floatType;
    uint32_t              m_vec4Type;
    uint32_t              m_mat4Type;

    uint32_t              m_entryPointId;
  };

  D3D9FFShaderCompiler::D3D9FFShaderCompiler(
          Rc<DxvkDevice>     Device,
    const D3D9FFShaderKeyVS& Key) {
    m_programType = DxsoProgramTypes::VertexShader;
    m_vsKey    = Key;
  }


  D3D9FFShaderCompiler::D3D9FFShaderCompiler(
          Rc<DxvkDevice>     Device,
    const D3D9FFShaderKeyFS& Key) {
    m_programType = DxsoProgramTypes::PixelShader;
    m_fsKey    = Key;
  }


  Rc<DxvkShader> D3D9FFShaderCompiler::compile() {
    m_floatType = m_module.defFloatType(32);
    m_vec4Type  = m_module.defVectorType(m_floatType, 4);
    m_mat4Type  = m_module.defMatrixType(m_vec4Type, 4);

    m_entryPointId = m_module.allocateId();

    // Set the shader name so that we recognize it in renderdoc
    m_module.setDebugSource(
      spv::SourceLanguageUnknown, 0,
      m_module.addDebugString(m_filename.c_str()),
      nullptr);

    // Set the memory model. This is the same for all shaders.
    m_module.setMemoryModel(
      spv::AddressingModelLogical,
      spv::MemoryModelGLSL450);

    m_module.enableCapability(spv::CapabilityShader);
    m_module.enableCapability(spv::CapabilityImageQuery);

    m_module.functionBegin(
      m_module.defVoidType(), m_entryPointId, m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr),
      spv::FunctionControlMaskNone);
    m_module.setDebugName(m_entryPointId, "main");

    m_module.opLabel(m_module.allocateId());

    if (isVS())
      compileVS();
    else
      compilePS();

    m_module.opReturn();
    m_module.functionEnd();

    // Declare the entry point, we now have all the
    // information we need, including the interfaces
    m_module.addEntryPoint(m_entryPointId,
      isVS() ? spv::ExecutionModelVertex : spv::ExecutionModelFragment, "main",
      m_entryPointInterfaces.size(),
      m_entryPointInterfaces.data());

    DxvkShaderOptions shaderOptions = { };

    DxvkShaderConstData constData = { };

    // Create the shader module object
    return new DxvkShader(
      isVS() ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT,
      m_resourceSlots.size(),
      m_resourceSlots.data(),
      m_interfaceSlots,
      m_module.compile(),
      shaderOptions,
      std::move(constData));
  }


  uint32_t D3D9FFShaderCompiler::declareIO(bool input, DxsoSemantic semantic, spv::BuiltIn builtin) {
    // Declare in ISGN and do linkage
    auto& sgn = input
      ? m_isgn : m_osgn;

    uint32_t& slots = input
      ? m_interfaceSlots.inputSlots
      : m_interfaceSlots.outputSlots;

    uint32_t i = sgn.elemCount++;

    uint32_t slot = i;

    if (builtin == spv::BuiltInMax) {
      if (input != isVS()) {
        slot = RegisterLinkerSlot(semantic); // Requires linkage...
      }

      slots |= 1u << slot;
    }

    auto& elem = sgn.elems[i];
    elem.slot = slot;
    elem.semantic = semantic;

    // Declare variable
    spv::StorageClass storageClass = input ?
      spv::StorageClassInput : spv::StorageClassOutput;

    uint32_t ptrType = m_module.defPointerType(m_vec4Type, storageClass);

    uint32_t ptr = m_module.newVar(ptrType, storageClass);

    if (builtin == spv::BuiltInMax)
      m_module.decorateLocation(ptr, slot);
    else
      m_module.decorateBuiltIn(ptr, builtin);

    std::string name = str::format(input ? "in_" : "out_", semantic.usage, semantic.usageIndex);
    m_module.setDebugName(ptr, name.c_str());

    m_entryPointInterfaces.push_back(ptr);

    if (input)
      return m_module.opLoad(m_vec4Type, ptr);

    return ptr;
  }


  void D3D9FFShaderCompiler::compileVS() {
    struct VertexData {
      uint32_t constantBuffer = 0;

      enum FFConstantMembersVS {
        ConstWorldMatrix = 0,
        ConstViewMatrix  = 1,
        ConstProjMatrix  = 2,

        ConstMemberCount
      };

      struct {
        uint32_t world      = { 0 };
        uint32_t view       = { 0 };
        uint32_t proj       = { 0 };
      } constants;

      struct {
        uint32_t POSITION    = { 0 };
        uint32_t TEXCOORD[8] = { 0 };
        uint32_t COLOR[2]    = { 0 };
      } in;

      struct {
        uint32_t POSITION    = { 0 };
        uint32_t TEXCOORD[8] = { 0 };
        uint32_t COLOR[2]    = { 0 };
      } out;
    } vs;

    // VS Caps
    m_module.enableCapability(spv::CapabilityClipDistance);
    m_module.enableCapability(spv::CapabilityDrawParameters);

    m_module.enableExtension("SPV_KHR_shader_draw_parameters");

    // Constant Buffer for VS.
    std::array<uint32_t, VertexData::ConstMemberCount> members = {
      m_mat4Type, // World
      m_mat4Type, // View
      m_mat4Type, // Proj
    };

    const uint32_t structType =
      m_module.defStructType(members.size(), members.data());

    m_module.decorateBlock(structType);
    for (uint32_t i = 0; i < VertexData::ConstMemberCount; i++) {
      m_module.memberDecorateOffset(structType, i, i * sizeof(Matrix4));
      m_module.memberDecorateMatrixStride(structType, i, 16);
      m_module.memberDecorate(structType, i, spv::DecorationRowMajor);
    }

    m_module.setDebugName(structType, "D3D9FixedFunctionVS");
    m_module.setDebugMemberName(structType, 0, "world");
    m_module.setDebugMemberName(structType, 1, "view");
    m_module.setDebugMemberName(structType, 2, "proj");

    vs.constantBuffer = m_module.newVar(
      m_module.defPointerType(structType, spv::StorageClassUniform),
      spv::StorageClassUniform);

    m_module.setDebugName(vs.constantBuffer, "consts");

    const uint32_t bindingId = computeResourceSlotId(
      DxsoProgramType::VertexShader, DxsoBindingType::ConstantBuffer,
      DxsoConstantBuffers::VSFixedFunction);

    m_module.decorateDescriptorSet(vs.constantBuffer, 0);
    m_module.decorateBinding(vs.constantBuffer, bindingId);

    DxvkResourceSlot resource;
    resource.slot   = bindingId;
    resource.type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    resource.view   = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    resource.access = VK_ACCESS_UNIFORM_READ_BIT;
    m_resourceSlots.push_back(resource);

    // Load constants
    auto LoadMatrix = [&, this](uint32_t idx) {
      uint32_t offset  = m_module.constu32(idx);
      uint32_t mat4Ptr = m_module.defPointerType(m_mat4Type, spv::StorageClassUniform);

      return m_module.opLoad(m_mat4Type,
        m_module.opAccessChain(mat4Ptr, vs.constantBuffer, 1, &offset));
    };

    vs.constants.world = LoadMatrix(VertexData::ConstWorldMatrix);
    vs.constants.view = LoadMatrix(VertexData::ConstViewMatrix);
    vs.constants.proj = LoadMatrix(VertexData::ConstProjMatrix);

    // Do IO
    vs.in.POSITION = declareIO(true, DxsoSemantic{ DxsoUsage::Position, 0 });
    for (uint32_t i = 0; i < 8; i++)
      vs.in.TEXCOORD[i] = declareIO(true, DxsoSemantic{ DxsoUsage::Texcoord, i });

    if (m_vsKey.HasDiffuse)
      vs.in.COLOR[0] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 0 });
    else
      vs.in.COLOR[0] = m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f);

    vs.in.COLOR[1] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 1 });

    // Declare Outputs
    vs.out.POSITION = declareIO(false, DxsoSemantic{ DxsoUsage::Position, 0 }, spv::BuiltInPosition);

    for (uint32_t i = 0; i < 8; i++)
      vs.out.TEXCOORD[i] = declareIO(false, DxsoSemantic{ DxsoUsage::Texcoord, i });

    vs.out.COLOR[0] = declareIO(false, DxsoSemantic{ DxsoUsage::Color, 0 });
    vs.out.COLOR[1] = declareIO(false, DxsoSemantic{ DxsoUsage::Color, 1 });

    ////////////////////////////////////////
    // ACTUAL CODE

    // gl_Position = vec4(in_POSITION.xyz, 1);

    uint32_t gl_Position = vs.in.POSITION;

    if (!m_vsKey.HasPositionT) {
      uint32_t wvp = m_module.opMatrixTimesMatrix(m_mat4Type, vs.constants.world, vs.constants.view);
                wvp = m_module.opMatrixTimesMatrix(m_mat4Type, wvp,                vs.constants.proj);

      gl_Position = m_module.opVectorTimesMatrix(m_vec4Type, gl_Position, wvp);
    }

    m_module.opStore(vs.out.POSITION, gl_Position);

    for (uint32_t i = 0; i < 8; i++)
      m_module.opStore(vs.out.TEXCOORD[i], vs.in.TEXCOORD[i]);

    m_module.opStore(vs.out.COLOR[0], vs.in.COLOR[0]);
    m_module.opStore(vs.out.COLOR[1], vs.in.COLOR[1]);

    ////////////////////////////////////////
    // END OF ACTUAL CODE
  }


  void D3D9FFShaderCompiler::compilePS() {
    struct PixelData {
      struct {
        uint32_t TEXCOORD[8] = { 0 };
        uint32_t COLOR[2]    = { 0 };
      } in;

      struct {
        uint32_t typeId = { 0 };
        uint32_t varId  = { 0 };
      } samplers[8];

      struct {
        uint32_t COLOR = { 0 };
      } out;
    } ps;

    // PS Caps
    m_module.enableCapability(spv::CapabilityDerivativeControl);

    m_module.setExecutionMode(m_entryPointId,
      spv::ExecutionModeOriginUpperLeft);

    for (uint32_t i = 0; i < 8; i++)
      ps.in.TEXCOORD[i] = declareIO(true, DxsoSemantic{ DxsoUsage::Texcoord, i });

    ps.in.COLOR[0] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 0 });
    ps.in.COLOR[1] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 1 });

    ps.out.COLOR = declareIO(false, DxsoSemantic{ DxsoUsage::Color, 0 });

    // Samplers
    for (uint32_t i = 0; i < 8; i++) {
      // Only 2D for now...
      auto& sampler = ps.samplers[i];

      sampler.typeId = m_module.defImageType(
        m_module.defFloatType(32),
        spv::Dim2D, 0, 0, 0, 1,
        spv::ImageFormatUnknown);

      sampler.typeId = m_module.defSampledImageType(sampler.typeId);

      sampler.varId = m_module.newVar(
        m_module.defPointerType(
          sampler.typeId, spv::StorageClassUniformConstant),
        spv::StorageClassUniformConstant);

      std::string name = str::format("s", i);
      m_module.setDebugName(sampler.varId, name.c_str());

      const uint32_t bindingId = computeResourceSlotId(DxsoProgramType::PixelShader,
        DxsoBindingType::ColorImage, i);

      m_module.decorateDescriptorSet(sampler.varId, 0);
      m_module.decorateBinding(sampler.varId, bindingId);

      // Store descriptor info for the shader interface
      DxvkResourceSlot resource;
      resource.slot = bindingId;
      resource.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      resource.view = VK_IMAGE_VIEW_TYPE_2D;
      resource.access = VK_ACCESS_SHADER_READ_BIT;
      m_resourceSlots.push_back(resource);
    }

    ////////////////////////////////////////
    // ACTUAL CODE
    uint32_t color = ps.in.COLOR[0];

    SpirvImageOperands imageOperands;
    const uint32_t imageVarId = m_module.opLoad(ps.samplers[0].typeId, ps.samplers[0].varId);
    uint32_t sample = m_module.opImageSampleImplicitLod(m_vec4Type, imageVarId, ps.in.TEXCOORD[0], imageOperands);
    color = m_module.opFMul(m_vec4Type, color, sample);
    /*for (uint32_t i = 0; i < 8; i++) {
      
    }*/
    /////////////////////////////////////////
    // END OF ACTUAL CODE

    // ALPHA TESTING!
    uint32_t boolType = m_module.defBoolType();
    uint32_t floatType = m_module.defFloatType(32);
    uint32_t floatPtr = m_module.defPointerType(floatType, spv::StorageClassPushConstant);

    // Declare uniform buffer containing render states
    enum RenderStateMember : uint32_t {
      RsAlphaRef = 0,
    };

    std::array<uint32_t, 1> rsMembers = { {
      floatType,
    } };

    uint32_t rsStruct = m_module.defStructTypeUnique(rsMembers.size(), rsMembers.data());
    uint32_t rsBlock = m_module.newVar(
      m_module.defPointerType(rsStruct, spv::StorageClassPushConstant),
      spv::StorageClassPushConstant);

    m_module.setDebugName(rsStruct, "render_state_t");
    m_module.decorate(rsStruct, spv::DecorationBlock);
    m_module.setDebugMemberName(rsStruct, 0, "alpha_ref");
    m_module.memberDecorateOffset(rsStruct, 0, offsetof(D3D9RenderStateInfo, alphaRef));

    m_module.setDebugName(rsBlock, "render_state");

    m_interfaceSlots.pushConstOffset = 0;
    m_interfaceSlots.pushConstSize = sizeof(D3D9RenderStateInfo);

    // Declare spec constants for render states
    uint32_t alphaTestId = m_module.specConstBool(false);
    uint32_t alphaFuncId = m_module.specConst32(m_module.defIntType(32, 0), uint32_t(VK_COMPARE_OP_ALWAYS));

    m_module.setDebugName(alphaTestId, "alpha_test");
    m_module.decorateSpecId(alphaTestId, getSpecId(D3D9SpecConstantId::AlphaTestEnable));

    m_module.setDebugName(alphaFuncId, "alpha_func");
    m_module.decorateSpecId(alphaFuncId, getSpecId(D3D9SpecConstantId::AlphaCompareOp));

    m_module.opStore(ps.out.COLOR, color);

    // Implement alpha test
    auto oC0 = ps.out.COLOR;
    // Labels for the alpha test
    std::array<SpirvSwitchCaseLabel, 8> atestCaseLabels = { {
      { uint32_t(VK_COMPARE_OP_NEVER),            m_module.allocateId() },
      { uint32_t(VK_COMPARE_OP_LESS),             m_module.allocateId() },
      { uint32_t(VK_COMPARE_OP_EQUAL),            m_module.allocateId() },
      { uint32_t(VK_COMPARE_OP_LESS_OR_EQUAL),    m_module.allocateId() },
      { uint32_t(VK_COMPARE_OP_GREATER),          m_module.allocateId() },
      { uint32_t(VK_COMPARE_OP_NOT_EQUAL),        m_module.allocateId() },
      { uint32_t(VK_COMPARE_OP_GREATER_OR_EQUAL), m_module.allocateId() },
      { uint32_t(VK_COMPARE_OP_ALWAYS),           m_module.allocateId() },
    } };

    uint32_t atestBeginLabel = m_module.allocateId();
    uint32_t atestTestLabel = m_module.allocateId();
    uint32_t atestDiscardLabel = m_module.allocateId();
    uint32_t atestKeepLabel = m_module.allocateId();
    uint32_t atestSkipLabel = m_module.allocateId();

    // if (alpha_test) { ... }
    m_module.opSelectionMerge(atestSkipLabel, spv::SelectionControlMaskNone);
    m_module.opBranchConditional(alphaTestId, atestBeginLabel, atestSkipLabel);
    m_module.opLabel(atestBeginLabel);

    // Load alpha component
    uint32_t alphaComponentId = 3;
    uint32_t alphaId = m_module.opCompositeExtract(floatType,
      m_module.opLoad(m_vec4Type, oC0),
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
      atestVariables[i].varId = [&] {
        switch (VkCompareOp(atestCaseLabels[i].literal)) {
        case VK_COMPARE_OP_NEVER:            return m_module.constBool(false);
        case VK_COMPARE_OP_LESS:             return m_module.opFOrdLessThan(boolType, alphaId, alphaRefId);
        case VK_COMPARE_OP_EQUAL:            return m_module.opFOrdEqual(boolType, alphaId, alphaRefId);
        case VK_COMPARE_OP_LESS_OR_EQUAL:    return m_module.opFOrdLessThanEqual(boolType, alphaId, alphaRefId);
        case VK_COMPARE_OP_GREATER:          return m_module.opFOrdGreaterThan(boolType, alphaId, alphaRefId);
        case VK_COMPARE_OP_NOT_EQUAL:        return m_module.opFOrdNotEqual(boolType, alphaId, alphaRefId);
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


  D3D9FFShader::D3D9FFShader(
          D3D9DeviceEx*         pDevice,
    const D3D9FFShaderKeyVS&    Key) {
    D3D9FFShaderCompiler compiler(
      pDevice->GetDXVKDevice(),
      Key);

    m_shader = compiler.compile();
    m_isgn   = compiler.isgn();

    Dump<false>(Key);

    pDevice->GetDXVKDevice()->registerShader(m_shader);
  }


  D3D9FFShader::D3D9FFShader(
          D3D9DeviceEx*         pDevice,
    const D3D9FFShaderKeyFS&    Key) {
    D3D9FFShaderCompiler compiler(
      pDevice->GetDXVKDevice(),
      Key);

    m_shader = compiler.compile();
    m_isgn   = compiler.isgn();

    Dump<true>(Key);

    pDevice->GetDXVKDevice()->registerShader(m_shader);
  }

  template <bool FS, typename T>
  void D3D9FFShader::Dump(const T& Key) {
    const std::string dumpPath = env::getEnvVar("DXVK_SHADER_DUMP_PATH");

    if (dumpPath.size() != 0) {
      D3D9FFShaderKeyHash hash;

      std::ofstream dumpStream(
        str::format(dumpPath, "/", FS ? "FS_FF_" : "VS_FF_", hash(Key), ".spv"),
        std::ios_base::binary | std::ios_base::trunc);
      
      m_shader->dump(dumpStream);
    }
  }


  D3D9FFShader D3D9FFShaderModuleSet::GetShaderModule(
          D3D9DeviceEx*         pDevice,
    const D3D9FFShaderKeyVS&    ShaderKey) {
    // Use the shader's unique key for the lookup
    auto entry = m_vsModules.find(ShaderKey);
    if (entry != m_vsModules.end())
      return entry->second;
    
    D3D9FFShader shader(
      pDevice, ShaderKey);

    m_vsModules.insert({ShaderKey, shader});

    return shader;
  }


  D3D9FFShader D3D9FFShaderModuleSet::GetShaderModule(
          D3D9DeviceEx*         pDevice,
    const D3D9FFShaderKeyFS&    ShaderKey) {
    // Use the shader's unique key for the lookup
    auto entry = m_fsModules.find(ShaderKey);
    if (entry != m_fsModules.end())
      return entry->second;
    
    D3D9FFShader shader(
      pDevice, ShaderKey);

    m_fsModules.insert({ShaderKey, shader});

    return shader;
  }


  size_t D3D9FFShaderKeyHash::operator () (const D3D9FFShaderKeyVS& key) const {
    DxvkHashState state;

    std::hash<bool> bhash;

    state.add(bhash(key.HasPositionT));
    state.add(bhash(key.HasDiffuse));

    return state;
  }


  size_t D3D9FFShaderKeyHash::operator () (const D3D9FFShaderKeyFS& key) const {
    DxvkHashState state;

    return state;
  }


  bool operator == (const D3D9FFShaderKeyVS& a, const D3D9FFShaderKeyVS& b) {
    return a.HasPositionT == b.HasPositionT
        && a.HasDiffuse   == b.HasDiffuse;
  }


  bool operator == (const D3D9FFShaderKeyFS& a, const D3D9FFShaderKeyFS& b) {
    return true;
  }


  bool operator != (const D3D9FFShaderKeyVS& a, const D3D9FFShaderKeyVS& b) {
    return !(a == b);
  }


  bool operator != (const D3D9FFShaderKeyFS& a, const D3D9FFShaderKeyFS& b) {
    return !(a == b);
  }


  bool D3D9FFShaderKeyEq::operator () (const D3D9FFShaderKeyVS& a, const D3D9FFShaderKeyVS& b) const {
    return a == b;
  }


  bool D3D9FFShaderKeyEq::operator () (const D3D9FFShaderKeyFS& a, const D3D9FFShaderKeyFS& b) const {
    return a == b;
  }

}