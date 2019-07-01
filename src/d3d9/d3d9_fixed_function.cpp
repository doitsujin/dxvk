#include "d3d9_fixed_function.h"

#include "d3d9_device.h"
#include "d3d9_spec_constants.h"

#include "../dxvk/dxvk_hash.h"
#include "../dxvk/dxvk_spec_const.h"

#include "../spirv/spirv_module.h"

namespace dxvk {

    enum FFConstantMembersVS {
      VSConstWorldMatrix   = 0,
      VSConstViewMatrix    = 1,
      VSConstProjMatrix    = 2,

      VSConstInverseOffset = 3,
      VSConstInverseExtent = 4,

      VSConstGlobalAmbient = 5,

      VSConstMaterialDiffuse  = 6,
      VSConstMaterialAmbient  = 7,
      VSConstMaterialSpecular = 8,
      VSConstMaterialEmissive = 9,
      VSConstMaterialPower    = 10,

      VSConstMemberCount
    };

  struct D3D9FFVertexData {
    uint32_t constantBuffer = 0;

    struct {
      uint32_t world = { 0 };
      uint32_t view = { 0 };
      uint32_t proj = { 0 };

      uint32_t invOffset = { 0 };
      uint32_t invExtent = { 0 };

      uint32_t globalAmbient = { 0 };

      uint32_t materialDiffuse = { 0 };
      uint32_t materialSpecular = { 0 };
      uint32_t materialAmbient = { 0 };
      uint32_t materialEmissive = { 0 };
      uint32_t materialPower = { 0 };
    } constants;

    struct {
      uint32_t POSITION = { 0 };
      uint32_t TEXCOORD[8] = { 0 };
      uint32_t COLOR[2] = { 0 };
    } in;

    struct {
      uint32_t POSITION = { 0 };
      uint32_t TEXCOORD[8] = { 0 };
      uint32_t COLOR[2] = { 0 };
    } out;
  };

  enum FFConstantMembersPS {
    PSConstTextureFactor = 0,

    PSConstMemberCount
  };

  struct D3D9FFPixelData {
    uint32_t constantBuffer = 0;

    struct {
      uint32_t textureFactor = { 0 };
    } constants;

    struct {
      uint32_t TEXCOORD[8] = { 0 };
      uint32_t COLOR[2]    = { 0 };
    } in;

    struct {
      uint32_t typeId = { 0 };
      uint32_t varId  = { 0 };
      uint32_t bound  = { 0 };
    } samplers[8];

    struct {
      uint32_t COLOR = { 0 };
    } out;
  };

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

    void setupVS();

    void compilePS();

    void setupPS();

    void alphaTestPS();

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

    D3D9FFVertexData      m_vs;
    D3D9FFPixelData       m_ps;

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
    setupVS();

    uint32_t gl_Position = m_vs.in.POSITION;

    if (!m_vsKey.HasPositionT) {
      uint32_t wvp = m_module.opMatrixTimesMatrix(m_mat4Type,  m_vs.constants.world, m_vs.constants.view);
                wvp = m_module.opMatrixTimesMatrix(m_mat4Type, wvp,                  m_vs.constants.proj);

      gl_Position = m_module.opVectorTimesMatrix(m_vec4Type, gl_Position, wvp);
    } else {
      gl_Position = m_module.opFMul(m_vec4Type, gl_Position, m_vs.constants.invExtent);
      gl_Position = m_module.opFAdd(m_vec4Type, gl_Position, m_vs.constants.invOffset);

      // Set W to 1.
      // TODO: Is this the correct solution?
      // other implementations do not do this...
      const uint32_t wIndex = 3;
      gl_Position  = m_module.opCompositeInsert(m_vec4Type, m_module.constf32(1.0f), gl_Position, 1, &wIndex);
    }

    m_module.opStore(m_vs.out.POSITION, gl_Position);

    for (uint32_t i = 0; i < caps::TextureStageCount; i++)
      m_module.opStore(m_vs.out.TEXCOORD[i], m_vs.in.TEXCOORD[i]);

    if (m_vsKey.UseLighting) {
      auto PickSource = [&](D3DMATERIALCOLORSOURCE Source, uint32_t Material) {
        if (Source == D3DMCS_MATERIAL)
          return Material;
        else if (Source == D3DMCS_COLOR1)
          return m_vs.in.COLOR[0];
        else
          return m_vs.in.COLOR[1];
      };

      std::array<uint32_t, 4> indices = { 0, 1, 2, 4 + 3 };

      uint32_t diffuse  = PickSource(m_vsKey.DiffuseSource,  m_vs.constants.materialDiffuse);
      uint32_t ambient  = PickSource(m_vsKey.AmbientSource,  m_vs.constants.materialAmbient);
      uint32_t emissive = PickSource(m_vsKey.EmissiveSource, m_vs.constants.materialEmissive);
      uint32_t specular = PickSource(m_vsKey.SpecularSource, m_vs.constants.materialSpecular);

      // Currently we do not handle real lighting... Just darkness.

      // Handle ambient.
      uint32_t finalColor = m_module.opFFma(m_vec4Type, ambient, m_vs.constants.globalAmbient, emissive);
      // Set alpha to zero.
      const uint32_t alphaIndex = 3;
      finalColor = m_module.opCompositeInsert(m_vec4Type, m_module.constf32(0.0f), finalColor, 1, &alphaIndex);

      // Add the diffuse
      finalColor = m_module.opFAdd(m_vec4Type, finalColor, diffuse);
      // Saturate
      finalColor = m_module.opFClamp(m_vec4Type, finalColor,
        m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f),
        m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f));

      m_module.opStore(m_vs.out.COLOR[0], finalColor);
      m_module.opStore(m_vs.out.COLOR[1], specular);
    }
    else {
      m_module.opStore(m_vs.out.COLOR[0], m_vs.in.COLOR[0]);
      m_module.opStore(m_vs.out.COLOR[1], m_vs.in.COLOR[1]);
    }
  }


  void D3D9FFShaderCompiler::setupVS() {
    // VS Caps
    m_module.enableCapability(spv::CapabilityClipDistance);
    m_module.enableCapability(spv::CapabilityDrawParameters);

    m_module.enableExtension("SPV_KHR_shader_draw_parameters");

    // Constant Buffer for VS.
    std::array<uint32_t, VSConstMemberCount> members = {
      m_mat4Type, // World
      m_mat4Type, // View
      m_mat4Type, // Proj

      m_vec4Type, // Inverse Offset
      m_vec4Type, // Inverse Extent

      m_vec4Type, // Global Ambient

      m_vec4Type,  // Material Diffuse
      m_vec4Type,  // Material Ambient
      m_vec4Type,  // Material Specular
      m_vec4Type,  // Material Emissive
      m_floatType, // Material Power
    };

    const uint32_t structType =
      m_module.defStructType(members.size(), members.data());

    m_module.decorateBlock(structType);
    uint32_t offset = 0;
    for (uint32_t i = 0; i < VSConstInverseOffset; i++) {
      m_module.memberDecorateOffset(structType, i, offset);
      offset += sizeof(Matrix4);
      m_module.memberDecorateMatrixStride(structType, i, 16);
      m_module.memberDecorate(structType, i, spv::DecorationRowMajor);
    }

    for (uint32_t i = VSConstInverseOffset; i < VSConstMaterialPower; i++) {
      m_module.memberDecorateOffset(structType, i, offset);
      offset += sizeof(Vector4);
    }

    m_module.memberDecorateOffset(structType, VSConstMaterialPower, offset);
    offset += sizeof(float);

    m_module.setDebugName(structType, "D3D9FixedFunctionVS");
    m_module.setDebugMemberName(structType, 0, "world");
    m_module.setDebugMemberName(structType, 1, "view");
    m_module.setDebugMemberName(structType, 2, "proj");
    m_module.setDebugMemberName(structType, 3, "inverseOffset");

    m_vs.constantBuffer = m_module.newVar(
      m_module.defPointerType(structType, spv::StorageClassUniform),
      spv::StorageClassUniform);

    m_module.setDebugName(m_vs.constantBuffer, "consts");

    const uint32_t bindingId = computeResourceSlotId(
      DxsoProgramType::VertexShader, DxsoBindingType::ConstantBuffer,
      DxsoConstantBuffers::VSFixedFunction);

    m_module.decorateDescriptorSet(m_vs.constantBuffer, 0);
    m_module.decorateBinding(m_vs.constantBuffer, bindingId);

    DxvkResourceSlot resource;
    resource.slot   = bindingId;
    resource.type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    resource.view   = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    resource.access = VK_ACCESS_UNIFORM_READ_BIT;
    m_resourceSlots.push_back(resource);

    // Load constants
    auto LoadConstant = [&](uint32_t type, uint32_t idx) {
      uint32_t offset  = m_module.constu32(idx);
      uint32_t typePtr = m_module.defPointerType(type, spv::StorageClassUniform);

      return m_module.opLoad(type,
        m_module.opAccessChain(typePtr, m_vs.constantBuffer, 1, &offset));
    };

    m_vs.constants.world = LoadConstant(m_mat4Type, VSConstWorldMatrix);
    m_vs.constants.view  = LoadConstant(m_mat4Type, VSConstViewMatrix);
    m_vs.constants.proj  = LoadConstant(m_mat4Type, VSConstProjMatrix);

    m_vs.constants.invOffset = LoadConstant(m_vec4Type, VSConstInverseOffset);
    m_vs.constants.invExtent = LoadConstant(m_vec4Type, VSConstInverseExtent);

    m_vs.constants.globalAmbient = LoadConstant(m_vec4Type, VSConstGlobalAmbient);

    m_vs.constants.materialDiffuse  = LoadConstant(m_vec4Type,  VSConstMaterialDiffuse);
    m_vs.constants.materialAmbient  = LoadConstant(m_vec4Type,  VSConstMaterialAmbient);
    m_vs.constants.materialSpecular = LoadConstant(m_vec4Type,  VSConstMaterialSpecular);
    m_vs.constants.materialEmissive = LoadConstant(m_vec4Type,  VSConstMaterialEmissive);
    m_vs.constants.materialPower    = LoadConstant(m_floatType, VSConstMaterialPower);

    // Do IO
    m_vs.in.POSITION = declareIO(true, DxsoSemantic{ DxsoUsage::Position, 0 });
    for (uint32_t i = 0; i < caps::TextureStageCount; i++)
      m_vs.in.TEXCOORD[i] = declareIO(true, DxsoSemantic{ DxsoUsage::Texcoord, i });

    if (m_vsKey.HasColor0)
      m_vs.in.COLOR[0] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 0 });
    else
      m_vs.in.COLOR[0] = m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f);

    if (m_vsKey.HasColor1)
      m_vs.in.COLOR[1] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 1 });
    else
      m_vs.in.COLOR[1] = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);

    // Declare Outputs
    m_vs.out.POSITION = declareIO(false, DxsoSemantic{ DxsoUsage::Position, 0 }, spv::BuiltInPosition);

    for (uint32_t i = 0; i < caps::TextureStageCount; i++)
      m_vs.out.TEXCOORD[i] = declareIO(false, DxsoSemantic{ DxsoUsage::Texcoord, i });

    m_vs.out.COLOR[0] = declareIO(false, DxsoSemantic{ DxsoUsage::Color, 0 });
    m_vs.out.COLOR[1] = declareIO(false, DxsoSemantic{ DxsoUsage::Color, 1 });
  }


  void D3D9FFShaderCompiler::compilePS() {
    setupPS();

    uint32_t diffuse  = m_ps.in.COLOR[0];
    uint32_t specular = m_ps.in.COLOR[1];

    // Current starts of as equal to diffuse.
    uint32_t current = diffuse;
    // Temp starts off as equal to vec4(0)
    uint32_t temp  = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);
    
    uint32_t texture = m_module.newVarInit(
      m_module.defPointerType(m_vec4Type, spv::StorageClassPrivate),
      spv::StorageClassPrivate,
      m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f));

    for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
      const auto& stage = m_fsKey.Stages[i].data;

      bool processedTexture = false;

      auto GetTexture = [&]() {
        if (!processedTexture) {
          uint32_t newTextureLabel = m_module.allocateId();
          uint32_t oldTextureLabel = m_module.allocateId();
          uint32_t endLabel        = m_module.allocateId();

          m_module.opSelectionMerge(endLabel, spv::SelectionControlMaskNone);
          m_module.opBranchConditional(m_ps.samplers[i].bound, newTextureLabel, oldTextureLabel);

          m_module.opLabel(newTextureLabel);
          SpirvImageOperands imageOperands;
          uint32_t imageVarId = m_module.opLoad(m_ps.samplers[i].typeId, m_ps.samplers[i].varId);
          uint32_t sample     = m_module.opImageSampleImplicitLod(m_vec4Type, imageVarId, m_ps.in.TEXCOORD[i], imageOperands);
          m_module.opStore(texture, sample);
          m_module.opBranch(endLabel);

          m_module.opLabel(oldTextureLabel);
          m_module.opBranch(endLabel);

          m_module.opLabel(endLabel);
        }

        processedTexture = true;

        return m_module.opLoad(m_vec4Type, texture);
      };

      auto AlphaReplicate = [&](uint32_t reg) {
        uint32_t alphaComponentId = 3;
        uint32_t alpha = m_module.opCompositeExtract(m_floatType, reg, 1, &alphaComponentId);

        std::array<uint32_t, 4> replicant = { alpha, alpha, alpha, alpha };
        return m_module.opCompositeConstruct(m_vec4Type, replicant.size(), replicant.data());
      };

      auto Complement = [&](uint32_t reg) {
        return m_module.opFSub(m_vec4Type,
          m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f),
          reg);
      };

      auto GetArg = [&] (uint32_t arg) {
        uint32_t reg = m_module.constvec4f32(0,0,0,0);

        switch (arg & D3DTA_SELECTMASK) {
          case D3DTA_CONSTANT:
            Logger::warn("D3DTA_CONSTANT: not supported right now.");
            break;
          case D3DTA_CURRENT:
            reg = current;
            break;
          case D3DTA_DIFFUSE:
            reg = diffuse;
            break;
          case D3DTA_SPECULAR:
            reg = specular;
            break;
          case D3DTA_TEMP:
            reg = temp;
            break;
          case D3DTA_TEXTURE:
            reg = GetTexture();
            break;
          case D3DTA_TFACTOR:
            reg = m_ps.constants.textureFactor;
            break;
          default:
            break;
        }

        // reg = 1 - reg
        if (arg & D3DTA_COMPLEMENT)
          reg = Complement(reg);

        // reg = reg.wwww
        if (arg & D3DTA_ALPHAREPLICATE)
          reg = AlphaReplicate(reg);

        return reg;
      };

      auto DoOp = [&](D3DTEXTUREOP op, uint32_t dst, std::array<uint32_t, TextureArgCount> arg) {

        // Dest should be self-saturated if it is used.
        if (op != D3DTOP_SELECTARG1        && op != D3DTOP_SELECTARG2          &&
            op != D3DTOP_MODULATE          && op != D3DTOP_PREMODULATE         &&
            op != D3DTOP_BLENDDIFFUSEALPHA && op != D3DTOP_BLENDTEXTUREALPHA   &&
            op != D3DTOP_BLENDFACTORALPHA  && op != D3DTOP_BLENDCURRENTALPHA   &&
            op != D3DTOP_BUMPENVMAP        && op != D3DTOP_BUMPENVMAPLUMINANCE &&
            op != D3DTOP_LERP)
          dst = m_module.opFClamp(m_vec4Type, dst,
            m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f),
            m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f));

        switch (op) {
          case D3DTOP_SELECTARG1:
            dst = arg[1];
            break;
          case D3DTOP_SELECTARG2:
            dst = arg[2];
            break;

          case D3DTOP_MODULATE4X:
          case D3DTOP_MODULATE2X:
          case D3DTOP_MODULATE:
            dst = m_module.opFMul(m_vec4Type, arg[1], arg[2]);
            if (op == D3DTOP_MODULATE4X || op == D3DTOP_MODULATE2X) {
              float m = op == D3DTOP_MODULATE4X ? 4.0f : 2.0f;
              dst = m_module.opFMul(m_vec4Type, dst,
                m_module.constvec4f32(m, m, m, m));
            }
            break;

          // Fallthrough...
          case D3DTOP_ADDSIGNED2X:
          case D3DTOP_ADDSIGNED:
            arg[2] = m_module.opFSub(m_vec4Type, arg[2],
              m_module.constvec4f32(0.5f, 0.5f, 0.5f, 0.5f));
          case D3DTOP_ADD:
            dst = m_module.opFAdd(m_vec4Type, arg[1], arg[2]);
            if (op == D3DTOP_ADDSIGNED2X)
              dst = m_module.opFMul(m_vec4Type, dst, m_module.constvec4f32(2.0f, 2.0f, 2.0f, 2.0f));
            break;

          case D3DTOP_SUBTRACT:
            dst = m_module.opFSub(m_vec4Type, arg[1], arg[2]);
            break;

          case D3DTOP_ADDSMOOTH: {
            uint32_t comp = Complement(arg[1]);
            dst = m_module.opFFma(m_vec4Type, comp, arg[2], arg[1]);
            break;
          }

          case D3DTOP_BLENDDIFFUSEALPHA:
            dst = m_module.opFMix(m_vec4Type, arg[1], arg[2], AlphaReplicate(diffuse));
            break;

          case D3DTOP_BLENDTEXTUREALPHA:
            dst = m_module.opFMix(m_vec4Type, arg[1], arg[2], AlphaReplicate(GetTexture()));
            break;

          case D3DTOP_BLENDFACTORALPHA:
            dst = m_module.opFMix(m_vec4Type, arg[1], arg[2], AlphaReplicate(m_ps.constants.textureFactor));
            break;

          case D3DTOP_BLENDTEXTUREALPHAPM:
            Logger::warn("D3DTOP_BLENDTEXTUREALPHAPM: not implemented");
            break;

          case D3DTOP_BLENDCURRENTALPHA:
            dst = m_module.opFMix(m_vec4Type, arg[1], arg[2], AlphaReplicate(current));
            break;

          case D3DTOP_PREMODULATE:
            Logger::warn("D3DTOP_PREMODULATE: not implemented");
            break;

          case D3DTOP_MODULATEALPHA_ADDCOLOR:
            dst = m_module.opFFma(m_vec4Type, AlphaReplicate(arg[1]), arg[2], arg[1]);
            break;

          case D3DTOP_MODULATECOLOR_ADDALPHA:
            dst = m_module.opFFma(m_vec4Type, arg[1], arg[2], AlphaReplicate(arg[1]));
            break;

          case D3DTOP_MODULATEINVALPHA_ADDCOLOR:
            dst = m_module.opFFma(m_vec4Type, Complement(AlphaReplicate(arg[1])), arg[2], arg[1]);
            break;

          case D3DTOP_MODULATEINVCOLOR_ADDALPHA:
            dst = m_module.opFFma(m_vec4Type, Complement(arg[1]), arg[2], AlphaReplicate(arg[1]));
            break;

          case D3DTOP_BUMPENVMAP:
            Logger::warn("D3DTOP_BUMPENVMAP: not implemented");
            break;

          case D3DTOP_BUMPENVMAPLUMINANCE:
            Logger::warn("D3DTOP_BUMPENVMAPLUMINANCE: not implemented");
            break;

          case D3DTOP_DOTPRODUCT3:
            Logger::warn("D3DTOP_DOTPRODUCT3: not implemented");
            break;

          case D3DTOP_MULTIPLYADD:
            dst = m_module.opFFma(m_vec4Type, arg[1], arg[2], arg[0]);
            break;

          case D3DTOP_LERP:
            dst = m_module.opFMix(m_vec4Type, arg[2], arg[0], arg[1]);
            break;

          case D3DTOP_DISABLE:
            Logger::warn("D3DTOP_DISABLE: this should be handled already!");
            break;

          default:
            Logger::warn("Unhandled texture op!");
            break;
        }

        return dst;
      };

      uint32_t& dst = stage.ResultIsTemp ? temp : current;

      D3DTEXTUREOP colorOp = (D3DTEXTUREOP)stage.ColorOp;

      // This cancels all subsequent stages.
      if (colorOp == D3DTOP_DISABLE)
        break;

      std::array<uint32_t, TextureArgCount> colorArgs = {
          colorOp != D3DTOP_DISABLE ? GetArg(stage.ColorArg0) : 0,
          colorOp != D3DTOP_DISABLE ? GetArg(stage.ColorArg1) : 0,
          colorOp != D3DTOP_DISABLE ? GetArg(stage.ColorArg2) : 0};

      D3DTEXTUREOP alphaOp = (D3DTEXTUREOP)stage.AlphaOp;
      std::array<uint32_t, TextureArgCount> alphaArgs = {
          alphaOp != D3DTOP_DISABLE ? GetArg(stage.AlphaArg0) : 0,
          alphaOp != D3DTOP_DISABLE ? GetArg(stage.AlphaArg1) : 0,
          alphaOp != D3DTOP_DISABLE ? GetArg(stage.AlphaArg2) : 0 };

      // Fast path if alpha/color path is identical.
      if (colorOp == alphaOp && colorArgs == alphaArgs) {
        if (colorOp != D3DTOP_DISABLE)
          dst = DoOp(colorOp, dst, colorArgs);
      }
      else {
        std::array<uint32_t, 4> indices = { 0, 1, 2, 4 + 3 };

        uint32_t colorResult = dst;
        uint32_t alphaResult = dst;
        if (colorOp != D3DTOP_DISABLE)
          colorResult = DoOp(colorOp, dst, colorArgs);

        if (alphaOp != D3DTOP_DISABLE)
          alphaResult = DoOp(alphaOp, dst, alphaArgs);

        // src0.x, src0.y, src0.z src1.w
        if (colorResult != dst)
          dst = m_module.opVectorShuffle(m_vec4Type, colorResult, dst, indices.size(), indices.data());

        // src0.x, src0.y, src0.z src1.w
        // But we flip src0, src1 to be inverse of color.
        if (alphaResult != dst)
          dst = m_module.opVectorShuffle(m_vec4Type, dst, alphaResult, indices.size(), indices.data());
      }
    }

    m_module.opStore(m_ps.out.COLOR, current);

    alphaTestPS();
  }

  void D3D9FFShaderCompiler::setupPS() {
    // PS Caps
    m_module.enableCapability(spv::CapabilityDerivativeControl);

    m_module.setExecutionMode(m_entryPointId,
      spv::ExecutionModeOriginUpperLeft);

    for (uint32_t i = 0; i < caps::TextureStageCount; i++)
      m_ps.in.TEXCOORD[i] = declareIO(true, DxsoSemantic{ DxsoUsage::Texcoord, i });

    m_ps.in.COLOR[0] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 0 });
    m_ps.in.COLOR[1] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 1 });

    m_ps.out.COLOR   = declareIO(false, DxsoSemantic{ DxsoUsage::Color, 0 });

    // Constant Buffer for PS.
    std::array<uint32_t, PSConstMemberCount> members = {
      m_vec4Type // Texture Factor
    };

    const uint32_t structType =
      m_module.defStructType(members.size(), members.data());

    m_module.decorateBlock(structType);
    uint32_t offset = 0;

    for (uint32_t i = 0; i < PSConstMemberCount; i++) {
      m_module.memberDecorateOffset(structType, i, offset);
      offset += sizeof(Vector4);
    }

    m_module.setDebugName(structType, "D3D9FixedFunctionPS");
    m_module.setDebugMemberName(structType, 0, "textureFactor");

    m_ps.constantBuffer = m_module.newVar(
      m_module.defPointerType(structType, spv::StorageClassUniform),
      spv::StorageClassUniform);

    m_module.setDebugName(m_ps.constantBuffer, "consts");

    const uint32_t bindingId = computeResourceSlotId(
      DxsoProgramType::PixelShader, DxsoBindingType::ConstantBuffer,
      DxsoConstantBuffers::PSFixedFunction);

    m_module.decorateDescriptorSet(m_ps.constantBuffer, 0);
    m_module.decorateBinding(m_ps.constantBuffer, bindingId);

    DxvkResourceSlot resource;
    resource.slot   = bindingId;
    resource.type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    resource.view   = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    resource.access = VK_ACCESS_UNIFORM_READ_BIT;
    m_resourceSlots.push_back(resource);

    // Load constants
    auto LoadConstant = [&](uint32_t type, uint32_t idx) {
      uint32_t offset  = m_module.constu32(idx);
      uint32_t typePtr = m_module.defPointerType(type, spv::StorageClassUniform);

      return m_module.opLoad(type,
        m_module.opAccessChain(typePtr, m_ps.constantBuffer, 1, &offset));
    };

    m_ps.constants.textureFactor = LoadConstant(m_vec4Type, PSConstTextureFactor);

    // Samplers
    for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
      // Only 2D for now...
      auto& sampler = m_ps.samplers[i];

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

      // Declare a specialization constant which will
      // store whether or not the resource is bound.
      const uint32_t specConstId = m_module.specConstBool(true);
      m_module.decorateSpecId(specConstId, bindingId);
      m_module.setDebugName(specConstId,
        str::format(name, "_bound").c_str());

      sampler.bound = specConstId;

      // Store descriptor info for the shader interface
      DxvkResourceSlot resource;
      resource.slot = bindingId;
      resource.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      resource.view = VK_IMAGE_VIEW_TYPE_2D;
      resource.access = VK_ACCESS_SHADER_READ_BIT;
      m_resourceSlots.push_back(resource);
    }

  }

  void D3D9FFShaderCompiler::alphaTestPS() {
    // Alpha testing
    uint32_t boolType = m_module.defBoolType();
    uint32_t floatPtr = m_module.defPointerType(m_floatType, spv::StorageClassPushConstant);

    // Declare uniform buffer containing render states
    enum RenderStateMember : uint32_t {
      RsAlphaRef = 0,
    };

    std::array<uint32_t, 1> rsMembers = { {
      m_floatType,
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

    // Implement alpha test
    auto oC0 = m_ps.out.COLOR;
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
    uint32_t alphaId = m_module.opCompositeExtract(m_floatType,
      m_module.opLoad(m_vec4Type, oC0),
      1, &alphaComponentId);

    // Load alpha reference
    uint32_t alphaRefMember = m_module.constu32(RsAlphaRef);
    uint32_t alphaRefId = m_module.opLoad(m_floatType,
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

    std::hash<bool>                   bhash;
    std::hash<D3DMATERIALCOLORSOURCE> colorSourceHash;

    state.add(bhash(key.HasPositionT));
    state.add(bhash(key.HasColor0));
    state.add(bhash(key.HasColor1));
    state.add(bhash(key.UseLighting));

    state.add(colorSourceHash(key.DiffuseSource));
    state.add(colorSourceHash(key.AmbientSource));
    state.add(colorSourceHash(key.SpecularSource));
    state.add(colorSourceHash(key.EmissiveSource));

    return state;
  }


  size_t D3D9FFShaderKeyHash::operator () (const D3D9FFShaderKeyFS& key) const {
    DxvkHashState state;

    std::hash<uint64_t> uint64hash;

    for (uint32_t i = 0; i < caps::TextureStageCount; i++)
      state.add(uint64hash(key.Stages->uint64[i]));

    return state;
  }


  bool operator == (const D3D9FFShaderKeyVS& a, const D3D9FFShaderKeyVS& b) {
    return std::memcmp(&a, &b, sizeof(D3D9FFShaderKeyVS)) == 0;
  }


  bool operator == (const D3D9FFShaderKeyFS& a, const D3D9FFShaderKeyFS& b) {
    return std::memcmp(&a, &b, sizeof(D3D9FFShaderKeyFS)) == 0;
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