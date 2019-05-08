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
  : m_vkd(pipeMgr->m_device->vkd()), m_pipeMgr(pipeMgr),
    m_vs(vs), m_tcs(tcs), m_tes(tes), m_gs(gs), m_fs(fs) {
    if (vs  != nullptr) vs ->defineResourceSlots(m_slotMapping);
    if (tcs != nullptr) tcs->defineResourceSlots(m_slotMapping);
    if (tes != nullptr) tes->defineResourceSlots(m_slotMapping);
    if (gs  != nullptr) gs ->defineResourceSlots(m_slotMapping);
    if (fs  != nullptr) fs ->defineResourceSlots(m_slotMapping);
    
    m_slotMapping.makeDescriptorsDynamic(
      pipeMgr->m_device->options().maxNumDynamicUniformBuffers,
      pipeMgr->m_device->options().maxNumDynamicStorageBuffers);
    
    m_layout = new DxvkPipelineLayout(m_vkd,
      m_slotMapping, VK_PIPELINE_BIND_POINT_GRAPHICS);
    
    m_vsIn  = vs != nullptr ? vs->interfaceSlots().inputSlots  : 0;
    m_fsOut = fs != nullptr ? fs->interfaceSlots().outputSlots : 0;

    if (gs != nullptr && gs->hasCapability(spv::CapabilityTransformFeedback))
      m_flags.set(DxvkGraphicsPipelineFlag::HasTransformFeedback);
    
    VkShaderStageFlags stoStages = m_layout->getStorageDescriptorStages();

    if (stoStages & VK_SHADER_STAGE_FRAGMENT_BIT)
      m_flags.set(DxvkGraphicsPipelineFlag::HasFsStorageDescriptors);
    
    if (stoStages & ~VK_SHADER_STAGE_FRAGMENT_BIT)
      m_flags.set(DxvkGraphicsPipelineFlag::HasVsStorageDescriptors);
    
    m_common.msSampleShadingEnable = fs != nullptr && fs->hasCapability(spv::CapabilitySampleRateShading);
    m_common.msSampleShadingFactor = 1.0f;
  }
  
  
  DxvkGraphicsPipeline::~DxvkGraphicsPipeline() {
    for (const auto& instance : m_pipelines)
      this->destroyPipeline(instance.pipeline());
  }
  
  
  Rc<DxvkShader> DxvkGraphicsPipeline::getShader(
          VkShaderStageFlagBits             stage) const {
    switch (stage) {
      case VK_SHADER_STAGE_VERTEX_BIT:                  return m_vs;
      case VK_SHADER_STAGE_GEOMETRY_BIT:                return m_gs;
      case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    return m_tcs;
      case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return m_tes;
      case VK_SHADER_STAGE_FRAGMENT_BIT:                return m_fs;
      default:
        return nullptr;
    }
  }


  VkPipeline DxvkGraphicsPipeline::getPipelineHandle(
    const DxvkGraphicsPipelineStateInfo& state,
    const DxvkRenderPass&                renderPass) {
    VkRenderPass renderPassHandle = renderPass.getDefaultHandle();
    
    VkPipeline newPipelineHandle = VK_NULL_HANDLE;

    { std::lock_guard<sync::Spinlock> lock(m_mutex);
    
      auto instance = this->findInstance(state, renderPassHandle);
      
      if (instance != nullptr)
        return instance->pipeline();
    
      // If the pipeline state vector is invalid, don't try
      // to create a new pipeline, it won't work anyway.
      if (!this->validatePipelineState(state))
        return VK_NULL_HANDLE;
      
      // If no pipeline instance exists with the given state
      // vector, create a new one and add it to the list.
      newPipelineHandle = this->compilePipeline(state, renderPass, m_basePipeline);

      // Add new pipeline to the set
      m_pipelines.emplace_back(state, renderPassHandle, newPipelineHandle);
      m_pipeMgr->m_numGraphicsPipelines += 1;
      
      if (!m_basePipeline && newPipelineHandle)
        m_basePipeline = newPipelineHandle;
    }
    
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
    const DxvkRenderPass&                renderPass,
          VkPipeline                     baseHandle) const {
    if (Logger::logLevel() <= LogLevel::Debug) {
      Logger::debug("Compiling graphics pipeline...");
      this->logPipelineState(LogLevel::Debug, state);
    }

    // Render pass format and image layouts
    DxvkRenderPassFormat passFormat = renderPass.format();
    
    // Set up dynamic states as needed
    std::array<VkDynamicState, 6> dynamicStates;
    uint32_t                      dynamicStateCount = 0;
    
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;

    if (state.useDynamicDepthBias())
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BIAS;
    
    if (state.useDynamicDepthBounds())
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;
    
    if (state.useDynamicBlendConstants())
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_BLEND_CONSTANTS;
    
    if (state.useDynamicStencilRef())
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;

    // Figure out the actual sample count to use
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

    if (state.msSampleCount)
      sampleCount = VkSampleCountFlagBits(state.msSampleCount);
    else if (state.rsSampleCount)
      sampleCount = VkSampleCountFlagBits(state.rsSampleCount);
    
    // Set up some specialization constants
    DxvkSpecConstants specData;
    specData.set(uint32_t(DxvkSpecConstantId::RasterizerSampleCount), sampleCount, VK_SAMPLE_COUNT_1_BIT);
    
    for (uint32_t i = 0; i < m_layout->bindingCount(); i++)
      specData.set(i, state.bsBindingMask.isBound(i), true);
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if ((m_fsOut & (1 << i)) != 0) {
        uint32_t specId = uint32_t(DxvkSpecConstantId::ColorComponentMappings) + 4 * i;
        specData.set(specId + 0, util::getComponentIndex(state.omComponentMapping[i].r, 0), 0u);
        specData.set(specId + 1, util::getComponentIndex(state.omComponentMapping[i].g, 1), 1u);
        specData.set(specId + 2, util::getComponentIndex(state.omComponentMapping[i].b, 2), 2u);
        specData.set(specId + 3, util::getComponentIndex(state.omComponentMapping[i].a, 3), 3u);
      }
    }

    for (uint32_t i = 0; i < MaxNumSpecConstants; i++)
      specData.set(getSpecId(i), state.scSpecConstants[i], 0u);
    
    VkSpecializationInfo specInfo = specData.getSpecInfo();
    
    DxvkShaderModuleCreateInfo moduleInfo;
    moduleInfo.fsDualSrcBlend = state.omBlendAttachments[0].blendEnable && (
      util::isDualSourceBlendFactor(state.omBlendAttachments[0].srcColorBlendFactor) ||
      util::isDualSourceBlendFactor(state.omBlendAttachments[0].dstColorBlendFactor) ||
      util::isDualSourceBlendFactor(state.omBlendAttachments[0].srcAlphaBlendFactor) ||
      util::isDualSourceBlendFactor(state.omBlendAttachments[0].dstAlphaBlendFactor));
    
    auto vsm  = createShaderModule(m_vs,  moduleInfo);
    auto gsm  = createShaderModule(m_gs,  moduleInfo);
    auto tcsm = createShaderModule(m_tcs, moduleInfo);
    auto tesm = createShaderModule(m_tes, moduleInfo);
    auto fsm  = createShaderModule(m_fs,  moduleInfo);

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    if (vsm)  stages.push_back(vsm.stageInfo(&specInfo));
    if (tcsm) stages.push_back(tcsm.stageInfo(&specInfo));
    if (tesm) stages.push_back(tesm.stageInfo(&specInfo));
    if (gsm)  stages.push_back(gsm.stageInfo(&specInfo));
    if (fsm)  stages.push_back(fsm.stageInfo(&specInfo));

    // Fix up color write masks using the component mappings
    std::array<VkPipelineColorBlendAttachmentState, MaxNumRenderTargets> omBlendAttachments;

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      omBlendAttachments[i] = state.omBlendAttachments[i];
      omBlendAttachments[i].colorWriteMask = util::remapComponentMask(
        state.omBlendAttachments[i].colorWriteMask,
        state.omComponentMapping[i]);
      
      if ((m_fsOut & (1 << i)) == 0)
        omBlendAttachments[i].colorWriteMask = 0;
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

    int32_t rasterizedStream = m_gs != nullptr
      ? m_gs->shaderOptions().rasterizedStream
      : 0;
    
    // Compact vertex bindings so that we can more easily update vertex buffers
    std::array<VkVertexInputAttributeDescription, MaxNumVertexAttributes> viAttribs;
    std::array<VkVertexInputBindingDescription,   MaxNumVertexBindings>   viBindings;
    std::array<uint32_t,                          MaxNumVertexBindings>   viBindingMap = { };

    for (uint32_t i = 0; i < state.ilBindingCount; i++) {
      viBindings[i] = state.ilBindings[i];
      viBindings[i].binding = i;
      viBindingMap[state.ilBindings[i].binding] = i;
    }

    for (uint32_t i = 0; i < state.ilAttributeCount; i++) {
      viAttribs[i] = state.ilAttributes[i];
      viAttribs[i].binding = viBindingMap[state.ilAttributes[i].binding];
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
    viInfo.pVertexBindingDescriptions       = viBindings.data();
    viInfo.vertexAttributeDescriptionCount  = state.ilAttributeCount;
    viInfo.pVertexAttributeDescriptions     = viAttribs.data();
    
    if (viDivisorCount == 0)
      viInfo.pNext = viDivisorInfo.pNext;
    
    // TODO remove this once the extension is widely supported
    if (!m_pipeMgr->m_device->features().extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor)
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
    
    VkPipelineRasterizationStateStreamCreateInfoEXT xfbStreamInfo;
    xfbStreamInfo.sType           = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT;
    xfbStreamInfo.pNext           = nullptr;
    xfbStreamInfo.flags           = 0;
    xfbStreamInfo.rasterizationStream = uint32_t(rasterizedStream);

    VkPipelineRasterizationDepthClipStateCreateInfoEXT rsDepthClipInfo;
    rsDepthClipInfo.sType         = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT;
    rsDepthClipInfo.pNext         = nullptr;
    rsDepthClipInfo.flags         = 0;
    rsDepthClipInfo.depthClipEnable = state.rsDepthClipEnable;

    VkPipelineRasterizationStateCreateInfo rsInfo;
    rsInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rsInfo.pNext                  = &rsDepthClipInfo;
    rsInfo.flags                  = 0;
    rsInfo.depthClampEnable       = VK_TRUE;
    rsInfo.rasterizerDiscardEnable = rasterizedStream < 0;
    rsInfo.polygonMode            = state.rsPolygonMode;
    rsInfo.cullMode               = state.rsCullMode;
    rsInfo.frontFace              = state.rsFrontFace;
    rsInfo.depthBiasEnable        = state.rsDepthBiasEnable;
    rsInfo.depthBiasConstantFactor= 0.0f;
    rsInfo.depthBiasClamp         = 0.0f;
    rsInfo.depthBiasSlopeFactor   = 0.0f;
    rsInfo.lineWidth              = 1.0f;
    
    if (rasterizedStream > 0)
      rsDepthClipInfo.pNext = &xfbStreamInfo;
    
    if (!m_pipeMgr->m_device->features().extDepthClipEnable.depthClipEnable) {
      rsInfo.pNext                = rsDepthClipInfo.pNext;
      rsInfo.depthClampEnable     = !state.rsDepthClipEnable;
    }

    VkPipelineMultisampleStateCreateInfo msInfo;
    msInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msInfo.pNext                  = nullptr;
    msInfo.flags                  = 0;
    msInfo.rasterizationSamples   = sampleCount;
    msInfo.sampleShadingEnable    = m_common.msSampleShadingEnable;
    msInfo.minSampleShading       = m_common.msSampleShadingFactor;
    msInfo.pSampleMask            = &state.msSampleMask;
    msInfo.alphaToCoverageEnable  = state.msEnableAlphaToCoverage;
    msInfo.alphaToOneEnable       = VK_FALSE;
    
    VkPipelineDepthStencilStateCreateInfo dsInfo;
    dsInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsInfo.pNext                  = nullptr;
    dsInfo.flags                  = 0;
    dsInfo.depthTestEnable        = state.dsEnableDepthTest;
    dsInfo.depthWriteEnable       = state.dsEnableDepthWrite && !util::isDepthReadOnlyLayout(passFormat.depth.layout);
    dsInfo.depthCompareOp         = state.dsDepthCompareOp;
    dsInfo.depthBoundsTestEnable  = state.dsEnableDepthBoundsTest;
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
    dyInfo.dynamicStateCount      = dynamicStateCount;
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
    info.renderPass               = renderPass.getDefaultHandle();
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


  DxvkShaderModule DxvkGraphicsPipeline::createShaderModule(
    const Rc<DxvkShader>&                shader,
    const DxvkShaderModuleCreateInfo&    info) const {
    return shader != nullptr
      ? shader->createShaderModule(m_vkd, m_slotMapping, info)
      : DxvkShaderModule();
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
    
    // Prevent unintended out-of-bounds access to the IL arrays
    if (state.ilAttributeCount > DxvkLimits::MaxNumVertexAttributes
     || state.ilBindingCount   > DxvkLimits::MaxNumVertexBindings)
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
    if (m_vs  != nullptr) Logger::log(level, str::format("  vs  : ", m_vs ->debugName()));
    if (m_tcs != nullptr) Logger::log(level, str::format("  tcs : ", m_tcs->debugName()));
    if (m_tes != nullptr) Logger::log(level, str::format("  tes : ", m_tes->debugName()));
    if (m_gs  != nullptr) Logger::log(level, str::format("  gs  : ", m_gs ->debugName()));
    if (m_fs  != nullptr) Logger::log(level, str::format("  fs  : ", m_fs ->debugName()));

    for (uint32_t i = 0; i < state.ilAttributeCount; i++) {
      const VkVertexInputAttributeDescription& attr = state.ilAttributes[i];
      Logger::log(level, str::format("  attr ", i, " : location ", attr.location, ", binding ", attr.binding, ", format ", attr.format, ", offset ", attr.offset));
    }
    for (uint32_t i = 0; i < state.ilBindingCount; i++) {
      const VkVertexInputBindingDescription& bind = state.ilBindings[i];
      Logger::log(level, str::format("  binding ", i, " : binding ", bind.binding, ", stride ", bind.stride, ", rate ", bind.inputRate, ", divisor ", state.ilDivisors[i]));
    }
    
    // TODO log more pipeline state
  }
  
}
