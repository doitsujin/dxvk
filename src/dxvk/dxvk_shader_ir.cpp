#include <ir/ir_serialize.h>

#include <spirv/spirv_builder.h>

#include <util/util_log.h>

#include "dxvk_shader_ir.h"

namespace dxvk {

  size_t DxvkIrShaderCreateInfo::hash() const {
    static_assert(std::is_trivially_copyable_v<DxvkShaderOptions>);

    DxvkHashState hash;
    hash.add(bit::fnv1a_hash(reinterpret_cast<const char*>(&options), sizeof(options)));
    hash.add(flatShadingInputs);
    hash.add(rasterizedStream);

    for (const auto& xfb : xfbEntries)
      hash.add(std::hash<dxbc_spv::ir::IoXfbInfo>()(xfb));

    return hash;
  }


  bool DxvkIrShaderCreateInfo::eq(const DxvkIrShaderCreateInfo& other) const {
    static_assert(std::is_trivially_copyable_v<DxvkShaderOptions>);

    if (std::memcmp(&options, &other.options, sizeof(options)))
      return false;

    if (flatShadingInputs != other.flatShadingInputs
     || rasterizedStream != other.rasterizedStream)
      return false;

    if (xfbEntries.size() != other.xfbEntries.size())
      return false;

    for (size_t i = 0u; i != xfbEntries.size(); i++) {
      if (xfbEntries[i] != other.xfbEntries[i])
        return false;
    }

    return true;
  }


  /**
   * \brief DXVK-specific logger for dxbc-spirv
   */
  class DxvkDxbcSpirvLogger : public dxbc_spv::util::Logger {

  public:

    DxvkDxbcSpirvLogger(std::string shaderName)
    : m_debugName(std::move(shaderName)) { }


    void message(dxbc_spv::util::LogLevel severity, const char* message) override {
      dxvk::Logger::log(convertLogLevel(severity), m_debugName + ": " + message);
    }

    dxbc_spv::util::LogLevel getMinimumSeverity() override {
      switch (dxvk::Logger::logLevel()) {
        case LogLevel::Debug:
          return dxbc_spv::util::LogLevel::eDebug;
        case LogLevel::Info:
          return dxbc_spv::util::LogLevel::eInfo;
        case LogLevel::Warn:
          return dxbc_spv::util::LogLevel::eWarn;
        default:
          return dxbc_spv::util::LogLevel::eError;
      }
    }

  private:

    std::string m_debugName;

    static LogLevel convertLogLevel(dxbc_spv::util::LogLevel severity) {
      switch (severity) {
        case dxbc_spv::util::LogLevel::eDebug:
          return LogLevel::Debug;
        case dxbc_spv::util::LogLevel::eInfo:
          return LogLevel::Info;
        case dxbc_spv::util::LogLevel::eWarn:
          return LogLevel::Warn;
        case dxbc_spv::util::LogLevel::eError:
          return LogLevel::Error;
      }

      return LogLevel::Info;
    }

  };


  /**
   * \brief DXVK-specific resource mapping for dxbc-spirv shaders
   *
   * Uses the pre-computed pipeline layout to map
   */
  class DxvkShaderResourceMapping : public dxbc_spv::spirv::ResourceMapping {

  public:

    explicit DxvkShaderResourceMapping(VkShaderStageFlagBits stage, const DxvkShaderBindingMap* bindings)
    : m_stage(stage), m_bindings(bindings) { }

    ~DxvkShaderResourceMapping() {

    }

    dxbc_spv::spirv::DescriptorBinding mapDescriptor(
          dxbc_spv::ir::ScalarType type,
          uint32_t                regSpace,
          uint32_t                regIndex) {
      DxvkShaderBinding binding(m_stage, setIndexForType(type), regIndex);

      if (m_bindings) {
        auto dstBinding = m_bindings->mapBinding(binding);

        if (dstBinding)
          binding = *dstBinding;
      }

      dxbc_spv::spirv::DescriptorBinding result = { };
      result.set = binding.getSet();
      result.binding = binding.getBinding();
      return result;
    }

    uint32_t mapPushData(dxbc_spv::ir::ShaderStageMask stages) {
      // Must be consistent with the lowering pass
      uint32_t offset = 0u;

      if (stages && stages == stages.first())
        offset = uint32_t(DxvkLimits::MaxSharedPushDataSize);

      if (m_bindings)
        offset = m_bindings->mapPushData(m_stage, offset);

      return offset;
    }

    static uint32_t setIndexForType(dxbc_spv::ir::ScalarType type) {
      switch (type) {
        case dxbc_spv::ir::ScalarType::eSampler: return 0u;
        case dxbc_spv::ir::ScalarType::eCbv: return 1u;
        case dxbc_spv::ir::ScalarType::eSrv: return 2u;
        case dxbc_spv::ir::ScalarType::eUav: return 3u;
        case dxbc_spv::ir::ScalarType::eUavCounter: return 4u;
        default: return -1u;
      }
    }

  private:

    VkShaderStageFlagBits       m_stage;
    const DxvkShaderBindingMap* m_bindings;

  };




  /**
   * \brief DXVK-specific pass to lower resource bindings
   *
   * Maps individual sampler bindings to the global sampler heap, promotes
   * UAV counters to BDA if available push data space allows it, and handles
   * built-ins that cannot be directly lowered to SPIR-V.
   *
   * Also generates pipeline layout information from lowered resources.
   */
  class DxvkIrLowerBindingModelPass {

  public:

    DxvkIrLowerBindingModelPass(
            dxbc_spv::ir::Builder&    builder,
      const DxvkIrShaderConverter&    shader,
      const DxvkIrShaderCreateInfo&   info)
    : m_builder (builder),
      m_shader  (shader),
      m_info    (info) {

    }

    /**
     * \brief Runs lowering pass
     */
    void run() {
      gatherAliasedResourceBindings();

      auto iter = m_builder.begin();

      while (iter != m_builder.getDeclarations().second) {
        switch (iter->getOpCode()) {
          case dxbc_spv::ir::OpCode::eEntryPoint: {
            iter = handleEntryPoint(iter);
          } break;

          case dxbc_spv::ir::OpCode::eDclSampler: {
            iter = handleSampler(iter);
          } break;

          case dxbc_spv::ir::OpCode::eDclUavCounter: {
            iter = handleUavCounter(iter);
          } break;

          case dxbc_spv::ir::OpCode::eDclUav: {
            iter = handleUav(iter);
          } break;

          case dxbc_spv::ir::OpCode::eDclSrv: {
            iter = handleSrv(iter);
          } break;

          case dxbc_spv::ir::OpCode::eDclCbv: {
            iter = handleCbv(iter);
          } break;

          case dxbc_spv::ir::OpCode::eDclXfb: {
            iter = handleXfb(iter);
          } break;

          case dxbc_spv::ir::OpCode::eDclInput: {
            iter = handleUserInput(iter);
          } break;

          case dxbc_spv::ir::OpCode::eDclInputBuiltIn: {
            iter = handleBuiltInInput(iter);
          } break;

          case dxbc_spv::ir::OpCode::eDclOutputBuiltIn: {
            iter = handleBuiltInOutput(iter);
          } break;

          case dxbc_spv::ir::OpCode::eDclPushData: {
            iter = handlePushData(iter);
          } break;

          case dxbc_spv::ir::OpCode::eDclSpecConstant: {
            iter = handleSpecConstant(iter);
          } break;

          default:
            ++iter;
        }
      }

      rewriteSamplers();
      rewriteUavCounters();

      if (m_sharedPushDataOffset) {
        auto stageMask = (m_metadata.stage & VK_SHADER_STAGE_ALL_GRAPHICS)
          ? VK_SHADER_STAGE_ALL_GRAPHICS : VK_SHADER_STAGE_COMPUTE_BIT;

        m_layout.addPushData(DxvkPushDataBlock(stageMask,
          0u, m_sharedPushDataOffset, sizeof(uint32_t), 0u));
      }

      if (m_localPushDataOffset) {
        m_layout.addPushData(DxvkPushDataBlock(m_metadata.stage, DxvkLimits::MaxSharedPushDataSize,
          m_localPushDataOffset, m_localPushDataAlign, m_localPushDataResourceMask));
      }

      m_metadata.inputs = convertIoMap(dxbc_spv::ir::IoMap::forInputs(m_builder));

      int32_t rasterizedStream = 0u;

      if (m_stage == dxbc_spv::ir::ShaderStage::eGeometry)
        rasterizedStream = m_info.rasterizedStream;

      m_metadata.outputs = convertIoMap(dxbc_spv::ir::IoMap::forOutputs(m_builder, uint32_t(rasterizedStream)));
    }


    /**
     * \brief Extracts layout info
     * \returns Binding layout
     */
    DxvkPipelineLayoutBuilder getLayout() {
      return std::move(m_layout);
    }

    /**
     * \brief Queries shader metadata
     * \returns Shader metadata
     */
    DxvkShaderMetadata getMetadata() const {
      return m_metadata;
    }

  private:

    struct SamplerInfo {
      dxbc_spv::ir::SsaDef sampler = { };
      uint16_t memberIndex = 0u;
      uint16_t wordIndex = 0u;
    };

    struct UavCounterInfo {
      dxbc_spv::ir::SsaDef dcl = { };
    };

    struct ResourceKey {
      dxbc_spv::ir::OpCode opCode = { };
      uint32_t registerSpace = 0u;
      uint32_t registerIndex = 0u;

      bool eq(const ResourceKey& other) const {
        return opCode        == other.opCode
            && registerSpace == other.registerSpace
            && registerIndex == other.registerIndex;
      }

      size_t hash() const {
        DxvkHashState hash;
        hash.add(uint32_t(opCode));
        hash.add(registerSpace);
        hash.add(registerIndex);
        return hash;
      }
    };

    struct ResourceAlias {
      bool hasAlias = false;
      bool hasBinding = false;
    };

    dxbc_spv::ir::Builder&        m_builder;
    const DxvkIrShaderConverter&  m_shader;
    DxvkIrShaderCreateInfo        m_info = { };

    DxvkShaderMetadata            m_metadata = { };
    DxvkPipelineLayoutBuilder     m_layout;

    dxbc_spv::ir::SsaDef          m_entryPoint = { };
    dxbc_spv::ir::ShaderStage     m_stage = { };

    dxbc_spv::ir::SsaDef          m_incUavCounterFunction = { };
    dxbc_spv::ir::SsaDef          m_decUavCounterFunction = { };

    uint32_t                      m_localPushDataAlign = 4u;
    uint32_t                      m_localPushDataOffset = 0u;
    uint32_t                      m_localPushDataResourceMask = 0u;

    uint32_t                      m_sharedPushDataOffset = 0u;

    small_vector<SamplerInfo,     16u>  m_samplers;
    small_vector<UavCounterInfo,  64u>  m_uavCounters;

    std::unordered_map<ResourceKey, ResourceAlias, DxvkHash, DxvkEq> m_resources;

    ResourceAlias& getResourceAlias(dxbc_spv::ir::OpCode opCode, uint32_t space, uint32_t index) {
      ResourceKey k = { };
      k.opCode = opCode;
      k.registerSpace = space;
      k.registerIndex = index;

      return m_resources.at(k);
    }

    void gatherAliasedResourceBindings() {
      auto iter = m_builder.begin();

      while (iter != m_builder.getDeclarations().second) {
        switch (iter->getOpCode()) {
          case dxbc_spv::ir::OpCode::eDclSrv:
          case dxbc_spv::ir::OpCode::eDclUav: {
            ResourceKey k = { };
            k.opCode = iter->getOpCode();
            k.registerSpace = uint32_t(iter->getOperand(1u));
            k.registerIndex = uint32_t(iter->getOperand(2u));

            auto e = m_resources.emplace(std::piecewise_construct, std::tuple(k), std::tuple());

            if (!e.second)
              e.first->second.hasAlias = true;
          } break;

          default:
            break;
        }

        iter++;
      }
    }


    dxbc_spv::ir::Builder::iterator handleEntryPoint(dxbc_spv::ir::Builder::iterator op) {
      m_entryPoint = op->getDef();
      m_stage = dxbc_spv::ir::ShaderStage(op->getOperand(op->getFirstLiteralOperandIndex()));
      m_metadata.stage = convertShaderStage(m_stage);
      m_layout = DxvkPipelineLayoutBuilder(m_metadata.stage);
      return ++op;
    }


    dxbc_spv::ir::Builder::iterator handleSampler(dxbc_spv::ir::Builder::iterator op) {
      // Emit global sampler heap later, we can't do much here yet
      auto& e = m_samplers.emplace_back();
      e.sampler = op->getDef();
      return ++op;
    }


    dxbc_spv::ir::Builder::iterator handleCbv(dxbc_spv::ir::Builder::iterator op) {
      auto regSpace = uint32_t(op->getOperand(1u));
      auto regIndex = uint32_t(op->getOperand(2u));

      DxvkBindingInfo binding = { };
      binding.set = DxvkShaderResourceMapping::setIndexForType(dxbc_spv::ir::ScalarType::eCbv);
      binding.binding = regIndex;
      binding.resourceIndex = m_shader.determineResourceIndex(m_stage,
        dxbc_spv::ir::ScalarType::eCbv, regSpace, regIndex);

      if (op->getType().byteSize() <= m_info.options.maxUniformBufferSize) {
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.access = VK_ACCESS_UNIFORM_READ_BIT;
      } else {
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.access = VK_ACCESS_SHADER_READ_BIT;
      }

      binding.flags.set(DxvkDescriptorFlag::UniformBuffer);

      addBinding(binding);
      return ++op;
    }


    dxbc_spv::ir::Builder::iterator handleSrv(dxbc_spv::ir::Builder::iterator op) {
      auto resourceKind = dxbc_spv::ir::ResourceKind(op->getOperand(4u));

      auto regSpace = uint32_t(op->getOperand(1u));
      auto regIndex = uint32_t(op->getOperand(2u));

      auto& resourceAlias = getResourceAlias(op->getOpCode(), regSpace, regIndex);

      DxvkBindingInfo binding = { };
      binding.set = DxvkShaderResourceMapping::setIndexForType(dxbc_spv::ir::ScalarType::eSrv);
      binding.binding = regIndex;
      binding.resourceIndex = m_shader.determineResourceIndex(m_stage,
        dxbc_spv::ir::ScalarType::eSrv, regSpace, regIndex);
      binding.access = VK_ACCESS_SHADER_READ_BIT;
      binding.viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;

      if (dxbc_spv::ir::resourceIsBuffer(resourceKind)) {
        if (dxbc_spv::ir::resourceIsTyped(resourceKind))
          binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        else
          binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      } else {
        if (!resourceAlias.hasAlias)
          binding.viewType = determineViewType(resourceKind);

        binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

        if (dxbc_spv::ir::resourceIsMultisampled(resourceKind))
          binding.flags.set(DxvkDescriptorFlag::Multisampled);
      }

      if (resourceHasSparseFeedbackLoads(op))
        m_metadata.flags.set(DxvkShaderFlag::UsesSparseResidency);

      if (!std::exchange(resourceAlias.hasBinding, true))
        addBinding(binding);

      return ++op;
    }


    dxbc_spv::ir::Builder::iterator handleUav(dxbc_spv::ir::Builder::iterator op) {
      auto regSpace = uint32_t(op->getOperand(1u));
      auto regIndex = uint32_t(op->getOperand(2u));

      auto& resourceAlias = getResourceAlias(op->getOpCode(), regSpace, regIndex);

      auto resourceKind = dxbc_spv::ir::ResourceKind(op->getOperand(4u));
      auto uavFlags = dxbc_spv::ir::UavFlags(op->getOperand(5u));

      DxvkBindingInfo binding = { };
      binding.set = DxvkShaderResourceMapping::setIndexForType(dxbc_spv::ir::ScalarType::eUav);
      binding.binding = regIndex;
      binding.resourceIndex = m_shader.determineResourceIndex(m_stage,
        dxbc_spv::ir::ScalarType::eUav, regSpace, regIndex);
      binding.viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;

      if (!(uavFlags & dxbc_spv::ir::UavFlag::eWriteOnly))
        binding.access |= VK_ACCESS_SHADER_READ_BIT;

      if (!(uavFlags & dxbc_spv::ir::UavFlag::eReadOnly)) {
        binding.access |= VK_ACCESS_SHADER_WRITE_BIT;
        binding.accessOp = determineAccessOpForUav(op);
      }

      if (dxbc_spv::ir::resourceIsBuffer(resourceKind)) {
        if (dxbc_spv::ir::resourceIsTyped(resourceKind))
          binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        else
          binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      } else {
        if (!resourceAlias.hasAlias)
          binding.viewType = determineViewType(resourceKind);

        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      }

      if (resourceHasSparseFeedbackLoads(op))
        m_metadata.flags.set(DxvkShaderFlag::UsesSparseResidency);

      if (!std::exchange(resourceAlias.hasBinding, true))
        addBinding(binding);

      return ++op;
    }


    dxbc_spv::ir::Builder::iterator handleUavCounter(dxbc_spv::ir::Builder::iterator op) {
      auto& e = m_uavCounters.emplace_back();
      e.dcl = op->getDef();
      return ++op;
    }


    dxbc_spv::ir::Builder::iterator handleXfb(dxbc_spv::ir::Builder::iterator op) {
      m_metadata.flags.set(DxvkShaderFlag::HasTransformFeedback);

      auto xfbBuffer = uint32_t(op->getOperand(1u));
      auto xfbStride = uint32_t(op->getOperand(2u));

      m_metadata.xfbStrides.at(xfbBuffer) = xfbStride;
      return ++op;
    }


    dxbc_spv::ir::Builder::iterator handleUserInput(dxbc_spv::ir::Builder::iterator op) {
      if (m_stage == dxbc_spv::ir::ShaderStage::ePixel)
        handleInputInterpolation(op);

      return ++op;
    }


    dxbc_spv::ir::Builder::iterator handleBuiltInInput(dxbc_spv::ir::Builder::iterator op) {
      if (m_stage == dxbc_spv::ir::ShaderStage::ePixel)
        handleInputInterpolation(op);

      auto builtIn = dxbc_spv::ir::BuiltIn(op->getOperand(op->getFirstLiteralOperandIndex()));

      if (builtIn == dxbc_spv::ir::BuiltIn::eSampleCount)
        return rewriteSampleCountBuiltIn(op);

      if (builtIn == dxbc_spv::ir::BuiltIn::eIsFullyCovered)
        m_metadata.flags.set(DxvkShaderFlag::UsesFragmentCoverage);

      return ++op;
    }


    dxbc_spv::ir::Builder::iterator handleBuiltInOutput(dxbc_spv::ir::Builder::iterator op) {
      auto builtIn = dxbc_spv::ir::BuiltIn(op->getOperand(op->getFirstLiteralOperandIndex()));

      switch (builtIn) {
        case dxbc_spv::ir::BuiltIn::ePosition: {
          m_metadata.flags.set(DxvkShaderFlag::ExportsPosition);
        } break;

        case dxbc_spv::ir::BuiltIn::eLayerIndex:
        case dxbc_spv::ir::BuiltIn::eViewportIndex: {
          if (m_stage != dxbc_spv::ir::ShaderStage::eGeometry)
            m_metadata.flags.set(DxvkShaderFlag::ExportsViewportIndexLayerFromVertexStage);
        } break;

        case dxbc_spv::ir::BuiltIn::eSampleMask: {
          m_metadata.flags.set(DxvkShaderFlag::ExportsSampleMask);
        } break;

        case dxbc_spv::ir::BuiltIn::eStencilRef: {
          m_metadata.flags.set(DxvkShaderFlag::ExportsStencilRef);
        } break;

        default:
          break;
      }

      return ++op;
    }


    dxbc_spv::ir::Builder::iterator handlePushData(dxbc_spv::ir::Builder::iterator op) {
      auto offset = uint32_t(op->getOperand(op->getFirstLiteralOperandIndex() + 0u));
      auto stages = dxbc_spv::ir::ShaderStageMask(op->getOperand(op->getFirstLiteralOperandIndex() + 1u));

      // Adjust local offset if this is a local declaration
      if (stages == m_stage)
        m_localPushDataOffset = std::max(m_localPushDataOffset, offset + op->getType().byteSize());
      else
        m_sharedPushDataOffset = std::max(m_sharedPushDataOffset, offset + op->getType().byteSize());

      return ++op;
    }


    dxbc_spv::ir::Builder::iterator handleSpecConstant(dxbc_spv::ir::Builder::iterator op) {
      auto specId = uint32_t(op->getOperand(op->getFirstLiteralOperandIndex()));
      m_metadata.specConstantMask |= 1u << specId;
      return ++op;
    }


    void handleInputInterpolation(dxbc_spv::ir::Builder::iterator op) {
      auto interpolation = dxbc_spv::ir::InterpolationModes(op->getOperand(op->getOperandCount() - 1u));

      if (interpolation & dxbc_spv::ir::InterpolationMode::eSample)
        m_metadata.flags.set(DxvkShaderFlag::HasSampleRateShading);
    }


    void addDebugMemberName(dxbc_spv::ir::SsaDef def, uint32_t member, const std::string& name) {
      if (!name.empty()) {
        if (m_builder.getOp(def).getType().isStructType())
          m_builder.add(dxbc_spv::ir::Op::DebugMemberName(def, member, name.c_str()));
        else
          m_builder.add(dxbc_spv::ir::Op::DebugName(def, name.c_str()));
      }
    }


    dxbc_spv::ir::SsaDef declareSamplerHeap() {
      // Declare sampler heap with unknown size since it may vary by device
      uint32_t set = DxvkShaderResourceMapping::setIndexForType(dxbc_spv::ir::ScalarType::eSampler);

      m_layout.addSamplerHeap(DxvkShaderBinding(m_metadata.stage, set, 0u));
      auto var = m_builder.add(dxbc_spv::ir::Op::DclSampler(m_entryPoint, 0u, 0u, 0u));

      m_builder.add(dxbc_spv::ir::Op::DebugName(var, "sampler_heap"));
      return var;
    }


    dxbc_spv::ir::SsaDef declareSamplerPushData() {
      dxbc_spv::ir::Type pushDataType = { };

      // Align to dword boundary, we need it for push data processing
      m_localPushDataOffset = align(m_localPushDataOffset, sizeof(uint32_t));

      // Compute index offsets for each sampler
      uint32_t wordCount = m_samplers.size();

      for (size_t i = 0u; i < m_samplers.size(); i++) {
        auto& e = m_samplers[i];

        if (m_info.options.flags.test(DxvkShaderCompileFlag::Supports16BitPushData)) {
          e.memberIndex = i;
          e.wordIndex = 0u;
        } else {
          e.memberIndex = i / 2u;
          e.wordIndex = i % 2u;
        }
      }

      // Mark corresponding dwords as resources
      uint32_t dwordIndex = m_localPushDataOffset / sizeof(uint32_t);
      uint32_t dwordCount = (wordCount + 1u) / 2u;

      m_localPushDataResourceMask |= ((1ull << dwordCount) - 1ull) << dwordIndex;

      if (m_info.options.flags.test(DxvkShaderCompileFlag::Supports16BitPushData)) {
        // Add each word separately and pad with a dummy entry if unaligned
        for (uint32_t i = 0u; i < wordCount; i++)
          pushDataType.addStructMember(dxbc_spv::ir::ScalarType::eU16);

        if (wordCount & 1u)
          pushDataType.addStructMember(dxbc_spv::ir::ScalarType::eU16);
      } else {
        // Add dword member for each pair fo samplers
        for (uint32_t i = 0u; i < dwordCount; i++)
          pushDataType.addStructMember(dxbc_spv::ir::ScalarType::eU32);
      }

      // Declare actual push data structure
      auto def = m_builder.add(dxbc_spv::ir::Op::DclPushData(
        pushDataType, m_entryPoint, m_localPushDataOffset, m_stage));

      m_localPushDataOffset += pushDataType.byteSize();

      // Add debug names for sampler indices
      if (m_info.options.flags.test(DxvkShaderCompileFlag::Supports16BitPushData)) {
        for (size_t i = 0u; i < m_samplers.size(); i++) {
          auto& e = m_samplers[i];
          addDebugMemberName(def, e.memberIndex, getDebugName(e.sampler));
        }
      }

      return def;
    }


    dxbc_spv::ir::Builder::iterator rewriteSampleCountBuiltIn(dxbc_spv::ir::Builder::iterator op) {
      small_vector<dxbc_spv::ir::SsaDef, 64u> uses;
      m_builder.getUses(op->getDef(), uses);

      m_builder.rewriteOp(op->getDef(), dxbc_spv::ir::Op::DclPushData(op->getType(),
        m_entryPoint, m_info.options.sampleCountPushDataOffset, dxbc_spv::ir::ShaderStageMask()));

      for (auto use : uses) {
        const auto& useOp = m_builder.getOp(use);

        if (useOp.getOpCode() == dxbc_spv::ir::OpCode::eInputLoad) {
          m_builder.rewriteOp(useOp.getDef(), dxbc_spv::ir::Op::PushDataLoad(
            useOp.getType(), op->getDef(), dxbc_spv::ir::SsaDef()));
        }
      }

      m_sharedPushDataOffset = std::max<uint32_t>(m_sharedPushDataOffset,
        m_info.options.sampleCountPushDataOffset + sizeof(uint32_t));
      return ++op;
    }


    dxbc_spv::ir::Builder::iterator rewriteSampler(dxbc_spv::ir::Builder::iterator sampler, dxbc_spv::ir::SsaDef heapDef, dxbc_spv::ir::SsaDef pushDataDef) {
      small_vector<dxbc_spv::ir::SsaDef, 64u> uses;
      m_builder.getUses(sampler->getDef(), uses);

      // Find sampler entry
      SamplerInfo info = { };

      for (size_t i = 0u; i < m_samplers.size(); i++) {
        if (m_samplers[i].sampler == sampler->getDef()) {
          info = m_samplers[i];
          break;
        }
      }

      // Rewrite descriptor load to fetch the index from the push data block,
      // and the sampler descriptor itself from the sampler heap
      for (size_t i = 0u; i < uses.size(); i++) {
        const auto& op = m_builder.getOp(uses[i]);

        if (op.getOpCode() == dxbc_spv::ir::OpCode::eDescriptorLoad) {
          dxbc_spv::ir::SsaDef samplerIndex = { };
          dxbc_spv::ir::SsaDef memberIndex = { };

          if (m_builder.getOp(pushDataDef).getType().isStructType())
            memberIndex = m_builder.makeConstant(uint32_t(info.memberIndex));

          if (m_info.options.flags.test(DxvkShaderCompileFlag::Supports16BitPushData)) {
            samplerIndex = m_builder.addBefore(op.getDef(), dxbc_spv::ir::Op::PushDataLoad(
              dxbc_spv::ir::ScalarType::eU16, pushDataDef, memberIndex));
            samplerIndex = m_builder.addBefore(op.getDef(), dxbc_spv::ir::Op::ConvertItoI(
              dxbc_spv::ir::ScalarType::eU32, samplerIndex));
          } else {
            samplerIndex = m_builder.addBefore(op.getDef(), dxbc_spv::ir::Op::PushDataLoad(
              dxbc_spv::ir::ScalarType::eU32, pushDataDef, memberIndex));
            samplerIndex = m_builder.addBefore(op.getDef(), dxbc_spv::ir::Op::UBitExtract(
              dxbc_spv::ir::ScalarType::eU32, samplerIndex, m_builder.makeConstant(uint32_t(16u * info.wordIndex)), m_builder.makeConstant(16u)));
          }

          m_builder.rewriteOp(op.getDef(), dxbc_spv::ir::Op::DescriptorLoad(
            op.getType(), heapDef, samplerIndex));
        } else if (op.isDeclarative()) {
          m_builder.removeOp(op);
        }
      }

      // Infer push data offset from member index and word index
      uint32_t localPushDataOffset = m_localPushDataOffset + 2u * info.wordIndex
        + m_builder.getOp(pushDataDef).getType().byteOffset(info.memberIndex)
        - m_builder.getOp(pushDataDef).getType().byteSize();

      // Add sampler info to the descriptor layout
      auto regSpace = uint32_t(sampler->getOperand(1u));
      auto regIndex = uint32_t(sampler->getOperand(2u));

      DxvkBindingInfo binding = { };
      binding.resourceIndex = m_shader.determineResourceIndex(m_stage,
        dxbc_spv::ir::ScalarType::eSampler, regSpace, regIndex);
      binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
      binding.blockOffset = MaxSharedPushDataSize + localPushDataOffset;
      binding.flags.set(DxvkDescriptorFlag::PushData);

      addBinding(binding);

      return m_builder.iter(m_builder.remove(sampler->getDef()));
    }


    void sortSamplers() {
      // Sort samplers by binding index for consistency
      std::sort(&m_samplers[0u], &m_samplers[0u] + m_samplers.size(), [&] (const SamplerInfo& a, const SamplerInfo& b) {
        const auto& aOp = m_builder.getOp(a.sampler);
        const auto& bOp = m_builder.getOp(b.sampler);

        return uint32_t(aOp.getOperand(2u)) < uint32_t(bOp.getOperand(2u));
      });
    }


    void rewriteSamplers() {
      if (m_samplers.empty())
        return;

      sortSamplers();

      auto samplerIndices = declareSamplerPushData();
      auto samplerHeap = declareSamplerHeap();

      auto iter = m_builder.begin();

      while (iter != m_builder.getDeclarations().second) {
        if (iter->getOpCode() == dxbc_spv::ir::OpCode::eDclSampler && iter->getDef() != samplerHeap)
          iter = rewriteSampler(iter, samplerHeap, samplerIndices);
        else
          ++iter;
      }
    }


    void sortUavCounters() {
      // Sort samplers by the corresponding UAV binding index for consistency
      std::sort(&m_uavCounters[0u], &m_uavCounters[0u] + m_uavCounters.size(), [&] (const UavCounterInfo& a, const UavCounterInfo& b) {
        const auto& aUav = m_builder.getOpForOperand(a.dcl, 1u);
        const auto& bUav = m_builder.getOpForOperand(b.dcl, 1u);

        return uint32_t(aUav.getOperand(2u)) < uint32_t(bUav.getOperand(2u));
      });
    }


    dxbc_spv::ir::SsaDef getUavCounterFunction(dxbc_spv::ir::AtomicOp atomicOp) {
      auto& def = atomicOp == dxbc_spv::ir::AtomicOp::eInc
        ? m_incUavCounterFunction
        : m_decUavCounterFunction;

      if (def)
        return def;

      auto mainFunc = m_builder.getOpForOperand(m_entryPoint, 0u).getDef();

      // Declare counter address parameter and function
      auto param = m_builder.add(dxbc_spv::ir::Op::DclParam(dxbc_spv::ir::ScalarType::eU64));
      m_builder.add(dxbc_spv::ir::Op::DebugName(param, "va"));

      def = m_builder.addBefore(mainFunc, dxbc_spv::ir::Op::Function(dxbc_spv::ir::ScalarType::eU32).addParam(param));
      m_builder.add(dxbc_spv::ir::Op::DebugName(def, atomicOp == dxbc_spv::ir::AtomicOp::eInc ? "uav_ctr_inc" : "uav_ctr_dec"));

      // Insert labels
      auto execBlock = m_builder.addBefore(mainFunc, dxbc_spv::ir::Op::Label());
      auto mergeBlock = m_builder.addBefore(mainFunc, dxbc_spv::ir::Op::Label());
      auto entryBlock = m_builder.addAfter(def, dxbc_spv::ir::Op::LabelSelection(mergeBlock));

      // Insert check whether the counter address is null
      auto address = m_builder.addBefore(execBlock, dxbc_spv::ir::Op::ParamLoad(dxbc_spv::ir::ScalarType::eU64, def, param));
      auto execCond = m_builder.addBefore(execBlock, dxbc_spv::ir::Op::INe(dxbc_spv::ir::ScalarType::eBool, address, m_builder.makeConstant(uint64_t(0u))));
      m_builder.addBefore(execBlock, dxbc_spv::ir::Op::BranchConditional(execCond, execBlock, mergeBlock));

      // Insert actual atomic op
      auto pointer = m_builder.addBefore(mergeBlock, dxbc_spv::ir::Op::Pointer(dxbc_spv::ir::ScalarType::eU32, address, dxbc_spv::ir::UavFlags()));
      auto value = m_builder.addBefore(mergeBlock, dxbc_spv::ir::Op::MemoryAtomic(atomicOp,
        dxbc_spv::ir::ScalarType::eU32, pointer, dxbc_spv::ir::SsaDef(), dxbc_spv::ir::SsaDef()));

      if (atomicOp == dxbc_spv::ir::AtomicOp::eDec) {
        value = m_builder.addBefore(mergeBlock, dxbc_spv::ir::Op::ISub(
          dxbc_spv::ir::ScalarType::eU32, value, m_builder.makeConstant(1u)));
      }

      m_builder.addBefore(mergeBlock, dxbc_spv::ir::Op::Branch(mergeBlock));

      // Insert phi and function return
      value = m_builder.addBefore(mainFunc, dxbc_spv::ir::Op::Phi(dxbc_spv::ir::ScalarType::eU32)
        .addPhi(execBlock, value)
        .addPhi(entryBlock, m_builder.makeConstant(0u)));

      m_builder.addBefore(mainFunc, dxbc_spv::ir::Op::Return(dxbc_spv::ir::ScalarType::eU32, value));
      m_builder.addBefore(mainFunc, dxbc_spv::ir::Op::FunctionEnd());
      return def;
    }


    void rewriteUavCounterUsesAsBda(dxbc_spv::ir::SsaDef descriptor, dxbc_spv::ir::SsaDef pushData, uint32_t pushMember) {
      small_vector<dxbc_spv::ir::SsaDef, 64u> uses;
      m_builder.getUses(descriptor, uses);

      // Rewrite descriptor load to load the raw pointer from push data
      dxbc_spv::ir::SsaDef memberIndex = { };

      if (m_builder.getOp(pushData).getType().isStructType())
        memberIndex = m_builder.makeConstant(pushMember);

      m_builder.rewriteOp(descriptor, dxbc_spv::ir::Op::PushDataLoad(
        dxbc_spv::ir::ScalarType::eU64, pushData, memberIndex));

      // Rewrite counter atomics as raw memory atomics. Counter decrement semantics differ
      // from regular decrement, so take that into account and subtract 1 from the result.
      for (auto use : uses) {
        const auto& useOp = m_builder.getOp(use);

        if (useOp.getOpCode() == dxbc_spv::ir::OpCode::eCounterAtomic) {
          auto atomicOp = dxbc_spv::ir::AtomicOp(useOp.getOperand(1u));
          auto func = getUavCounterFunction(atomicOp);

          m_builder.rewriteOp(use, dxbc_spv::ir::Op::FunctionCall(
            dxbc_spv::ir::ScalarType::eU32, func).addParam(descriptor));
        }
      }
    }


    void rewriteUavCounterAsBda(dxbc_spv::ir::SsaDef uavCounter, dxbc_spv::ir::SsaDef pushData, uint32_t pushMember) {
      small_vector<dxbc_spv::ir::SsaDef, 64u> uses;
      m_builder.getUses(uavCounter, uses);

      for (auto use : uses) {
        if (m_builder.getOp(use).getOpCode() == dxbc_spv::ir::OpCode::eDescriptorLoad)
          rewriteUavCounterUsesAsBda(use, pushData, pushMember);
        else
          m_builder.remove(use);
      }

      m_builder.remove(uavCounter);
    }


    void rewriteUavCounters() {
      if (m_uavCounters.empty())
        return;

      sortUavCounters();

      // In compute shaders, we can freely use push data space
      auto ssboAlignment = m_info.options.minStorageBufferAlignment;

      size_t maxPushDataSize = m_stage == dxbc_spv::ir::ShaderStage::eCompute
        ? MaxTotalPushDataSize - MaxReservedPushDataSize
        : MaxPerStagePushDataSize;

      size_t uavCounterIndex = 0u;

      if (m_localPushDataOffset + sizeof(uint64_t) <= maxPushDataSize && ssboAlignment <= 4u) {
        // Align push data to a multiple of 8 bytes before emitting counters
        m_localPushDataAlign = std::max<uint32_t>(m_localPushDataAlign, sizeof(uint64_t));
        m_localPushDataOffset = align(m_localPushDataOffset, m_localPushDataAlign);

        // Declare push data variable and type
        dxbc_spv::ir::Type pushDataType = { };

        size_t maxUavCounters = std::min<size_t>(m_uavCounters.size(),
          (maxPushDataSize - m_localPushDataOffset) / sizeof(uint64_t));

        for (uint32_t i = 0u; i < maxUavCounters; i++)
          pushDataType.addStructMember(dxbc_spv::ir::ScalarType::eU64);

        auto pushDataVar = m_builder.add(dxbc_spv::ir::Op::DclPushData(
          pushDataType, m_entryPoint, m_localPushDataOffset, m_stage));

        while (uavCounterIndex < m_uavCounters.size() && m_localPushDataOffset + sizeof(uint64_t) <= maxPushDataSize) {
          const auto& uavCounter = m_uavCounters[uavCounterIndex];
          const auto& uavOp = m_builder.getOpForOperand(uavCounter.dcl, 1u);

          auto regSpace = uint32_t(uavOp.getOperand(1u));
          auto regIndex = uint32_t(uavOp.getOperand(2u));

          DxvkBindingInfo binding = { };
          binding.resourceIndex = m_shader.determineResourceIndex(m_stage,
            dxbc_spv::ir::ScalarType::eUavCounter, regSpace, regIndex);
          binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
          binding.access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
          binding.blockOffset = MaxSharedPushDataSize + m_localPushDataOffset;
          binding.flags.set(DxvkDescriptorFlag::PushData);

          addBinding(binding);

          m_localPushDataResourceMask |= 3ull << (m_localPushDataOffset / sizeof(uint32_t));
          m_localPushDataOffset += sizeof(uint64_t);

          addDebugMemberName(pushDataVar, uavCounterIndex, getDebugName(uavCounter.dcl));

          rewriteUavCounterAsBda(uavCounter.dcl, pushDataVar, uavCounterIndex++);
        }
      }

      // Emit remaining UAV counters as regular descriptors
      while (uavCounterIndex < m_uavCounters.size()) {
        const auto& uavCounter = m_uavCounters[uavCounterIndex++];
        const auto& uavOp = m_builder.getOpForOperand(uavCounter.dcl, 1u);

        auto regSpace = uint32_t(uavOp.getOperand(1u));
        auto regIndex = uint32_t(uavOp.getOperand(2u));

        DxvkBindingInfo binding = { };
        binding.set = DxvkShaderResourceMapping::setIndexForType(dxbc_spv::ir::ScalarType::eUavCounter);
        binding.binding = regIndex;
        binding.resourceIndex = m_shader.determineResourceIndex(m_stage,
          dxbc_spv::ir::ScalarType::eUavCounter, regSpace, regIndex);
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        addBinding(binding);
      }
    }


    void addBinding(const DxvkBindingInfo& binding) {
      DxvkShaderDescriptor descriptor(binding, m_metadata.stage);
      m_layout.addBindings(1u, &descriptor);
    }


    DxvkAccessOp determineAccessOpForStore(const dxbc_spv::ir::Op& op) const {
      if (!op.isConstant() || !op.getType().isBasicType())
        return DxvkAccessOp::None;

      // If the constant is a vector, all scalars must be the same since we can
      // only encode one scalar value, and if values written to the same location
      // differ then the execution order matters.
      auto type = op.getType().getBaseType(0u);

      if (byteSize(type.getBaseType()) > 4u)
        return DxvkAccessOp::None;

      uint32_t value = uint32_t(op.getOperand(0u));

      for (uint32_t i = 1u; i < type.getVectorSize(); i++) {
        if (uint32_t(op.getOperand(i)) != value)
          return DxvkAccessOp::None;
      }

      constexpr uint32_t IMaxValue = 1u << DxvkAccessOp::StoreValueBits;
      constexpr uint32_t FBitShift = 32u - DxvkAccessOp::StoreValueBits;
      constexpr uint32_t FBitMask = (1u << FBitShift) - 1u;

      if (value < IMaxValue) {
        // Trivial case, represent as unsigned int
        return DxvkAccessOp(DxvkAccessOp::StoreUi, value);
      } else if (~value < IMaxValue) {
        // 'Signed' integer, use one's complement instead of the
        // usual two's here to gain an extra value we can encode
        return DxvkAccessOp(DxvkAccessOp::StoreSi, ~value);
      } else if (!(value & FBitMask)) {
        // Potential float bit pattern, need to ignore mantissa
        return DxvkAccessOp(DxvkAccessOp::StoreF, value >>FBitShift);
      }

      return DxvkAccessOp::None;
    }


    std::optional<DxvkAccessOp> determineAccessOpForAccess(const dxbc_spv::ir::Op& op) const {
      switch (op.getOpCode()) {
        case dxbc_spv::ir::OpCode::eBufferLoad:
        case dxbc_spv::ir::OpCode::eImageLoad:
          return DxvkAccessOp::Load;

        case dxbc_spv::ir::OpCode::eBufferStore:
        case dxbc_spv::ir::OpCode::eImageStore: {
          return determineAccessOpForStore(m_builder.getOpForOperand(op,
            op.getFirstLiteralOperandIndex() - 1u));
        }

        case dxbc_spv::ir::OpCode::eBufferAtomic:
        case dxbc_spv::ir::OpCode::eImageAtomic: {
          // Order matters if the result is used
          if (!op.getType().isVoidType())
            return DxvkAccessOp::None;

          auto atomicOp = dxbc_spv::ir::AtomicOp(op.getOperand(op.getFirstLiteralOperandIndex()));

          switch (atomicOp) {
            case dxbc_spv::ir::AtomicOp::eInc:
            case dxbc_spv::ir::AtomicOp::eDec:
            case dxbc_spv::ir::AtomicOp::eAdd:
            case dxbc_spv::ir::AtomicOp::eSub:
              return DxvkAccessOp::Add;

            case dxbc_spv::ir::AtomicOp::eOr:
              return DxvkAccessOp::Or;

            case dxbc_spv::ir::AtomicOp::eAnd:
              return DxvkAccessOp::And;

            case dxbc_spv::ir::AtomicOp::eXor:
              return DxvkAccessOp::Xor;

            case dxbc_spv::ir::AtomicOp::eSMin:
              return DxvkAccessOp::IMin;

            case dxbc_spv::ir::AtomicOp::eSMax:
              return DxvkAccessOp::IMax;

            case dxbc_spv::ir::AtomicOp::eUMin:
              return DxvkAccessOp::UMin;

            case dxbc_spv::ir::AtomicOp::eUMax:
              return DxvkAccessOp::UMax;

            case dxbc_spv::ir::AtomicOp::eLoad:
              return DxvkAccessOp::Load;

            case dxbc_spv::ir::AtomicOp::eStore: {
              return determineAccessOpForStore(m_builder.getOpForOperand(op,
                op.getFirstLiteralOperandIndex() - 1u));
            }

            default:
              return DxvkAccessOp::None;
          }
        }

        default:
          // Resource queries etc don't access resource memory,
          // so they must not affect the result
          return std::nullopt;
      }
    }


    DxvkAccessOp determineAccessOpForUav(dxbc_spv::ir::Builder::iterator op) const {
      std::optional<DxvkAccessOp> accessOp;

      auto [a, b] = m_builder.getUses(op->getDef());

      for (auto iter = a; iter != b; iter++) {
        if (iter->getOpCode() == dxbc_spv::ir::OpCode::eDescriptorLoad) {
          auto [aDesc, bDesc] = m_builder.getUses(iter->getDef());

          for (auto use = aDesc; use != bDesc; use++) {
            auto access = determineAccessOpForAccess(*use);

            if (!access)
              continue;

            if (access == DxvkAccessOp::None) {
              // Can't optimize the access
              return DxvkAccessOp::None;
            }

            if (!accessOp) {
              // First order-invariant access
              accessOp = access;
            } else if (accessOp != access) {
              // Different access type, can't merge
              return DxvkAccessOp::None;
            }
          }
        }
      }

      if (accessOp)
        return *accessOp;

      return DxvkAccessOp::None;
    }


    bool descriptorHasSparseFeedbackLoads(const dxbc_spv::ir::Op& op) const {
      auto [a, b] = m_builder.getUses(op.getDef());

      for (auto iter = a; iter != b; iter++) {
        if (iter->getFlags() & dxbc_spv::ir::OpFlag::eSparseFeedback)
          return true;
      }

      return false;
    }


    bool resourceHasSparseFeedbackLoads(dxbc_spv::ir::Builder::iterator op) const {
      auto [a, b] = m_builder.getUses(op->getDef());

      for (auto iter = a; iter != b; iter++) {
        if (iter->getOpCode() == dxbc_spv::ir::OpCode::eDescriptorLoad) {
          if (descriptorHasSparseFeedbackLoads(*iter))
            return true;
        }
      }

      return false;
    }


    DxvkShaderIo convertIoMap(const dxbc_spv::ir::IoMap& io) const {
      DxvkShaderIo map;

      for (const auto& e : io) {
        DxvkShaderIoVar var = { };

        if (e.getType() == dxbc_spv::ir::IoEntryType::eBuiltIn) {
          auto builtIn = convertBuiltIn(e.getBuiltIn());

          if (!builtIn)
            continue;

          var.builtIn = *builtIn;
          var.location = 0u;
          var.componentIndex = 0u;
          var.componentCount = e.computeComponentCount();
          var.isPatchConstant = builtIn == spv::BuiltInTessLevelInner ||
                                builtIn == spv::BuiltInTessLevelOuter;
        } else {
          var.builtIn = spv::BuiltInMax;
          var.location = e.getLocationIndex();
          var.componentIndex = e.getFirstComponentIndex();
          var.componentCount = e.computeComponentCount();
          var.isPatchConstant = e.getType() == dxbc_spv::ir::IoEntryType::ePerPatch;
        }

        map.add(var);
      }

      return map;
    }


    std::optional<spv::BuiltIn> convertBuiltIn(dxbc_spv::ir::BuiltIn builtIn) const {
      switch (builtIn) {
        case dxbc_spv::ir::BuiltIn::ePosition:
          return m_stage == dxbc_spv::ir::ShaderStage::ePixel
            ? spv::BuiltInFragCoord
            : spv::BuiltInPosition;
        case dxbc_spv::ir::BuiltIn::eClipDistance:
          return spv::BuiltInClipDistance;
        case dxbc_spv::ir::BuiltIn::eCullDistance:
          return spv::BuiltInCullDistance;
        case dxbc_spv::ir::BuiltIn::eVertexId:
          return spv::BuiltInVertexIndex;
        case dxbc_spv::ir::BuiltIn::eInstanceId:
          return spv::BuiltInInstanceIndex;
        case dxbc_spv::ir::BuiltIn::ePrimitiveId:
          return spv::BuiltInPrimitiveId;
        case dxbc_spv::ir::BuiltIn::eLayerIndex:
          return spv::BuiltInLayer;
        case dxbc_spv::ir::BuiltIn::eViewportIndex:
          return spv::BuiltInViewportIndex;
        case dxbc_spv::ir::BuiltIn::eGsVertexCountIn:
          return std::nullopt;
        case dxbc_spv::ir::BuiltIn::eGsInstanceId:
          return spv::BuiltInInvocationId;
        case dxbc_spv::ir::BuiltIn::eTessControlPointCountIn:
          return spv::BuiltInPatchVertices;
        case dxbc_spv::ir::BuiltIn::eTessControlPointId:
          return spv::BuiltInInvocationId;
        case dxbc_spv::ir::BuiltIn::eTessCoord:
          return spv::BuiltInTessCoord;
        case dxbc_spv::ir::BuiltIn::eTessFactorInner:
          return spv::BuiltInTessLevelInner;
        case dxbc_spv::ir::BuiltIn::eTessFactorOuter:
          return spv::BuiltInTessLevelOuter;
        case dxbc_spv::ir::BuiltIn::eSampleCount:
          return std::nullopt;
        case dxbc_spv::ir::BuiltIn::eSampleId:
          return spv::BuiltInSampleId;
        case dxbc_spv::ir::BuiltIn::eSamplePosition:
          return spv::BuiltInSamplePosition;
        case dxbc_spv::ir::BuiltIn::eSampleMask:
          return spv::BuiltInSampleMask;
        case dxbc_spv::ir::BuiltIn::eIsFrontFace:
          return spv::BuiltInFrontFacing;
        case dxbc_spv::ir::BuiltIn::eDepth:
          return spv::BuiltInFragDepth;
        case dxbc_spv::ir::BuiltIn::eStencilRef:
          return spv::BuiltInFragStencilRefEXT;
        case dxbc_spv::ir::BuiltIn::eIsFullyCovered:
          return spv::BuiltInFullyCoveredEXT;
        case dxbc_spv::ir::BuiltIn::eWorkgroupId:
          return spv::BuiltInWorkgroupId;
        case dxbc_spv::ir::BuiltIn::eGlobalThreadId:
          return spv::BuiltInGlobalInvocationId;
        case dxbc_spv::ir::BuiltIn::eLocalThreadId:
          return spv::BuiltInLocalInvocationId;
        case dxbc_spv::ir::BuiltIn::eLocalThreadIndex:
          return spv::BuiltInLocalInvocationIndex;
      }

      return std::nullopt;
    }


    static VkShaderStageFlagBits convertShaderStage(dxbc_spv::ir::ShaderStage stage) {
      switch (stage) {
        case dxbc_spv::ir::ShaderStage::eVertex:
          return VK_SHADER_STAGE_VERTEX_BIT;
        case dxbc_spv::ir::ShaderStage::eHull:
          return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case dxbc_spv::ir::ShaderStage::eDomain:
          return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case dxbc_spv::ir::ShaderStage::eGeometry:
          return VK_SHADER_STAGE_GEOMETRY_BIT;
        case dxbc_spv::ir::ShaderStage::ePixel:
          return VK_SHADER_STAGE_FRAGMENT_BIT;
        case dxbc_spv::ir::ShaderStage::eCompute:
          return VK_SHADER_STAGE_COMPUTE_BIT;
        case dxbc_spv::ir::ShaderStage::eFlagEnum:
          break;
      }

      return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    }


    static VkImageViewType determineViewType(dxbc_spv::ir::ResourceKind kind) {
      switch (kind) {
        case dxbc_spv::ir::ResourceKind::eImage1D:
          return VK_IMAGE_VIEW_TYPE_1D;
        case dxbc_spv::ir::ResourceKind::eImage1DArray:
          return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case dxbc_spv::ir::ResourceKind::eImage2D:
        case dxbc_spv::ir::ResourceKind::eImage2DMS:
          return VK_IMAGE_VIEW_TYPE_2D;
        case dxbc_spv::ir::ResourceKind::eImage2DArray:
        case dxbc_spv::ir::ResourceKind::eImage2DMSArray:
          return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case dxbc_spv::ir::ResourceKind::eImageCube:
          return VK_IMAGE_VIEW_TYPE_CUBE;
        case dxbc_spv::ir::ResourceKind::eImageCubeArray:
          return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        case dxbc_spv::ir::ResourceKind::eImage3D:
          return VK_IMAGE_VIEW_TYPE_3D;
        default:
          return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
      }
    }


    std::string getDebugName(dxbc_spv::ir::SsaDef def) {
      auto [a, b] = m_builder.getUses(def);

      for (auto iter = a; iter != b; iter++) {
        if (iter->getOpCode() == dxbc_spv::ir::OpCode::eDebugName)
          return iter->getLiteralString(iter->getFirstLiteralOperandIndex());
      }

      return std::to_string(def.getId());
    }

  };



  DxvkIrShaderConverter::~DxvkIrShaderConverter() {

  }



  DxvkIrShader::DxvkIrShader(
    const DxvkIrShaderCreateInfo&   info,
          Rc<DxvkIrShaderConverter> shader)
  : m_baseIr(std::move(shader)), m_debugName(m_baseIr->getDebugName()), m_info(info) {

  }


  DxvkIrShader::DxvkIrShader(
          std::string               name,
    const DxvkIrShaderCreateInfo&   info,
          DxvkShaderMetadata        metadata,
          DxvkPipelineLayoutBuilder layout,
          std::vector<uint8_t>      ir)
  : m_debugName   (std::move(name)), m_info(info),
    m_layout      (std::move(layout)),
    m_ir          (std::move(ir)),
    m_convertedIr (true),
    m_metadata    (std::move(metadata)) {

  }


  DxvkIrShader::~DxvkIrShader() {

  }


  DxvkShaderMetadata DxvkIrShader::getShaderMetadata() {
    convertIr("getShaderMetadata()");

    return m_metadata;
  }


  void DxvkIrShader::compile() {
    convertIr(nullptr);
  }


  SpirvCodeBuffer DxvkIrShader::getCode(
    const DxvkShaderBindingMap*       bindings,
    const DxvkShaderLinkage*          linkage) {
    convertIr("getCode()");

    DxvkDxbcSpirvLogger logger(debugName());

    dxbc_spv::ir::Builder irBuilder;
    deserializeIr(irBuilder);

    // Fix up shader I/O based on shader linkage
    { dxbc_spv::ir::LowerIoPass ioPass(irBuilder);
      if (linkage) {
        if (m_metadata.stage == VK_SHADER_STAGE_FRAGMENT_BIT && linkage->fsFlatShading && m_info.flatShadingInputs)
          ioPass.enableFlatInterpolation(m_info.flatShadingInputs);

        if (m_metadata.stage == VK_SHADER_STAGE_GEOMETRY_BIT && linkage->inputTopology != m_metadata.inputTopology)
          ioPass.changeGsInputPrimitiveType(convertPrimitiveType(linkage->inputTopology));

        if (m_metadata.stage == VK_SHADER_STAGE_FRAGMENT_BIT && linkage->fsDualSrcBlend) {
          dxbc_spv::ir::IoMap io = { };
          io.add(dxbc_spv::ir::IoLocation(dxbc_spv::ir::IoEntryType::ePerVertex, 0u, 0xfu));
          io.add(dxbc_spv::ir::IoLocation(dxbc_spv::ir::IoEntryType::ePerVertex, 1u, 0xfu));

          ioPass.resolveUnusedOutputs(io);
        }

        if (m_metadata.stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
          std::array<dxbc_spv::ir::IoOutputSwizzle, 8u> swizzles = { };
          uint32_t outputMask = m_metadata.outputs.computeMask();

          for (auto i : bit::BitMask(outputMask))
            swizzles.at(i) = convertOutputSwizzle(linkage->rtSwizzles.at(i));

          ioPass.swizzleOutputs(swizzles.size(), swizzles.data());
        }

        if (m_metadata.stage != VK_SHADER_STAGE_COMPUTE_BIT && !DxvkShaderIo::checkStageCompatibility(
            m_metadata.stage, m_metadata.inputs, linkage->prevStage, linkage->prevStageOutputs))
          ioPass.resolveMismatchedIo(convertShaderStage(linkage->prevStage), convertIoMap(linkage->prevStageOutputs, linkage->prevStage));

        if (m_metadata.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
          ioPass.resolvePatchConstantLocations(convertIoMap(m_metadata.outputs, m_metadata.stage));

        if (m_metadata.stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
          ioPass.resolvePatchConstantLocations(convertIoMap(linkage->prevStageOutputs, linkage->prevStage));
      }

      if (m_metadata.stage == VK_SHADER_STAGE_FRAGMENT_BIT && m_info.options.flags.test(DxvkShaderCompileFlag::EnableSampleRateShading))
        ioPass.enableSampleInterpolation();
    }

    // Set up SPIR-V options. Only enable float controls if a sufficient subset
    // of features is supported; this avoids running into performance issues on
    // Nvidia where just enabling RTE on FP32 causes a ~20% performance drop.
    dxbc_spv::spirv::SpirvBuilder::Options options = { };
    options.includeDebugNames = true;
    options.nvRawAccessChains = m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsNvRawAccessChains);
    options.dualSourceBlending = linkage && linkage->fsDualSrcBlend;

    if (m_info.options.spirv.all(DxvkShaderSpirvFlag::IndependentDenormMode,
                               DxvkShaderSpirvFlag::SupportsRte32,
                               DxvkShaderSpirvFlag::SupportsDenormFlush32)) {
      if (m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsRte16))
        options.supportedRoundModesF16 |= dxbc_spv::ir::RoundMode::eNearestEven;
      if (m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsRte32))
        options.supportedRoundModesF32 |= dxbc_spv::ir::RoundMode::eNearestEven;
      if (m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsRte64))
        options.supportedRoundModesF64 |= dxbc_spv::ir::RoundMode::eNearestEven;

      if (m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsRtz16))
        options.supportedRoundModesF16 |= dxbc_spv::ir::RoundMode::eZero;
      if (m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsRtz32))
        options.supportedRoundModesF32 |= dxbc_spv::ir::RoundMode::eZero;
      if (m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsRtz64))
        options.supportedRoundModesF64 |= dxbc_spv::ir::RoundMode::eZero;

      if (m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsDenormFlush16))
        options.supportedDenormModesF16 |= dxbc_spv::ir::DenormMode::eFlush;
      if (m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsDenormFlush32))
        options.supportedDenormModesF32 |= dxbc_spv::ir::DenormMode::eFlush;
      if (m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsDenormFlush64))
        options.supportedDenormModesF64 |= dxbc_spv::ir::DenormMode::eFlush;

      if (m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsDenormPreserve16))
        options.supportedDenormModesF16 |= dxbc_spv::ir::DenormMode::ePreserve;
      if (m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsDenormPreserve32))
        options.supportedDenormModesF32 |= dxbc_spv::ir::DenormMode::ePreserve;
      if (m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsDenormPreserve64))
        options.supportedDenormModesF64 |= dxbc_spv::ir::DenormMode::ePreserve;

      if (m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsSzInfNanPreserve32))
        options.floatControls2 = m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsFloatControls2);
    }

    options.supportsZeroInfNanPreserveF16 = m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsSzInfNanPreserve16);
    options.supportsZeroInfNanPreserveF32 = m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsSzInfNanPreserve32);
    options.supportsZeroInfNanPreserveF64 = m_info.options.spirv.test(DxvkShaderSpirvFlag::SupportsSzInfNanPreserve64);

    options.maxCbvSize = m_info.options.maxUniformBufferSize;
    options.maxCbvCount = m_info.options.maxUniformBufferCount;

    // Build final SPIR-V binary
    DxvkShaderResourceMapping mapping(m_metadata.stage, bindings);

    dxbc_spv::spirv::SpirvBuilder spirvBuilder(irBuilder, mapping, options);
    spirvBuilder.buildSpirvBinary();

    return SpirvCodeBuffer(spirvBuilder.getSpirvBinary());
  }


  DxvkPipelineLayoutBuilder DxvkIrShader::getLayout() {
    convertIr("getLayout()");

    return m_layout;
  }


  void DxvkIrShader::dump(std::ostream& outputStream) {
    auto code = getCode(nullptr, nullptr);
    outputStream.write(reinterpret_cast<const char*>(code.data()), code.size());
  }


  std::pair<const uint8_t*, size_t> DxvkIrShader::getSerializedIr() {
    convertIr("getSerializedIr()");

    return std::make_pair(m_ir.data(), m_ir.size());
  }


  std::string DxvkIrShader::debugName() {
    return m_debugName;
  }


  void DxvkIrShader::convertIr(const char* reason) {
    if (m_convertedIr.load(std::memory_order_acquire))
      return;

    std::lock_guard lock(m_mutex);

    if (m_convertedIr.load(std::memory_order_relaxed))
      return;

    if (reason && Logger::logLevel() <= LogLevel::Debug)
      Logger::debug(str::format(m_debugName, ": Early compile: ", reason));

    const auto& dumpPath = getShaderDumpPath();

    if (!dumpPath.empty())
      dumpSource(dumpPath);

    convertShader();

    // Destroy original converter, we no longer need it
    m_baseIr = nullptr;

    m_convertedIr.store(true, std::memory_order_release);

    // Need to do this *after* marking the conversion as done since lowering
    // to SPIR-V itself will otherwise call into this method again
    if (!dumpPath.empty())
      dumpSpv(dumpPath);
  }


  void DxvkIrShader::convertShader() {
    DxvkDxbcSpirvLogger logger(m_debugName);

    dxbc_spv::ir::Builder builder;
    m_baseIr->convertShader(builder);

    if (!m_info.xfbEntries.empty()) {
      dxbc_spv::ir::LowerIoPass ioPass(builder);

      ioPass.resolveXfbOutputs(
        m_info.xfbEntries.size(),
        m_info.xfbEntries.data(),
        m_info.rasterizedStream);
    }

    if (m_info.options.flags.test(DxvkShaderCompileFlag::DisableMsaa)) {
      dxbc_spv::ir::LowerIoPass ioPass(builder);
      ioPass.demoteMultisampledSrv();
    }

    dxbc_spv::ir::CompileOptions options;
    options.arithmeticOptions.lowerDot = true;
    options.arithmeticOptions.lowerSinCos = m_info.options.flags.test(DxvkShaderCompileFlag::LowerSinCos);
    options.arithmeticOptions.lowerMsad = true;
    options.arithmeticOptions.lowerF32toF16 = m_info.options.flags.test(DxvkShaderCompileFlag::LowerF32toF16);
    options.arithmeticOptions.lowerConvertFtoI = m_info.options.flags.test(DxvkShaderCompileFlag::LowerFtoI);
    options.arithmeticOptions.lowerGsVertexCountIn = false;
    options.arithmeticOptions.hasNvUnsignedItoFBug = m_info.options.flags.test(DxvkShaderCompileFlag::LowerItoF);

    options.min16Options.enableFloat16 = m_info.options.flags.test(DxvkShaderCompileFlag::Supports16BitArithmetic);
    options.min16Options.enableInt16 = m_info.options.flags.test(DxvkShaderCompileFlag::Supports16BitArithmetic);

    options.resourceOptions.allowSubDwordScratchAndLds = true;
    options.resourceOptions.flattenLds = false;
    options.resourceOptions.flattenScratch = false;
    options.resourceOptions.structuredCbv = true;
    options.resourceOptions.structuredSrvUav = true;

    auto ssboAlignment = m_info.options.minStorageBufferAlignment;
    options.bufferOptions.useTypedForRaw = ssboAlignment > 16u;
    options.bufferOptions.useTypedForStructured = ssboAlignment > 4u;
    options.bufferOptions.useTypedForSparseFeedback = true;
    options.bufferOptions.useRawForTypedAtomic = ssboAlignment <= 4u;
    options.bufferOptions.forceFormatForTypedUavRead = m_info.options.flags.test(DxvkShaderCompileFlag::TypedR32LoadRequiresFormat);
    options.bufferOptions.minStructureAlignment = ssboAlignment;

    options.scalarizeOptions.subDwordVectors = true;

    options.syncOptions.insertRovLocks = true;
    options.syncOptions.insertLdsBarriers = m_info.options.flags.test(DxvkShaderCompileFlag::InsertSharedMemoryBarriers);
    options.syncOptions.insertUavBarriers = m_info.options.flags.test(DxvkShaderCompileFlag::InsertResourceBarriers);

    options.derivativeOptions.hoistNontrivialDerivativeOps = true;
    options.derivativeOptions.hoistNontrivialImplicitLodOps = false;
    options.derivativeOptions.hoistDescriptorLoads = true;

    dxbc_spv::ir::legalizeIr(builder, options);

    // Generate shader metadata based on the final code
    DxvkIrLowerBindingModelPass lowerBindingModelPass(builder, *m_baseIr, m_info);
    lowerBindingModelPass.run();

    m_metadata = lowerBindingModelPass.getMetadata();
    m_layout = lowerBindingModelPass.getLayout();

    serializeIr(builder);
  }


  void DxvkIrShader::serializeIr(const dxbc_spv::ir::Builder& builder) {
    dxbc_spv::ir::Serializer serializer(builder);

    std::vector<uint8_t> data(serializer.computeSerializedSize());
    serializer.serialize(data.data(), data.size());

    m_ir = std::move(data);
  }


  void DxvkIrShader::deserializeIr(dxbc_spv::ir::Builder& builder) const {
    dxbc_spv::ir::Deserializer deserializer(m_ir.data(), m_ir.size());

    if (!deserializer.deserialize(builder))
      throw DxvkError("Failed to deserialize shader");
  }


  void DxvkIrShader::dumpSource(const std::string& path) {
    if (m_baseIr)
      m_baseIr->dumpSource(path);
  }


  void DxvkIrShader::dumpSpv(const std::string& path) {
    std::ofstream file(str::topath(str::format(path, "/", m_debugName, ".spv").c_str()).c_str(), std::ios_base::trunc | std::ios_base::binary);

    auto code = getCode(nullptr, nullptr);
    file.write(reinterpret_cast<const char*>(code.data()), code.size());
  }


  dxbc_spv::ir::PrimitiveType DxvkIrShader::convertPrimitiveType(VkPrimitiveTopology topology) {
    switch (topology) {
      case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        return dxbc_spv::ir::PrimitiveType::ePoints;

      case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
      case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
        return dxbc_spv::ir::PrimitiveType::eLines;

      case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
      case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
        return dxbc_spv::ir::PrimitiveType::eLinesAdj;

      case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
      case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
      case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
        return dxbc_spv::ir::PrimitiveType::eTriangles;

      case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
      case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
        return dxbc_spv::ir::PrimitiveType::eTrianglesAdj;

      default:
        return dxbc_spv::ir::PrimitiveType();
    }
  }


  dxbc_spv::ir::IoOutputSwizzle DxvkIrShader::convertOutputSwizzle(VkComponentMapping mapping) {
    dxbc_spv::ir::IoOutputSwizzle result;
    result.x = convertOutputComponent(mapping.r, dxbc_spv::ir::IoOutputComponent::eX);
    result.y = convertOutputComponent(mapping.g, dxbc_spv::ir::IoOutputComponent::eY);
    result.z = convertOutputComponent(mapping.b, dxbc_spv::ir::IoOutputComponent::eZ);
    result.w = convertOutputComponent(mapping.a, dxbc_spv::ir::IoOutputComponent::eW);
    return result;
  }


  dxbc_spv::ir::IoOutputComponent DxvkIrShader::convertOutputComponent(VkComponentSwizzle swizzle, dxbc_spv::ir::IoOutputComponent identity) {
    switch (swizzle) {
      case VK_COMPONENT_SWIZZLE_R: return dxbc_spv::ir::IoOutputComponent::eX;
      case VK_COMPONENT_SWIZZLE_G: return dxbc_spv::ir::IoOutputComponent::eY;
      case VK_COMPONENT_SWIZZLE_B: return dxbc_spv::ir::IoOutputComponent::eZ;
      case VK_COMPONENT_SWIZZLE_A: return dxbc_spv::ir::IoOutputComponent::eW;
      default: return identity;
    }
  }


  dxbc_spv::ir::ShaderStage DxvkIrShader::convertShaderStage(VkShaderStageFlagBits stage) {
    switch (stage) {
      case VK_SHADER_STAGE_VERTEX_BIT:
        return dxbc_spv::ir::ShaderStage::eVertex;
      case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return dxbc_spv::ir::ShaderStage::eHull;
      case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return dxbc_spv::ir::ShaderStage::eDomain;
      case VK_SHADER_STAGE_GEOMETRY_BIT:
        return dxbc_spv::ir::ShaderStage::eGeometry;
      case VK_SHADER_STAGE_FRAGMENT_BIT:
        return dxbc_spv::ir::ShaderStage::ePixel;
      case VK_SHADER_STAGE_COMPUTE_BIT:
        return dxbc_spv::ir::ShaderStage::eCompute;
      default:
        return dxbc_spv::ir::ShaderStage();
    }
  }


  dxbc_spv::ir::IoMap DxvkIrShader::convertIoMap(const DxvkShaderIo& io, VkShaderStageFlagBits stage) {
    dxbc_spv::ir::IoMap map = { };

    for (uint32_t i = 0u; i < io.getVarCount(); i++) {
      const auto& var = io.getVar(i);

      if (var.builtIn != spv::BuiltInMax) {
        auto builtIn = convertBuiltIn(var.builtIn, stage);

        if (builtIn)
          map.add(dxbc_spv::ir::IoLocation(*builtIn, uint8_t((1u << var.componentCount) - 1u)));
      } else {
        auto type = var.isPatchConstant
          ? dxbc_spv::ir::IoEntryType::ePerPatch
          : dxbc_spv::ir::IoEntryType::ePerVertex;

        map.add(dxbc_spv::ir::IoLocation(type, var.location,
          ((1u << var.componentCount) - 1u) << var.componentIndex));
      }
    }

    return map;
  }


  std::optional<dxbc_spv::ir::BuiltIn> DxvkIrShader::convertBuiltIn(spv::BuiltIn builtIn, VkShaderStageFlagBits stage) {
    switch (builtIn) {
      case spv::BuiltInFragCoord:
      case spv::BuiltInPosition:
        return dxbc_spv::ir::BuiltIn::ePosition;
      case spv::BuiltInClipDistance:
        return dxbc_spv::ir::BuiltIn::eClipDistance;
      case spv::BuiltInCullDistance:
        return dxbc_spv::ir::BuiltIn::eCullDistance;
      case spv::BuiltInVertexId:
      case spv::BuiltInVertexIndex:
        return dxbc_spv::ir::BuiltIn::eVertexId;
      case spv::BuiltInInstanceId:
      case spv::BuiltInInstanceIndex:
        return dxbc_spv::ir::BuiltIn::eInstanceId;
      case spv::BuiltInPrimitiveId:
        return dxbc_spv::ir::BuiltIn::ePrimitiveId;
      case spv::BuiltInLayer:
        return dxbc_spv::ir::BuiltIn::eLayerIndex;
      case spv::BuiltInViewportIndex:
        return dxbc_spv::ir::BuiltIn::eViewportIndex;
      case spv::BuiltInInvocationId: {
        if (stage == VK_SHADER_STAGE_GEOMETRY_BIT)
          return dxbc_spv::ir::BuiltIn::eGsInstanceId;
        if (stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
          return dxbc_spv::ir::BuiltIn::eTessControlPointId;
      } return std::nullopt;
      case spv::BuiltInPatchVertices:
        return dxbc_spv::ir::BuiltIn::eTessControlPointCountIn;
      case spv::BuiltInTessCoord:
        return dxbc_spv::ir::BuiltIn::eTessCoord;
      case spv::BuiltInTessLevelInner:
        return dxbc_spv::ir::BuiltIn::eTessFactorInner;
      case spv::BuiltInTessLevelOuter:
        return dxbc_spv::ir::BuiltIn::eTessFactorOuter;
      case spv::BuiltInSampleId:
        return dxbc_spv::ir::BuiltIn::eSampleId;
      case spv::BuiltInSamplePosition:
        return dxbc_spv::ir::BuiltIn::eSamplePosition;
      case spv::BuiltInSampleMask:
        return dxbc_spv::ir::BuiltIn::eSampleMask;
      case spv::BuiltInFrontFacing:
        return dxbc_spv::ir::BuiltIn::eIsFrontFace;
      case spv::BuiltInFragDepth:
        return dxbc_spv::ir::BuiltIn::eDepth;
      case spv::BuiltInFragStencilRefEXT:
        return dxbc_spv::ir::BuiltIn::eStencilRef;
      case spv::BuiltInFullyCoveredEXT:
        return dxbc_spv::ir::BuiltIn::eIsFullyCovered;
      case spv::BuiltInWorkgroupId:
        return dxbc_spv::ir::BuiltIn::eWorkgroupId;
      case spv::BuiltInGlobalInvocationId:
        return dxbc_spv::ir::BuiltIn::eGlobalThreadId;
      case spv::BuiltInLocalInvocationId:
        return dxbc_spv::ir::BuiltIn::eLocalThreadId;
      case spv::BuiltInLocalInvocationIndex:
        return dxbc_spv::ir::BuiltIn::eLocalThreadIndex;
      default:
        return std::nullopt;
    }
  }

}
