#include "../util/util_time.h"

#include "dxvk_device.h"
#include "dxvk_graphics.h"
#include "dxvk_pipemanager.h"
#include "dxvk_spec_const.h"
#include "dxvk_state_cache.h"

namespace dxvk {

  DxvkGraphicsPipeline::DxvkGraphicsPipeline(
          DxvkPipelineManager*        pipeMgr,
          DxvkGraphicsPipelineShaders shaders)
  : m_vkd(pipeMgr->m_device->vkd()), m_pipeMgr(pipeMgr),
    m_shaders(std::move(shaders)) {
    if (m_shaders.vs  != nullptr) m_shaders.vs ->defineResourceSlots(m_slotMapping);
    if (m_shaders.tcs != nullptr) m_shaders.tcs->defineResourceSlots(m_slotMapping);
    if (m_shaders.tes != nullptr) m_shaders.tes->defineResourceSlots(m_slotMapping);
    if (m_shaders.gs  != nullptr) m_shaders.gs ->defineResourceSlots(m_slotMapping);
    if (m_shaders.fs  != nullptr) m_shaders.fs ->defineResourceSlots(m_slotMapping);
    
    m_slotMapping.makeDescriptorsDynamic(
      pipeMgr->m_device->options().maxNumDynamicUniformBuffers,
      pipeMgr->m_device->options().maxNumDynamicStorageBuffers);
    
    m_layout = new DxvkPipelineLayout(m_vkd,
      m_slotMapping, VK_PIPELINE_BIND_POINT_GRAPHICS);
    
    m_vsIn  = m_shaders.vs != nullptr ? m_shaders.vs->interfaceSlots().inputSlots  : 0;
    m_fsOut = m_shaders.fs != nullptr ? m_shaders.fs->interfaceSlots().outputSlots : 0;

    if (m_shaders.gs != nullptr && m_shaders.gs->flags().test(DxvkShaderFlag::HasTransformFeedback))
      m_flags.set(DxvkGraphicsPipelineFlag::HasTransformFeedback);
    
    if (m_layout->getStorageDescriptorStages())
      m_flags.set(DxvkGraphicsPipelineFlag::HasStorageDescriptors);
    
    m_common.msSampleShadingEnable = m_shaders.fs != nullptr && m_shaders.fs->flags().test(DxvkShaderFlag::HasSampleRateShading);
    m_common.msSampleShadingFactor = 1.0f;
  }
  
  
  DxvkGraphicsPipeline::~DxvkGraphicsPipeline() {
    for (const auto& instance : m_pipelines)
      this->destroyPipeline(instance.pipeline());
  }
  
  
  Rc<DxvkShader> DxvkGraphicsPipeline::getShader(
          VkShaderStageFlagBits             stage) const {
    switch (stage) {
      case VK_SHADER_STAGE_VERTEX_BIT:                  return m_shaders.vs;
      case VK_SHADER_STAGE_GEOMETRY_BIT:                return m_shaders.gs;
      case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    return m_shaders.tcs;
      case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return m_shaders.tes;
      case VK_SHADER_STAGE_FRAGMENT_BIT:                return m_shaders.fs;
      default:
        return nullptr;
    }
  }


  VkPipeline DxvkGraphicsPipeline::getPipelineHandle(
    const DxvkGraphicsPipelineStateInfo& state,
    const DxvkRenderPass*                renderPass) {
    DxvkGraphicsPipelineInstance* instance = nullptr;

    { std::lock_guard<sync::Spinlock> lock(m_mutex);
    
      instance = this->findInstance(state, renderPass);
      
      if (instance)
        return instance->pipeline();
      
      instance = this->createInstance(state, renderPass);
    }
    
    if (!instance)
      return VK_NULL_HANDLE;

    this->writePipelineStateToCache(state, renderPass->format());
    return instance->pipeline();
  }


  void DxvkGraphicsPipeline::compilePipeline(
    const DxvkGraphicsPipelineStateInfo& state,
    const DxvkRenderPass*                renderPass) {
    std::lock_guard<sync::Spinlock> lock(m_mutex);

    if (!this->findInstance(state, renderPass))
      this->createInstance(state, renderPass);
  }


  DxvkGraphicsPipelineInstance* DxvkGraphicsPipeline::createInstance(
    const DxvkGraphicsPipelineStateInfo& state,
    const DxvkRenderPass*                renderPass) {
    // If the pipeline state vector is invalid, don't try
    // to create a new pipeline, it won't work anyway.
    if (!this->validatePipelineState(state))
      return nullptr;

    VkPipeline newPipelineHandle = this->createPipeline(state, renderPass);

    m_pipeMgr->m_numGraphicsPipelines += 1;
    return &m_pipelines.emplace_back(state, renderPass, newPipelineHandle);
  }
  
  
  DxvkGraphicsPipelineInstance* DxvkGraphicsPipeline::findInstance(
    const DxvkGraphicsPipelineStateInfo& state,
    const DxvkRenderPass*                renderPass) {
    for (auto& instance : m_pipelines) {
      if (instance.isCompatible(state, renderPass))
        return &instance;
    }
    
    return nullptr;
  }
  
  
  VkPipeline DxvkGraphicsPipeline::createPipeline(
    const DxvkGraphicsPipelineStateInfo& state,
    const DxvkRenderPass*                renderPass) const {
    if (Logger::logLevel() <= LogLevel::Debug) {
      Logger::debug("Compiling graphics pipeline...");
      this->logPipelineState(LogLevel::Debug, state);
    }

    // Render pass format and image layouts
    DxvkRenderPassFormat passFormat = renderPass->format();
    
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

    if (state.ms.sampleCount())
      sampleCount = VkSampleCountFlagBits(state.ms.sampleCount());
    else if (state.rs.sampleCount())
      sampleCount = VkSampleCountFlagBits(state.rs.sampleCount());
    
    // Set up some specialization constants
    DxvkSpecConstants specData;
    specData.set(uint32_t(DxvkSpecConstantId::RasterizerSampleCount), sampleCount, VK_SAMPLE_COUNT_1_BIT);
    
    for (uint32_t i = 0; i < m_layout->bindingCount(); i++)
      specData.set(i, state.bsBindingMask.test(i), true);
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if ((m_fsOut & (1 << i)) != 0) {
        specData.set(uint32_t(DxvkSpecConstantId::ColorComponentMappings) + i,
          state.omSwizzle[i].rIndex() << 0 | state.omSwizzle[i].gIndex() << 4 |
          state.omSwizzle[i].bIndex() << 8 | state.omSwizzle[i].aIndex() << 12, 0x3210u);
      }
    }

    for (uint32_t i = 0; i < MaxNumSpecConstants; i++)
      specData.set(getSpecId(i), state.sc.specConstants[i], 0u);
    
    VkSpecializationInfo specInfo = specData.getSpecInfo();
    
    auto vsm  = createShaderModule(m_shaders.vs,  state);
    auto tcsm = createShaderModule(m_shaders.tcs, state);
    auto tesm = createShaderModule(m_shaders.tes, state);
    auto gsm  = createShaderModule(m_shaders.gs,  state);
    auto fsm  = createShaderModule(m_shaders.fs,  state);

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    if (vsm)  stages.push_back(vsm.stageInfo(&specInfo));
    if (tcsm) stages.push_back(tcsm.stageInfo(&specInfo));
    if (tesm) stages.push_back(tesm.stageInfo(&specInfo));
    if (gsm)  stages.push_back(gsm.stageInfo(&specInfo));
    if (fsm)  stages.push_back(fsm.stageInfo(&specInfo));

    // Fix up color write masks using the component mappings
    std::array<VkPipelineColorBlendAttachmentState, MaxNumRenderTargets> omBlendAttachments;

    const VkColorComponentFlags fullMask
      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
      | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      omBlendAttachments[i] = state.omBlend[i].state();

      if (omBlendAttachments[i].colorWriteMask != fullMask) {
        omBlendAttachments[i].colorWriteMask = util::remapComponentMask(
          state.omBlend[i].colorWriteMask(), state.omSwizzle[i].mapping());
      }
      
      if ((m_fsOut & (1 << i)) == 0)
        omBlendAttachments[i].colorWriteMask = 0;
    }

    // Generate per-instance attribute divisors
    std::array<VkVertexInputBindingDivisorDescriptionEXT, MaxNumVertexBindings> viDivisorDesc;
    uint32_t                                                                    viDivisorCount = 0;

    for (uint32_t i = 0; i < state.il.bindingCount(); i++) {
      if (state.ilBindings[i].inputRate() == VK_VERTEX_INPUT_RATE_INSTANCE
       && state.ilBindings[i].divisor()   != 1) {
        const uint32_t id = viDivisorCount++;
        
        viDivisorDesc[id].binding = i; /* see below */
        viDivisorDesc[id].divisor = state.ilBindings[i].divisor();
      }
    }

    int32_t rasterizedStream = m_shaders.gs != nullptr
      ? m_shaders.gs->shaderOptions().rasterizedStream
      : 0;
    
    // Compact vertex bindings so that we can more easily update vertex buffers
    std::array<VkVertexInputAttributeDescription, MaxNumVertexAttributes> viAttribs;
    std::array<VkVertexInputBindingDescription,   MaxNumVertexBindings>   viBindings;
    std::array<uint32_t,                          MaxNumVertexBindings>   viBindingMap = { };

    for (uint32_t i = 0; i < state.il.bindingCount(); i++) {
      viBindings[i] = state.ilBindings[i].description();
      viBindings[i].binding = i;
      viBindingMap[state.ilBindings[i].binding()] = i;
    }

    for (uint32_t i = 0; i < state.il.attributeCount(); i++) {
      viAttribs[i] = state.ilAttributes[i].description();
      viAttribs[i].binding = viBindingMap[state.ilAttributes[i].binding()];
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
    viInfo.vertexBindingDescriptionCount    = state.il.bindingCount();
    viInfo.pVertexBindingDescriptions       = viBindings.data();
    viInfo.vertexAttributeDescriptionCount  = state.il.attributeCount();
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
    iaInfo.topology               = state.ia.primitiveTopology();
    iaInfo.primitiveRestartEnable = state.ia.primitiveRestart();
    
    VkPipelineTessellationStateCreateInfo tsInfo;
    tsInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tsInfo.pNext                  = nullptr;
    tsInfo.flags                  = 0;
    tsInfo.patchControlPoints     = state.ia.patchVertexCount();
    
    VkPipelineViewportStateCreateInfo vpInfo;
    vpInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpInfo.pNext                  = nullptr;
    vpInfo.flags                  = 0;
    vpInfo.viewportCount          = state.rs.viewportCount();
    vpInfo.pViewports             = nullptr;
    vpInfo.scissorCount           = state.rs.viewportCount();
    vpInfo.pScissors              = nullptr;
    
    VkPipelineRasterizationConservativeStateCreateInfoEXT conservativeInfo;
    conservativeInfo.sType        = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT;
    conservativeInfo.pNext        = nullptr;
    conservativeInfo.flags        = 0;
    conservativeInfo.conservativeRasterizationMode = state.rs.conservativeMode();
    conservativeInfo.extraPrimitiveOverestimationSize = 0.0f;

    VkPipelineRasterizationStateStreamCreateInfoEXT xfbStreamInfo;
    xfbStreamInfo.sType           = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT;
    xfbStreamInfo.pNext           = nullptr;
    xfbStreamInfo.flags           = 0;
    xfbStreamInfo.rasterizationStream = uint32_t(rasterizedStream);

    VkPipelineRasterizationDepthClipStateCreateInfoEXT rsDepthClipInfo;
    rsDepthClipInfo.sType         = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT;
    rsDepthClipInfo.pNext         = nullptr;
    rsDepthClipInfo.flags         = 0;
    rsDepthClipInfo.depthClipEnable = state.rs.depthClipEnable();

    VkPipelineRasterizationStateCreateInfo rsInfo;
    rsInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rsInfo.pNext                  = nullptr;
    rsInfo.flags                  = 0;
    rsInfo.depthClampEnable       = VK_TRUE;
    rsInfo.rasterizerDiscardEnable = rasterizedStream < 0;
    rsInfo.polygonMode            = state.rs.polygonMode();
    rsInfo.cullMode               = state.rs.cullMode();
    rsInfo.frontFace              = state.rs.frontFace();
    rsInfo.depthBiasEnable        = state.rs.depthBiasEnable();
    rsInfo.depthBiasConstantFactor= 0.0f;
    rsInfo.depthBiasClamp         = 0.0f;
    rsInfo.depthBiasSlopeFactor   = 0.0f;
    rsInfo.lineWidth              = 1.0f;
    
    if (rasterizedStream > 0)
      xfbStreamInfo.pNext = std::exchange(rsInfo.pNext, &xfbStreamInfo);

    if (conservativeInfo.conservativeRasterizationMode != VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT)
      conservativeInfo.pNext = std::exchange(rsInfo.pNext, &conservativeInfo);

    if (m_pipeMgr->m_device->features().extDepthClipEnable.depthClipEnable)
      rsDepthClipInfo.pNext = std::exchange(rsInfo.pNext, &rsDepthClipInfo);
    else
      rsInfo.depthClampEnable = !state.rs.depthClipEnable();

    uint32_t sampleMask = state.ms.sampleMask();

    VkPipelineMultisampleStateCreateInfo msInfo;
    msInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msInfo.pNext                  = nullptr;
    msInfo.flags                  = 0;
    msInfo.rasterizationSamples   = sampleCount;
    msInfo.sampleShadingEnable    = m_common.msSampleShadingEnable;
    msInfo.minSampleShading       = m_common.msSampleShadingFactor;
    msInfo.pSampleMask            = &sampleMask;
    msInfo.alphaToCoverageEnable  = state.ms.enableAlphaToCoverage();
    msInfo.alphaToOneEnable       = VK_FALSE;
    
    VkPipelineDepthStencilStateCreateInfo dsInfo;
    dsInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsInfo.pNext                  = nullptr;
    dsInfo.flags                  = 0;
    dsInfo.depthTestEnable        = state.ds.enableDepthTest();
    dsInfo.depthWriteEnable       = state.ds.enableDepthWrite() && !util::isDepthReadOnlyLayout(passFormat.depth.layout);
    dsInfo.depthCompareOp         = state.ds.depthCompareOp();
    dsInfo.depthBoundsTestEnable  = state.ds.enableDepthBoundsTest();
    dsInfo.stencilTestEnable      = state.ds.enableStencilTest();
    dsInfo.front                  = state.dsFront.state();
    dsInfo.back                   = state.dsBack.state();
    dsInfo.minDepthBounds         = 0.0f;
    dsInfo.maxDepthBounds         = 1.0f;
    
    VkPipelineColorBlendStateCreateInfo cbInfo;
    cbInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbInfo.pNext                  = nullptr;
    cbInfo.flags                  = 0;
    cbInfo.logicOpEnable          = state.om.enableLogicOp();
    cbInfo.logicOp                = state.om.logicOp();
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
    info.renderPass               = renderPass->getDefaultHandle();
    info.subpass                  = 0;
    info.basePipelineHandle       = VK_NULL_HANDLE;
    info.basePipelineIndex        = -1;
    
    if (tsInfo.patchControlPoints == 0)
      info.pTessellationState = nullptr;
    
    // Time pipeline compilation for debugging purposes
    dxvk::high_resolution_clock::time_point t0, t1;

    if (Logger::logLevel() <= LogLevel::Debug)
      t0 = dxvk::high_resolution_clock::now();
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (m_vkd->vkCreateGraphicsPipelines(m_vkd->device(),
          m_pipeMgr->m_cache->handle(), 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
      Logger::err("DxvkGraphicsPipeline: Failed to compile pipeline");
      this->logPipelineState(LogLevel::Error, state);
      return VK_NULL_HANDLE;
    }
    
    if (Logger::logLevel() <= LogLevel::Debug) {
      t1 = dxvk::high_resolution_clock::now();
      auto td = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
      Logger::debug(str::format("DxvkGraphicsPipeline: Finished in ", td.count(), " ms"));
    }

    return pipeline;
  }
  
  
  void DxvkGraphicsPipeline::destroyPipeline(VkPipeline pipeline) const {
    m_vkd->vkDestroyPipeline(m_vkd->device(), pipeline, nullptr);
  }


  DxvkShaderModule DxvkGraphicsPipeline::createShaderModule(
    const Rc<DxvkShader>&                shader,
    const DxvkGraphicsPipelineStateInfo& state) const {
    if (shader == nullptr)
      return DxvkShaderModule();

    DxvkShaderModuleCreateInfo info;

    // Fix up fragment shader outputs for dual-source blending
    if (shader->stage() == VK_SHADER_STAGE_FRAGMENT_BIT) {
      info.fsDualSrcBlend = state.omBlend[0].blendEnable() && (
        util::isDualSourceBlendFactor(state.omBlend[0].srcColorBlendFactor()) ||
        util::isDualSourceBlendFactor(state.omBlend[0].dstColorBlendFactor()) ||
        util::isDualSourceBlendFactor(state.omBlend[0].srcAlphaBlendFactor()) ||
        util::isDualSourceBlendFactor(state.omBlend[0].dstAlphaBlendFactor()));
    }

    // Deal with undefined shader inputs
    uint32_t consumedInputs = shader->interfaceSlots().inputSlots;
    uint32_t providedInputs = 0;

    if (shader->stage() == VK_SHADER_STAGE_VERTEX_BIT) {
      for (uint32_t i = 0; i < state.il.attributeCount(); i++)
        providedInputs |= 1u << state.ilAttributes[i].location();
    } else if (shader->stage() != VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
      auto prevStage = getPrevStageShader(shader->stage());
      providedInputs = prevStage->interfaceSlots().outputSlots;
    } else {
      // Technically not correct, but this
      // would need a lot of extra care
      providedInputs = consumedInputs;
    }

    info.undefinedInputs = (providedInputs & consumedInputs) ^ consumedInputs;
    return shader->createShaderModule(m_vkd, m_slotMapping, info);
  }


  Rc<DxvkShader> DxvkGraphicsPipeline::getPrevStageShader(VkShaderStageFlagBits stage) const {
    if (stage == VK_SHADER_STAGE_VERTEX_BIT)
      return nullptr;

    if (stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
      return m_shaders.tcs;

    Rc<DxvkShader> result = m_shaders.vs;

    if (stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
      return result;

    if (m_shaders.tes != nullptr)
      result = m_shaders.tes;

    if (stage == VK_SHADER_STAGE_GEOMETRY_BIT)
      return result;

    if (m_shaders.gs != nullptr)
      result = m_shaders.gs;

    return result;
  }


  bool DxvkGraphicsPipeline::validatePipelineState(
    const DxvkGraphicsPipelineStateInfo& state) const {
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
    
    // No errors
    return true;
  }
  
  
  void DxvkGraphicsPipeline::writePipelineStateToCache(
    const DxvkGraphicsPipelineStateInfo& state,
    const DxvkRenderPassFormat&          format) const {
    if (m_pipeMgr->m_stateCache == nullptr)
      return;
    
    DxvkStateCacheKey key;
    if (m_shaders.vs  != nullptr) key.vs = m_shaders.vs->getShaderKey();
    if (m_shaders.tcs != nullptr) key.tcs = m_shaders.tcs->getShaderKey();
    if (m_shaders.tes != nullptr) key.tes = m_shaders.tes->getShaderKey();
    if (m_shaders.gs  != nullptr) key.gs = m_shaders.gs->getShaderKey();
    if (m_shaders.fs  != nullptr) key.fs = m_shaders.fs->getShaderKey();

    m_pipeMgr->m_stateCache->addGraphicsPipeline(key, state, format);
  }
  
  
  void DxvkGraphicsPipeline::logPipelineState(
          LogLevel                       level,
    const DxvkGraphicsPipelineStateInfo& state) const {
    if (m_shaders.vs  != nullptr) Logger::log(level, str::format("  vs  : ", m_shaders.vs ->debugName()));
    if (m_shaders.tcs != nullptr) Logger::log(level, str::format("  tcs : ", m_shaders.tcs->debugName()));
    if (m_shaders.tes != nullptr) Logger::log(level, str::format("  tes : ", m_shaders.tes->debugName()));
    if (m_shaders.gs  != nullptr) Logger::log(level, str::format("  gs  : ", m_shaders.gs ->debugName()));
    if (m_shaders.fs  != nullptr) Logger::log(level, str::format("  fs  : ", m_shaders.fs ->debugName()));

    for (uint32_t i = 0; i < state.il.attributeCount(); i++) {
      const auto& attr = state.ilAttributes[i];
      Logger::log(level, str::format("  attr ", i, " : location ", attr.location(), ", binding ", attr.binding(), ", format ", attr.format(), ", offset ", attr.offset()));
    }
    for (uint32_t i = 0; i < state.il.bindingCount(); i++) {
      const auto& bind = state.ilBindings[i];
      Logger::log(level, str::format("  binding ", i, " : binding ", bind.binding(), ", stride ", bind.stride(), ", rate ", bind.inputRate(), ", divisor ", bind.divisor()));
    }
    
    // TODO log more pipeline state
  }
  
}
