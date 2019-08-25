#include "d3d9_fixed_function.h"

#include "d3d9_device.h"
#include "d3d9_spec_constants.h"

#include "../dxvk/dxvk_hash.h"
#include "../dxvk/dxvk_spec_const.h"

#include "../spirv/spirv_module.h"

namespace dxvk {

  uint32_t DoFixedFunctionFog(SpirvModule& spvModule, const D3D9FogContext& fogCtx) {
    uint32_t boolType   = spvModule.defBoolType();
    uint32_t floatType  = spvModule.defFloatType(32);
    uint32_t uint32Type = spvModule.defIntType(32, 0);
    uint32_t vec3Type   = spvModule.defVectorType(floatType, 3);
    uint32_t vec4Type   = spvModule.defVectorType(floatType, 4);
    uint32_t floatPtr   = spvModule.defPointerType(floatType, spv::StorageClassPushConstant);
    uint32_t vec4Ptr    = spvModule.defPointerType(vec4Type,  spv::StorageClassPushConstant);

    uint32_t fogColorMember = spvModule.constu32(uint32_t(D3D9RenderStateItem::FogColor));
    uint32_t fogColor = spvModule.opLoad(vec4Type,
      spvModule.opAccessChain(vec4Ptr, fogCtx.RenderState, 1, &fogColorMember));

    uint32_t fogScaleMember = spvModule.constu32(uint32_t(D3D9RenderStateItem::FogScale));
    uint32_t fogScale = spvModule.opLoad(floatType,
      spvModule.opAccessChain(floatPtr, fogCtx.RenderState, 1, &fogScaleMember));

    uint32_t fogEndMember = spvModule.constu32(uint32_t(D3D9RenderStateItem::FogEnd));
    uint32_t fogEnd = spvModule.opLoad(floatType,
      spvModule.opAccessChain(floatPtr, fogCtx.RenderState, 1, &fogEndMember));

    uint32_t fogDensityMember = spvModule.constu32(uint32_t(D3D9RenderStateItem::FogDensity));
    uint32_t fogDensity = spvModule.opLoad(floatType,
      spvModule.opAccessChain(floatPtr, fogCtx.RenderState, 1, &fogDensityMember));

    uint32_t fogMode = spvModule.specConst32(uint32Type, 0);

    if (!fogCtx.IsPixel) {
      spvModule.setDebugName(fogMode, "vertex_fog_mode");
      spvModule.decorateSpecId(fogMode, getSpecId(D3D9SpecConstantId::VertexFogMode));
    }
    else {
      spvModule.setDebugName(fogMode, "pixel_fog_mode");
      spvModule.decorateSpecId(fogMode, getSpecId(D3D9SpecConstantId::PixelFogMode));
    }

    uint32_t fogEnabled = spvModule.specConstBool(false);
    spvModule.setDebugName(fogEnabled, "fog_enabled");
    spvModule.decorateSpecId(fogEnabled, getSpecId(D3D9SpecConstantId::FogEnabled));

    uint32_t doFog   = spvModule.allocateId();
    uint32_t skipFog = spvModule.allocateId();

    uint32_t returnType     = fogCtx.IsPixel ? vec4Type : floatType;
    uint32_t returnTypePtr  = spvModule.defPointerType(returnType, spv::StorageClassPrivate);
    uint32_t returnValuePtr = spvModule.newVar(returnTypePtr, spv::StorageClassPrivate);
    spvModule.opStore(returnValuePtr, fogCtx.IsPixel ? fogCtx.oColor : spvModule.constf32(0.0f));

    // Actually do the fog now we have all the vars in-place.

    spvModule.opSelectionMerge(skipFog, spv::SelectionControlMaskNone);
    spvModule.opBranchConditional(fogEnabled, doFog, skipFog);

    spvModule.opLabel(doFog);

    uint32_t wIndex = 3;
    uint32_t zIndex = 2;

    uint32_t w = spvModule.opCompositeExtract(floatType, fogCtx.vPos, 1, &wIndex);
    uint32_t z = spvModule.opCompositeExtract(floatType, fogCtx.vPos, 1, &zIndex);

    uint32_t depth = 0;
    if (fogCtx.IsPixel)
      depth = spvModule.opFMul(floatType, z, spvModule.opFDiv(floatType, spvModule.constf32(1.0f), w));
    else
      depth = spvModule.opFAbs(floatType, z);

    uint32_t applyFogFactor = spvModule.allocateId();

    std::array<SpirvPhiLabel, 4> fogVariables;

    std::array<SpirvSwitchCaseLabel, 4> fogCaseLabels = { {
      { uint32_t(D3DFOG_NONE),      spvModule.allocateId() },
      { uint32_t(D3DFOG_EXP),       spvModule.allocateId() },
      { uint32_t(D3DFOG_EXP2),      spvModule.allocateId() },
      { uint32_t(D3DFOG_LINEAR),    spvModule.allocateId() },
    } };


    spvModule.opSelectionMerge(applyFogFactor, spv::SelectionControlMaskNone);
    spvModule.opSwitch(fogMode,
      fogCaseLabels[D3DFOG_NONE].labelId,
      fogCaseLabels.size(),
      fogCaseLabels.data());

    for (uint32_t i = 0; i < fogCaseLabels.size(); i++) {
      spvModule.opLabel(fogCaseLabels[i].labelId);
        
      fogVariables[i].labelId = fogCaseLabels[i].labelId;
      fogVariables[i].varId   = [&] {
        auto mode = D3DFOGMODE(fogCaseLabels[i].literal);
        switch (mode) {
          default:
          // vFog
          case D3DFOG_NONE: {
            return fogCtx.vFog;
          }

          // (end - d) / (end - start)
          case D3DFOG_LINEAR: {
            uint32_t fogFactor = spvModule.opFSub(floatType, fogEnd, depth);
            fogFactor = spvModule.opFMul(floatType, fogFactor, fogScale);
            fogFactor = spvModule.opFClamp(floatType, fogFactor, spvModule.constf32(0.0f), spvModule.constf32(1.0f));
            return fogFactor;
          }

          // 1 / (e^[d * density])^2
          case D3DFOG_EXP2:
          // 1 / (e^[d * density])
          case D3DFOG_EXP: {
            uint32_t fogFactor = spvModule.opFMul(floatType, depth, fogDensity);

            if (mode == D3DFOG_EXP2)
              fogFactor = spvModule.opFMul(floatType, fogFactor, fogFactor);

            // Provides the rcp.
            fogFactor = spvModule.opFNegate(floatType, fogFactor);
            fogFactor = spvModule.opExp(floatType, fogFactor);
            return fogFactor;
          }
        }
      }();
        
      spvModule.opBranch(applyFogFactor);
    }

    spvModule.opLabel(applyFogFactor);

    uint32_t fogFactor = spvModule.opPhi(floatType,
      fogVariables.size(),
      fogVariables.data());

    uint32_t fogRetValue = 0;

    // Return the new color if we are doing this in PS
    // or just the fog factor for oFog in VS
    if (fogCtx.IsPixel) {
      std::array<uint32_t, 4> indices = { 0, 1, 2, 6 };

      uint32_t color = fogCtx.oColor;

      uint32_t color3 = spvModule.opVectorShuffle(vec3Type, color, color, 3, indices.data());
      uint32_t fogColor3 = spvModule.opVectorShuffle(vec3Type, fogColor, fogColor, 3, indices.data());

      std::array<uint32_t, 3> fogFacIndices = { fogFactor, fogFactor, fogFactor };
      uint32_t fogFact3 = spvModule.opCompositeConstruct(vec3Type, fogFacIndices.size(), fogFacIndices.data());

      uint32_t lerpedFrog = spvModule.opFMix(vec3Type, fogColor3, color3, fogFact3);

      fogRetValue = spvModule.opVectorShuffle(vec4Type, lerpedFrog, color, indices.size(), indices.data());
    }
    else
      fogRetValue = fogFactor;

    spvModule.opStore(returnValuePtr, fogRetValue);

    spvModule.opBranch(skipFog);

    spvModule.opLabel(skipFog);

    return spvModule.opLoad(returnType, returnValuePtr);
  }

    enum FFConstantMembersVS {
      VSConstWorldViewMatrix   = 0,
      VSConstNormalMatrix    = 1,
      VSConstProjMatrix,
      
      VsConstTexcoord0,
      VsConstTexcoord1,
      VsConstTexcoord2,
      VsConstTexcoord3,
      VsConstTexcoord4,
      VsConstTexcoord5,
      VsConstTexcoord6,
      VsConstTexcoord7,

      VSConstInverseOffset,
      VSConstInverseExtent,

      VSConstGlobalAmbient,

      VSConstLight0,
      VSConstLight1,
      VSConstLight2,
      VSConstLight3,
      VSConstLight4,
      VSConstLight5,
      VSConstLight6,
      VSConstLight7,

      VSConstMaterialDiffuse,
      VSConstMaterialAmbient,
      VSConstMaterialSpecular,
      VSConstMaterialEmissive,
      VSConstMaterialPower,

      VSConstMemberCount
    };

  struct D3D9FFVertexData {
    uint32_t constantBuffer = 0;
    uint32_t lightType      = 0;

    struct {
      uint32_t worldview = { 0 };
      uint32_t normal    = { 0 };
      uint32_t proj = { 0 };

      uint32_t texcoord[8] = { 0 };

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
      uint32_t NORMAL = { 0 };
      uint32_t TEXCOORD[8] = { 0 };
      uint32_t COLOR[2] = { 0 };
      uint32_t FOG = { 0 };
    } in;

    struct {
      uint32_t POSITION = { 0 };
      uint32_t NORMAL = { 0 };
      uint32_t TEXCOORD[8] = { 0 };
      uint32_t COLOR[2] = { 0 };
      uint32_t FOG = { 0 };
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
      uint32_t FOG         = { 0 };
      uint32_t POS         = { 0 };
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
      const D3D9FFShaderKeyVS& Key,
      const std::string&       Name);

    D3D9FFShaderCompiler(
            Rc<DxvkDevice>     Device,
      const D3D9FFShaderKeyFS& Key,
      const std::string&       Name);

    Rc<DxvkShader> compile();

    DxsoIsgn isgn() { return m_isgn; }

  private:

    // Returns value for inputs
    // Returns ptr for outputs
    uint32_t declareIO(bool input, DxsoSemantic semantic, spv::BuiltIn builtin = spv::BuiltInMax);

    void compileVS();

    void setupRenderStateInfo();

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
    uint32_t              m_uint32Type;
    uint32_t              m_vec4Type;
    uint32_t              m_vec3Type;
    uint32_t              m_mat3Type;
    uint32_t              m_mat4Type;

    uint32_t              m_entryPointId;

    uint32_t              m_rsBlock;
    uint32_t              m_mainFuncLabel;
  };

  D3D9FFShaderCompiler::D3D9FFShaderCompiler(
          Rc<DxvkDevice>     Device,
    const D3D9FFShaderKeyVS& Key,
    const std::string&       Name) {
    m_programType = DxsoProgramTypes::VertexShader;
    m_vsKey    = Key;
    m_filename = Name;
  }


  D3D9FFShaderCompiler::D3D9FFShaderCompiler(
          Rc<DxvkDevice>     Device,
    const D3D9FFShaderKeyFS& Key,
    const std::string& Name) {
    m_programType = DxsoProgramTypes::PixelShader;
    m_fsKey    = Key;
    m_filename = Name;
  }


  Rc<DxvkShader> D3D9FFShaderCompiler::compile() {
    m_floatType  = m_module.defFloatType(32);
    m_uint32Type = m_module.defIntType(32, 0);
    m_vec4Type   = m_module.defVectorType(m_floatType, 4);
    m_vec3Type   = m_module.defVectorType(m_floatType, 3);
    m_mat3Type   = m_module.defMatrixType(m_vec3Type, 3);
    m_mat4Type   = m_module.defMatrixType(m_vec4Type, 4);

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

    m_mainFuncLabel = m_module.allocateId();
    m_module.opLabel(m_mainFuncLabel);

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

    uint32_t type = semantic.usage == DxsoUsage::Fog ? m_floatType : m_vec4Type;

    uint32_t ptrType = m_module.defPointerType(type, storageClass);

    uint32_t ptr = m_module.newVar(ptrType, storageClass);

    if (builtin == spv::BuiltInMax)
      m_module.decorateLocation(ptr, slot);
    else
      m_module.decorateBuiltIn(ptr, builtin);

    std::string name = str::format(input ? "in_" : "out_", semantic.usage, semantic.usageIndex);
    m_module.setDebugName(ptr, name.c_str());

    m_entryPointInterfaces.push_back(ptr);

    if (input)
      return m_module.opLoad(type, ptr);

    return ptr;
  }


  void D3D9FFShaderCompiler::compileVS() {
    setupVS();

    std::array<uint32_t, 4> indices = { 0, 1, 2, 3 };

    uint32_t gl_Position = m_vs.in.POSITION;
    uint32_t vtx         = m_vs.in.POSITION;
    uint32_t normal      = m_module.opVectorShuffle(m_vec3Type, m_vs.in.NORMAL, m_vs.in.NORMAL, 3, indices.data());

    const uint32_t wIndex = 3;

    if (!m_vsKey.HasPositionT) {
      uint32_t wv = m_vs.constants.worldview;
      uint32_t nrmMtx = m_vs.constants.normal;

      std::array<uint32_t, 3> mtxIndices;
      for (uint32_t i = 0; i < 3; i++) {
        mtxIndices[i] = m_module.opCompositeExtract(m_vec4Type, nrmMtx, 1, &i);
        mtxIndices[i] = m_module.opVectorShuffle(m_vec3Type, mtxIndices[i], mtxIndices[i], 3, indices.data());
      }
      nrmMtx = m_module.opCompositeConstruct(m_mat3Type, mtxIndices.size(), mtxIndices.data());

      normal = m_module.opMatrixTimesVector(m_vec3Type, nrmMtx, normal);

      // Some games rely no normals not being normal.
      if (m_vsKey.NormalizeNormals)
        normal = m_module.opNormalize(m_vec3Type, normal);

      vtx         = m_module.opVectorTimesMatrix(m_vec4Type, vtx, wv);
      gl_Position = m_module.opVectorTimesMatrix(m_vec4Type, vtx, m_vs.constants.proj);
    } else {
      gl_Position = m_module.opFMul(m_vec4Type, gl_Position, m_vs.constants.invExtent);
      gl_Position = m_module.opFAdd(m_vec4Type, gl_Position, m_vs.constants.invOffset);

      // We still need to account for perspective correction here...

      // gl_Position.w    = 1.0f / gl_Position.w
      // gl_Position.xyz *= gl_Position.w;

      uint32_t w   = m_module.opCompositeExtract (m_floatType, gl_Position, 1, &wIndex);      // w = gl_Position.w
      uint32_t rhw = m_module.opFDiv             (m_floatType, m_module.constf32(1.0f), w);   // rhw = 1.0f / w
      gl_Position  = m_module.opVectorTimesScalar(m_vec4Type,  gl_Position, rhw);             // gl_Position.xyz *= rhw
      gl_Position  = m_module.opCompositeInsert  (m_vec4Type,  rhw, gl_Position, 1, &wIndex); // gl_Position.w = rhw
    }

    m_module.opStore(m_vs.out.POSITION, gl_Position);

    std::array<uint32_t, 4> outNrmIndices;
    for (uint32_t i = 0; i < 3; i++)
      outNrmIndices[i] = m_module.opCompositeExtract(m_floatType, normal, 1, &i);
    outNrmIndices[3] = m_module.constf32(1.0f);

    uint32_t outNrm = m_module.opCompositeConstruct(m_vec4Type, outNrmIndices.size(), outNrmIndices.data());

    m_module.opStore(m_vs.out.NORMAL, outNrm);

    for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
      uint32_t inputIndex  = m_vsKey.TexcoordIndices[i];

      uint32_t transformed;
      if (inputIndex & D3DTSS_TCI_CAMERASPACENORMAL) {
        const uint32_t wIndex = 3;
        transformed = outNrm;
      }
      else if (inputIndex & D3DTSS_TCI_CAMERASPACEPOSITION) {
        const uint32_t wIndex = 3;
        transformed = m_module.opCompositeInsert(m_vec4Type, m_module.constf32(1.0f), vtx, 1, &wIndex);
      } else {
        if (inputIndex > 8)
          Logger::warn(str::format("Unsupported texcoordindex flag (D3DTSS_TCI) ", inputIndex & ~0xFF, " for index ", inputIndex & 0xFF));

        transformed = m_vs.in.TEXCOORD[inputIndex & 0xFF];
      }

      uint32_t type = m_vsKey.TransformFlags[i];
      if (type != D3DTTFF_DISABLE) {
        // Project is already removed in the key.
        uint32_t count = type;

        if (!m_vsKey.HasPositionT) {
          uint32_t one  = m_module.constf32(1.0f);

          for (uint32_t i = count; i < 4; i++)
            transformed = m_module.opCompositeInsert(m_vec4Type, one, transformed, 1, &i);

          transformed = m_module.opVectorTimesMatrix(m_vec4Type, transformed, m_vs.constants.texcoord[i]);
        }

        // Pad the unused section of it with the value for projection.
        uint32_t lastIdx = count - 1;
        uint32_t projValue = m_module.opCompositeExtract(m_floatType, transformed, 1, &lastIdx);

        for (uint32_t j = count; j < 4; j++)
          transformed = m_module.opCompositeInsert(m_vec4Type, projValue, transformed, 1, &j);
      }

      m_module.opStore(m_vs.out.TEXCOORD[i], transformed);
    }

    if (m_vsKey.UseLighting) {
      auto PickSource = [&](D3DMATERIALCOLORSOURCE Source, uint32_t Material) {
        if (Source == D3DMCS_MATERIAL)
          return Material;
        else if (Source == D3DMCS_COLOR1)
          return m_vs.in.COLOR[0];
        else
          return m_vs.in.COLOR[1];
      };

      uint32_t diffuseValue  = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);
      uint32_t specularValue = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);
      uint32_t ambientValue  = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);

      for (uint32_t i = 0; i < m_vsKey.LightCount; i++) {
        uint32_t light_ptr_t = m_module.defPointerType(m_vs.lightType, spv::StorageClassUniform);

        uint32_t indexVal = m_module.constu32(VSConstLight0 + i);
        uint32_t lightPtr = m_module.opAccessChain(light_ptr_t, m_vs.constantBuffer, 1, &indexVal);

        auto LoadLightItem = [&](uint32_t type, uint32_t idx) {
          uint32_t typePtr = m_module.defPointerType(type, spv::StorageClassUniform);

          idx = m_module.constu32(idx);

          return m_module.opLoad(type,
            m_module.opAccessChain(typePtr, lightPtr, 1, &idx));
        };

        uint32_t diffuse   = LoadLightItem(m_vec4Type,   0);
        uint32_t specular  = LoadLightItem(m_vec4Type,   1);
        uint32_t ambient   = LoadLightItem(m_vec4Type,   2);
        uint32_t position  = LoadLightItem(m_vec4Type,   3);
        uint32_t direction = LoadLightItem(m_vec4Type,   4);
        uint32_t type      = LoadLightItem(m_uint32Type, 5);
        uint32_t range     = LoadLightItem(m_floatType,  6);
        uint32_t falloff   = LoadLightItem(m_floatType,  7);
        uint32_t atten0    = LoadLightItem(m_floatType,  8);
        uint32_t atten1    = LoadLightItem(m_floatType,  9);
        uint32_t atten2    = LoadLightItem(m_floatType,  10);
        uint32_t theta     = LoadLightItem(m_floatType, 11);
        uint32_t phi       = LoadLightItem(m_floatType, 12);

        uint32_t bool_t  = m_module.defBoolType();
        uint32_t bool3_t = m_module.defVectorType(bool_t, 3);

        uint32_t isPoint       = m_module.opIEqual(bool_t, type, m_module.constu32(D3DLIGHT_POINT));
        uint32_t isSpot        = m_module.opIEqual(bool_t, type, m_module.constu32(D3DLIGHT_SPOT));
        uint32_t isDirectional = m_module.opIEqual(bool_t, type, m_module.constu32(D3DLIGHT_DIRECTIONAL));

        std::array<uint32_t, 3> members = { isDirectional, isDirectional, isDirectional };

        uint32_t isDirectional3 = m_module.opCompositeConstruct(bool3_t, members.size(), members.data());

        uint32_t vtx3      = m_module.opVectorShuffle(m_vec3Type, vtx, vtx, 3, indices.data());
                 position  = m_module.opVectorShuffle(m_vec3Type, position, position, 3, indices.data());
                 direction = m_module.opVectorShuffle(m_vec3Type, direction, direction, 3, indices.data());

        uint32_t delta  = m_module.opFSub(m_vec3Type, position, vtx3);
        uint32_t d      = m_module.opLength(m_floatType, delta);
        uint32_t hitDir = m_module.opFNegate(m_vec3Type, direction);
                 hitDir = m_module.opSelect(m_vec3Type, isDirectional3, hitDir, delta);
                 hitDir = m_module.opNormalize(m_vec3Type, hitDir);

        uint32_t atten  = m_module.opFFma  (m_floatType, d, atten2, atten1);
                 atten  = m_module.opFFma  (m_floatType, d, atten,  atten0);
                 atten  = m_module.opFDiv  (m_floatType, m_module.constf32(1.0f), atten);

                 atten  = m_module.opSelect(m_floatType, m_module.opFOrdGreaterThan(bool_t, d, range), m_module.constf32(0.0f), atten);
                 atten  = m_module.opSelect(m_floatType, isDirectional, m_module.constf32(1.0f), atten);

        // Spot Lighting
        {
          uint32_t rho        = m_module.opDot (m_floatType, m_module.opFNegate(m_vec3Type, hitDir), direction);
          uint32_t spotAtten  = m_module.opFSub(m_floatType, rho, phi);
                   spotAtten  = m_module.opFDiv(m_floatType, spotAtten, m_module.opFSub(m_floatType, theta, phi));
                   spotAtten  = m_module.opPow (m_floatType, spotAtten, falloff);

          uint32_t insideThetaAndPhi = m_module.opFOrdGreaterThanEqual(bool_t, rho, theta);
          uint32_t insidePhi         = m_module.opFOrdGreaterThanEqual(bool_t, rho, phi);
                   spotAtten  = m_module.opSelect(m_floatType, insidePhi,         spotAtten, m_module.constf32(0.0f));
                   spotAtten  = m_module.opSelect(m_floatType, insideThetaAndPhi, spotAtten, m_module.constf32(1.0f));
                   spotAtten  = m_module.opFClamp(m_floatType, spotAtten, m_module.constf32(0.0f), m_module.constf32(1.0f));

                   spotAtten = m_module.opFMul(m_floatType, atten, spotAtten);
                   atten     = m_module.opSelect(m_floatType, isSpot, spotAtten, atten);
        }


        uint32_t hitDot = m_module.opDot(m_floatType, normal, hitDir);
                 hitDot = m_module.opFClamp(m_floatType, hitDot, m_module.constf32(0.0f), m_module.constf32(1.0f));

        uint32_t diffuseness = m_module.opFMul(m_floatType, hitDot, atten);

        uint32_t mid;
        if (m_vsKey.LocalViewer) {
          mid = m_module.opNormalize(m_vec3Type, vtx3);
          mid = m_module.opFSub(m_vec3Type, hitDir, mid);
        }
        else
          mid = m_module.opFSub(m_vec3Type, hitDir, m_module.constvec3f32(0.0f, 0.0f, 1.0f));

        mid = m_module.opNormalize(m_vec3Type, mid);

        uint32_t midDot = m_module.opDot(m_floatType, normal, mid);
                 midDot = m_module.opFClamp(m_floatType, midDot, m_module.constf32(0.0f), m_module.constf32(1.0f));
        uint32_t doSpec = m_module.opFOrdGreaterThan(bool_t, midDot, m_module.constf32(0.0f));
        uint32_t specularness = m_module.opPow(m_floatType, midDot, m_vs.constants.materialPower);
                 specularness = m_module.opFMul(m_floatType, specularness, atten);
                 specularness = m_module.opSelect(m_floatType, doSpec, specularness, m_module.constf32(0.0f));

        uint32_t lightAmbient  = m_module.opVectorTimesScalar(m_vec4Type, ambient,  atten);
        uint32_t lightDiffuse  = m_module.opVectorTimesScalar(m_vec4Type, diffuse,  diffuseness);
        uint32_t lightSpecular = m_module.opVectorTimesScalar(m_vec4Type, specular, specularness);

        ambientValue  = m_module.opFAdd(m_vec4Type, ambientValue,  lightAmbient);
        diffuseValue  = m_module.opFAdd(m_vec4Type, diffuseValue,  lightDiffuse);
        specularValue = m_module.opFAdd(m_vec4Type, specularValue, lightSpecular);
      }

      uint32_t mat_diffuse  = PickSource(m_vsKey.DiffuseSource,  m_vs.constants.materialDiffuse);
      uint32_t mat_ambient  = PickSource(m_vsKey.AmbientSource,  m_vs.constants.materialAmbient);
      uint32_t mat_emissive = PickSource(m_vsKey.EmissiveSource, m_vs.constants.materialEmissive);
      uint32_t mat_specular = PickSource(m_vsKey.SpecularSource, m_vs.constants.materialSpecular);
      
      std::array<uint32_t, 4> alphaSwizzle = {0, 1, 2, 7};
      uint32_t finalColor0 = m_module.opFFma(m_vec4Type, mat_ambient, m_vs.constants.globalAmbient, mat_emissive);
               finalColor0 = m_module.opFFma(m_vec4Type, mat_ambient, ambientValue, finalColor0);
               finalColor0 = m_module.opFFma(m_vec4Type, mat_diffuse, diffuseValue, finalColor0);
               finalColor0 = m_module.opVectorShuffle(m_vec4Type, finalColor0, mat_diffuse, alphaSwizzle.size(), alphaSwizzle.data());

      uint32_t finalColor1 = m_module.opFMul(m_vec4Type, mat_specular, specularValue);

      // Saturate
      finalColor0 = m_module.opFClamp(m_vec4Type, finalColor0,
        m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f),
        m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f));

      finalColor1 = m_module.opFClamp(m_vec4Type, finalColor1,
        m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f),
        m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f));

      m_module.opStore(m_vs.out.COLOR[0], finalColor0);
      m_module.opStore(m_vs.out.COLOR[1], finalColor1);
    }
    else {
      m_module.opStore(m_vs.out.COLOR[0], m_vs.in.COLOR[0]);
      m_module.opStore(m_vs.out.COLOR[1], m_vs.in.COLOR[1]);
    }

    D3D9FogContext fogCtx;
    fogCtx.IsPixel     = false;
    fogCtx.RenderState = m_rsBlock;
    fogCtx.vPos        = vtx;
    fogCtx.vFog        = m_vs.in.FOG;
    fogCtx.oColor      = 0;
    m_module.opStore(m_vs.out.FOG, DoFixedFunctionFog(m_module, fogCtx));
  }


  void D3D9FFShaderCompiler::setupRenderStateInfo() {
    // TODO: fix duplication of this

    std::array<uint32_t, 5> rsMembers = {{
      m_vec4Type,
      m_floatType,
      m_floatType,
      m_floatType,
      m_floatType
    }};
    
    uint32_t rsStruct = m_module.defStructTypeUnique(rsMembers.size(), rsMembers.data());
    m_rsBlock = m_module.newVar(
      m_module.defPointerType(rsStruct, spv::StorageClassPushConstant),
      spv::StorageClassPushConstant);
    
    m_module.setDebugName         (rsStruct, "render_state_t");
    m_module.decorate             (rsStruct, spv::DecorationBlock);
    m_module.setDebugMemberName   (rsStruct, 0, "fog_color");
    m_module.memberDecorateOffset (rsStruct, 0, offsetof(D3D9RenderStateInfo, fogColor));
    m_module.setDebugMemberName   (rsStruct, 1, "fog_scale");
    m_module.memberDecorateOffset (rsStruct, 1, offsetof(D3D9RenderStateInfo, fogScale));
    m_module.setDebugMemberName   (rsStruct, 2, "fog_end");
    m_module.memberDecorateOffset (rsStruct, 2, offsetof(D3D9RenderStateInfo, fogEnd));
    m_module.setDebugMemberName   (rsStruct, 3, "fog_density");
    m_module.memberDecorateOffset (rsStruct, 3, offsetof(D3D9RenderStateInfo, fogDensity));
    m_module.setDebugMemberName   (rsStruct, 4, "alpha_ref");
    m_module.memberDecorateOffset (rsStruct, 4, offsetof(D3D9RenderStateInfo, alphaRef));
    
    m_module.setDebugName         (m_rsBlock, "render_state");

    m_interfaceSlots.pushConstOffset = 0;
    m_interfaceSlots.pushConstSize = sizeof(D3D9RenderStateInfo);
  }


  void D3D9FFShaderCompiler::setupVS() {
    setupRenderStateInfo();

    // VS Caps
    m_module.enableCapability(spv::CapabilityClipDistance);
    m_module.enableCapability(spv::CapabilityDrawParameters);

    m_module.enableExtension("SPV_KHR_shader_draw_parameters");

    std::array<uint32_t, 13> light_members = {
      m_vec4Type,   // Diffuse
      m_vec4Type,   // Specular
      m_vec4Type,   // Ambient
      m_vec4Type,   // Position
      m_vec4Type,   // Direction
      m_uint32Type, // Type
      m_floatType,  // Range
      m_floatType,  // Falloff
      m_floatType,  // Attenuation0
      m_floatType,  // Attenuation1
      m_floatType,  // Attenuation2
      m_floatType,  // Theta
      m_floatType,  // Phi
    };

    m_vs.lightType =
      m_module.defStructType(light_members.size(), light_members.data());

    m_module.setDebugName(m_vs.lightType, "light_t");

    uint32_t offset = 0;
    m_module.memberDecorateOffset(m_vs.lightType, 0, offset);  offset += 4 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 0, "Diffuse");
    m_module.memberDecorateOffset(m_vs.lightType, 1, offset);  offset += 4 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 1, "Specular");
    m_module.memberDecorateOffset(m_vs.lightType, 2, offset);  offset += 4 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 2, "Ambient");

    m_module.memberDecorateOffset(m_vs.lightType, 3, offset);  offset += 4 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 3, "Position");
    m_module.memberDecorateOffset(m_vs.lightType, 4, offset);  offset += 4 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 4, "Direction");

    m_module.memberDecorateOffset(m_vs.lightType, 5, offset);  offset += 1 * sizeof(uint32_t);
    m_module.setDebugMemberName  (m_vs.lightType, 5, "Type");

    m_module.memberDecorateOffset(m_vs.lightType, 6, offset);  offset += 1 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 6, "Range");
    m_module.memberDecorateOffset(m_vs.lightType, 7, offset);  offset += 1 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 7, "Falloff");

    m_module.memberDecorateOffset(m_vs.lightType, 8, offset);  offset += 1 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 8, "Attenuation0");
    m_module.memberDecorateOffset(m_vs.lightType, 9, offset);  offset += 1 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 9, "Attenuation1");
    m_module.memberDecorateOffset(m_vs.lightType, 10, offset); offset += 1 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 10, "Attenuation2");

    m_module.memberDecorateOffset(m_vs.lightType, 11, offset); offset += 1 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 11, "Theta");
    m_module.memberDecorateOffset(m_vs.lightType, 12, offset); offset += 1 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 12, "Phi");

    // Constant Buffer for VS.
    std::array<uint32_t, VSConstMemberCount> members = {
      m_mat4Type, // World
      m_mat4Type, // View
      m_mat4Type, // Proj

      m_mat4Type, // Texture0
      m_mat4Type, // Texture1
      m_mat4Type, // Texture2
      m_mat4Type, // Texture3
      m_mat4Type, // Texture4
      m_mat4Type, // Texture5
      m_mat4Type, // Texture6
      m_mat4Type, // Texture7

      m_vec4Type, // Inverse Offset
      m_vec4Type, // Inverse Extent

      m_vec4Type, // Global Ambient

      m_vs.lightType, // Light0
      m_vs.lightType, // Light1
      m_vs.lightType, // Light2
      m_vs.lightType, // Light3
      m_vs.lightType, // Light4
      m_vs.lightType, // Light5
      m_vs.lightType, // Light6
      m_vs.lightType, // Light7

      m_vec4Type,  // Material Diffuse
      m_vec4Type,  // Material Ambient
      m_vec4Type,  // Material Specular
      m_vec4Type,  // Material Emissive
      m_floatType, // Material Power
    };

    const uint32_t structType =
      m_module.defStructType(members.size(), members.data());

    m_module.decorateBlock(structType);
    offset = 0;
    for (uint32_t i = 0; i < VSConstInverseOffset; i++) {
      m_module.memberDecorateOffset(structType, i, offset);
      offset += sizeof(Matrix4);
      m_module.memberDecorateMatrixStride(structType, i, 16);
      m_module.memberDecorate(structType, i, spv::DecorationRowMajor);
    }

    for (uint32_t i = VSConstInverseOffset; i < VSConstLight0; i++) {
      m_module.memberDecorateOffset(structType, i, offset);
      offset += sizeof(Vector4);
    }

    for (uint32_t i = 0; i < caps::MaxEnabledLights; i++) {
      m_module.memberDecorateOffset(structType, VSConstLight0 + i, offset);
      offset += sizeof(D3D9Light);
    }

    for (uint32_t i = VSConstMaterialDiffuse; i < VSConstMaterialPower; i++) {
      m_module.memberDecorateOffset(structType, i, offset);
      offset += sizeof(Vector4);
    }

    m_module.memberDecorateOffset(structType, VSConstMaterialPower, offset);
    offset += sizeof(float);

    m_module.setDebugName(structType, "D3D9FixedFunctionVS");
    uint32_t member = 0;
    m_module.setDebugMemberName(structType, member++, "WorldView");
    m_module.setDebugMemberName(structType, member++, "Normal");
    m_module.setDebugMemberName(structType, member++, "Projection");

    m_module.setDebugMemberName(structType, member++, "TexcoordTransform0");
    m_module.setDebugMemberName(structType, member++, "TexcoordTransform1");
    m_module.setDebugMemberName(structType, member++, "TexcoordTransform2");
    m_module.setDebugMemberName(structType, member++, "TexcoordTransform3");
    m_module.setDebugMemberName(structType, member++, "TexcoordTransform4");
    m_module.setDebugMemberName(structType, member++, "TexcoordTransform5");
    m_module.setDebugMemberName(structType, member++, "TexcoordTransform6");
    m_module.setDebugMemberName(structType, member++, "TexcoordTransform7");

    m_module.setDebugMemberName(structType, member++, "ViewportInfo_InverseOffset");
    m_module.setDebugMemberName(structType, member++, "ViewportInfo_InverseExtent");

    m_module.setDebugMemberName(structType, member++, "GlobalAmbient");

    m_module.setDebugMemberName(structType, member++, "Light0");
    m_module.setDebugMemberName(structType, member++, "Light1");
    m_module.setDebugMemberName(structType, member++, "Light2");
    m_module.setDebugMemberName(structType, member++, "Light3");
    m_module.setDebugMemberName(structType, member++, "Light4");
    m_module.setDebugMemberName(structType, member++, "Light5");
    m_module.setDebugMemberName(structType, member++, "Light6");
    m_module.setDebugMemberName(structType, member++, "Light7");

    m_module.setDebugMemberName(structType, member++, "Material_Diffuse");
    m_module.setDebugMemberName(structType, member++, "Material_Ambient");
    m_module.setDebugMemberName(structType, member++, "Material_Specular");
    m_module.setDebugMemberName(structType, member++, "Material_Emissive");
    m_module.setDebugMemberName(structType, member++, "Material_Power");

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

    m_vs.constants.worldview = LoadConstant(m_mat4Type, VSConstWorldViewMatrix);
    m_vs.constants.normal  = LoadConstant(m_mat4Type, VSConstNormalMatrix);
    m_vs.constants.proj  = LoadConstant(m_mat4Type, VSConstProjMatrix);

    for (uint32_t i = 0; i < caps::TextureStageCount; i++)
      m_vs.constants.texcoord[i] = LoadConstant(m_mat4Type, VsConstTexcoord0 + i);

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
    m_vs.in.NORMAL   = declareIO(true, DxsoSemantic{ DxsoUsage::Normal, 0 });
    for (uint32_t i = 0; i < caps::TextureStageCount; i++)
      m_vs.in.TEXCOORD[i] = declareIO(true, DxsoSemantic{ DxsoUsage::Texcoord, i });

    if (m_vsKey.HasColor0)
      m_vs.in.COLOR[0] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 0 });
    else {
      m_vs.in.COLOR[0] = m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f);
      m_isgn.elemCount++;
    }

    if (m_vsKey.HasColor1)
      m_vs.in.COLOR[1] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 1 });
    else {
      m_vs.in.COLOR[1] = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);
      m_isgn.elemCount++;
    }

    // Declare Outputs
    m_vs.out.POSITION = declareIO(false, DxsoSemantic{ DxsoUsage::Position, 0 }, spv::BuiltInPosition);

    m_vs.out.NORMAL   = declareIO(false, DxsoSemantic{ DxsoUsage::Normal, 0 });

    for (uint32_t i = 0; i < caps::TextureStageCount; i++)
      m_vs.out.TEXCOORD[i] = declareIO(false, DxsoSemantic{ DxsoUsage::Texcoord, i });

    m_vs.out.COLOR[0] = declareIO(false, DxsoSemantic{ DxsoUsage::Color, 0 });
    m_vs.out.COLOR[1] = declareIO(false, DxsoSemantic{ DxsoUsage::Color, 1 });

    m_vs.in.FOG       = declareIO(true,  DxsoSemantic{ DxsoUsage::Fog,   0 });
    m_vs.out.FOG      = declareIO(false, DxsoSemantic{ DxsoUsage::Fog,   0 });
  }


  void D3D9FFShaderCompiler::compilePS() {
    setupPS();

    uint32_t diffuse  = m_ps.in.COLOR[0];
    uint32_t specular = m_ps.in.COLOR[1];

    // Current starts of as equal to diffuse.
    uint32_t current = diffuse;
    // Temp starts off as equal to vec4(0)
    uint32_t temp  = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);
    
    uint32_t texture = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 1.0f);

    for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
      const auto& stage = m_fsKey.Stages[i].data;

      bool processedTexture = false;

      auto GetTexture = [&]() {
        if (!processedTexture) {
          SpirvImageOperands imageOperands;
          uint32_t imageVarId = m_module.opLoad(m_ps.samplers[i].typeId, m_ps.samplers[i].varId);

          if (m_fsKey.Stages[i].data.Projected)
            texture = m_module.opImageSampleProjImplicitLod(m_vec4Type, imageVarId, m_ps.in.TEXCOORD[i], imageOperands);
          else
            texture = m_module.opImageSampleImplicitLod(m_vec4Type, imageVarId, m_ps.in.TEXCOORD[i], imageOperands);
        }

        processedTexture = true;

        return texture;
      };

      auto ScalarReplicate = [&](uint32_t reg) {
        std::array<uint32_t, 4> replicant = { reg, reg, reg, reg };
        return m_module.opCompositeConstruct(m_vec4Type, replicant.size(), replicant.data());
      };

      auto AlphaReplicate = [&](uint32_t reg) {
        uint32_t alphaComponentId = 3;
        uint32_t alpha = m_module.opCompositeExtract(m_floatType, reg, 1, &alphaComponentId);

        return ScalarReplicate(alpha);
      };

      auto Complement = [&](uint32_t reg) {
        return m_module.opFSub(m_vec4Type,
          m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f),
          reg);
      };

      auto GetArg = [&] (uint32_t arg) {
        uint32_t reg = m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f);

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
            dst = m_module.opFMix(m_vec4Type, arg[2], arg[1], AlphaReplicate(diffuse));
            break;

          case D3DTOP_BLENDTEXTUREALPHA:
            dst = m_module.opFMix(m_vec4Type, arg[2], arg[1], AlphaReplicate(GetTexture()));
            break;

          case D3DTOP_BLENDFACTORALPHA:
            dst = m_module.opFMix(m_vec4Type, arg[2], arg[1], AlphaReplicate(m_ps.constants.textureFactor));
            break;

          case D3DTOP_BLENDTEXTUREALPHAPM:
            Logger::warn("D3DTOP_BLENDTEXTUREALPHAPM: not implemented");
            break;

          case D3DTOP_BLENDCURRENTALPHA:
            dst = m_module.opFMix(m_vec4Type, arg[2], arg[1], AlphaReplicate(current));
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

          case D3DTOP_DOTPRODUCT3: {
            // Get vec3 of arg1 & 2
            uint32_t vec3Type = m_module.defVectorType(m_floatType, 3);
            std::array<uint32_t, 3> indices = { 0, 1, 2 };
            arg[1] = m_module.opVectorShuffle(vec3Type, arg[1], arg[1], indices.size(), indices.data());
            arg[2] = m_module.opVectorShuffle(vec3Type, arg[2], arg[2], indices.size(), indices.data());

            // Bias according to spec.
            arg[1] = m_module.opFSub(vec3Type, arg[1], m_module.constvec3f32(-0.5f, -0.5f, -0.5f));
            arg[2] = m_module.opFSub(vec3Type, arg[2], m_module.constvec3f32(-0.5f, -0.5f, -0.5f));

            // Do the dotting!
            dst = ScalarReplicate(m_module.opDot(m_floatType, arg[1], arg[2]));

            // *= 4.0f
            dst = m_module.opFMul(m_vec4Type, dst, m_module.constvec4f32(4.0f, 4.0f, 4.0f, 4.0f));

            // Saturate
            dst = m_module.opFClamp(m_vec4Type, dst,
              m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f),
              m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f));

            break;
          }

          case D3DTOP_MULTIPLYADD:
            dst = m_module.opFFma(m_vec4Type, arg[1], arg[2], arg[0]);
            break;

          case D3DTOP_LERP:
            dst = m_module.opFMix(m_vec4Type, arg[2], arg[1], arg[0]);
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

    if (m_fsKey.SpecularEnable) {
      uint32_t specular = m_module.opFMul(m_vec4Type, m_ps.in.COLOR[1], m_module.constvec4f32(1.0f, 1.0f, 1.0f, 0.0f));

      current = m_module.opFAdd(m_vec4Type, current, specular);
    }

    D3D9FogContext fogCtx;
    fogCtx.IsPixel     = true;
    fogCtx.RenderState = m_rsBlock;
    fogCtx.vPos        = m_ps.in.POS;
    fogCtx.vFog        = m_ps.in.FOG;
    fogCtx.oColor      = current;
    current = DoFixedFunctionFog(m_module, fogCtx);

    m_module.opStore(m_ps.out.COLOR, current);

    alphaTestPS();
  }

  void D3D9FFShaderCompiler::setupPS() {
    setupRenderStateInfo();

    // PS Caps
    m_module.enableCapability(spv::CapabilityDerivativeControl);

    m_module.setExecutionMode(m_entryPointId,
      spv::ExecutionModeOriginUpperLeft);

    for (uint32_t i = 0; i < caps::TextureStageCount; i++)
      m_ps.in.TEXCOORD[i] = declareIO(true, DxsoSemantic{ DxsoUsage::Texcoord, i });

    m_ps.in.COLOR[0] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 0 });
    m_ps.in.COLOR[1] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 1 });

    m_ps.in.FOG      = declareIO(true, DxsoSemantic{ DxsoUsage::Fog, 0 });
    m_ps.in.POS      = declareIO(true, DxsoSemantic{ DxsoUsage::Position, 0 }, spv::BuiltInFragCoord);

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
      auto& sampler = m_ps.samplers[i];
      D3DRESOURCETYPE type = D3DRESOURCETYPE(m_fsKey.Stages[i].data.Type + D3DRTYPE_TEXTURE);

      spv::Dim dimensionality;
      VkImageViewType viewType;

      switch (type) {
        default:
        case D3DRTYPE_TEXTURE:
          dimensionality = spv::Dim2D;
          viewType       = VK_IMAGE_VIEW_TYPE_2D;
          break;
        case D3DRTYPE_CUBETEXTURE:
          dimensionality = spv::DimCube;
          viewType       = VK_IMAGE_VIEW_TYPE_CUBE;
          break;
        case D3DRTYPE_VOLUMETEXTURE:
          dimensionality = spv::Dim3D;
          viewType       = VK_IMAGE_VIEW_TYPE_3D;
          break;
      }

      sampler.typeId = m_module.defImageType(
        m_module.defFloatType(32),
        dimensionality, 0, 0, 0, 1,
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
      resource.slot   = bindingId;
      resource.type   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      resource.view   = viewType;
      resource.access = VK_ACCESS_SHADER_READ_BIT;
      m_resourceSlots.push_back(resource);
    }

  }

  void D3D9FFShaderCompiler::alphaTestPS() {
    // Alpha testing
    uint32_t boolType = m_module.defBoolType();
    uint32_t floatPtr = m_module.defPointerType(m_floatType, spv::StorageClassPushConstant);

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
    uint32_t alphaRefMember = m_module.constu32(uint32_t(D3D9RenderStateItem::AlphaRef));
    uint32_t alphaRefId = m_module.opLoad(m_floatType,
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
    Sha1Hash hash = Sha1Hash::compute(&Key, sizeof(Key));
    DxvkShaderKey shaderKey = { VK_SHADER_STAGE_VERTEX_BIT, hash };

    std::string name = str::format("FF_", shaderKey.toString());

    D3D9FFShaderCompiler compiler(
      pDevice->GetDXVKDevice(),
      Key, name);

    m_shader = compiler.compile();
    m_isgn   = compiler.isgn();

    Dump(Key, name);

    m_shader->setShaderKey(shaderKey);
    pDevice->GetDXVKDevice()->registerShader(m_shader);
  }


  D3D9FFShader::D3D9FFShader(
          D3D9DeviceEx*         pDevice,
    const D3D9FFShaderKeyFS&    Key) {
    Sha1Hash hash = Sha1Hash::compute(&Key, sizeof(Key));
    DxvkShaderKey shaderKey = { VK_SHADER_STAGE_FRAGMENT_BIT, hash };

    std::string name = str::format("FF_", shaderKey.toString());

    D3D9FFShaderCompiler compiler(
      pDevice->GetDXVKDevice(),
      Key, name);

    m_shader = compiler.compile();
    m_isgn   = compiler.isgn();

    Dump(Key, name);

    m_shader->setShaderKey(shaderKey);
    pDevice->GetDXVKDevice()->registerShader(m_shader);
  }

  template <typename T>
  void D3D9FFShader::Dump(const T& Key, const std::string& Name) {
    const std::string dumpPath = env::getEnvVar("DXVK_SHADER_DUMP_PATH");

    if (dumpPath.size() != 0) {
      D3D9FFShaderKeyHash hash;

      std::ofstream dumpStream(
        str::format(dumpPath, "/", Name, ".spv"),
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
    std::hash<uint8_t>                uint8hash;
    std::hash<uint32_t>               uint32hash;

    state.add(bhash(key.HasPositionT));
    state.add(bhash(key.HasColor0));
    state.add(bhash(key.HasColor1));
    state.add(bhash(key.UseLighting));
    state.add(bhash(key.NormalizeNormals));
    state.add(bhash(key.LocalViewer));

    state.add(colorSourceHash(key.DiffuseSource));
    state.add(colorSourceHash(key.AmbientSource));
    state.add(colorSourceHash(key.SpecularSource));
    state.add(colorSourceHash(key.EmissiveSource));

    for (auto index : key.TexcoordIndices)
      state.add(uint8hash(index));

    for (auto index : key.TransformFlags)
      state.add(uint32hash(index));

    state.add(uint32hash(key.LightCount));

    return state;
  }


  size_t D3D9FFShaderKeyHash::operator () (const D3D9FFShaderKeyFS& key) const {
    DxvkHashState state;

    std::hash<bool> boolhash;
    std::hash<uint64_t> uint64hash;

    state.add(boolhash(key.SpecularEnable));

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