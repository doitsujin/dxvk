#include <iomanip>

#include "../util/util_time.h"

#include "dxvk_device.h"
#include "dxvk_graphics.h"
#include "dxvk_pipemanager.h"
#include "dxvk_state_cache.h"

namespace dxvk {

  VkPrimitiveTopology determineGsInputTopology(
          VkPrimitiveTopology            shader,
          VkPrimitiveTopology            state) {
    switch (state) {
      case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

      case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
      case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
        if (shader == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY)
          return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
        [[fallthrough]];

      case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
      case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

      case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
      case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
        if (shader == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY)
          return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
        [[fallthrough]];

      case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
      case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
      case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

      default:
        Logger::err(str::format("Unhandled primitive topology ", state));
        return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    }
  }


  VkPrimitiveTopology determinePreGsTopology(
    const DxvkGraphicsPipelineShaders&      shaders,
    const DxvkGraphicsPipelineStateInfo&    state) {
    if (shaders.tcs && shaders.tcs->flags().test(DxvkShaderFlag::TessellationPoints))
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    if (shaders.tes)
      return shaders.tes->info().outputTopology;

    return state.ia.primitiveTopology();
  }


  VkPrimitiveTopology determinePipelineTopology(
    const DxvkGraphicsPipelineShaders&      shaders,
    const DxvkGraphicsPipelineStateInfo&    state) {
    if (shaders.gs)
      return shaders.gs->info().outputTopology;

    return determinePreGsTopology(shaders, state);
  }


  DxvkGraphicsPipelineVertexInputState::DxvkGraphicsPipelineVertexInputState() {
    
  }


  DxvkGraphicsPipelineVertexInputState::DxvkGraphicsPipelineVertexInputState(
    const DxvkDevice*                     device,
    const DxvkGraphicsPipelineStateInfo&  state,
    const DxvkGraphicsPipelineShaders&    shaders) {
    std::array<uint32_t, MaxNumVertexBindings> viBindingMap = { };

    iaInfo.topology               = state.ia.primitiveTopology();
    iaInfo.primitiveRestartEnable = state.ia.primitiveRestart();

    uint32_t attrMask = shaders.vs->info().inputMask;
    uint32_t bindingMask = 0;

    // Find out which bindings are used based on the attribute mask
    for (uint32_t i = 0; i < state.il.attributeCount(); i++) {
      if (attrMask & (1u << state.ilAttributes[i].location()))
        bindingMask |= 1u << state.ilAttributes[i].binding();
    }

    // Process vertex bindings. We will compact binding numbers on
    // the fly so that vertex buffers can be updated more easily.
    uint32_t bindingCount = 0;

    for (uint32_t i = 0; i < state.il.bindingCount(); i++) {
      uint32_t bindingIndex = state.ilBindings[i].binding();

      if (bindingMask & (1u << bindingIndex)) {
        viBindingMap[bindingIndex] = i;

        VkVertexInputBindingDescription& binding = viBindings[bindingCount++];
        binding.binding = i;
        binding.stride = state.ilBindings[i].stride();
        binding.inputRate = state.ilBindings[i].inputRate();

        if (state.ilBindings[i].inputRate() == VK_VERTEX_INPUT_RATE_INSTANCE
         && state.ilBindings[i].divisor()   != 1) {
          VkVertexInputBindingDivisorDescriptionEXT& divisor = viDivisors[viDivisorInfo.vertexBindingDivisorCount++];
          divisor.binding = i;
          divisor.divisor = state.ilBindings[i].divisor();
        }
      }
    }

    if (bindingCount) {
      bool supportsDivisor = device->features().extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor;

      viInfo.vertexBindingDescriptionCount = bindingCount;
      viInfo.pVertexBindingDescriptions = viBindings.data();

      if (viDivisorInfo.vertexBindingDivisorCount && supportsDivisor) {
        viDivisorInfo.pVertexBindingDivisors = viDivisors.data();
        viInfo.pNext = &viDivisorInfo;
      }
    }

    // Process vertex attributes, filtering out unused ones
    uint32_t attrCount = 0;

    for (uint32_t i = 0; i < state.il.attributeCount(); i++) {
      if (attrMask & (1u << state.ilAttributes[i].location())) {
        VkVertexInputAttributeDescription& attr = viAttributes[attrCount++];
        attr.location = state.ilAttributes[i].location();
        attr.binding = viBindingMap[state.ilAttributes[i].binding()];
        attr.format = state.ilAttributes[i].format();
        attr.offset = state.ilAttributes[i].offset();
      }
    }

    if (attrCount) {
      viInfo.vertexAttributeDescriptionCount = attrCount;
      viInfo.pVertexAttributeDescriptions = viAttributes.data();
    }

    // We need to be consistent with the pipeline state vector since
    // the normalized state may otherwise change beavhiour here.
    viUseDynamicVertexStrides = state.useDynamicVertexStrides();
  }


  bool DxvkGraphicsPipelineVertexInputState::eq(const DxvkGraphicsPipelineVertexInputState& other) const {
    bool eq = iaInfo.topology                         == other.iaInfo.topology
           && iaInfo.primitiveRestartEnable           == other.iaInfo.primitiveRestartEnable
           && viInfo.vertexBindingDescriptionCount    == other.viInfo.vertexBindingDescriptionCount
           && viInfo.vertexAttributeDescriptionCount  == other.viInfo.vertexAttributeDescriptionCount
           && viDivisorInfo.vertexBindingDivisorCount == other.viDivisorInfo.vertexBindingDivisorCount
           && viUseDynamicVertexStrides               == other.viUseDynamicVertexStrides;

    for (uint32_t i = 0; i < viInfo.vertexBindingDescriptionCount && eq; i++) {
      const auto& a = viBindings[i];
      const auto& b = other.viBindings[i];

      eq = a.binding    == b.binding
        && a.stride     == b.stride
        && a.inputRate  == b.inputRate;
    }

    for (uint32_t i = 0; i < viInfo.vertexAttributeDescriptionCount && eq; i++) {
      const auto& a = viAttributes[i];
      const auto& b = other.viAttributes[i];

      eq = a.location   == b.location
        && a.binding    == b.binding
        && a.format     == b.format
        && a.offset     == b.offset;
    }

    for (uint32_t i = 0; i < viDivisorInfo.vertexBindingDivisorCount; i++) {
      const auto& a = viDivisors[i];
      const auto& b = other.viDivisors[i];

      eq = a.binding    == b.binding
        && a.divisor    == b.divisor;
    }

    return eq;
  }


  size_t DxvkGraphicsPipelineVertexInputState::hash() const {
    DxvkHashState hash;
    hash.add(uint32_t(iaInfo.topology));
    hash.add(uint32_t(iaInfo.primitiveRestartEnable));
    hash.add(uint32_t(viInfo.vertexBindingDescriptionCount));
    hash.add(uint32_t(viInfo.vertexAttributeDescriptionCount));
    hash.add(uint32_t(viDivisorInfo.vertexBindingDivisorCount));
    hash.add(uint32_t(viUseDynamicVertexStrides));

    for (uint32_t i = 0; i < viInfo.vertexBindingDescriptionCount; i++) {
      hash.add(uint32_t(viBindings[i].binding));
      hash.add(uint32_t(viBindings[i].stride));
      hash.add(uint32_t(viBindings[i].inputRate));
    }

    for (uint32_t i = 0; i < viInfo.vertexAttributeDescriptionCount; i++) {
      hash.add(uint32_t(viAttributes[i].location));
      hash.add(uint32_t(viAttributes[i].binding));
      hash.add(uint32_t(viAttributes[i].format));
      hash.add(uint32_t(viAttributes[i].offset));
    }

    for (uint32_t i = 0; i < viDivisorInfo.vertexBindingDivisorCount; i++) {
      hash.add(uint32_t(viDivisors[i].binding));
      hash.add(uint32_t(viDivisors[i].divisor));
    }

    return hash;
  }


  DxvkGraphicsPipelineVertexInputLibrary::DxvkGraphicsPipelineVertexInputLibrary(
          DxvkDevice*                           device,
    const DxvkGraphicsPipelineVertexInputState& state)
  : m_device(device) {
    auto vk = m_device->vkd();

    VkDynamicState dynamicState = VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE;
    VkPipelineDynamicStateCreateInfo dyInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };

    if (state.viUseDynamicVertexStrides) {
      dyInfo.dynamicStateCount = 1;
      dyInfo.pDynamicStates = &dynamicState;
    }

    VkGraphicsPipelineLibraryCreateInfoEXT libInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT };
    libInfo.flags             = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
    info.flags                = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    info.pVertexInputState    = &state.viInfo;
    info.pInputAssemblyState  = &state.iaInfo;
    info.pDynamicState        = &dyInfo;
    info.basePipelineIndex    = -1;

    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(),
      VK_NULL_HANDLE, 1, &info, nullptr, &m_pipeline);

    if (vr)
      throw DxvkError("Failed to create vertex input pipeline library");
  }


  DxvkGraphicsPipelineVertexInputLibrary::~DxvkGraphicsPipelineVertexInputLibrary() {
    auto vk = m_device->vkd();

    vk->vkDestroyPipeline(vk->device(), m_pipeline, nullptr);
  }


  DxvkGraphicsPipelineFragmentOutputState::DxvkGraphicsPipelineFragmentOutputState() {

  }


  DxvkGraphicsPipelineFragmentOutputState::DxvkGraphicsPipelineFragmentOutputState(
    const DxvkDevice*                     device,
    const DxvkGraphicsPipelineStateInfo&  state,
    const DxvkGraphicsPipelineShaders&    shaders) {
    // Set up color formats and attachment blend states. Disable the write
    // mask for any attachment that the fragment shader does not write to.
    uint32_t fsOutputMask = shaders.fs ? shaders.fs->info().outputMask : 0u;

    // Dual-source blending can only write to one render target
    if (state.useDualSourceBlending())
      fsOutputMask &= 0x1;

    const VkColorComponentFlags rgbaWriteMask
      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
      | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    cbInfo.logicOpEnable  = state.om.enableLogicOp();
    cbInfo.logicOp        = state.om.logicOp();

    feedbackLoop = state.om.feedbackLoop();

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      rtColorFormats[i] = state.rt.getColorFormat(i);

      if (rtColorFormats[i]) {
        rtInfo.colorAttachmentCount = i + 1;

        auto formatInfo = lookupFormatInfo(rtColorFormats[i]);

        if ((fsOutputMask & (1 << i)) && formatInfo) {
          VkColorComponentFlags writeMask = state.omBlend[i].colorWriteMask();

          if (writeMask != rgbaWriteMask) {
            writeMask = util::remapComponentMask(
              state.omBlend[i].colorWriteMask(), state.omSwizzle[i].mapping());
          }

          writeMask &= formatInfo->componentMask;

          if (writeMask == formatInfo->componentMask)
            writeMask = rgbaWriteMask;

          if (writeMask) {
            cbAttachments[i] = state.omBlend[i].state();
            cbAttachments[i].colorWriteMask = writeMask;

            // If we're rendering to an emulated alpha-only render target, fix up blending
            if (cbAttachments[i].blendEnable && formatInfo->componentMask == VK_COLOR_COMPONENT_R_BIT && state.omSwizzle[i].rIndex() == 3) {
              cbAttachments[i].srcColorBlendFactor = util::remapAlphaToColorBlendFactor(
                std::exchange(cbAttachments[i].srcAlphaBlendFactor, VK_BLEND_FACTOR_ONE));
              cbAttachments[i].dstColorBlendFactor = util::remapAlphaToColorBlendFactor(
                std::exchange(cbAttachments[i].dstAlphaBlendFactor, VK_BLEND_FACTOR_ZERO));
              cbAttachments[i].colorBlendOp =
                std::exchange(cbAttachments[i].alphaBlendOp, VK_BLEND_OP_ADD);
            }
          }
        }
      }
    }

    if (rtInfo.colorAttachmentCount) {
      rtInfo.pColorAttachmentFormats = rtColorFormats.data();

      cbInfo.attachmentCount = rtInfo.colorAttachmentCount;
      cbInfo.pAttachments = cbAttachments.data();
    }

    // Set up depth-stencil format accordingly.
    VkFormat rtDepthFormat = state.rt.getDepthStencilFormat();

    if (rtDepthFormat) {
      auto rtDepthFormatInfo = lookupFormatInfo(rtDepthFormat);

      if (rtDepthFormatInfo->aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
        rtInfo.depthAttachmentFormat = rtDepthFormat;

      if (rtDepthFormatInfo->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
        rtInfo.stencilAttachmentFormat = rtDepthFormat;
    }

    // Set up multisample state based on shader info as well
    // as rasterization state and render target sample counts.
    msInfo.rasterizationSamples = VkSampleCountFlagBits(state.ms.sampleCount());

    if (!msInfo.rasterizationSamples) {
      msInfo.rasterizationSamples = state.rs.sampleCount()
        ? VkSampleCountFlagBits(state.rs.sampleCount())
        : VK_SAMPLE_COUNT_1_BIT;
    }

    if (shaders.fs && shaders.fs->flags().test(DxvkShaderFlag::HasSampleRateShading)) {
      msInfo.sampleShadingEnable  = VK_TRUE;
      msInfo.minSampleShading     = 1.0f;
    }

    // Alpha to coverage is not supported with sample mask exports.
    cbUseDynamicAlphaToCoverage = !shaders.fs || !shaders.fs->flags().test(DxvkShaderFlag::ExportsSampleMask);

    msSampleMask                  = state.ms.sampleMask() & ((1u << msInfo.rasterizationSamples) - 1);
    msInfo.pSampleMask            = &msSampleMask;
    msInfo.alphaToCoverageEnable  = state.ms.enableAlphaToCoverage() && cbUseDynamicAlphaToCoverage;

    // We need to be fully consistent with the pipeline state here, and
    // while we could consistently infer it, just don't take any chances
    cbUseDynamicBlendConstants = state.useDynamicBlendConstants();
  }


  bool DxvkGraphicsPipelineFragmentOutputState::eq(const DxvkGraphicsPipelineFragmentOutputState& other) const {
    bool eq = rtInfo.colorAttachmentCount     == other.rtInfo.colorAttachmentCount
           && rtInfo.depthAttachmentFormat    == other.rtInfo.depthAttachmentFormat
           && rtInfo.stencilAttachmentFormat  == other.rtInfo.stencilAttachmentFormat
           && cbInfo.logicOpEnable            == other.cbInfo.logicOpEnable
           && cbInfo.logicOp                  == other.cbInfo.logicOp
           && cbInfo.attachmentCount          == other.cbInfo.attachmentCount
           && msInfo.rasterizationSamples     == other.msInfo.rasterizationSamples
           && msInfo.sampleShadingEnable      == other.msInfo.sampleShadingEnable
           && msInfo.minSampleShading         == other.msInfo.minSampleShading
           && msInfo.alphaToCoverageEnable    == other.msInfo.alphaToCoverageEnable
           && msInfo.alphaToOneEnable         == other.msInfo.alphaToOneEnable
           && msSampleMask                    == other.msSampleMask
           && cbUseDynamicBlendConstants      == other.cbUseDynamicBlendConstants
           && cbUseDynamicAlphaToCoverage     == other.cbUseDynamicAlphaToCoverage
           && feedbackLoop                    == other.feedbackLoop;

    for (uint32_t i = 0; i < rtInfo.colorAttachmentCount && eq; i++)
      eq = rtColorFormats[i] == other.rtColorFormats[i];

    for (uint32_t i = 0; i < cbInfo.attachmentCount && eq; i++) {
      const auto& a = cbAttachments[i];
      const auto& b = other.cbAttachments[i];

      eq = a.blendEnable    == b.blendEnable
        && a.colorWriteMask == b.colorWriteMask;

      if (a.blendEnable && eq) {
        eq = a.srcColorBlendFactor == b.srcColorBlendFactor
          && a.dstColorBlendFactor == b.dstColorBlendFactor
          && a.colorBlendOp        == b.colorBlendOp
          && a.srcAlphaBlendFactor == b.srcAlphaBlendFactor
          && a.dstAlphaBlendFactor == b.dstAlphaBlendFactor
          && a.alphaBlendOp        == b.alphaBlendOp;
      }
    }

    return eq;
  }


  size_t DxvkGraphicsPipelineFragmentOutputState::hash() const {
    DxvkHashState hash;
    hash.add(uint32_t(rtInfo.colorAttachmentCount));
    hash.add(uint32_t(rtInfo.depthAttachmentFormat));
    hash.add(uint32_t(rtInfo.stencilAttachmentFormat));
    hash.add(uint32_t(cbInfo.logicOpEnable));
    hash.add(uint32_t(cbInfo.logicOp));
    hash.add(uint32_t(cbInfo.attachmentCount));
    hash.add(uint32_t(msInfo.rasterizationSamples));
    hash.add(uint32_t(msInfo.alphaToCoverageEnable));
    hash.add(uint32_t(msInfo.alphaToOneEnable));
    hash.add(uint32_t(msSampleMask));
    hash.add(uint32_t(cbUseDynamicBlendConstants));
    hash.add(uint32_t(cbUseDynamicAlphaToCoverage));
    hash.add(uint32_t(feedbackLoop));

    for (uint32_t i = 0; i < rtInfo.colorAttachmentCount; i++)
      hash.add(uint32_t(rtColorFormats[i]));

    for (uint32_t i = 0; i < cbInfo.attachmentCount; i++) {
      hash.add(uint32_t(cbAttachments[i].blendEnable));
      hash.add(uint32_t(cbAttachments[i].colorWriteMask));

      if (cbAttachments[i].blendEnable) {
        hash.add(uint32_t(cbAttachments[i].srcColorBlendFactor));
        hash.add(uint32_t(cbAttachments[i].dstColorBlendFactor));
        hash.add(uint32_t(cbAttachments[i].colorBlendOp));
        hash.add(uint32_t(cbAttachments[i].srcAlphaBlendFactor));
        hash.add(uint32_t(cbAttachments[i].dstAlphaBlendFactor));
        hash.add(uint32_t(cbAttachments[i].alphaBlendOp));
      }
    }

    return hash;
  }


  DxvkGraphicsPipelineFragmentOutputLibrary::DxvkGraphicsPipelineFragmentOutputLibrary(
          DxvkDevice*                               device,
    const DxvkGraphicsPipelineFragmentOutputState&  state)
  : m_device(device) {
    auto vk = m_device->vkd();

    uint32_t dynamicStateCount = 0;
    std::array<VkDynamicState, 4> dynamicStates = { };

    bool hasDynamicMultisampleState = state.msInfo.sampleShadingEnable
      && m_device->features().extExtendedDynamicState3.extendedDynamicState3RasterizationSamples
      && m_device->features().extExtendedDynamicState3.extendedDynamicState3SampleMask;

    bool hasDynamicAlphaToCoverage = hasDynamicMultisampleState && state.cbUseDynamicAlphaToCoverage
      && device->features().extExtendedDynamicState3.extendedDynamicState3AlphaToCoverageEnable;

    if (hasDynamicMultisampleState) {
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT;
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_SAMPLE_MASK_EXT;
    }

    if (hasDynamicAlphaToCoverage)
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT;

    if (state.cbUseDynamicBlendConstants)
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_BLEND_CONSTANTS;

    VkPipelineDynamicStateCreateInfo dyInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };

    if (dynamicStateCount) {
      dyInfo.dynamicStateCount  = dynamicStateCount;
      dyInfo.pDynamicStates     = dynamicStates.data();
    }

    VkPipelineCreateFlags flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    if (state.feedbackLoop & VK_IMAGE_ASPECT_COLOR_BIT)
      flags |= VK_PIPELINE_CREATE_COLOR_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;

    if (state.feedbackLoop & VK_IMAGE_ASPECT_DEPTH_BIT)
      flags |= VK_PIPELINE_CREATE_DEPTH_STENCIL_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;

    // Fix up multisample state based on dynamic state. Needed to
    // silence validation errors in case we hit the full EDS3 path.
    VkPipelineMultisampleStateCreateInfo msInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msInfo.sampleShadingEnable = state.msInfo.sampleShadingEnable;
    msInfo.minSampleShading = state.msInfo.minSampleShading;

    if (!hasDynamicMultisampleState) {
      msInfo.rasterizationSamples = state.msInfo.rasterizationSamples;
      msInfo.pSampleMask = state.msInfo.pSampleMask;
    }

    if (!hasDynamicAlphaToCoverage)
      msInfo.alphaToCoverageEnable = state.msInfo.alphaToCoverageEnable;

    // pNext is non-const for some reason, but this is only an input
    // structure, so we should be able to safely use const_cast.
    VkGraphicsPipelineLibraryCreateInfoEXT libInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT };
    libInfo.pNext             = const_cast<VkPipelineRenderingCreateInfo*>(&state.rtInfo);
    libInfo.flags             = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
    info.flags                = flags;
    info.pColorBlendState     = &state.cbInfo;
    info.pMultisampleState    = &msInfo;
    info.pDynamicState        = &dyInfo;
    info.basePipelineIndex    = -1;

    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(),
      VK_NULL_HANDLE, 1, &info, nullptr, &m_pipeline);

    if (vr)
      throw DxvkError("Failed to create vertex input pipeline library");
  }


  DxvkGraphicsPipelineFragmentOutputLibrary::~DxvkGraphicsPipelineFragmentOutputLibrary() {
    auto vk = m_device->vkd();

    vk->vkDestroyPipeline(vk->device(), m_pipeline, nullptr);
  }


  DxvkGraphicsPipelinePreRasterizationState::DxvkGraphicsPipelinePreRasterizationState() {
    
  }


  DxvkGraphicsPipelinePreRasterizationState::DxvkGraphicsPipelinePreRasterizationState(
    const DxvkDevice*                     device,
    const DxvkGraphicsPipelineStateInfo&  state,
    const DxvkGraphicsPipelineShaders&    shaders) {
    // Set up tessellation state
    tsInfo.patchControlPoints = state.ia.patchVertexCount();
    
    // Set up basic rasterization state
    rsInfo.depthClampEnable         = VK_TRUE;
    rsInfo.polygonMode              = state.rs.polygonMode();
    rsInfo.depthBiasEnable          = state.rs.depthBiasEnable();
    rsInfo.lineWidth                = 1.0f;

    // Set up rasterized stream depending on geometry shader state.
    // Rasterizing stream 0 is default behaviour in all situations.
    int32_t streamIndex = shaders.gs ? shaders.gs->info().xfbRasterizedStream : 0;

    if (streamIndex > 0) {
      rsXfbStreamInfo.pNext = std::exchange(rsInfo.pNext, &rsXfbStreamInfo);
      rsXfbStreamInfo.rasterizationStream = uint32_t(streamIndex);
    } else if (streamIndex < 0) {
      rsInfo.rasterizerDiscardEnable = VK_TRUE;
    }

    // Set up depth clip state. If the extension is not supported,
    // use depth clamp instead, even though this is not accurate.
    if (device->features().extDepthClipEnable.depthClipEnable) {
      rsDepthClipInfo.pNext = std::exchange(rsInfo.pNext, &rsDepthClipInfo);
      rsDepthClipInfo.depthClipEnable = state.rs.depthClipEnable();
    } else {
      rsInfo.depthClampEnable = !state.rs.depthClipEnable();
    }

    // Set up conservative rasterization if requested by the application.
    if (state.rs.conservativeMode() != VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT) {
      rsConservativeInfo.pNext = std::exchange(rsInfo.pNext, &rsConservativeInfo);
      rsConservativeInfo.conservativeRasterizationMode = state.rs.conservativeMode();
      rsConservativeInfo.extraPrimitiveOverestimationSize = 0.0f;
    }

    // Set up line rasterization mode as requested by the application.
    if (state.rs.lineMode() != VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT && isLineRendering(shaders, state)) {
      rsLineInfo.pNext = std::exchange(rsInfo.pNext, &rsLineInfo);
      rsLineInfo.lineRasterizationMode = state.rs.lineMode();

      if (rsLineInfo.lineRasterizationMode == VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT) {
        // This line width matches expected D3D behaviour, hard-code this
        // so that we don't need to introduce an extra bit of render state.
        rsInfo.lineWidth = 1.4f;
      } else {
        // Vulkan does not allow alphaToCoverage or sample rate shading
        // in combination with smooth lines. Override the line mode to
        // rectangular to fix this, but keep the width fixed at 1.0.
        bool needsOverride = state.ms.enableAlphaToCoverage()
          || (shaders.fs && shaders.fs->flags().test(DxvkShaderFlag::HasSampleRateShading));

        if (needsOverride)
          rsLineInfo.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT;
      }
    }
  }



  bool DxvkGraphicsPipelinePreRasterizationState::eq(const DxvkGraphicsPipelinePreRasterizationState& other) const {
    bool eq = tsInfo.patchControlPoints == other.tsInfo.patchControlPoints;

    if (eq) {
      eq = rsInfo.depthClampEnable         == other.rsInfo.depthClampEnable
        && rsInfo.rasterizerDiscardEnable  == other.rsInfo.rasterizerDiscardEnable
        && rsInfo.polygonMode              == other.rsInfo.polygonMode
        && rsInfo.depthBiasEnable          == other.rsInfo.depthBiasEnable
        && rsInfo.lineWidth                == other.rsInfo.lineWidth;
    }

    if (eq)
      eq = rsXfbStreamInfo.rasterizationStream == other.rsXfbStreamInfo.rasterizationStream;

    if (eq)
      eq = rsDepthClipInfo.depthClipEnable == other.rsDepthClipInfo.depthClipEnable;

    if (eq) {
      eq = rsConservativeInfo.conservativeRasterizationMode    == other.rsConservativeInfo.conservativeRasterizationMode
        && rsConservativeInfo.extraPrimitiveOverestimationSize == other.rsConservativeInfo.extraPrimitiveOverestimationSize;
    }

    if (eq)
      eq = rsLineInfo.lineRasterizationMode == other.rsLineInfo.lineRasterizationMode;

    return eq;
  }


  size_t DxvkGraphicsPipelinePreRasterizationState::hash() const {
    DxvkHashState hash;
    hash.add(tsInfo.patchControlPoints);

    hash.add(rsInfo.depthClampEnable);
    hash.add(rsInfo.rasterizerDiscardEnable);
    hash.add(rsInfo.polygonMode);
    hash.add(rsInfo.depthBiasEnable);
    hash.add(bit::cast<uint32_t>(rsInfo.lineWidth));

    hash.add(rsXfbStreamInfo.rasterizationStream);

    hash.add(rsDepthClipInfo.depthClipEnable);

    hash.add(rsConservativeInfo.conservativeRasterizationMode);
    hash.add(bit::cast<uint32_t>(rsConservativeInfo.extraPrimitiveOverestimationSize));

    hash.add(rsLineInfo.lineRasterizationMode);
    return hash;
  }


  bool DxvkGraphicsPipelinePreRasterizationState::isLineRendering(
    const DxvkGraphicsPipelineShaders&    shaders,
    const DxvkGraphicsPipelineStateInfo&  state) {
    VkPrimitiveTopology topology = determinePipelineTopology(shaders, state);

    if (state.rs.polygonMode() == VK_POLYGON_MODE_LINE) {
      return topology != VK_PRIMITIVE_TOPOLOGY_POINT_LIST
          && topology != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    }

    return topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST
        || topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP
        || topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY
        || topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
  }


  DxvkGraphicsPipelineFragmentShaderState::DxvkGraphicsPipelineFragmentShaderState() {

  }


  DxvkGraphicsPipelineFragmentShaderState::DxvkGraphicsPipelineFragmentShaderState(
    const DxvkDevice*                     device,
    const DxvkGraphicsPipelineStateInfo&  state) {
    VkImageAspectFlags dsReadOnlyAspects = state.rt.getDepthStencilReadOnlyAspects();

    bool enableDepthWrites = !(dsReadOnlyAspects & VK_IMAGE_ASPECT_DEPTH_BIT);
    bool enableStencilWrites = !(dsReadOnlyAspects & VK_IMAGE_ASPECT_STENCIL_BIT);

    dsInfo.depthTestEnable        = state.ds.enableDepthTest();
    dsInfo.depthWriteEnable       = state.ds.enableDepthWrite() && enableDepthWrites;
    dsInfo.depthCompareOp         = state.ds.depthCompareOp();
    dsInfo.depthBoundsTestEnable  = state.ds.enableDepthBoundsTest();
    dsInfo.stencilTestEnable      = state.ds.enableStencilTest();
    dsInfo.front                  = state.dsFront.state(enableStencilWrites);
    dsInfo.back                   = state.dsBack.state(enableStencilWrites);
  }


  bool DxvkGraphicsPipelineFragmentShaderState::eq(const DxvkGraphicsPipelineFragmentShaderState& other) const {
    bool eq = dsInfo.depthTestEnable       == other.dsInfo.depthTestEnable
           && dsInfo.depthBoundsTestEnable == other.dsInfo.depthBoundsTestEnable
           && dsInfo.stencilTestEnable     == other.dsInfo.stencilTestEnable;

    if (eq && dsInfo.depthTestEnable) {
      eq = dsInfo.depthWriteEnable == other.dsInfo.depthWriteEnable
        && dsInfo.depthCompareOp   == other.dsInfo.depthCompareOp;
    }

    if (eq && dsInfo.stencilTestEnable) {
      eq = dsInfo.front.failOp      == other.dsInfo.front.failOp
        && dsInfo.front.passOp      == other.dsInfo.front.passOp
        && dsInfo.front.depthFailOp == other.dsInfo.front.depthFailOp
        && dsInfo.front.compareOp   == other.dsInfo.front.compareOp
        && dsInfo.front.compareMask == other.dsInfo.front.compareMask
        && dsInfo.front.writeMask   == other.dsInfo.front.writeMask
        && dsInfo.back.failOp       == other.dsInfo.back.failOp
        && dsInfo.back.passOp       == other.dsInfo.back.passOp
        && dsInfo.back.depthFailOp  == other.dsInfo.back.depthFailOp
        && dsInfo.back.compareOp    == other.dsInfo.back.compareOp
        && dsInfo.back.compareMask  == other.dsInfo.back.compareMask
        && dsInfo.back.writeMask    == other.dsInfo.back.writeMask;
    }

    return eq;
  }


  size_t DxvkGraphicsPipelineFragmentShaderState::hash() const {
    DxvkHashState hash;
    hash.add(dsInfo.depthTestEnable);
    hash.add(dsInfo.depthBoundsTestEnable);
    hash.add(dsInfo.stencilTestEnable);

    if (dsInfo.depthTestEnable) {
      hash.add(dsInfo.depthWriteEnable);
      hash.add(dsInfo.depthCompareOp);
    }

    if (dsInfo.stencilTestEnable) {
      hash.add(dsInfo.front.failOp);
      hash.add(dsInfo.front.passOp);
      hash.add(dsInfo.front.depthFailOp);
      hash.add(dsInfo.front.compareOp);
      hash.add(dsInfo.front.compareMask);
      hash.add(dsInfo.front.writeMask);
      hash.add(dsInfo.back.failOp);
      hash.add(dsInfo.back.passOp);
      hash.add(dsInfo.back.depthFailOp);
      hash.add(dsInfo.back.compareOp);
      hash.add(dsInfo.back.compareMask);
      hash.add(dsInfo.back.writeMask);
    }

    return hash;
  }


  DxvkGraphicsPipelineDynamicState::DxvkGraphicsPipelineDynamicState() {
    
  }


  DxvkGraphicsPipelineDynamicState::DxvkGraphicsPipelineDynamicState(
    const DxvkDevice*                     device,
    const DxvkGraphicsPipelineStateInfo&  state,
          DxvkGraphicsPipelineFlags       flags) {
    dyStates[dyInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT;
    dyStates[dyInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT;

    if (state.useDynamicVertexStrides())
      dyStates[dyInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE;

    if (state.useDynamicDepthBias())
      dyStates[dyInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BIAS;
    
    if (state.useDynamicDepthBounds())
      dyStates[dyInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;
    
    if (state.useDynamicBlendConstants())
      dyStates[dyInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_BLEND_CONSTANTS;
    
    if (state.useDynamicStencilRef())
      dyStates[dyInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;

    if (!flags.test(DxvkGraphicsPipelineFlag::HasRasterizerDiscard)) {
      dyStates[dyInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_CULL_MODE;
      dyStates[dyInfo.dynamicStateCount++] = VK_DYNAMIC_STATE_FRONT_FACE;
    }

    if (dyInfo.dynamicStateCount)
      dyInfo.pDynamicStates = dyStates.data();
  }


  bool DxvkGraphicsPipelineDynamicState::eq(const DxvkGraphicsPipelineDynamicState& other) const {
    bool eq = dyInfo.dynamicStateCount == other.dyInfo.dynamicStateCount;

    for (uint32_t i = 0; i < dyInfo.dynamicStateCount && eq; i++)
      eq = dyStates[i] == other.dyStates[i];

    return eq;
  }


  size_t DxvkGraphicsPipelineDynamicState::hash() const {
    DxvkHashState hash;
    hash.add(dyInfo.dynamicStateCount);

    for (uint32_t i = 0; i < dyInfo.dynamicStateCount; i++)
      hash.add(dyStates[i]);

    return hash;
  }


  DxvkGraphicsPipelineShaderState::DxvkGraphicsPipelineShaderState() {

  }


  DxvkGraphicsPipelineShaderState::DxvkGraphicsPipelineShaderState(
    const DxvkGraphicsPipelineShaders&    shaders,
    const DxvkGraphicsPipelineStateInfo&  state)
  : vsInfo  (getCreateInfo(shaders, shaders.vs, state)),
    tcsInfo (getCreateInfo(shaders, shaders.tcs, state)),
    tesInfo (getCreateInfo(shaders, shaders.tes, state)),
    gsInfo  (getCreateInfo(shaders, shaders.gs, state)),
    fsInfo  (getCreateInfo(shaders, shaders.fs, state)) {

  }


  bool DxvkGraphicsPipelineShaderState::eq(const DxvkGraphicsPipelineShaderState& other) const {
    return vsInfo.eq(other.vsInfo)
        && tcsInfo.eq(other.tcsInfo)
        && tesInfo.eq(other.tesInfo)
        && gsInfo.eq(other.gsInfo)
        && fsInfo.eq(other.fsInfo);
  }


  size_t DxvkGraphicsPipelineShaderState::hash() const {
    DxvkHashState hash;
    hash.add(vsInfo.hash());
    hash.add(tcsInfo.hash());
    hash.add(tesInfo.hash());
    hash.add(gsInfo.hash());
    hash.add(fsInfo.hash());
    return hash;
  }


  DxvkShaderModuleCreateInfo DxvkGraphicsPipelineShaderState::getCreateInfo(
    const DxvkGraphicsPipelineShaders&    shaders,
    const Rc<DxvkShader>&                 shader,
    const DxvkGraphicsPipelineStateInfo&  state) {
    DxvkShaderModuleCreateInfo info;

    if (shader == nullptr)
      return info;

    // Fix up fragment shader outputs for dual-source blending
    const DxvkShaderCreateInfo& shaderInfo = shader->info();

    if (shaderInfo.stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
      info.fsDualSrcBlend = state.useDualSourceBlending();
      info.fsFlatShading = state.rs.flatShading() && shader->info().flatShadingInputs;

      for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
        if ((shaderInfo.outputMask & (1u << i)) && state.writesRenderTarget(i))
          info.rtSwizzles[i] = state.omSwizzle[i].mapping();
      }
    }

    // Deal with undefined shader inputs
    uint32_t consumedInputs = shaderInfo.inputMask;
    uint32_t providedInputs = 0;

    if (shaderInfo.stage == VK_SHADER_STAGE_VERTEX_BIT) {
      for (uint32_t i = 0; i < state.il.attributeCount(); i++)
        providedInputs |= 1u << state.ilAttributes[i].location();
    } else if (shaderInfo.stage != VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
      auto prevStage = getPrevStageShader(shaders, shaderInfo.stage);
      providedInputs = prevStage->info().outputMask;
    } else {
      // Technically not correct, but this
      // would need a lot of extra care
      providedInputs = consumedInputs;
    }

    info.undefinedInputs = (providedInputs & consumedInputs) ^ consumedInputs;

    // Fix up input topology for geometry shaders as necessary
    if (shaderInfo.stage == VK_SHADER_STAGE_GEOMETRY_BIT) {
      VkPrimitiveTopology iaTopology = determinePreGsTopology(shaders, state);
      info.inputTopology = determineGsInputTopology(shaderInfo.inputTopology, iaTopology);
    }

    return info;
  }


  Rc<DxvkShader> DxvkGraphicsPipelineShaderState::getPrevStageShader(
    const DxvkGraphicsPipelineShaders&    shaders,
    const VkShaderStageFlagBits           stage) {
    if (stage == VK_SHADER_STAGE_VERTEX_BIT)
      return nullptr;

    if (stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
      return shaders.tcs;

    Rc<DxvkShader> result = shaders.vs;

    if (stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
      return result;

    if (shaders.tes != nullptr)
      result = shaders.tes;

    if (stage == VK_SHADER_STAGE_GEOMETRY_BIT)
      return result;

    if (shaders.gs != nullptr)
      result = shaders.gs;

    return result;
  }


  DxvkPipelineSpecConstantState::DxvkPipelineSpecConstantState() {

  }


  DxvkPipelineSpecConstantState::DxvkPipelineSpecConstantState(
          uint32_t                        mask,
    const DxvkScInfo&                     state) {
    for (uint32_t i = 0; i < MaxNumSpecConstants; i++) {
      if (mask & (1u << i))
        addConstant(i, state.specConstants[i]);
    }

    if (mask & (1u << MaxNumSpecConstants))
      addConstant(MaxNumSpecConstants, VK_TRUE);

    if (scInfo.mapEntryCount) {
      scInfo.pMapEntries = scConstantMap.data();
      scInfo.dataSize = scInfo.mapEntryCount * sizeof(uint32_t);
      scInfo.pData = scConstantData.data();
    }
  }


  bool DxvkPipelineSpecConstantState::eq(const DxvkPipelineSpecConstantState& other) const {
    bool eq = scInfo.mapEntryCount == other.scInfo.mapEntryCount;

    for (uint32_t i = 0; i < scInfo.mapEntryCount && eq; i++) {
      eq = scConstantMap[i].constantID == other.scConstantMap[i].constantID
        && scConstantData[i]           == other.scConstantData[i];
    }

    return eq;
  }


  size_t DxvkPipelineSpecConstantState::hash() const {
    DxvkHashState hash;
    hash.add(scInfo.mapEntryCount);

    for (uint32_t i = 0; i < scInfo.mapEntryCount; i++) {
      hash.add(scConstantMap[i].constantID);
      hash.add(scConstantData[i]);
    }

    return hash;
  }


  void DxvkPipelineSpecConstantState::addConstant(uint32_t id, uint32_t value) {
    if (value) {
      uint32_t index = scInfo.mapEntryCount++;

      scConstantMap[index].constantID = id;
      scConstantMap[index].offset = sizeof(uint32_t) * index;
      scConstantMap[index].size = sizeof(uint32_t);

      scConstantData[index] = value;
    }
  }


  DxvkGraphicsPipeline::DxvkGraphicsPipeline(
          DxvkDevice*                 device,
          DxvkPipelineManager*        pipeMgr,
          DxvkGraphicsPipelineShaders shaders,
          DxvkBindingLayoutObjects*   layout,
          DxvkShaderPipelineLibrary*  vsLibrary,
          DxvkShaderPipelineLibrary*  fsLibrary)
  : m_device        (device),
    m_manager       (pipeMgr),
    m_workers       (&pipeMgr->m_workers),
    m_stateCache    (&pipeMgr->m_stateCache),
    m_stats         (&pipeMgr->m_stats),
    m_shaders       (std::move(shaders)),
    m_bindings      (layout),
    m_barrier       (layout->getGlobalBarrier()),
    m_vsLibrary     (vsLibrary),
    m_fsLibrary     (fsLibrary),
    m_debugName     (createDebugName()) {
    m_vsIn  = m_shaders.vs != nullptr ? m_shaders.vs->info().inputMask  : 0;
    m_fsOut = m_shaders.fs != nullptr ? m_shaders.fs->info().outputMask : 0;
    m_specConstantMask = this->computeSpecConstantMask();

    if (m_shaders.gs != nullptr) {
      if (m_shaders.gs->flags().test(DxvkShaderFlag::HasTransformFeedback)) {
        m_flags.set(DxvkGraphicsPipelineFlag::HasTransformFeedback);

        m_barrier.stages |= VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT;
        m_barrier.access |= VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT
                         |  VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT
                         |  VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT;
      }

      if (m_shaders.gs->info().xfbRasterizedStream < 0)
        m_flags.set(DxvkGraphicsPipelineFlag::HasRasterizerDiscard);
    }
    
    if (m_barrier.access & VK_ACCESS_SHADER_WRITE_BIT) {
      m_flags.set(DxvkGraphicsPipelineFlag::HasStorageDescriptors);

      if (layout->layout().getHazardousSetMask())
        m_flags.set(DxvkGraphicsPipelineFlag::UnrollMergedDraws);
    }

    if (m_shaders.fs != nullptr) {
      if (m_shaders.fs->flags().test(DxvkShaderFlag::HasSampleRateShading))
        m_flags.set(DxvkGraphicsPipelineFlag::HasSampleRateShading);
      if (m_shaders.fs->flags().test(DxvkShaderFlag::ExportsSampleMask))
        m_flags.set(DxvkGraphicsPipelineFlag::HasSampleMaskExport);
    }
  }
  
  
  DxvkGraphicsPipeline::~DxvkGraphicsPipeline() {
    this->destroyBasePipelines();
    this->destroyOptimizedPipelines();
  }
  
  
  DxvkGlobalPipelineBarrier DxvkGraphicsPipeline::getGlobalBarrier(
    const DxvkGraphicsPipelineStateInfo&    state) const {
    DxvkGlobalPipelineBarrier barrier = m_barrier;

    if (state.il.bindingCount()) {
      barrier.stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
      barrier.access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }

    return barrier;
  }


  DxvkGraphicsPipelineHandle DxvkGraphicsPipeline::getPipelineHandle(
    const DxvkGraphicsPipelineStateInfo& state) {
    DxvkGraphicsPipelineInstance* instance = this->findInstance(state);

    if (unlikely(!instance)) {
      // Exit early if the state vector is invalid
      if (!this->validatePipelineState(state, true))
        return DxvkGraphicsPipelineHandle();

      // Prevent other threads from adding new instances and check again
      std::unique_lock<dxvk::mutex> lock(m_mutex);
      instance = this->findInstance(state);

      if (!instance) {
        // Keep pipeline object locked, at worst we're going to stall
        // a state cache worker and the current thread needs priority.
        bool canCreateBasePipeline = this->canCreateBasePipeline(state);
        instance = this->createInstance(state, canCreateBasePipeline);

        // Unlock here since we may dispatch the pipeline to a worker,
        // which will then acquire it to increment the use counter.
        lock.unlock();

        // If necessary, compile an optimized pipeline variant
        if (!instance->fastHandle.load())
          m_workers->compileGraphicsPipeline(this, state, DxvkPipelinePriority::Low);

        // Only store pipelines in the state cache that cannot benefit
        // from pipeline libraries, or if that feature is disabled.
        if (!canCreateBasePipeline)
          this->writePipelineStateToCache(state);
      }
    }

    return instance->getHandle();
  }


  void DxvkGraphicsPipeline::compilePipeline(
    const DxvkGraphicsPipelineStateInfo& state) {
    if (m_device->config().enableGraphicsPipelineLibrary == Tristate::True)
      return;

    // Try to find an existing instance that contains a base pipeline
    DxvkGraphicsPipelineInstance* instance = this->findInstance(state);

    if (!instance) {
      // Exit early if the state vector is invalid
      if (!this->validatePipelineState(state, false))
        return;

      // Do not compile if this pipeline can be fast linked. This essentially
      // disables the state cache for pipelines that do not benefit from it.
      if (this->canCreateBasePipeline(state))
        return;

      // Prevent other threads from adding new instances and check again
      std::unique_lock<dxvk::mutex> lock(m_mutex);
      instance = this->findInstance(state);

      if (!instance)
        instance = this->createInstance(state, false);
    }

    // Exit if another thread is already compiling
    // an optimized version of this pipeline
    if (instance->isCompiling.load()
     || instance->isCompiling.exchange(VK_TRUE, std::memory_order_acquire))
      return;

    VkPipeline pipeline = this->getOptimizedPipeline(state);
    instance->fastHandle.store(pipeline, std::memory_order_release);

    // Log pipeline state on error
    if (!pipeline)
      this->logPipelineState(LogLevel::Error, state);
  }


  void DxvkGraphicsPipeline::acquirePipeline() {
    if (!m_device->mustTrackPipelineLifetime())
      return;

    // We need to lock here to make sure that any ongoing pipeline
    // destruction finishes before the calling thread can access the
    // pipeline, and that no pipelines get destroyed afterwards.
    std::unique_lock<dxvk::mutex> lock(m_mutex);
    m_useCount += 1;
  }


  void DxvkGraphicsPipeline::releasePipeline() {
    if (!m_device->mustTrackPipelineLifetime())
      return;

    std::unique_lock<dxvk::mutex> lock(m_mutex);

    if (!(--m_useCount)) {
      // Don't destroy base pipelines if that's all we're going to
      // use, since that would pretty much ruin the experience.
      if (m_device->config().enableGraphicsPipelineLibrary == Tristate::True)
        return;

      // Exit early if there's nothing to do
      if (m_basePipelines.empty())
        return;

      // Remove any base pipeline references, but
      // keep the optimized pipelines around.
      for (auto& entry : m_pipelines)
        entry.baseHandle.store(VK_NULL_HANDLE);

      // Destroy the actual Vulkan pipelines
      this->destroyBasePipelines();
    }
  }


  DxvkGraphicsPipelineInstance* DxvkGraphicsPipeline::createInstance(
    const DxvkGraphicsPipelineStateInfo& state,
          bool                           doCreateBasePipeline) {
    VkPipeline baseHandle = VK_NULL_HANDLE;
    VkPipeline fastHandle = VK_NULL_HANDLE;

    if (doCreateBasePipeline)
      baseHandle = this->getBasePipeline(state);

    // Fast-linking may fail in some situations
    if (!baseHandle)
      fastHandle = this->getOptimizedPipeline(state);

    // Log pipeline state if requested, or on failure
    if (!fastHandle && !baseHandle)
      this->logPipelineState(LogLevel::Error, state);

    m_stats->numGraphicsPipelines += 1;
    return &(*m_pipelines.emplace(state, baseHandle, fastHandle, computeAttachmentMask(state)));
  }
  
  
  DxvkGraphicsPipelineInstance* DxvkGraphicsPipeline::findInstance(
    const DxvkGraphicsPipelineStateInfo& state) {
    for (auto& instance : m_pipelines) {
      if (instance.state == state)
        return &instance;
    }
    
    return nullptr;
  }
  
  
  bool DxvkGraphicsPipeline::canCreateBasePipeline(
    const DxvkGraphicsPipelineStateInfo& state) const {
    if (!m_vsLibrary || !m_fsLibrary)
      return false;

    // We do not implement setting certain rarely used render
    // states dynamically since they are generally not used
    bool isLineRendering = DxvkGraphicsPipelinePreRasterizationState::isLineRendering(m_shaders, state);

    if (state.rs.polygonMode() != VK_POLYGON_MODE_FILL
     || state.rs.conservativeMode() != VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT
     || (state.rs.lineMode() != VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT && isLineRendering))
      return false;

    // Depth clip is assumed to be enabled. If the driver does not
    // support dynamic depth clip, we'd have to late-compile anyway
    // unless the pipeline is used multiple times.
    if (!m_device->features().extExtendedDynamicState3.extendedDynamicState3DepthClipEnable
     && !state.rs.depthClipEnable())
      return false;

    // If the vertex shader uses any input locations not provided by
    // the input layout, we need to patch the shader.
    uint32_t vsInputMask = m_shaders.vs->info().inputMask;
    uint32_t ilAttributeMask = 0u;

    for (uint32_t i = 0; i < state.il.attributeCount(); i++)
      ilAttributeMask |= 1u << state.ilAttributes[i].location();

    if ((vsInputMask & ilAttributeMask) != vsInputMask)
      return false;

    if (m_shaders.gs != nullptr) {
      // If the geometry shader's input topology is not compatible with
      // the topology set to the pipeline, we need to patch the GS.
      VkPrimitiveTopology iaTopology = determinePreGsTopology(m_shaders, state);
      VkPrimitiveTopology gsTopology = m_shaders.gs->info().inputTopology;

      if (determineGsInputTopology(gsTopology, iaTopology) != gsTopology)
        return false;
    }

    if (m_shaders.tcs != nullptr) {
      // If tessellation shaders are present, the input patch
      // vertex count must match the shader's definition.
      if (m_shaders.tcs->info().patchVertexCount != state.ia.patchVertexCount())
        return false;
    }

    if (m_shaders.fs != nullptr) {
      // If the fragment shader has inputs not produced by the last
      // pre-rasterization stage, we need to patch the fragment shader
      uint32_t fsIoMask = m_shaders.fs->info().inputMask;
      uint32_t vsIoMask = m_shaders.vs->info().outputMask;

      if (m_shaders.gs != nullptr)
        vsIoMask = m_shaders.gs->info().outputMask;
      else if (m_shaders.tes != nullptr)
        vsIoMask = m_shaders.tes->info().outputMask;

      if ((vsIoMask & fsIoMask) != fsIoMask)
        return false;

      // Dual-source blending requires patching the fragment shader
      if (state.useDualSourceBlending())
        return false;

      // Flat shading requires patching the fragment shader
      if (state.rs.flatShading() && m_shaders.fs->info().flatShadingInputs)
        return false;

      // If dynamic multisample state is not supported and sample shading
      // is enabled, the library is compiled with a sample count of 1.
      if (m_shaders.fs->flags().test(DxvkShaderFlag::HasSampleRateShading)) {
        bool canUseDynamicMultisampleState =
          m_device->features().extExtendedDynamicState3.extendedDynamicState3RasterizationSamples &&
          m_device->features().extExtendedDynamicState3.extendedDynamicState3SampleMask;

        bool canUseDynamicAlphaToCoverage = canUseDynamicMultisampleState &&
          m_device->features().extExtendedDynamicState3.extendedDynamicState3AlphaToCoverageEnable;

        if (!canUseDynamicMultisampleState
         && (state.ms.sampleCount() != VK_SAMPLE_COUNT_1_BIT
          || state.ms.sampleMask() == 0))
          return false;

        if (!canUseDynamicAlphaToCoverage
         && (state.ms.enableAlphaToCoverage())
         && !m_shaders.fs->flags().test(DxvkShaderFlag::ExportsSampleMask))
          return false;
      }
    }

    // Remapping fragment shader outputs would require spec constants
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if ((m_fsOut & (1u << i)) && state.writesRenderTarget(i)
       && !util::isIdentityMapping(state.omSwizzle[i].mapping()))
        return false;
    }

    return true;
  }


  VkPipeline DxvkGraphicsPipeline::getBasePipeline(
    const DxvkGraphicsPipelineStateInfo& state) {
    DxvkGraphicsPipelineVertexInputState    viState(m_device, state, m_shaders);
    DxvkGraphicsPipelineFragmentOutputState foState(m_device, state, m_shaders);

    DxvkGraphicsPipelineBaseInstanceKey key;
    key.viLibrary = m_manager->createVertexInputLibrary(viState);
    key.foLibrary = m_manager->createFragmentOutputLibrary(foState);

    auto entry = m_basePipelines.find(key);
    if (entry != m_basePipelines.end())
      return entry->second;

    VkPipeline handle = createBasePipeline(key);
    m_basePipelines.insert({ key, handle });
    return handle;
  }


  VkPipeline DxvkGraphicsPipeline::createBasePipeline(
    const DxvkGraphicsPipelineBaseInstanceKey& key) const {
    auto vk = m_device->vkd();

    DxvkShaderPipelineLibraryHandle vs = m_vsLibrary->acquirePipelineHandle();
    DxvkShaderPipelineLibraryHandle fs = m_fsLibrary->acquirePipelineHandle();

    std::array<VkPipeline, 4> libraries = {{
      key.viLibrary->getHandle(), vs.handle, fs.handle,
      key.foLibrary->getHandle(),
    }};

    VkPipelineLibraryCreateInfoKHR libInfo = { VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR };
    libInfo.libraryCount    = libraries.size();
    libInfo.pLibraries      = libraries.data();

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
    info.flags              = vs.linkFlags | fs.linkFlags;
    info.layout             = m_bindings->getPipelineLayout(true);
    info.basePipelineIndex  = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr && vr != VK_PIPELINE_COMPILE_REQUIRED_EXT)
      Logger::err(str::format("DxvkGraphicsPipeline: Failed to create base pipeline: ", vr));

    return pipeline;
  }


  VkPipeline DxvkGraphicsPipeline::getOptimizedPipeline(
    const DxvkGraphicsPipelineStateInfo& state) {
    DxvkGraphicsPipelineFastInstanceKey key(m_device,
      m_shaders, state, m_flags, m_specConstantMask);

    std::lock_guard lock(m_fastMutex);

    auto entry = m_fastPipelines.find(key);
    if (entry != m_fastPipelines.end())
      return entry->second;

    // Keep pipeline locked to prevent multiple threads from compiling
    // identical Vulkan pipelines. This should be rare, but has been
    // buggy on some drivers in the past, so just don't allow it.
    VkPipeline handle = createOptimizedPipeline(key);

    if (handle)
      m_fastPipelines.insert({ key, handle });

    return handle;
  }


  VkPipeline DxvkGraphicsPipeline::createOptimizedPipeline(
    const DxvkGraphicsPipelineFastInstanceKey& key) const {
    auto vk = m_device->vkd();

    DxvkShaderStageInfo stageInfo(m_device);
    stageInfo.addStage(VK_SHADER_STAGE_VERTEX_BIT, getShaderCode(m_shaders.vs, key.shState.vsInfo), &key.scState.scInfo);

    if (m_shaders.tcs != nullptr)
      stageInfo.addStage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, getShaderCode(m_shaders.tcs, key.shState.tcsInfo), &key.scState.scInfo);
    if (m_shaders.tes != nullptr)
      stageInfo.addStage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, getShaderCode(m_shaders.tes, key.shState.tesInfo), &key.scState.scInfo);
    if (m_shaders.gs != nullptr)
      stageInfo.addStage(VK_SHADER_STAGE_GEOMETRY_BIT, getShaderCode(m_shaders.gs, key.shState.gsInfo), &key.scState.scInfo);
    if (m_shaders.fs != nullptr)
      stageInfo.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, getShaderCode(m_shaders.fs, key.shState.fsInfo), &key.scState.scInfo);

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &key.foState.rtInfo };
    info.stageCount               = stageInfo.getStageCount();
    info.pStages                  = stageInfo.getStageInfos();
    info.pVertexInputState        = &key.viState.viInfo;
    info.pInputAssemblyState      = &key.viState.iaInfo;
    info.pTessellationState       = &key.prState.tsInfo;
    info.pViewportState           = &key.prState.vpInfo;
    info.pRasterizationState      = &key.prState.rsInfo;
    info.pMultisampleState        = &key.foState.msInfo;
    info.pDepthStencilState       = &key.fsState.dsInfo;
    info.pColorBlendState         = &key.foState.cbInfo;
    info.pDynamicState            = &key.dyState.dyInfo;
    info.layout                   = m_bindings->getPipelineLayout(false);
    info.basePipelineIndex        = -1;
    
    if (!key.prState.tsInfo.patchControlPoints)
      info.pTessellationState = nullptr;
    
    if (key.foState.feedbackLoop & VK_IMAGE_ASPECT_COLOR_BIT)
      info.flags |= VK_PIPELINE_CREATE_COLOR_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;

    if (key.foState.feedbackLoop & VK_IMAGE_ASPECT_DEPTH_BIT)
      info.flags |= VK_PIPELINE_CREATE_DEPTH_STENCIL_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr != VK_SUCCESS) {
      Logger::err(str::format("DxvkGraphicsPipeline: Failed to compile pipeline: ", vr));
      return VK_NULL_HANDLE;
    }

    return pipeline;
  }
  
  
  void DxvkGraphicsPipeline::destroyBasePipelines() {
    for (const auto& instance : m_basePipelines) {
      this->destroyVulkanPipeline(instance.second);

      m_vsLibrary->releasePipelineHandle();
      m_fsLibrary->releasePipelineHandle();
    }

    m_basePipelines.clear();
  }


  void DxvkGraphicsPipeline::destroyOptimizedPipelines() {
    for (const auto& instance : m_fastPipelines)
      this->destroyVulkanPipeline(instance.second);

    m_fastPipelines.clear();
  }


  void DxvkGraphicsPipeline::destroyVulkanPipeline(VkPipeline pipeline) const {
    auto vk = m_device->vkd();

    vk->vkDestroyPipeline(vk->device(), pipeline, nullptr);
  }


  SpirvCodeBuffer DxvkGraphicsPipeline::getShaderCode(
    const Rc<DxvkShader>&                shader,
    const DxvkShaderModuleCreateInfo&    info) const {
    return shader->getCode(m_bindings, info);
  }


  uint32_t DxvkGraphicsPipeline::computeSpecConstantMask() const {
    uint32_t mask = m_shaders.vs->getSpecConstantMask();

    if (m_shaders.tcs != nullptr)
      mask |= m_shaders.tcs->getSpecConstantMask();
    if (m_shaders.tes != nullptr)
      mask |= m_shaders.tes->getSpecConstantMask();
    if (m_shaders.gs != nullptr)
      mask |= m_shaders.gs->getSpecConstantMask();
    if (m_shaders.fs != nullptr)
      mask |= m_shaders.fs->getSpecConstantMask();

    return mask;
  }


  DxvkAttachmentMask DxvkGraphicsPipeline::computeAttachmentMask(
    const DxvkGraphicsPipelineStateInfo& state) const {
    // Scan color attachments first, we only need to check if any given
    // attachment is accessed by the shader and has a non-zero write mask.
    DxvkAttachmentMask result = { };

    if (m_flags.test(DxvkGraphicsPipelineFlag::HasRasterizerDiscard))
      return result;

    if (m_shaders.fs) {
      uint32_t colorMask = m_shaders.fs->info().outputMask;

      for (auto i : bit::BitMask(colorMask)) {
        if (state.writesRenderTarget(i))
          result.trackColorWrite(i);
      }
    }

    // Check depth buffer access
    auto depthFormat = state.rt.getDepthStencilFormat();

    if (depthFormat) {
      auto dsReadable = lookupFormatInfo(depthFormat)->aspectMask;
      auto dsWritable = dsReadable & ~state.rt.getDepthStencilReadOnlyAspects();

      if (dsReadable & VK_IMAGE_ASPECT_DEPTH_BIT) {
        if (state.ds.enableDepthBoundsTest())
          result.trackDepthRead();

        if (state.ds.enableDepthTest()) {
          result.trackDepthRead();

          if (state.ds.enableDepthWrite() && (dsWritable & VK_IMAGE_ASPECT_DEPTH_BIT))
            result.trackDepthWrite();
        }
      }

      if (dsReadable & VK_IMAGE_ASPECT_STENCIL_BIT) {
        if (state.ds.enableStencilTest()) {
          auto f = state.dsFront.state(dsWritable & VK_IMAGE_ASPECT_STENCIL_BIT);
          auto b = state.dsBack.state(dsWritable & VK_IMAGE_ASPECT_STENCIL_BIT);

          result.trackStencilRead();

          if (f.writeMask | b.writeMask)
            result.trackStencilWrite();
        }
      }
    }

    return result;
  }


  bool DxvkGraphicsPipeline::validatePipelineState(
    const DxvkGraphicsPipelineStateInfo&  state,
          bool                            trusted) const {
    // Tessellation shaders and patches must be used together
    bool hasPatches = state.ia.primitiveTopology() == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

    bool hasTcs = m_shaders.tcs != nullptr;
    bool hasTes = m_shaders.tes != nullptr;

    if (hasPatches != hasTcs || hasPatches != hasTes)
      return false;
    
    // Filter out undefined primitive topologies
    if (state.ia.primitiveTopology() == VK_PRIMITIVE_TOPOLOGY_MAX_ENUM)
      return false;
    
    // Prevent unintended out-of-bounds access to the IL arrays
    if (state.il.attributeCount() > DxvkLimits::MaxNumVertexAttributes
     || state.il.bindingCount()   > DxvkLimits::MaxNumVertexBindings)
      return false;

    // Exit here on the fast path, perform more thorough validation if
    // the state vector comes from an untrusted source (i.e. the cache)
    if (trusted)
      return true;

    // Validate shaders
    if (!m_shaders.validate())
      return false;

    // Validate vertex input layout
    uint32_t ilLocationMask = 0;
    uint32_t ilBindingMask = 0;

    for (uint32_t i = 0; i < state.il.bindingCount(); i++)
      ilBindingMask |= 1u << state.ilBindings[i].binding();

    for (uint32_t i = 0; i < state.il.attributeCount(); i++) {
      const DxvkIlAttribute& attribute = state.ilAttributes[i];

      if (ilLocationMask & (1u << attribute.location()))
        return false;

      if (!(ilBindingMask & (1u << attribute.binding())))
        return false;

      DxvkFormatFeatures formatInfo = m_device->getFormatFeatures(attribute.format());

      if (!(formatInfo.buffer & VK_FORMAT_FEATURE_2_VERTEX_BUFFER_BIT))
        return false;

      ilLocationMask |= 1u << attribute.location();
    }

    // Validate rasterization state
    if (state.rs.conservativeMode() != VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT) {
      if (!m_device->features().extConservativeRasterization)
        return false;

      if (state.rs.conservativeMode() == VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT
       && !m_device->properties().extConservativeRasterization.primitiveUnderestimation)
        return false;
    }

    // Validate depth-stencil state
    if (state.ds.enableDepthBoundsTest() && !m_device->features().core.features.depthBounds)
      return false;

    // Validate render target format support
    VkFormat depthFormat = state.rt.getDepthStencilFormat();

    if (depthFormat) {
      DxvkFormatFeatures formatInfo = m_device->getFormatFeatures(depthFormat);

      if (!(formatInfo.optimal & VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT))
        return false;
    }

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      VkFormat colorFormat = state.rt.getColorFormat(i);

      if (colorFormat) {
        DxvkFormatFeatures formatInfo = m_device->getFormatFeatures(colorFormat);

        if (!(formatInfo.optimal & VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT))
          return false;
      }
    }

    // Validate spec constant state
    for (uint32_t i = 0; i < MaxNumSpecConstants; i++) {
      if (state.sc.specConstants[i] && !(m_specConstantMask & (1u << i)))
        return false;
    }

    return true;
  }
  
  
  void DxvkGraphicsPipeline::writePipelineStateToCache(
    const DxvkGraphicsPipelineStateInfo& state) const {
    DxvkStateCacheKey key;
    if (m_shaders.vs  != nullptr) key.vs = m_shaders.vs->getShaderKey();
    if (m_shaders.tcs != nullptr) key.tcs = m_shaders.tcs->getShaderKey();
    if (m_shaders.tes != nullptr) key.tes = m_shaders.tes->getShaderKey();
    if (m_shaders.gs  != nullptr) key.gs = m_shaders.gs->getShaderKey();
    if (m_shaders.fs  != nullptr) key.fs = m_shaders.fs->getShaderKey();

    m_stateCache->addGraphicsPipeline(key, state);
  }
  
  
  void DxvkGraphicsPipeline::logPipelineState(
          LogLevel                       level,
    const DxvkGraphicsPipelineStateInfo& state) const {
    std::stringstream sstr;
    sstr << "Shader stages:" << std::endl;
    if (m_shaders.vs  != nullptr) sstr << "  vs  : " << m_shaders.vs ->debugName() << std::endl;
    if (m_shaders.tcs != nullptr) sstr << "  tcs : " << m_shaders.tcs->debugName() << std::endl;
    if (m_shaders.tes != nullptr) sstr << "  tes : " << m_shaders.tes->debugName() << std::endl;
    if (m_shaders.gs  != nullptr) sstr << "  gs  : " << m_shaders.gs ->debugName() << std::endl;
    if (m_shaders.fs  != nullptr) sstr << "  fs  : " << m_shaders.fs ->debugName() << std::endl;

    // Log input assembly state
    VkPrimitiveTopology topology = state.ia.primitiveTopology();
    sstr << std::dec << "Primitive topology: " << topology;

    if (topology == VK_PRIMITIVE_TOPOLOGY_PATCH_LIST)
      sstr << " [" << state.ia.patchVertexCount() << "]" << std::endl;
    else
      sstr << " [restart: " << (state.ia.primitiveRestart() ? "yes]" : "no]") << std::endl;

    // Log vertex input state
    for (uint32_t i = 0; i < state.il.bindingCount(); i++) {
      const auto& binding = state.ilBindings[i];
      sstr << "Vertex binding " << binding.binding() << " [" << binding.stride() << "]" << std::endl;

      for (uint32_t j = 0; j < state.il.attributeCount(); j++) {
        const auto& attribute = state.ilAttributes[j];

        if (attribute.binding() == binding.binding())
          sstr << "  " << attribute.location() << " [" << attribute.offset() << "]: " << attribute.format() << std::endl;
      }
    }

    // Log rasterizer state
    sstr << "Rasterizer state:" << std::endl
         << "  depth clip:      " << (state.rs.depthClipEnable() ? "yes" : "no") << std::endl
         << "  depth bias:      " << (state.rs.depthBiasEnable() ? "yes" : "no") << std::endl
         << "  polygon mode:    " << state.rs.polygonMode() << std::endl
         << "  conservative:    " << (state.rs.conservativeMode() == VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT ? "no" : "yes") << std::endl;

    // Log multisample state
    VkSampleCountFlags sampleCount = VK_SAMPLE_COUNT_1_BIT;

    if (state.ms.sampleCount())
      sampleCount = state.ms.sampleCount();
    else if (state.rs.sampleCount())
      sampleCount = state.rs.sampleCount();

    sstr << "Sample count: " << sampleCount << " [0x" << std::hex << state.ms.sampleMask() << std::dec << "]" << std::endl
         << "  alphaToCoverage: " << (state.ms.enableAlphaToCoverage() ? "yes" : "no") << std::endl;

    // Log depth-stencil state
    sstr << "Depth test:        ";

    if (state.ds.enableDepthTest())
      sstr << "yes [write: " << (state.ds.enableDepthWrite() ? "yes" : "no") << ", op: " << state.ds.depthCompareOp() << "]" << std::endl;
    else
      sstr << "no" << std::endl;

    sstr << "Depth bounds test: " << (state.ds.enableDepthBoundsTest() ? "yes" : "no") << std::endl
         << "Stencil test:      " << (state.ds.enableStencilTest() ? "yes" : "no") << std::endl;

    if (state.ds.enableStencilTest()) {
      std::array<VkStencilOpState, 2> states = {{
        state.dsFront.state(true),
        state.dsBack.state(true),
      }};

      for (size_t i = 0; i < states.size(); i++) {
        sstr << std::hex << (i ? "  back:  " : "  front: ")
             << "[c=0x" << states[i].compareMask << ",w=0x" << states[i].writeMask << ",op=" << states[i].compareOp << "] "
             << "fail=" << states[i].failOp << ",pass=" << states[i].passOp << ",depthFail=" << states[i].depthFailOp << std::dec << std::endl;
      }
    }

    // Log logic op state
    sstr << "Logic op:          ";

    if (state.om.enableLogicOp())
      sstr << "yes [" << state.om.logicOp() << "]" << std::endl;
    else
      sstr << "no" << std::endl;

    // Log render target and blend state
    auto depthFormat = state.rt.getDepthStencilFormat();
    auto depthFormatInfo = lookupFormatInfo(depthFormat);

    VkImageAspectFlags writableAspects = depthFormat
      ? (depthFormatInfo->aspectMask & ~state.rt.getDepthStencilReadOnlyAspects())
      : 0u;

    sstr << "Depth attachment: " << depthFormat;

    if (depthFormat) {
      sstr << " ["
        << ((writableAspects & VK_IMAGE_ASPECT_DEPTH_BIT) ? "d" : " ")
        << ((writableAspects & VK_IMAGE_ASPECT_STENCIL_BIT) ? "s" : " ")
        << "]" << std::endl;
    } else {
      sstr << std::endl;
    }

    bool hasColorAttachments = false;

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      auto format = state.rt.getColorFormat(i);

      if (format) {
        if (!hasColorAttachments) {
          sstr << "Color attachments:" << std::endl;
          hasColorAttachments = true;
        }

        const char* components = "rgba";
        const auto& blend = state.omBlend[i];
        const auto& swizzle = state.omSwizzle[i];

        VkColorComponentFlags writeMask = blend.colorWriteMask();
        char r = (writeMask & (1u << swizzle.rIndex())) ? components[swizzle.rIndex()] : ' ';
        char g = (writeMask & (1u << swizzle.gIndex())) ? components[swizzle.gIndex()] : ' ';
        char b = (writeMask & (1u << swizzle.bIndex())) ? components[swizzle.bIndex()] : ' ';
        char a = (writeMask & (1u << swizzle.aIndex())) ? components[swizzle.aIndex()] : ' ';

        sstr << "  " << i << ": " << format << " [" << r << g << b << a << "] blend: ";

        if (blend.blendEnable())
          sstr << "yes (c:" << blend.srcColorBlendFactor() << "," << blend.dstColorBlendFactor() << "," << blend.colorBlendOp()
               <<     ";a:" << blend.srcAlphaBlendFactor() << "," << blend.dstAlphaBlendFactor() << "," << blend.alphaBlendOp() << ")" << std::endl;
        else
          sstr << "no" << std::endl;
      }
    }

    // Log spec constants
    bool hasSpecConstants = false;

    for (uint32_t i = 0; i < MaxNumSpecConstants; i++) {
      if (state.sc.specConstants[i]) {
        if (!hasSpecConstants) {
          sstr << "Specialization constants:" << std::endl;
          hasSpecConstants = true;
        }

        sstr << "  " << i << ": 0x" << std::hex << std::setw(8) << std::setfill('0') << state.sc.specConstants[i] << std::dec << std::endl;
      }
    }

    Logger::log(level, sstr.str());
  }


  std::string DxvkGraphicsPipeline::createDebugName() const {
    std::stringstream name;

    std::array<Rc<DxvkShader>, 5> shaders = {{
      m_shaders.vs,
      m_shaders.tcs,
      m_shaders.tes,
      m_shaders.gs,
      m_shaders.fs,
    }};

    for (const auto& shader : shaders) {
      if (shader) {
        std::string shaderName = shader->debugName();
        size_t len = std::min(shaderName.size(), size_t(10));
        name << "[" << shaderName.substr(0, len) << "] ";
      }
    }

    return name.str();
  }
  
}
