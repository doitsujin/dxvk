#include <optional>
#include <utility>

#include <dxbc/dxbc_container.h>
#include <dxbc/dxbc_interface.h>
#include <dxbc/dxbc_parser.h>

#include "d3d11_device.h"
#include "d3d11_shader.h"

#include <cstring>

namespace dxvk {

  // Builds a geometry shader that does not exist in the application's
  // bytecode. It broadcasts one input primitive to NumViews viewports
  // using GS instancing (SetGsInstances plus gl_InvocationID), selects
  // the matching per-view position for each invocation, and routes the
  // primitive using the viewport mask the vertex shader supplies.
  // Triangle input only, which covers every NV multi-view vertex shader
  // observed so far; no point or line multi-view shaders have been seen.
  //
  // Deliberately does not touch dxbc-spirv. DxvkIrShaderConverter is
  // DXVK's own interface, so anything that fills in an ir::Builder is
  // acceptable, whether hand-built or converted from DXBC.
  class D3D11NvAmplificationGsConverter : public DxvkIrShaderConverter {

  public:

    D3D11NvAmplificationGsConverter(
      const DxvkShaderHash&                        VsKey,
      std::vector<DxvkNvPassthroughIoEntry>        PassthroughIo,
            uint32_t                                NumViews,
      const DxvkNvMultiviewInfo&                    NvMultiview)
    : m_key(VsKey), m_passthroughIo(std::move(PassthroughIo)),
      m_numViews(NumViews), m_nvMultiview(NvMultiview) { }

void convertShader(dxbc_spv::ir::Builder& builder) override {
      using namespace dxbc_spv;

      auto mainFunc = builder.add(ir::Op::Function(ir::ScalarType::eVoid));
      builder.add(ir::Op::FunctionEnd());
      builder.add(ir::Op::DebugName(mainFunc, "main"));

      auto entryPoint = builder.addAfter(ir::SsaDef(),
        ir::Op::EntryPoint(mainFunc, ir::ShaderStage::eGeometry));

      auto debugName = m_key.toString() + "_nvAmpGs";
      builder.add(ir::Op::DebugName(entryPoint, debugName.c_str()));
      builder.setCursor(mainFunc);

      builder.add(ir::Op::SetGsInstances(entryPoint, m_numViews));
      builder.add(ir::Op::SetGsInputPrimitive(entryPoint, ir::PrimitiveType::eTriangles));
      builder.add(ir::Op::SetGsOutputPrimitive(entryPoint, ir::PrimitiveType::eTriangles, 0x1u));
      builder.add(ir::Op::SetGsOutputVertices(entryPoint, 3u));

      auto instanceIdDecl = builder.add(ir::Op::DclInputBuiltIn(
        ir::ScalarType::eU32, entryPoint, ir::BuiltIn::eGsInstanceId, ir::InterpolationModes()));
      auto viewportDecl = builder.add(ir::Op::DclOutputBuiltIn(
        ir::Type(ir::ScalarType::eU32), entryPoint, ir::BuiltIn::eViewportIndex));

      auto instanceId = builder.add(ir::Op::InputLoad(
        ir::ScalarType::eU32, instanceIdDecl, ir::SsaDef()));

      // The position output, declared once, up front.
      auto positionOutDecl = builder.add(ir::Op::DclOutputBuiltIn(
        ir::Type(ir::BasicType(ir::ScalarType::eF32, 4u)), entryPoint, ir::BuiltIn::ePosition));

      // [view][vertex] -> that view's position for that vertex. Index 0
      // is view 0, which uses the ordinary SV_POSITION built-in; indices
      // 1 to 3 come from the NV_POSITION_VIEW_* registers below.
      std::array<std::array<ir::SsaDef, 3u>, 4u> positionPerViewPerVertex = { };

      // Per-vertex pass-through: 3 input vertices (triangle), every
      // captured entry carried through unchanged, at its own component
      // count (the resolver's getVectorType() - never a fixed float4).
      //
      // Location assignment: use the source register index and component
      // offset directly. A geometry shader's input interface has to match
      // the vertex shader output interface it consumes, and DXVK numbers
      // vertex shader outputs by register index. Two signature entries can
      // share a register with different write masks, in which case they
      // share a location and differ by component; renumbering sequentially
      // loses that pairing and slides every later location.

      // Collect every passthrough attribute's per-corner value AND its
      // output declaration here, instead of writing to the output the
      // moment it's loaded. We need all 3 corners' values in hand before
      // we can safely write+emit corner-by-corner further down.
      std::vector<ir::SsaDef> passthroughOutDecls;
      std::vector<std::array<ir::SsaDef, 3u>> passthroughValuesPerVertex;

      // View 0's position is SV_POSITION, which the vertex shader emits as
      // the Position built-in rather than at a generic location. It is
      // excluded from m_passthroughIo for that reason and read here.
      auto positionInDecl = builder.add(ir::Op::DclInputBuiltIn(
        ir::Type(ir::BasicType(ir::ScalarType::eF32, 4u)).addArrayDimension(3u),
        entryPoint, ir::BuiltIn::ePosition, ir::InterpolationModes()));

      for (uint32_t v = 0u; v < 3u; v++) {
        positionPerViewPerVertex[0][v] = builder.add(ir::Op::InputLoad(
          ir::Type(ir::BasicType(ir::ScalarType::eF32, 4u)),
          positionInDecl, builder.makeConstant(v)));
      }

      for (const auto& io : m_passthroughIo) {
        auto inputType = ir::Type(io.type).addArrayDimension(3u);

        auto inDecl = builder.add(ir::Op::DclInput(
          inputType, entryPoint, io.regIndex, io.component));
        auto outDecl = builder.add(ir::Op::DclOutput(
          ir::Type(io.type), entryPoint, io.regIndex, io.component));

        builder.add(ir::Op::Semantic(inDecl, io.semanticIndex, io.semanticName.c_str()));
        builder.add(ir::Op::Semantic(outDecl, io.semanticIndex, io.semanticName.c_str()));

        std::array<ir::SsaDef, 3u> values = { };
        for (uint32_t v = 0u; v < 3u; v++) {
          values[v] = builder.add(ir::Op::InputLoad(
            ir::Type(io.type), inDecl, builder.makeConstant(v)));
        }

        passthroughOutDecls.push_back(outDecl);
        passthroughValuesPerVertex.push_back(values);
      }

      // Per-view position registers. This only loads values into
      // positionPerViewPerVertex; it never writes to an output.
      static const std::array<const char*, 3> s_positionViewSemanticNames = {{
        "NV_POSITION_VIEW_1_SEMANTIC", "NV_POSITION_VIEW_2_SEMANTIC", "NV_POSITION_VIEW_3_SEMANTIC" }};

      for (uint32_t view = 0u; view < 3u; view++) {
        if (m_nvMultiview.positionViewReg[view] < 0)
          continue;

        auto viewPosType = m_nvMultiview.positionViewType[view];
        auto viewPosDecl = builder.add(ir::Op::DclInput(
          ir::Type(viewPosType).addArrayDimension(3u), entryPoint,
          uint32_t(m_nvMultiview.positionViewReg[view]), 0u));
        builder.add(ir::Op::Semantic(viewPosDecl, 0u, s_positionViewSemanticNames[view]));

        for (uint32_t v = 0u; v < 3u; v++) {
          positionPerViewPerVertex[view + 1][v] = builder.add(ir::Op::InputLoad(
            ir::Type(viewPosType), viewPosDecl, builder.makeConstant(v)));
        }
      }

      // Viewport routing. iRacing packs two 16-bit viewport bitmasks
      // per register - NV_VIEWPORT_MASK holds views 0 and 1, and
      // NV_VIEWPORT_MASK_2 holds views 2 and 3, low half first. Only the
      // .x component carries routing; .yzw are written with the same value
      // and their purpose is unknown, so they are ignored.
      static const std::array<const char*, 2> s_viewportMaskSemanticNames = {{
        "NV_VIEWPORT_MASK", "NV_VIEWPORT_MASK_2_SEMANTIC" }};

      std::array<ir::SsaDef, 2u> maskRegValues = { };

      for (uint32_t m = 0u; m < 2u; m++) {
        int32_t reg = m ? m_nvMultiview.viewportMask2Reg
                        : m_nvMultiview.viewportMaskReg;

        if (reg < 0)
          continue;

        auto maskType = m_nvMultiview.viewportMaskType[m];

        // A register with no recorded type would build an array-of-void
        // declaration. Skip rather than emit something malformed.
        if (maskType.isVoidType())
          continue;

        auto maskDecl = builder.add(ir::Op::DclInput(
          ir::Type(maskType).addArrayDimension(3u),
          entryPoint, uint32_t(reg), 0u));
        builder.add(ir::Op::Semantic(maskDecl, 0u, s_viewportMaskSemanticNames[m]));

        // Viewport routing is per-primitive, so the provoking vertex's
        // copy decides for the whole triangle.
        auto maskVector = builder.add(ir::Op::InputLoad(
          ir::Type(maskType), maskDecl, builder.makeConstant(0u)));

        maskRegValues[m] = builder.add(ir::Op::CompositeExtract(
          ir::ScalarType::eU32, maskVector, builder.makeConstant(0u)));
      }

      // This invocation's 16-bit half, chosen with the same IEq/Select
      // chain shape the positions use. Register = view >> 1, half = view & 1.
      ir::SsaDef viewportMaskHalf = { };

      for (uint32_t view = 0u; view < 4u; view++) {
        if (!maskRegValues[view >> 1])
          continue;

        auto half = builder.add(ir::Op::UBitExtract(
          ir::ScalarType::eU32, maskRegValues[view >> 1],
          builder.makeConstant((view & 1u) * 16u),
          builder.makeConstant(16u)));

        if (!viewportMaskHalf) {
          viewportMaskHalf = half;
          continue;
        }

        auto isThisView = builder.add(ir::Op::IEq(
          ir::ScalarType::eBool, instanceId, builder.makeConstant(view)));
        viewportMaskHalf = builder.add(ir::Op::Select(
          ir::Type(ir::ScalarType::eU32), isThisView, half, viewportMaskHalf));
      }

      // Cold path, once per synthesised shader. Records whether the
      // mask read was wired up for this shader; the mask values are
      // draw-time data and cannot be known here.
      Logger::info(str::format("NvMultiview: ", debugName,
        ": maskReg=o", m_nvMultiview.viewportMaskReg,
        " mask2Reg=o", m_nvMultiview.viewportMask2Reg,
        " routing=", viewportMaskHalf ? "mask" : "instanceId"));

      // Compute each corner's chosen position and stash it per-corner,
      // rather than writing to the output immediately. Same reasoning as
      // the passthrough loop above.
      std::array<ir::SsaDef, 3u> chosenPositionPerVertex = { };

      for (uint32_t v = 0u; v < 3u; v++) {
        ir::SsaDef chosen = positionPerViewPerVertex[0][v];

        for (uint32_t view = 1u; view < 4u; view++) {
          if (!positionPerViewPerVertex[view][v])
            continue;
          auto isThisView = builder.add(ir::Op::IEq(
            ir::ScalarType::eBool, instanceId, builder.makeConstant(view)));
          chosen = builder.add(ir::Op::Select(
            ir::Type(ir::BasicType(ir::ScalarType::eF32, 4u)),
            isThisView, positionPerViewPerVertex[view][v], chosen));
        }

        chosenPositionPerVertex[v] = chosen;
      }

      // Only the lowest set bit of the mask is honoured, because
      // ViewportIndex takes a single index. iRacing names exactly one
      // viewport per view, so nothing is lost here; landing one primitive
      // on several viewports at once needs ViewportMaskNV, which is M5.
      ir::SsaDef viewportIndex = instanceId;
      ir::SsaDef shouldEmit = builder.makeConstant(true);

      if (viewportMaskHalf) {
        viewportIndex = builder.add(ir::Op::IFindLsb(
          ir::Type(ir::ScalarType::eU32), viewportMaskHalf));
        shouldEmit = builder.add(ir::Op::INe(
          ir::Type(ir::ScalarType::eBool), viewportMaskHalf, builder.makeConstant(0u)));
      }

      // iRacing masks a view off entirely when the configuration doesn't
      // use it - view 3 at three screens. Skip it rather than emitting the
      // degenerate geometry its -1,-1,-1,-1 position sentinel would give.
      // The ViewportIndex store lives inside the guard because IFindLsb(0)
      // is -1, which is out of range for a viewport index.
      auto emitGuard = builder.add(ir::Op::ScopedIf(ir::SsaDef(), shouldEmit));

      builder.add(ir::Op::OutputStore(viewportDecl, ir::SsaDef(), viewportIndex));

      // One corner at a time - write every output for this corner, THEN
      // emit, THEN move to the next corner. This is the store -> emit
      // pattern a geometry shader requires.
      for (uint32_t v = 0u; v < 3u; v++) {
        for (size_t i = 0u; i < passthroughOutDecls.size(); i++) {
          builder.add(ir::Op::OutputStore(
            passthroughOutDecls[i], ir::SsaDef(), passthroughValuesPerVertex[i][v]));
        }

        builder.add(ir::Op::OutputStore(positionOutDecl, ir::SsaDef(), chosenPositionPerVertex[v]));
        builder.add(ir::Op::EmitVertex(0u));
      }

      auto emitGuardEnd = builder.add(ir::Op::ScopedEndIf(emitGuard));
      builder.rewriteOp(emitGuard,
        ir::Op(builder.getOp(emitGuard)).setOperand(0u, emitGuardEnd));
    }

    uint32_t determineResourceIndex(
            dxbc_spv::ir::ShaderStage stage,
            dxbc_spv::ir::ScalarType  type,
            uint32_t                  regSpace,
            uint32_t                  regIndex) const override {
      // This converter states no resource bindings at all (no CBV/
      // SRV/UAV/sampler - only IO pass-through and one builtin write),
      // so this should never actually be called. A harmless answer if
      // it ever is.
      return regIndex;
    }

    void dumpSource(const std::string& path) const override {
      // No DXBC source behind a hand-built converter - nothing to dump.
    }

    std::string getDebugName() const override {
      return m_key.toString() + "_nvAmpGs";
    }

  private:

    DxvkShaderHash                          m_key;
    std::vector<DxvkNvPassthroughIoEntry>   m_passthroughIo;
    uint32_t                                m_numViews;
    DxvkNvMultiviewInfo                     m_nvMultiview;

  };

  class D3D11ShaderConverter : public DxvkIrShaderConverter {

  public:

    D3D11ShaderConverter(
      const DxvkShaderHash&         ShaderKey,
      const DxvkIrShaderCreateInfo& ModuleInfo,
      const void*                   pShaderBytecode,
            size_t                  BytecodeLength,
            bool                    LowerIcb)
    : m_key(ShaderKey), m_info(ModuleInfo), m_lowerIcb(LowerIcb) {
      m_dxbc.resize(BytecodeLength);
      std::memcpy(m_dxbc.data(), pShaderBytecode, BytecodeLength);
    }

    ~D3D11ShaderConverter() { }

    void convertShader(
            dxbc_spv::ir::Builder&    builder) {
      auto debugName = m_key.toString();

      dxbc_spv::dxbc::Converter::Options options = { };
      options.name = debugName.c_str();
      options.includeDebugNames = true;
      options.boundCheckShaderIo = true;
      options.lowerIcb = m_lowerIcb;
      options.icbRegisterSpace = 0u;
      options.icbRegisterIndex = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
      options.classInstanceRegisterSpace = 0u;
      options.classInstanceRegisterIndex = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT + 1u;
      options.limitTessFactor = true;

      if (m_info.nvMultiview.enabled()) {
        Logger::info(str::format("NvMultiview: compiling ", debugName,
          " (posViews o", m_info.nvMultiview.positionViewReg[0],
          "/o", m_info.nvMultiview.positionViewReg[1],
          "/o", m_info.nvMultiview.positionViewReg[2],
          ", mask o", m_info.nvMultiview.viewportMaskReg,
          ", vpMask=", m_info.nvMultiview.useViewportMask, ")"));
      }

      dxbc_spv::dxbc::Container container(m_dxbc.data(), m_dxbc.size());

      dxbc_spv::dxbc::ShaderInfo shaderInfo =
        dxbc_spv::dxbc::Parser(container.getCodeChunk()).getShaderInfo();

      dxbc_spv::dxbc::Converter converter(std::move(container), options);

      // Determine whether to create a regular shader or a pass-through GS
      auto dstIsGs = m_key.stage() == VK_SHADER_STAGE_GEOMETRY_BIT;
      auto srcIsGs = shaderInfo.getType() == dxbc_spv::dxbc::ShaderType::eGeometry;

      if (dstIsGs && !srcIsGs) {
        if (!converter.createPassthroughGs(builder))
          throw DxvkError(str::format("Failed to create pass-through geometry shader: ", m_key.toString()));
      } else {
        if (!converter.convertShader(builder))
          throw DxvkError(str::format("Failed to convert shader: ", m_key.toString()));

        lowerBuiltIns(builder);
      }
    }

    uint32_t determineResourceIndex(
            dxbc_spv::ir::ShaderStage stage,
            dxbc_spv::ir::ScalarType  type,
            uint32_t                  regSpace,
            uint32_t                  regIndex) const {
      switch (type) {
        case dxbc_spv::ir::ScalarType::eSampler:
          return D3D11ShaderResourceMapping::computeSamplerBinding(stage, regIndex);
        case dxbc_spv::ir::ScalarType::eCbv:
          return D3D11ShaderResourceMapping::computeCbvBinding(stage, regIndex);
        case dxbc_spv::ir::ScalarType::eSrv:
          return D3D11ShaderResourceMapping::computeSrvBinding(stage, regIndex);
        case dxbc_spv::ir::ScalarType::eUav:
          return D3D11ShaderResourceMapping::computeUavBinding(stage, regIndex);
        case dxbc_spv::ir::ScalarType::eUavCounter:
          return D3D11ShaderResourceMapping::computeUavCounterBinding(stage, regIndex);
        default:
          return -1u;
      }
    }

    void dumpSource(const std::string& path) const {
      std::ofstream file(str::topath(str::format(path, "/", m_key.toString(), ".dxbc").c_str()).c_str(), std::ios_base::trunc | std::ios_base::binary);
      file.write(reinterpret_cast<const char*>(m_dxbc.data()), m_dxbc.size());
    }

    std::string getDebugName() const {
      return m_key.toString();
    }

  private:

    std::vector<uint8_t> m_dxbc;

    DxvkShaderHash          m_key;
    DxvkIrShaderCreateInfo  m_info;

    bool                    m_lowerIcb = false;

    struct BuiltInInfo {
      dxbc_spv::ir::BuiltIn builtIn;
      dxbc_spv::ir::BasicType type;
      const char* name;
    };

    static void lowerBuiltIns(dxbc_spv::ir::Builder& builder) {
      using namespace dxbc_spv;

      // Nothing to do for compute, our speshul built-ins are
      // only used in graphics pipelines.
      auto [entryPoint, shaderStage] = findEntryPoint(builder);

      ir::SsaDef pushData = {};
      ir::SsaDef specSampleCount = {};

      switch (shaderStage) {
        case ir::ShaderStage::eHull: {
          pushData = builder.add(ir::Op::DclPushData(
            ir::ScalarType::eF32, entryPoint, 0u, shaderStage));
          builder.add(ir::Op::DebugName(pushData, "maxTessFactor"));
        } break;

        case ir::ShaderStage::ePixel: {
          specSampleCount = builder.add(ir::Op::DclSpecConstant(
            ir::ScalarType::eU32, entryPoint, 0u, 0u));
          builder.add(ir::Op::DebugName(specSampleCount, "vRasterizer"));
        } break;

        default:
          return;
      }

      // Gather built-in inputs
      small_vector<ir::SsaDef, 32u> inputs;

      for (auto iter = builder.getDeclarations().first;
                iter != builder.getDeclarations().second; iter++) {
        if (iter->getOpCode() == ir::OpCode::eDclInputBuiltIn)
          inputs.push_back(iter->getDef());
      }

      // Rewrite input loads as push data loads
      for (auto inputDef : inputs) {
        const auto& inputOp = builder.getOp(inputDef);

        auto builtIn = ir::BuiltIn(inputOp.getOperand(inputOp.getFirstLiteralOperandIndex()));

        switch (builtIn) {
          case ir::BuiltIn::eSampleCount: {
            dxbc_spv_assert(specSampleCount);
            rewriteBuiltIn(builder, inputDef, specSampleCount);
          } break;

          case ir::BuiltIn::eTessFactorLimit: {
            dxbc_spv_assert(pushData);
            rewriteBuiltIn(builder, inputDef, pushData);
          } break;

          default:
            break;
        }
      }
    }

    static void rewriteBuiltIn(dxbc_spv::ir::Builder& builder, dxbc_spv::ir::SsaDef oldDef, dxbc_spv::ir::SsaDef newDef) {
      using namespace dxbc_spv;

      small_vector<ir::SsaDef, 32u> uses;
      builder.getUses(oldDef, uses);

      const auto& newOp = builder.getOp(newDef);

      for (auto use : uses) {
        const auto& useOp = builder.getOp(use);

        if (useOp.getOpCode() == ir::OpCode::eInputLoad) {
          if (newOp.getOpCode() == ir::OpCode::eDclPushData) {
            auto loadType = useOp.getType().getBaseType(0u);
            builder.rewriteOp(use, ir::Op::PushDataLoad(loadType, newDef, ir::SsaDef()));
          } else {
            builder.rewriteDef(use, newDef);
          }
        }
      }
    }

    static std::pair<dxbc_spv::ir::SsaDef, dxbc_spv::ir::ShaderStage> findEntryPoint(dxbc_spv::ir::Builder& builder) {
      using namespace dxbc_spv;

      auto [a, b] = builder.getDeclarations();

      for (auto iter = a; iter != b; iter++) {
        if (iter->getOpCode() == ir::OpCode::eEntryPoint)
          return std::make_pair(iter->getDef(), ir::ShaderStage(iter->getOperand(iter->getFirstLiteralOperandIndex())));
      }

      return {};
    }

  };


  
  D3D11CommonShader:: D3D11CommonShader() { }
  D3D11CommonShader::~D3D11CommonShader() { }
  
  
  D3D11CommonShader::D3D11CommonShader(
          D3D11Device*            pDevice,
          D3D11ClassLinkage*      pLinkage,
    const DxvkShaderHash&         ShaderKey,
    const DxvkIrShaderCreateInfo& ModuleInfo,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength,
    const D3D11ShaderIcbInfo&     Icb,
    const D3D11BindingMask&       BindingMask)
  : m_bindings(BindingMask) {
    m_shaderKey = ShaderKey;
    if (Logger::logLevel() <= LogLevel::Debug)
      Logger::debug(str::format("Compiling shader ", ShaderKey.toString()));

    if (pLinkage)
      GatherInterefaceInfo(pLinkage, pShaderBytecode, BytecodeLength);

    CreateIrShader(pDevice, ShaderKey, ModuleInfo, pShaderBytecode, BytecodeLength, Icb);
    pDevice->GetDXVKDevice()->registerShader(m_shader);
  }


  Rc<DxvkShader> D3D11CommonShader::GetOrCreateNvAmplificationGs(
          D3D11Device*            pDevice,
    const DxvkShaderHash&         VsKey,
          uint32_t                NumViews,
    const DxvkNvMultiviewInfo&    NvMultiview) const {
    std::lock_guard lock(*m_nvAmplificationMutex);

    // A cache entry is only trustworthy if it was actually built for
    // THIS NumViews/NvMultiview. Trusting "non-null" alone let a VS
    // that was first amplified under one live config silently keep
    // serving that config forever, even to later draws that need a
    // genuinely different one.
    bool cacheMatches = *m_nvAmplificationGs != nullptr
      && *m_nvAmplificationNumViews == NumViews
      && std::memcmp(m_nvAmplificationInfo.get(), &NvMultiview, sizeof(DxvkNvMultiviewInfo)) == 0;

    if (cacheMatches) {
      static std::atomic<int32_t> s_reuseLogBudget = { 8 };

      if (s_reuseLogBudget.fetch_sub(1, std::memory_order_relaxed) > 0) {
        Logger::info(str::format("NvAmplificationGs: reusing cached companion for ",
          VsKey.toString(), " (", m_nvPassthroughIo.size(), " passthrough entries)"));
      }
      return *m_nvAmplificationGs;
    }

    if (*m_nvAmplificationGs != nullptr) {
      static std::atomic<int32_t> s_staleLogBudget = { 8 };

      if (s_staleLogBudget.fetch_sub(1, std::memory_order_relaxed) > 0) {
        Logger::info(str::format("NvAmplificationGs: cached companion for ",
          VsKey.toString(), " no longer matches (was ", *m_nvAmplificationNumViews,
          " views, now ", NumViews, " views) - rebuilding"));
      }
    }

    Logger::info(str::format("NvAmplificationGs: building companion for ",
      VsKey.toString(), " (", m_nvPassthroughIo.size(), " passthrough entries, ",
      NumViews, " views)"));

    Rc<D3D11NvAmplificationGsConverter> converter =
      new D3D11NvAmplificationGsConverter(VsKey, m_nvPassthroughIo, NumViews, NvMultiview);

    DxvkIrShaderCreateInfo nvAmpGsInfo = { };
    nvAmpGsInfo.options.flags.set(DxvkShaderCompileFlag::SemanticIo);

    *m_nvAmplificationGs = pDevice->GetDXVKDevice()->createCachedShader(
      VsKey.toString() + "_nvAmpGs", nvAmpGsInfo, std::move(converter));

    *m_nvAmplificationNumViews = NumViews;
    *m_nvAmplificationInfo = NvMultiview;

    return *m_nvAmplificationGs;
  }


  void D3D11CommonShader::CreateIrShader(
          D3D11Device*            pDevice,
    const DxvkShaderHash&         ShaderKey,
    const DxvkIrShaderCreateInfo& ModuleInfo,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength,
    const D3D11ShaderIcbInfo&     Icb) {
    constexpr size_t MaxEmbeddedIcbSize = 64u;

    // Create icb if lowering is required
    size_t icbSizeInBytes = Icb.size * sizeof(*Icb.data);

    if (ModuleInfo.options.flags.test(DxvkShaderCompileFlag::LowerConstantArrays) && icbSizeInBytes > MaxEmbeddedIcbSize) {
      DxvkBufferCreateInfo info = { };
      info.size   = align(icbSizeInBytes, 256u);
      info.usage  = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                  | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                  | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      info.stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT
                  | util::pipelineStages(ShaderKey.stage());
      info.access = VK_ACCESS_UNIFORM_READ_BIT
                  | VK_ACCESS_TRANSFER_READ_BIT
                  | VK_ACCESS_TRANSFER_WRITE_BIT;
      info.debugName = "Icb";

      m_buffer = pDevice->GetDXVKDevice()->createBuffer(info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

      pDevice->InitShaderIcb(this, icbSizeInBytes, Icb.data);
    }

    // Create actual shader converter
    m_shader = pDevice->GetDXVKDevice()->createCachedShader(
      ShaderKey.toString(), ModuleInfo, nullptr);

    if (!m_shader) {
      Rc<D3D11ShaderConverter> converter = new D3D11ShaderConverter(ShaderKey,
        ModuleInfo, pShaderBytecode, BytecodeLength, bool(m_buffer));

      m_shader = pDevice->GetDXVKDevice()->createCachedShader(
        ShaderKey.toString(), ModuleInfo, std::move(converter));
    }
  }


  void D3D11CommonShader::GatherInterefaceInfo(
          D3D11ClassLinkage*      pLinkage,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength) {
    dxbc_spv::dxbc::Container container(pShaderBytecode, BytecodeLength);
    dxbc_spv::util::ByteReader ifaceChunk(container.getInterfaceChunk());

    if (!ifaceChunk)
      return;

    dxbc_spv::dxbc::InterfaceChunk ifaceInfo(ifaceChunk);

    if (!ifaceInfo)
      return;

    auto typeInfos = ifaceInfo.getClassTypes();
    auto slotInfos = ifaceInfo.getInterfaceSlots();

    for (auto i = typeInfos.first; i != typeInfos.second; i++) {
      m_interfaces.AddType(i->id, i->name.c_str());
      pLinkage->AddType(i->name.c_str(), i->cbSize, i->srvCount, i->samplerCount);
    }

    for (auto i = slotInfos.first; i != slotInfos.second; i++) {
      for (const auto& e : i->entries)
        m_interfaces.AddSlotInfo(i->index, i->count, e.typeId, e.tableId);
    }

    auto instances = ifaceInfo.getClassInstances();

    for (auto i = instances.first; i != instances.second; i++) {
      D3D11_CLASS_INSTANCE_DESC desc = { };
      desc.ConstantBuffer = i->cbvIndex;
      desc.BaseConstantBufferOffset = i->cbvOffset;
      desc.BaseTexture = i->srvIndex & 0x7fu;
      desc.BaseSampler = i->samplerIndex & 0xfu;

      auto typeName = m_interfaces.GetTypeName(i->typeId);

      if (typeName)
        pLinkage->AddInstance(&desc, typeName, i->name.c_str());
    }
  }

  
  D3D11ShaderModuleSet:: D3D11ShaderModuleSet() { }
  D3D11ShaderModuleSet::~D3D11ShaderModuleSet() { }
  
  
  HRESULT D3D11ShaderModuleSet::GetShaderModule(
          D3D11Device*            pDevice,
          D3D11ClassLinkage*      pLinkage,
    const DxvkShaderHash&         ShaderKey,
    const DxvkIrShaderCreateInfo& ModuleInfo,
    const void*                   pShaderBytecode,
          size_t                  BytecodeLength,
    const D3D11ShaderIcbInfo&     Icb,
    const D3D11BindingMask&       BindingMask,
          D3D11CommonShader*      pShader) {
    std::unique_lock<dxvk::mutex> lock(m_mutex);

    auto entry = m_modules.find(ShaderKey);
    if (entry != m_modules.end()) {
      *pShader = entry->second;
      return S_OK;
    }

    D3D11CommonShader module;

    try {
      module = D3D11CommonShader(pDevice, pLinkage, ShaderKey,
        ModuleInfo, pShaderBytecode, BytecodeLength, Icb, BindingMask);
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_INVALIDARG;
    }

    m_modules.insert({ ShaderKey, module });
    *pShader = std::move(module);
    return S_OK;
  }

}
