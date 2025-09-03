#include <ir/ir_serialize.h>

#include <spirv/spirv_builder.h>

#include <util/util_log.h>

#include "dxvk_shader_ir.h"

namespace dxvk {

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
      const DxvkIrResourceMapping&    mapping,
      const DxvkIrShaderCreateInfo&   info)
    : m_builder (builder),
      m_mapping (mapping),
      m_info    (info) {

    }

    /**
     * \brief Runs lowering pass
     */
    void run() {
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
      dxbc_spv::ir::SsaDef uavCounter = { };
      uint32_t memberIndex = 0u;
    };

    dxbc_spv::ir::Builder&        m_builder;
    const DxvkIrResourceMapping&  m_mapping;
    DxvkIrShaderCreateInfo        m_info = { };

    DxvkShaderMetadata            m_metadata = { };
    DxvkPipelineLayoutBuilder     m_layout;

    dxbc_spv::ir::SsaDef          m_entryPoint = { };
    dxbc_spv::ir::ShaderStage     m_stage = { };

    uint32_t                      m_localPushDataAlign = 4u;
    uint32_t                      m_localPushDataOffset = 0u;
    uint32_t                      m_localPushDataResourceMask = 0u;

    uint32_t                      m_sharedPushDataOffset = 0u;

    small_vector<SamplerInfo,     16u>  m_samplers;
    small_vector<UavCounterInfo,  64u>  m_uavCounters;


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
      binding.resourceIndex = m_mapping.determineResourceIndex(m_stage,
        dxbc_spv::ir::ScalarType::eCbv, regSpace, regIndex);

      if (op->getType().byteSize() <= m_info.options.spirvOptions.maxUniformBufferSize) {
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

      DxvkBindingInfo binding = { };
      binding.set = DxvkShaderResourceMapping::setIndexForType(dxbc_spv::ir::ScalarType::eSrv);
      binding.binding = regIndex;
      binding.resourceIndex = m_mapping.determineResourceIndex(m_stage,
        dxbc_spv::ir::ScalarType::eSrv, regSpace, regIndex);
      binding.access = VK_ACCESS_SHADER_READ_BIT;

      if (dxbc_spv::ir::resourceIsBuffer(resourceKind)) {
        if (dxbc_spv::ir::resourceIsTyped(resourceKind))
          binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        else
          binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      } else {
        binding.viewType = determineViewType(resourceKind);
        binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

        if (dxbc_spv::ir::resourceIsMultisampled(resourceKind))
          binding.flags.set(DxvkDescriptorFlag::Multisampled);
      }

      if (resourceHasSparseFeedbackLoads(op))
        m_metadata.flags.set(DxvkShaderFlag::UsesSparseResidency);

      addBinding(binding);
      return ++op;
    }


    dxbc_spv::ir::Builder::iterator handleUav(dxbc_spv::ir::Builder::iterator op) {
      auto regSpace = uint32_t(op->getOperand(1u));
      auto regIndex = uint32_t(op->getOperand(2u));

      auto resourceKind = dxbc_spv::ir::ResourceKind(op->getOperand(4u));
      auto uavFlags = dxbc_spv::ir::UavFlags(op->getOperand(5u));

      DxvkBindingInfo binding = { };
      binding.set = DxvkShaderResourceMapping::setIndexForType(dxbc_spv::ir::ScalarType::eUav);
      binding.binding = regIndex;
      binding.resourceIndex = m_mapping.determineResourceIndex(m_stage,
        dxbc_spv::ir::ScalarType::eUav, regSpace, regIndex);

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
        binding.viewType = determineViewType(resourceKind);
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      }

      if (resourceHasSparseFeedbackLoads(op))
        m_metadata.flags.set(DxvkShaderFlag::UsesSparseResidency);

      addBinding(binding);
      return ++op;
    }


    dxbc_spv::ir::Builder::iterator handleUavCounter(dxbc_spv::ir::Builder::iterator op) {
      const auto& uavOp = m_builder.getOpForOperand(*op, 1u);

      auto regSpace = uint32_t(uavOp.getOperand(1u));
      auto regIndex = uint32_t(uavOp.getOperand(2u));

      // TODO promote to BDA
      DxvkBindingInfo binding = { };
      binding.set = DxvkShaderResourceMapping::setIndexForType(dxbc_spv::ir::ScalarType::eUavCounter);
      binding.binding = regIndex;
      binding.resourceIndex = m_mapping.determineResourceIndex(m_stage,
        dxbc_spv::ir::ScalarType::eUavCounter, regSpace, regIndex);
      binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      binding.access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

      addBinding(binding);
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

        if (m_info.options.compileOptions.flags.test(DxvkShaderCompileFlag::Supports16BitPushData)) {
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

      m_localPushDataResourceMask |= ((1u << dwordCount) - 1u) << dwordIndex;

      if (m_info.options.compileOptions.flags.test(DxvkShaderCompileFlag::Supports16BitPushData)) {
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
      if (m_info.options.compileOptions.flags.test(DxvkShaderCompileFlag::Supports16BitPushData)) {
        for (size_t i = 0u; i < m_samplers.size(); i++) {
          auto& e = m_samplers[i];
          auto debugName = getDebugName(e.sampler);

          if (!debugName.empty())
            m_builder.add(dxbc_spv::ir::Op::DebugMemberName(def, e.memberIndex, debugName.c_str()));
        }
      }

      return def;
    }


    dxbc_spv::ir::Builder::iterator rewriteSampleCountBuiltIn(dxbc_spv::ir::Builder::iterator op) {
      small_vector<dxbc_spv::ir::SsaDef, 64u> uses;
      m_builder.getUses(op->getDef(), uses);

      m_builder.rewriteOp(op->getDef(), dxbc_spv::ir::Op::DclPushData(op->getType(),
        m_entryPoint, m_info.options.compileOptions.sampleCountPushDataOffset, dxbc_spv::ir::ShaderStageMask()));

      for (auto use : uses) {
        const auto& useOp = m_builder.getOp(use);

        if (useOp.getOpCode() == dxbc_spv::ir::OpCode::eInputLoad) {
          m_builder.rewriteOp(useOp.getDef(), dxbc_spv::ir::Op::PushDataLoad(
            useOp.getType(), op->getDef(), dxbc_spv::ir::SsaDef()));
        }
      }

      m_sharedPushDataOffset = std::max<uint32_t>(m_sharedPushDataOffset,
        m_info.options.compileOptions.sampleCountPushDataOffset + sizeof(uint32_t));
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
          dxbc_spv::ir::SsaDef indexDef = { };

          if (m_info.options.compileOptions.flags.test(DxvkShaderCompileFlag::Supports16BitPushData)) {
            indexDef = m_builder.addBefore(op.getDef(), dxbc_spv::ir::Op::PushDataLoad(
              dxbc_spv::ir::ScalarType::eU16, pushDataDef, m_builder.makeConstant(uint32_t(info.memberIndex))));
            indexDef = m_builder.addBefore(op.getDef(), dxbc_spv::ir::Op::ConvertItoI(
              dxbc_spv::ir::ScalarType::eU32, indexDef));
          } else {
            indexDef = m_builder.addBefore(op.getDef(), dxbc_spv::ir::Op::PushDataLoad(
              dxbc_spv::ir::ScalarType::eU32, pushDataDef, m_builder.makeConstant(uint32_t(info.memberIndex))));
            indexDef = m_builder.addBefore(op.getDef(), dxbc_spv::ir::Op::UBitExtract(
              dxbc_spv::ir::ScalarType::eU32, indexDef, m_builder.makeConstant(uint32_t(16u * info.wordIndex)), m_builder.makeConstant(16u)));
          }

          m_builder.rewriteOp(op.getDef(), dxbc_spv::ir::Op::DescriptorLoad(
            op.getType(), heapDef, indexDef));
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
      binding.resourceIndex = m_mapping.determineResourceIndex(m_stage,
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


    void rewriteUavCounters() {
      if (m_uavCounters.empty())
        return;
    }


    void addBinding(const DxvkBindingInfo& binding) {
      DxvkShaderDescriptor descriptor(binding, m_metadata.stage);
      m_layout.addBindings(1u, &descriptor);
    }


    DxvkAccessOp determineAccessOpForAccess(const dxbc_spv::ir::Op& op) const {
      switch (op.getOpCode()) {
        case dxbc_spv::ir::OpCode::eBufferStore:
        case dxbc_spv::ir::OpCode::eImageStore: {

        } return DxvkAccessOp::None;

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

            default:
              return DxvkAccessOp::None;
          }
        }

        default:
          return DxvkAccessOp::None;
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




  DxvkIrResourceMapping::~DxvkIrResourceMapping() {

  }


  DxvkIrShader::DxvkIrShader(
    const DxvkIrShaderCreateInfo&   info,
    const DxvkIrResourceMapping&    mapping,
          dxbc_spv::ir::Builder&&   builder)
  : m_debugName(getDebugName(builder)), m_info(info) {
    lowerIoBindingModel(builder, mapping);
    serializeIr(builder);
  }


  DxvkIrShader::~DxvkIrShader() {

  }


  SpirvCodeBuffer DxvkIrShader::getCode(
    const DxvkShaderBindingMap*       bindings,
    const DxvkShaderLinkage*          linkage) {
    DxvkDxbcSpirvLogger logger(debugName());

    legalizeIr();

    dxbc_spv::ir::Builder irBuilder;
    deserializeIr(irBuilder);

    // Fix up shader I/O based on shader linkage
    { dxbc_spv::ir::LowerIoPass ioPass(irBuilder);
      if (linkage) {
        if (m_metadata.stage == VK_SHADER_STAGE_FRAGMENT_BIT && linkage->fsFlatShading && m_info.flatShadingInputs)
          ioPass.enableFlatInterpolation(m_info.flatShadingInputs);

        if (m_metadata.stage == VK_SHADER_STAGE_GEOMETRY_BIT && linkage->inputTopology != m_metadata.inputTopology)
          ioPass.changeGsInputPrimitiveType(convertPrimitiveType(linkage->inputTopology));

        if (m_metadata.stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
          std::array<dxbc_spv::ir::IoOutputSwizzle, 8u> swizzles = { };
          uint32_t outputMask = m_metadata.outputs.computeMask();

          for (auto i : bit::BitMask(outputMask))
            swizzles.at(i) = convertOutputSwizzle(linkage->rtSwizzles.at(i));
        }

        if (m_metadata.stage != VK_SHADER_STAGE_COMPUTE_BIT && !DxvkShaderIo::checkStageCompatibility(
            m_metadata.stage, m_metadata.inputs, linkage->prevStage, linkage->prevStageOutputs))
          ioPass.resolveMismatchedIo(convertShaderStage(linkage->prevStage), convertIoMap(linkage->prevStageOutputs, linkage->prevStage));

        if (m_metadata.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
          ioPass.resolvePatchConstantLocations(convertIoMap(m_metadata.outputs, m_metadata.stage));

        if (m_metadata.stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
          ioPass.resolvePatchConstantLocations(convertIoMap(linkage->prevStageOutputs, linkage->prevStage));
      }

      if (m_metadata.stage == VK_SHADER_STAGE_FRAGMENT_BIT && m_info.options.compileOptions.flags.test(DxvkShaderCompileFlag::EnableSampleRateShading))
        ioPass.enableSampleInterpolation();
    }

    // Set up SPIR-V options. Only enable float controls if a sufficient subset
    // of features is supported; this avoids running into performance issues on
    // Nvidia where just enabling RTE on FP32 causes a ~20% performance drop.
    auto spirvOptions = m_info.options.spirvOptions;

    dxbc_spv::spirv::SpirvBuilder::Options options = { };
    options.includeDebugNames = true;
    options.nvRawAccessChains = spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsNvRawAccessChains);
    options.dualSourceBlending = linkage && linkage->fsDualSrcBlend;

    if (spirvOptions.flags.all(DxvkShaderSpirvFlag::IndependentDenormMode,
                               DxvkShaderSpirvFlag::SupportsRte32,
                               DxvkShaderSpirvFlag::SupportsDenormFlush32)) {
      if (spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsRte16))
        options.supportedRoundModesF16 |= dxbc_spv::ir::RoundMode::eNearestEven;
      if (spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsRte32))
        options.supportedRoundModesF32 |= dxbc_spv::ir::RoundMode::eNearestEven;
      if (spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsRte64))
        options.supportedRoundModesF64 |= dxbc_spv::ir::RoundMode::eNearestEven;

      if (spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsRtz16))
        options.supportedRoundModesF16 |= dxbc_spv::ir::RoundMode::eZero;
      if (spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsRtz32))
        options.supportedRoundModesF32 |= dxbc_spv::ir::RoundMode::eZero;
      if (spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsRtz64))
        options.supportedRoundModesF64 |= dxbc_spv::ir::RoundMode::eZero;

      if (spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsDenormFlush16))
        options.supportedDenormModesF16 |= dxbc_spv::ir::DenormMode::eFlush;
      if (spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsDenormFlush32))
        options.supportedDenormModesF32 |= dxbc_spv::ir::DenormMode::eFlush;
      if (spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsDenormFlush64))
        options.supportedDenormModesF64 |= dxbc_spv::ir::DenormMode::eFlush;

      if (spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsDenormPreserve16))
        options.supportedDenormModesF16 |= dxbc_spv::ir::DenormMode::ePreserve;
      if (spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsDenormPreserve32))
        options.supportedDenormModesF32 |= dxbc_spv::ir::DenormMode::ePreserve;
      if (spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsDenormPreserve64))
        options.supportedDenormModesF64 |= dxbc_spv::ir::DenormMode::ePreserve;

      if (spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsSzInfNanPreserve32))
        options.floatControls2 = spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsFloatControls2);
    }

    options.supportsZeroInfNanPreserveF16 = spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsSzInfNanPreserve16);
    options.supportsZeroInfNanPreserveF32 = spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsSzInfNanPreserve32);
    options.supportsZeroInfNanPreserveF64 = spirvOptions.flags.test(DxvkShaderSpirvFlag::SupportsSzInfNanPreserve64);

    // Build final SPIR-V binary
    DxvkShaderResourceMapping mapping(m_metadata.stage, bindings);

    dxbc_spv::spirv::SpirvBuilder spirvBuilder(irBuilder, mapping, options);
    spirvBuilder.buildSpirvBinary();

    return SpirvCodeBuffer(spirvBuilder.getSpirvBinary());
  }


  DxvkPipelineLayoutBuilder DxvkIrShader::getLayout() const {
    return m_layout;
  }


  void DxvkIrShader::dump(std::ostream& outputStream) {
    auto code = getCode(nullptr, nullptr);
    outputStream.write(reinterpret_cast<const char*>(code.data()), code.size());
  }


  std::string DxvkIrShader::debugName() const {
    return m_debugName;
  }


  void DxvkIrShader::legalizeIr() {
    if (m_legalizedIr.load(std::memory_order_acquire))
      return;

    std::lock_guard lock(m_mutex);

    if (m_legalizedIr.load(std::memory_order_relaxed))
      return;

    dxbc_spv::dxbc::CompileOptions options;
    options.arithmeticOptions.fuseMad = true;
    options.arithmeticOptions.lowerDot = true;
    options.arithmeticOptions.lowerSinCos = m_info.options.compileOptions.flags.test(DxvkShaderCompileFlag::LowerSinCos);
    options.arithmeticOptions.lowerMsad = true;
    options.arithmeticOptions.lowerF32toF16 = true;
    options.arithmeticOptions.lowerConvertFtoI = true;
    options.arithmeticOptions.lowerGsVertexCountIn = false;
    options.arithmeticOptions.hasNvUnsignedItoFBug = m_info.options.compileOptions.flags.test(DxvkShaderCompileFlag::LowerItoF);

    options.min16Options.enableFloat16 = m_info.options.compileOptions.flags.test(DxvkShaderCompileFlag::Supports16BitArithmetic);
    options.min16Options.enableInt16 = m_info.options.compileOptions.flags.test(DxvkShaderCompileFlag::Supports16BitArithmetic);

    options.resourceOptions.allowSubDwordScratchAndLds = true;
    options.resourceOptions.flattenLds = false;
    options.resourceOptions.flattenScratch = false;
    options.resourceOptions.structuredCbv = true;
    options.resourceOptions.structuredSrvUav = true;

    options.bufferOptions = getBufferPassOptions();

    options.scalarizeOptions.subDwordVectors = true;

    options.syncOptions.insertRovLocks = true;
    options.syncOptions.insertLdsBarriers = m_info.options.compileOptions.flags.test(DxvkShaderCompileFlag::InsertSharedMemoryBarriers);
    options.syncOptions.insertUavBarriers = m_info.options.compileOptions.flags.test(DxvkShaderCompileFlag::InsertResourceBarriers);

    options.derivativeOptions.hoistNontrivialDerivativeOps = true;
    options.derivativeOptions.hoistNontrivialImplicitLodOps = false;
    options.derivativeOptions.hoistDescriptorLoads = true;

    dxbc_spv::ir::Builder builder;
    deserializeIr(builder);

    dxbc_spv::dxbc::legalizeIr(builder, options);

    serializeIr(builder);

    m_legalizedIr.store(true, std::memory_order_release);
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


  std::string DxvkIrShader::getDebugName(const dxbc_spv::ir::Builder& builder) const {
    dxbc_spv::ir::SsaDef entryPoint;

    for (const auto& op : builder) {
      if (op.getOpCode() == dxbc_spv::ir::OpCode::eEntryPoint)
        entryPoint = op.getDef();
    }

    if (entryPoint) {
      auto uses = builder.getUses(entryPoint);

      for (auto iter = uses.first; iter != uses.second; iter++) {
        if (iter->getOpCode() == dxbc_spv::ir::OpCode::eDebugName)
          return iter->getLiteralString(iter->getFirstLiteralOperandIndex());
      }
    }

    // Shouldn't happen
    return str::format("ir_shader_", getCookie());
  }


  void DxvkIrShader::lowerIoBindingModel(dxbc_spv::ir::Builder& builder, const DxvkIrResourceMapping& mapping) {
    // To generate binding info, we need to know the final descriptor type of each buffer
    dxbc_spv::ir::ConvertBufferKindPass::runPass(builder, getBufferPassOptions());

    if (!m_info.xfbEntries.empty()) {
      dxbc_spv::ir::LowerIoPass ioPass(builder);

      ioPass.resolveXfbOutputs(
        m_info.xfbEntries.size(),
        m_info.xfbEntries.data(),
        m_info.rasterizedStream);
    }

    DxvkIrLowerBindingModelPass pass(builder, mapping, m_info);
    pass.run();

    m_metadata = pass.getMetadata();
    m_layout = pass.getLayout();
  }


  dxbc_spv::ir::ConvertBufferKindPass::Options DxvkIrShader::getBufferPassOptions() const {
    auto ssboAlignment = m_info.options.compileOptions.minStorageBufferAlignment;

    dxbc_spv::ir::ConvertBufferKindPass::Options options = { };
    options.useTypedForRaw = ssboAlignment > 16u;
    options.useTypedForStructured = ssboAlignment > 4u;
    options.useTypedForSparseFeedback = true;
    options.useRawForTypedAtomic = ssboAlignment <= 4u;
    options.forceFormatForTypedUavRead = m_info.options.compileOptions.flags.test(DxvkShaderCompileFlag::TypedR32LoadRequiresFormat);
    options.minStructureAlignment = ssboAlignment;
    return options;
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
