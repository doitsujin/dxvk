#include <cstring>

#include "dxvk_device.h"
#include "dxvk_graphics.h"

namespace dxvk {
  
  DxvkGraphicsPipelineStateInfo::DxvkGraphicsPipelineStateInfo() {
    std::memset(this, 0, sizeof(DxvkGraphicsPipelineStateInfo));
  }
  
  
  DxvkGraphicsPipelineStateInfo::DxvkGraphicsPipelineStateInfo(
    const DxvkGraphicsPipelineStateInfo& other) {
    std::memcpy(this, &other, sizeof(DxvkGraphicsPipelineStateInfo));
  }
  
  
  DxvkGraphicsPipelineStateInfo& DxvkGraphicsPipelineStateInfo::operator = (
    const DxvkGraphicsPipelineStateInfo& other) {
    std::memcpy(this, &other, sizeof(DxvkGraphicsPipelineStateInfo));
    return *this;
  }
  
  
  bool DxvkGraphicsPipelineStateInfo::operator == (const DxvkGraphicsPipelineStateInfo& other) const {
    return std::memcmp(this, &other, sizeof(DxvkGraphicsPipelineStateInfo)) == 0;
  }
  
  
  bool DxvkGraphicsPipelineStateInfo::operator != (const DxvkGraphicsPipelineStateInfo& other) const {
    return std::memcmp(this, &other, sizeof(DxvkGraphicsPipelineStateInfo)) != 0;
  }
  
  
  DxvkGraphicsPipeline::DxvkGraphicsPipeline(
    const DxvkDevice*             device,
    const Rc<DxvkPipelineCache>&  cache,
    const Rc<DxvkShader>&         vs,
    const Rc<DxvkShader>&         tcs,
    const Rc<DxvkShader>&         tes,
    const Rc<DxvkShader>&         gs,
    const Rc<DxvkShader>&         fs)
  : m_device(device), m_vkd(device->vkd()),
    m_cache(cache) {
    DxvkDescriptorSlotMapping slotMapping;
    if (vs  != nullptr) vs ->defineResourceSlots(slotMapping);
    if (tcs != nullptr) tcs->defineResourceSlots(slotMapping);
    if (tes != nullptr) tes->defineResourceSlots(slotMapping);
    if (gs  != nullptr) gs ->defineResourceSlots(slotMapping);
    if (fs  != nullptr) fs ->defineResourceSlots(slotMapping);
    
    m_layout = new DxvkPipelineLayout(m_vkd,
      slotMapping.bindingCount(),
      slotMapping.bindingInfos());
    
    if (vs  != nullptr) m_vs  = vs ->createShaderModule(m_vkd, slotMapping);
    if (tcs != nullptr) m_tcs = tcs->createShaderModule(m_vkd, slotMapping);
    if (tes != nullptr) m_tes = tes->createShaderModule(m_vkd, slotMapping);
    if (gs  != nullptr) m_gs  = gs ->createShaderModule(m_vkd, slotMapping);
    if (fs  != nullptr) m_fs  = fs ->createShaderModule(m_vkd, slotMapping);
    
    m_vsIn  = vs != nullptr ? vs->interfaceSlots().inputSlots  : 0;
    m_fsOut = fs != nullptr ? fs->interfaceSlots().outputSlots : 0;
  }
  
  
  DxvkGraphicsPipeline::~DxvkGraphicsPipeline() {
    this->destroyPipelines();
  }
  
  
  VkPipeline DxvkGraphicsPipeline::getPipelineHandle(
    const DxvkGraphicsPipelineStateInfo& state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const PipelineStruct& pair : m_pipelines) {
      if (pair.stateVector == state)
        return pair.pipeline;
    }
    
    VkPipeline pipeline = this->validatePipelineState(state)
      ? this->compilePipeline(state, m_basePipeline)
      : VK_NULL_HANDLE;
    
    m_pipelines.push_back({ state, pipeline });
    
    if (m_basePipeline == VK_NULL_HANDLE)
      m_basePipeline = pipeline;
    return pipeline;
  }
  
  
  VkPipeline DxvkGraphicsPipeline::compilePipeline(
    const DxvkGraphicsPipelineStateInfo& state,
          VkPipeline                     baseHandle) const {
    if (Logger::logLevel() <= LogLevel::Debug)
      this->logPipelineState(state);
    
    std::array<VkDynamicState, 4> dynamicStates = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_BLEND_CONSTANTS,
      VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    };
    
    std::array<VkBool32,                 MaxNumActiveBindings> specData;
    std::array<VkSpecializationMapEntry, MaxNumActiveBindings> specMap;
    
    for (uint32_t i = 0; i < MaxNumActiveBindings; i++) {
      specData[i] = state.bsBindingState.isBound(i) ? VK_TRUE : VK_FALSE;
      specMap [i] = { i, static_cast<uint32_t>(sizeof(VkBool32)) * i, sizeof(VkBool32) };
    }
    
    VkSpecializationInfo specInfo;
    specInfo.mapEntryCount        = specMap.size();
    specInfo.pMapEntries          = specMap.data();
    specInfo.dataSize             = specData.size() * sizeof(VkBool32);
    specInfo.pData                = specData.data();
    
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    
    if (m_vs  != nullptr) stages.push_back(m_vs->stageInfo(&specInfo));
    if (m_tcs != nullptr) stages.push_back(m_tcs->stageInfo(&specInfo));
    if (m_tes != nullptr) stages.push_back(m_tes->stageInfo(&specInfo));
    if (m_gs  != nullptr) stages.push_back(m_gs->stageInfo(&specInfo));
    if (m_fs  != nullptr) stages.push_back(m_fs->stageInfo(&specInfo));
    
    VkPipelineVertexInputStateCreateInfo viInfo;
    viInfo.sType                            = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    viInfo.pNext                            = nullptr;
    viInfo.flags                            = 0;
    viInfo.vertexBindingDescriptionCount    = state.ilBindingCount;
    viInfo.pVertexBindingDescriptions       = state.ilBindings;
    viInfo.vertexAttributeDescriptionCount  = state.ilAttributeCount;
    viInfo.pVertexAttributeDescriptions     = state.ilAttributes;
    
    VkPipelineInputAssemblyStateCreateInfo iaInfo;
    iaInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaInfo.pNext                  = nullptr;
    iaInfo.flags                  = 0;
    iaInfo.topology               = state.iaPrimitiveTopology;
    iaInfo.primitiveRestartEnable = state.iaPrimitiveRestart;
    
    VkPipelineTessellationStateCreateInfo tsInfo;
    tsInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tsInfo.pNext                  = nullptr;
    tsInfo.flags                  = 0;
    tsInfo.patchControlPoints     = state.iaPatchVertexCount;
    
    VkPipelineViewportStateCreateInfo vpInfo;
    vpInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpInfo.pNext                  = nullptr;
    vpInfo.flags                  = 0;
    vpInfo.viewportCount          = state.rsViewportCount;
    vpInfo.pViewports             = nullptr;
    vpInfo.scissorCount           = state.rsViewportCount;
    vpInfo.pScissors              = nullptr;
    
    VkPipelineRasterizationStateRasterizationOrderAMD rsOrder;
    rsOrder.sType                 = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD;
    rsOrder.pNext                 = nullptr;
    rsOrder.rasterizationOrder    = this->pickRasterizationOrder(state);
    
    VkPipelineRasterizationStateCreateInfo rsInfo;
    rsInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rsInfo.pNext                  = m_device->extensions().amdRasterizationOrder.enabled() ? &rsOrder : rsOrder.pNext;
    rsInfo.flags                  = 0;
    rsInfo.depthClampEnable       = state.rsEnableDepthClamp;
    rsInfo.rasterizerDiscardEnable= state.rsEnableDiscard;
    rsInfo.polygonMode            = state.rsPolygonMode;
    rsInfo.cullMode               = state.rsCullMode;
    rsInfo.frontFace              = state.rsFrontFace;
    rsInfo.depthBiasEnable        = state.rsDepthBiasEnable;
    rsInfo.depthBiasConstantFactor= state.rsDepthBiasConstant;
    rsInfo.depthBiasClamp         = state.rsDepthBiasClamp;
    rsInfo.depthBiasSlopeFactor   = state.rsDepthBiasSlope;
    rsInfo.lineWidth              = 1.0f;
    
    VkPipelineMultisampleStateCreateInfo msInfo;
    msInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msInfo.pNext                  = nullptr;
    msInfo.flags                  = 0;
    msInfo.rasterizationSamples   = state.msSampleCount;
    msInfo.sampleShadingEnable    = state.msEnableSampleShading;
    msInfo.minSampleShading       = state.msMinSampleShading;
    msInfo.pSampleMask            = &state.msSampleMask;
    msInfo.alphaToCoverageEnable  = state.msEnableAlphaToCoverage;
    msInfo.alphaToOneEnable       = state.msEnableAlphaToOne;
    
    VkPipelineDepthStencilStateCreateInfo dsInfo;
    dsInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsInfo.pNext                  = nullptr;
    dsInfo.flags                  = 0;
    dsInfo.depthTestEnable        = state.dsEnableDepthTest;
    dsInfo.depthWriteEnable       = state.dsEnableDepthWrite;
    dsInfo.depthCompareOp         = state.dsDepthCompareOp;
    dsInfo.depthBoundsTestEnable  = state.dsEnableDepthBounds;
    dsInfo.stencilTestEnable      = state.dsEnableStencilTest;
    dsInfo.front                  = state.dsStencilOpFront;
    dsInfo.back                   = state.dsStencilOpBack;
    dsInfo.minDepthBounds         = state.dsDepthBoundsMin;
    dsInfo.maxDepthBounds         = state.dsDepthBoundsMax;
    
    VkPipelineColorBlendStateCreateInfo cbInfo;
    cbInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbInfo.pNext                  = nullptr;
    cbInfo.flags                  = 0;
    cbInfo.logicOpEnable          = state.omEnableLogicOp;
    cbInfo.logicOp                = state.omLogicOp;
    cbInfo.attachmentCount        = DxvkLimits::MaxNumRenderTargets;
    cbInfo.pAttachments           = state.omBlendAttachments;
    
    for (uint32_t i = 0; i < 4; i++)
      cbInfo.blendConstants[i] = 0.0f;
    
    VkPipelineDynamicStateCreateInfo dyInfo;
    dyInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyInfo.pNext                  = nullptr;
    dyInfo.flags                  = 0;
    dyInfo.dynamicStateCount      = dynamicStates.size();
    dyInfo.pDynamicStates         = dynamicStates.data();
    
    VkGraphicsPipelineCreateInfo info;
    info.sType                    = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext                    = nullptr;
    info.flags                    = baseHandle == VK_NULL_HANDLE
      ? VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT
      : VK_PIPELINE_CREATE_DERIVATIVE_BIT;
    info.stageCount               = stages.size();
    info.pStages                  = stages.data();
    info.pVertexInputState        = &viInfo;
    info.pInputAssemblyState      = &iaInfo;
    info.pTessellationState       = &tsInfo;
    info.pViewportState           = &vpInfo;
    info.pRasterizationState      = &rsInfo;
    info.pMultisampleState        = &msInfo;
    info.pDepthStencilState       = &dsInfo;
    info.pColorBlendState         = &cbInfo;
    info.pDynamicState            = &dyInfo;
    info.layout                   = m_layout->pipelineLayout();
    info.renderPass               = state.omRenderPass;
    info.subpass                  = 0;
    info.basePipelineHandle       = baseHandle;
    info.basePipelineIndex        = -1;
    
    if (tsInfo.patchControlPoints == 0)
      info.pTessellationState = nullptr;
    
    if ((tsInfo.patchControlPoints != 0) && (m_tcs == nullptr || m_tes == nullptr)) {
      Logger::err("DxvkGraphicsPipeline: Cannot use tessellation patches without tessellation shaders");
      return VK_NULL_HANDLE;
    }
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (m_vkd->vkCreateGraphicsPipelines(m_vkd->device(),
          m_cache->handle(), 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
      Logger::err("DxvkGraphicsPipeline: Failed to compile pipeline");
      return VK_NULL_HANDLE;
    }
    
    return pipeline;
  }
  
  
  void DxvkGraphicsPipeline::destroyPipelines() {
    for (const PipelineStruct& pair : m_pipelines)
      m_vkd->vkDestroyPipeline(m_vkd->device(), pair.pipeline, nullptr);
  }
  
  
  bool DxvkGraphicsPipeline::validatePipelineState(
    const DxvkGraphicsPipelineStateInfo& state) const {
    // Validate vertex input - each input slot consumed by the
    // vertex shader must be provided by the input layout.
    uint32_t providedVertexInputs = 0;
    
    for (uint32_t i = 0; i < state.ilAttributeCount; i++)
      providedVertexInputs |= 1u << state.ilAttributes[i].location;
    
    if ((providedVertexInputs & m_vsIn) != m_vsIn) {
      Logger::err("DxvkGraphicsPipeline: Input layout mismatches vertex shader input");
      return false;
    }
    
    // No errors
    return true;
  }
  
  
  VkRasterizationOrderAMD DxvkGraphicsPipeline::pickRasterizationOrder(
    const DxvkGraphicsPipelineStateInfo& state) const {
    // If blending is not enabled, we can enable out-of-order
    // rasterization for certain depth-compare modes.
    bool blendingEnabled = false;
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_fsOut & (1u << i))
        blendingEnabled |= state.omBlendAttachments[i].blendEnable;
    }
    
    if (!blendingEnabled) {
      if (m_device->hasOption(DxvkOption::AssumeNoZfight))
        return VK_RASTERIZATION_ORDER_RELAXED_AMD;
      
      if (state.dsEnableDepthTest && state.dsEnableDepthWrite
       && (state.dsDepthCompareOp == VK_COMPARE_OP_LESS
        || state.dsDepthCompareOp == VK_COMPARE_OP_GREATER))
        return VK_RASTERIZATION_ORDER_RELAXED_AMD;
    }
    
    return VK_RASTERIZATION_ORDER_STRICT_AMD;
  }
  
  
  void DxvkGraphicsPipeline::logPipelineState(
    const DxvkGraphicsPipelineStateInfo& state) const {
    Logger::debug("Compiling graphics pipeline...");
    
    if (m_vs  != nullptr) Logger::debug(str::format("  vs  : ", m_vs ->debugName()));
    if (m_tcs != nullptr) Logger::debug(str::format("  tcs : ", m_tcs->debugName()));
    if (m_tes != nullptr) Logger::debug(str::format("  tes : ", m_tes->debugName()));
    if (m_gs  != nullptr) Logger::debug(str::format("  gs  : ", m_gs ->debugName()));
    if (m_fs  != nullptr) Logger::debug(str::format("  fs  : ", m_fs ->debugName()));
    
    // TODO log more pipeline state
  }
  
}