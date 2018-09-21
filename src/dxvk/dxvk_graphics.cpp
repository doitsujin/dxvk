#include <chrono>
#include <cstring>

#include "dxvk_device.h"
#include "dxvk_graphics.h"
#include "dxvk_pipemanager.h"
#include "dxvk_spec_const.h"
#include "dxvk_state_cache.h"

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
          DxvkPipelineManager*      pipeMgr,
    const Rc<DxvkShader>&           vs,
    const Rc<DxvkShader>&           tcs,
    const Rc<DxvkShader>&           tes,
    const Rc<DxvkShader>&           gs,
    const Rc<DxvkShader>&           fs)
  : m_vkd(pipeMgr->m_device->vkd()), m_pipeMgr(pipeMgr) {
    DxvkDescriptorSlotMapping slotMapping;
    if (vs  != nullptr) vs ->defineResourceSlots(slotMapping);
    if (tcs != nullptr) tcs->defineResourceSlots(slotMapping);
    if (tes != nullptr) tes->defineResourceSlots(slotMapping);
    if (gs  != nullptr) gs ->defineResourceSlots(slotMapping);
    if (fs  != nullptr) fs ->defineResourceSlots(slotMapping);
    
    slotMapping.makeDescriptorsDynamic(
      pipeMgr->m_device->options().maxNumDynamicUniformBuffers,
      pipeMgr->m_device->options().maxNumDynamicStorageBuffers);
    
    m_layout = new DxvkPipelineLayout(m_vkd,
      slotMapping.bindingCount(),
      slotMapping.bindingInfos(),
      VK_PIPELINE_BIND_POINT_GRAPHICS);
    
    if (vs  != nullptr) m_vs  = vs ->createShaderModule(m_vkd, slotMapping);
    if (tcs != nullptr) m_tcs = tcs->createShaderModule(m_vkd, slotMapping);
    if (tes != nullptr) m_tes = tes->createShaderModule(m_vkd, slotMapping);
    if (gs  != nullptr) m_gs  = gs ->createShaderModule(m_vkd, slotMapping);
    if (fs  != nullptr) m_fs  = fs ->createShaderModule(m_vkd, slotMapping);
    
    m_vsIn  = vs != nullptr ? vs->interfaceSlots().inputSlots  : 0;
    m_fsOut = fs != nullptr ? fs->interfaceSlots().outputSlots : 0;
    
    m_common.msSampleShadingEnable = fs != nullptr && fs->hasCapability(spv::CapabilitySampleRateShading);
    m_common.msSampleShadingFactor = 1.0f;
  }
  
  
  DxvkGraphicsPipeline::~DxvkGraphicsPipeline() {
    for (const auto& instance : m_pipelines)
      this->destroyPipeline(instance.pipeline());
  }
  
  
  VkPipeline DxvkGraphicsPipeline::getPipelineHandle(
    const DxvkGraphicsPipelineStateInfo& state,
    const DxvkRenderPass&                renderPass) {
    VkRenderPass renderPassHandle = renderPass.getDefaultHandle();

    { std::lock_guard<sync::Spinlock> lock(m_mutex);
      
      const DxvkGraphicsPipelineInstance* instance =
        this->findInstance(state, renderPassHandle);
      
      if (instance != nullptr)
        return instance->pipeline();
    }
    
    // If the pipeline state vector is invalid, don't try
    // to create a new pipeline, it won't work anyway.
    if (!this->validatePipelineState(state))
      return VK_NULL_HANDLE;
    
    // If no pipeline instance exists with the given state
    // vector, create a new one and add it to the list.
    VkPipeline newPipelineBase   = m_basePipeline.load();
    VkPipeline newPipelineHandle = VK_NULL_HANDLE;
    
    // FIXME for some reason, compiling the exact
    // same pipeline crashes inside driver code
    { std::lock_guard<sync::Spinlock> lock(m_mutex);
      newPipelineHandle = this->compilePipeline(
        state, renderPassHandle, newPipelineBase);
    }

    { std::lock_guard<sync::Spinlock> lock(m_mutex);
      
      // Discard the pipeline if another thread
      // was faster compiling the same pipeline
      const DxvkGraphicsPipelineInstance* instance =
        this->findInstance(state, renderPassHandle);
      
      if (instance != nullptr) {
        this->destroyPipeline(newPipelineHandle);
        return instance->pipeline();
      }
      
      // Add new pipeline to the set
      m_pipelines.emplace_back(state, renderPassHandle, newPipelineHandle);
      m_pipeMgr->m_numGraphicsPipelines += 1;
    }
    
    // Use the new pipeline as the base pipeline for derivative pipelines
    if (newPipelineBase == VK_NULL_HANDLE && newPipelineHandle != VK_NULL_HANDLE)
      m_basePipeline.compare_exchange_strong(newPipelineBase, newPipelineHandle);
    
    if (newPipelineHandle != VK_NULL_HANDLE)
      this->writePipelineStateToCache(state, renderPass.format());
    
    return newPipelineHandle;
  }
  
  
  const DxvkGraphicsPipelineInstance* DxvkGraphicsPipeline::findInstance(
    const DxvkGraphicsPipelineStateInfo& state,
          VkRenderPass                   renderPass) const {
    for (const auto& instance : m_pipelines) {
      if (instance.isCompatible(state, renderPass))
        return &instance;
    }
    
    return nullptr;
  }
  
  
  VkPipeline DxvkGraphicsPipeline::compilePipeline(
    const DxvkGraphicsPipelineStateInfo& state,
          VkRenderPass                   renderPass,
          VkPipeline                     baseHandle) const {
    if (Logger::logLevel() <= LogLevel::Debug) {
      Logger::debug("Compiling graphics pipeline...");
      this->logPipelineState(LogLevel::Debug, state);
    }
    
    std::array<VkDynamicState, 5> dynamicStates = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_DEPTH_BIAS,
      VK_DYNAMIC_STATE_BLEND_CONSTANTS,
      VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    };

    // Figure out the actual sample count to use
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

    if (state.msSampleCount)
      sampleCount = VkSampleCountFlagBits(state.msSampleCount);
    else if (state.rsSampleCount)
      sampleCount = VkSampleCountFlagBits(state.rsSampleCount);
    
    // Set up some specialization constants
    DxvkSpecConstantData specData;
    specData.rasterizerSampleCount = uint32_t(sampleCount);
    
    for (uint32_t i = 0; i < MaxNumActiveBindings; i++)
      specData.activeBindings[i] = state.bsBindingMask.isBound(i) ? VK_TRUE : VK_FALSE;
    
    VkSpecializationInfo specInfo;
    specInfo.mapEntryCount        = g_specConstantMap.mapEntryCount();
    specInfo.pMapEntries          = g_specConstantMap.mapEntryData();
    specInfo.dataSize             = sizeof(specData);
    specInfo.pData                = &specData;
    
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    
    if (m_vs  != nullptr) stages.push_back(m_vs->stageInfo(&specInfo));
    if (m_tcs != nullptr) stages.push_back(m_tcs->stageInfo(&specInfo));
    if (m_tes != nullptr) stages.push_back(m_tes->stageInfo(&specInfo));
    if (m_gs  != nullptr) stages.push_back(m_gs->stageInfo(&specInfo));
    if (m_fs  != nullptr) stages.push_back(m_fs->stageInfo(&specInfo));

    // Fix up color write masks using the component mappings
    std::array<VkPipelineColorBlendAttachmentState, MaxNumRenderTargets> omBlendAttachments;

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      omBlendAttachments[i] = state.omBlendAttachments[i];
      omBlendAttachments[i].colorWriteMask = util::remapComponentMask(
        state.omBlendAttachments[i].colorWriteMask,
        state.omComponentMapping[i]);
      
      specData.outputMappings[4 * i + 0] = util::getComponentIndex(state.omComponentMapping[i].r, 0);
      specData.outputMappings[4 * i + 1] = util::getComponentIndex(state.omComponentMapping[i].g, 1);
      specData.outputMappings[4 * i + 2] = util::getComponentIndex(state.omComponentMapping[i].b, 2);
      specData.outputMappings[4 * i + 3] = util::getComponentIndex(state.omComponentMapping[i].a, 3);
    }

    // Generate per-instance attribute divisors
    std::array<VkVertexInputBindingDivisorDescriptionEXT, MaxNumVertexBindings> viDivisorDesc;
    uint32_t                                                                    viDivisorCount = 0;
    
    for (uint32_t i = 0; i < state.ilBindingCount; i++) {
      if (state.ilBindings[i].inputRate == VK_VERTEX_INPUT_RATE_INSTANCE) {
        const uint32_t id = viDivisorCount++;
        
        viDivisorDesc[id].binding = state.ilBindings[i].binding;
        viDivisorDesc[id].divisor = state.ilDivisors[i];
      }
    }
    
    VkPipelineVertexInputDivisorStateCreateInfoEXT viDivisorInfo;
    viDivisorInfo.sType                     = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT;
    viDivisorInfo.pNext                     = nullptr;
    viDivisorInfo.vertexBindingDivisorCount = viDivisorCount;
    viDivisorInfo.pVertexBindingDivisors    = viDivisorDesc.data();
    
    VkPipelineVertexInputStateCreateInfo viInfo;
    viInfo.sType                            = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    viInfo.pNext                            = &viDivisorInfo;
    viInfo.flags                            = 0;
    viInfo.vertexBindingDescriptionCount    = state.ilBindingCount;
    viInfo.pVertexBindingDescriptions       = state.ilBindings;
    viInfo.vertexAttributeDescriptionCount  = state.ilAttributeCount;
    viInfo.pVertexAttributeDescriptions     = state.ilAttributes;
    
    if (viDivisorCount == 0)
      viInfo.pNext = viDivisorInfo.pNext;
    
    // TODO remove this once the extension is widely supported
    if (!m_pipeMgr->m_device->extensions().extVertexAttributeDivisor)
      viInfo.pNext = viDivisorInfo.pNext;
    
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
    
    VkPipelineRasterizationStateCreateInfo rsInfo;
    rsInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rsInfo.pNext                  = nullptr;
    rsInfo.flags                  = 0;
    rsInfo.depthClampEnable       = state.rsDepthClampEnable;
    rsInfo.rasterizerDiscardEnable= VK_FALSE;
    rsInfo.polygonMode            = state.rsPolygonMode;
    rsInfo.cullMode               = state.rsCullMode;
    rsInfo.frontFace              = state.rsFrontFace;
    rsInfo.depthBiasEnable        = state.rsDepthBiasEnable;
    rsInfo.depthBiasConstantFactor= 0.0f;
    rsInfo.depthBiasClamp         = 0.0f;
    rsInfo.depthBiasSlopeFactor   = 0.0f;
    rsInfo.lineWidth              = 1.0f;
    
    VkPipelineMultisampleStateCreateInfo msInfo;
    msInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msInfo.pNext                  = nullptr;
    msInfo.flags                  = 0;
    msInfo.rasterizationSamples   = sampleCount;
    msInfo.sampleShadingEnable    = m_common.msSampleShadingEnable;
    msInfo.minSampleShading       = m_common.msSampleShadingFactor;
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
    dsInfo.depthBoundsTestEnable  = VK_FALSE;
    dsInfo.stencilTestEnable      = state.dsEnableStencilTest;
    dsInfo.front                  = state.dsStencilOpFront;
    dsInfo.back                   = state.dsStencilOpBack;
    dsInfo.minDepthBounds         = 0.0f;
    dsInfo.maxDepthBounds         = 1.0f;
    
    VkPipelineColorBlendStateCreateInfo cbInfo;
    cbInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbInfo.pNext                  = nullptr;
    cbInfo.flags                  = 0;
    cbInfo.logicOpEnable          = state.omEnableLogicOp;
    cbInfo.logicOp                = state.omLogicOp;
    cbInfo.attachmentCount        = DxvkLimits::MaxNumRenderTargets;
    cbInfo.pAttachments           = omBlendAttachments.data();
    
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
    info.flags                    = 0;
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
    info.renderPass               = renderPass;
    info.subpass                  = 0;
    info.basePipelineHandle       = baseHandle;
    info.basePipelineIndex        = -1;
    
    info.flags |= baseHandle == VK_NULL_HANDLE
      ? VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT
      : VK_PIPELINE_CREATE_DERIVATIVE_BIT;
    
    if (tsInfo.patchControlPoints == 0)
      info.pTessellationState = nullptr;
    
    // Time pipeline compilation for debugging purposes
    auto t0 = std::chrono::high_resolution_clock::now();
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (m_vkd->vkCreateGraphicsPipelines(m_vkd->device(),
          m_pipeMgr->m_cache->handle(), 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
      Logger::err("DxvkGraphicsPipeline: Failed to compile pipeline");
      this->logPipelineState(LogLevel::Error, state);
      return VK_NULL_HANDLE;
    }
    
    auto t1 = std::chrono::high_resolution_clock::now();
    auto td = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    Logger::debug(str::format("DxvkGraphicsPipeline: Finished in ", td.count(), " ms"));
    return pipeline;
  }
  
  
  void DxvkGraphicsPipeline::destroyPipeline(VkPipeline pipeline) const {
    m_vkd->vkDestroyPipeline(m_vkd->device(), pipeline, nullptr);
  }


  bool DxvkGraphicsPipeline::validatePipelineState(
    const DxvkGraphicsPipelineStateInfo& state) const {
    // Validate vertex input - each input slot consumed by the
    // vertex shader must be provided by the input layout.
    uint32_t providedVertexInputs = 0;
    
    for (uint32_t i = 0; i < state.ilAttributeCount; i++)
      providedVertexInputs |= 1u << state.ilAttributes[i].location;
    
    if ((providedVertexInputs & m_vsIn) != m_vsIn)
      return false;
    
    // If there are no tessellation shaders, we
    // obviously cannot use tessellation patches.
    if ((state.iaPatchVertexCount != 0) && (m_tcs == nullptr || m_tes == nullptr))
      return false;
    
    // No errors
    return true;
  }
  
  
  void DxvkGraphicsPipeline::writePipelineStateToCache(
    const DxvkGraphicsPipelineStateInfo& state,
    const DxvkRenderPassFormat&          format) const {
    if (m_pipeMgr->m_stateCache == nullptr)
      return;
    
    DxvkStateCacheKey key;
    if (m_vs  != nullptr) key.vs = m_vs->getShaderKey();
    if (m_tcs != nullptr) key.tcs = m_tcs->getShaderKey();
    if (m_tes != nullptr) key.tes = m_tes->getShaderKey();
    if (m_gs  != nullptr) key.gs = m_gs->getShaderKey();
    if (m_fs  != nullptr) key.fs = m_fs->getShaderKey();

    m_pipeMgr->m_stateCache->addGraphicsPipeline(key, state, format);
  }
  
  
  void DxvkGraphicsPipeline::logPipelineState(
          LogLevel                       level,
    const DxvkGraphicsPipelineStateInfo& state) const {
    if (m_vs  != nullptr) Logger::log(level, str::format("  vs  : ", m_vs ->shader()->debugName()));
    if (m_tcs != nullptr) Logger::log(level, str::format("  tcs : ", m_tcs->shader()->debugName()));
    if (m_tes != nullptr) Logger::log(level, str::format("  tes : ", m_tes->shader()->debugName()));
    if (m_gs  != nullptr) Logger::log(level, str::format("  gs  : ", m_gs ->shader()->debugName()));
    if (m_fs  != nullptr) Logger::log(level, str::format("  fs  : ", m_fs ->shader()->debugName()));
    
    // TODO log more pipeline state
  }
  
}