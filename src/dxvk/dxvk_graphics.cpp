#include "../util/util_time.h"

#include "dxvk_device.h"
#include "dxvk_graphics.h"
#include "dxvk_pipemanager.h"
#include "dxvk_spec_const.h"
#include "dxvk_state_cache.h"

namespace dxvk {

  DxvkGraphicsPipeline::DxvkGraphicsPipeline(
          DxvkPipelineManager*        pipeMgr,
          DxvkGraphicsPipelineShaders shaders,
          DxvkBindingLayoutObjects*   layout)
  : m_vkd(pipeMgr->m_device->vkd()), m_pipeMgr(pipeMgr),
    m_shaders(std::move(shaders)), m_bindings(layout) {
    m_vsIn  = m_shaders.vs != nullptr ? m_shaders.vs->info().inputMask  : 0;
    m_fsOut = m_shaders.fs != nullptr ? m_shaders.fs->info().outputMask : 0;

    if (m_shaders.gs != nullptr && m_shaders.gs->flags().test(DxvkShaderFlag::HasTransformFeedback))
      m_flags.set(DxvkGraphicsPipelineFlag::HasTransformFeedback);
    
    if (layout->getAccessFlags() & VK_ACCESS_SHADER_WRITE_BIT)
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
    DxvkGraphicsPipelineInstance* instance = this->findInstance(state);

    if (unlikely(!instance)) {
      // Exit early if the state vector is invalid
      if (!this->validatePipelineState(state, true))
        return VK_NULL_HANDLE;

      // Prevent other threads from adding new instances and check again
      std::lock_guard<dxvk::mutex> lock(m_mutex);
      instance = this->findInstance(state);

      if (!instance) {
        // Keep pipeline object locked, at worst we're going to stall
        // a state cache worker and the current thread needs priority.
        instance = this->createInstance(state);
        this->writePipelineStateToCache(state);
      }
    }

    return instance->pipeline();
  }


  void DxvkGraphicsPipeline::compilePipeline(
    const DxvkGraphicsPipelineStateInfo& state) {
    // Exit early if the state vector is invalid
    if (!this->validatePipelineState(state, false))
      return;

    // Keep the object locked while compiling a pipeline since compiling
    // similar pipelines concurrently is fragile on some drivers
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!this->findInstance(state))
      this->createInstance(state);
  }


  DxvkGraphicsPipelineInstance* DxvkGraphicsPipeline::createInstance(
    const DxvkGraphicsPipelineStateInfo& state) {
    VkPipeline pipeline = this->createPipeline(state);

    m_pipeMgr->m_numGraphicsPipelines += 1;
    return &(*m_pipelines.emplace(state, pipeline));
  }
  
  
  DxvkGraphicsPipelineInstance* DxvkGraphicsPipeline::findInstance(
    const DxvkGraphicsPipelineStateInfo& state) {
    for (auto& instance : m_pipelines) {
      if (instance.isCompatible(state))
        return &instance;
    }
    
    return nullptr;
  }
  
  
  VkPipeline DxvkGraphicsPipeline::createPipeline(
    const DxvkGraphicsPipelineStateInfo& state) const {
    if (Logger::logLevel() <= LogLevel::Debug) {
      Logger::debug("Compiling graphics pipeline...");
      this->logPipelineState(LogLevel::Debug, state);
    }

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
    VkImageAspectFlags rtReadOnlyAspects = state.rt.getDepthStencilReadOnlyAspects();
    VkFormat rtDepthFormat = state.rt.getDepthStencilFormat();
    auto rtDepthFormatInfo = imageFormatInfo(rtDepthFormat);

    std::array<VkPipelineColorBlendAttachmentState, MaxNumRenderTargets> omBlendAttachments = { };
    std::array<VkFormat, DxvkLimits::MaxNumRenderTargets> rtColorFormats;
    uint32_t rtColorFormatCount = 0;

    const VkColorComponentFlags fullMask
      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
      | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      rtColorFormats[i] = state.rt.getColorFormat(i);

      if (rtColorFormats[i]) {
        rtColorFormatCount = i + 1;

        auto formatInfo = imageFormatInfo(rtColorFormats[i]);
        omBlendAttachments[i] = state.omBlend[i].state();

        if (!(m_fsOut & (1 << i)) || !formatInfo) {
          omBlendAttachments[i].colorWriteMask = 0;
        } else {
          if (omBlendAttachments[i].colorWriteMask != fullMask) {
            omBlendAttachments[i].colorWriteMask = util::remapComponentMask(
              state.omBlend[i].colorWriteMask(), state.omSwizzle[i].mapping());
          }

          omBlendAttachments[i].colorWriteMask &= formatInfo->componentMask;

          if (omBlendAttachments[i].colorWriteMask == formatInfo->componentMask) {
            omBlendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                                 | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
          }
        }
      }
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
      ? m_shaders.gs->info().xfbRasterizedStream
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

    VkPipelineVertexInputDivisorStateCreateInfoEXT viDivisorInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT };
    viDivisorInfo.vertexBindingDivisorCount = viDivisorCount;
    viDivisorInfo.pVertexBindingDivisors    = viDivisorDesc.data();
    
    VkPipelineVertexInputStateCreateInfo viInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, &viDivisorInfo };
    viInfo.vertexBindingDescriptionCount    = state.il.bindingCount();
    viInfo.pVertexBindingDescriptions       = viBindings.data();
    viInfo.vertexAttributeDescriptionCount  = state.il.attributeCount();
    viInfo.pVertexAttributeDescriptions     = viAttribs.data();
    
    if (viDivisorCount == 0)
      viInfo.pNext = viDivisorInfo.pNext;
    
    // TODO remove this once the extension is widely supported
    if (!m_pipeMgr->m_device->features().extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor)
      viInfo.pNext = viDivisorInfo.pNext;
    
    VkPipelineInputAssemblyStateCreateInfo iaInfo = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    iaInfo.topology               = state.ia.primitiveTopology();
    iaInfo.primitiveRestartEnable = state.ia.primitiveRestart();
    
    VkPipelineTessellationStateCreateInfo tsInfo = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };
    tsInfo.patchControlPoints     = state.ia.patchVertexCount();
    
    VkPipelineViewportStateCreateInfo vpInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vpInfo.viewportCount          = state.rs.viewportCount();
    vpInfo.scissorCount           = state.rs.viewportCount();
    
    VkPipelineRasterizationConservativeStateCreateInfoEXT conservativeInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT };
    conservativeInfo.conservativeRasterizationMode = state.rs.conservativeMode();
    conservativeInfo.extraPrimitiveOverestimationSize = 0.0f;

    VkPipelineRasterizationStateStreamCreateInfoEXT xfbStreamInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT };
    xfbStreamInfo.rasterizationStream = uint32_t(rasterizedStream);

    VkPipelineRasterizationDepthClipStateCreateInfoEXT rsDepthClipInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT };
    rsDepthClipInfo.depthClipEnable = state.rs.depthClipEnable();

    VkPipelineRasterizationStateCreateInfo rsInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsInfo.depthClampEnable       = VK_TRUE;
    rsInfo.rasterizerDiscardEnable = rasterizedStream < 0;
    rsInfo.polygonMode            = state.rs.polygonMode();
    rsInfo.cullMode               = state.rs.cullMode();
    rsInfo.frontFace              = state.rs.frontFace();
    rsInfo.depthBiasEnable        = state.rs.depthBiasEnable();
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

    VkPipelineMultisampleStateCreateInfo msInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msInfo.rasterizationSamples   = sampleCount;
    msInfo.sampleShadingEnable    = m_common.msSampleShadingEnable;
    msInfo.minSampleShading       = m_common.msSampleShadingFactor;
    msInfo.pSampleMask            = &sampleMask;
    msInfo.alphaToCoverageEnable  = state.ms.enableAlphaToCoverage();
    msInfo.alphaToOneEnable       = VK_FALSE;
    
    VkPipelineDepthStencilStateCreateInfo dsInfo = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    dsInfo.depthTestEnable        = state.ds.enableDepthTest();
    dsInfo.depthWriteEnable       = state.ds.enableDepthWrite() && !(rtReadOnlyAspects & VK_IMAGE_ASPECT_DEPTH_BIT);
    dsInfo.depthCompareOp         = state.ds.depthCompareOp();
    dsInfo.depthBoundsTestEnable  = state.ds.enableDepthBoundsTest();
    dsInfo.stencilTestEnable      = state.ds.enableStencilTest();
    dsInfo.front                  = state.dsFront.state();
    dsInfo.back                   = state.dsBack.state();
    
    VkPipelineColorBlendStateCreateInfo cbInfo = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbInfo.logicOpEnable          = state.om.enableLogicOp();
    cbInfo.logicOp                = state.om.logicOp();
    cbInfo.attachmentCount        = rtColorFormatCount;
    cbInfo.pAttachments           = omBlendAttachments.data();
    
    VkPipelineDynamicStateCreateInfo dyInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dyInfo.dynamicStateCount      = dynamicStateCount;
    dyInfo.pDynamicStates         = dynamicStates.data();

    VkPipelineRenderingCreateInfoKHR rtInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR };

    if (rtColorFormatCount) {
      rtInfo.colorAttachmentCount = rtColorFormatCount;
      rtInfo.pColorAttachmentFormats = rtColorFormats.data();
    }

    if (rtDepthFormat) {
      if (rtDepthFormatInfo->aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
        rtInfo.depthAttachmentFormat = rtDepthFormat;

      if (rtDepthFormatInfo->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
        rtInfo.stencilAttachmentFormat = rtDepthFormat;
    }

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &rtInfo };
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
    info.layout                   = m_bindings->getPipelineLayout();
    info.basePipelineIndex        = -1;
    
    if (!tsInfo.patchControlPoints)
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

    const DxvkShaderCreateInfo& shaderInfo = shader->info();
    DxvkShaderModuleCreateInfo info;

    // Fix up fragment shader outputs for dual-source blending
    if (shaderInfo.stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
      info.fsDualSrcBlend = state.omBlend[0].blendEnable() && (
        util::isDualSourceBlendFactor(state.omBlend[0].srcColorBlendFactor()) ||
        util::isDualSourceBlendFactor(state.omBlend[0].dstColorBlendFactor()) ||
        util::isDualSourceBlendFactor(state.omBlend[0].srcAlphaBlendFactor()) ||
        util::isDualSourceBlendFactor(state.omBlend[0].dstAlphaBlendFactor()));
    }

    // Deal with undefined shader inputs
    uint32_t consumedInputs = shaderInfo.inputMask;
    uint32_t providedInputs = 0;

    if (shaderInfo.stage == VK_SHADER_STAGE_VERTEX_BIT) {
      for (uint32_t i = 0; i < state.il.attributeCount(); i++)
        providedInputs |= 1u << state.ilAttributes[i].location();
    } else if (shaderInfo.stage != VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
      auto prevStage = getPrevStageShader(shaderInfo.stage);
      providedInputs = prevStage->info().outputMask;
    } else {
      // Technically not correct, but this
      // would need a lot of extra care
      providedInputs = consumedInputs;
    }

    info.undefinedInputs = (providedInputs & consumedInputs) ^ consumedInputs;
    return shader->createShaderModule(m_vkd, m_bindings, info);
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
    if (!m_shaders.validate()) {
      Logger::err("Invalid pipeline: Shader types do not match stage");
      return false;
    }

    // Validate vertex input layout
    const DxvkDevice* device = m_pipeMgr->m_device;
    uint32_t ilLocationMask = 0;
    uint32_t ilBindingMask = 0;

    for (uint32_t i = 0; i < state.il.bindingCount(); i++)
      ilBindingMask |= 1u << state.ilBindings[i].binding();

    for (uint32_t i = 0; i < state.il.attributeCount(); i++) {
      const DxvkIlAttribute& attribute = state.ilAttributes[i];

      if (ilLocationMask & (1u << attribute.location())) {
        Logger::err(str::format("Invalid pipeline: Vertex location ", attribute.location(), " defined twice"));
        return false;
      }

      if (!(ilBindingMask & (1u << attribute.binding()))) {
        Logger::err(str::format("Invalid pipeline: Vertex binding ", attribute.binding(), " not defined"));
        return false;
      }

      VkFormatProperties formatInfo = device->adapter()->formatProperties(attribute.format());

      if (!(formatInfo.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT)) {
        Logger::err(str::format("Invalid pipeline: Format ", attribute.format(), " not supported for vertex buffers"));
        return false;
      }

      ilLocationMask |= 1u << attribute.location();
    }

    // Validate rasterization state
    if (state.rs.conservativeMode() != VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT) {
      if (!device->extensions().extConservativeRasterization) {
        Logger::err("Conservative rasterization not supported by device");
        return false;
      }

      if (state.rs.conservativeMode() == VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT
       && !device->properties().extConservativeRasterization.primitiveUnderestimation) {
        Logger::err("Primitive underestimation not supported by device");
        return false;
      }
    }

    // Validate depth-stencil state
    if (state.ds.enableDepthBoundsTest() && !device->features().core.features.depthBounds) {
      Logger::err("Depth bounds not supported by device");
      return false;
    }

    return true;
  }
  
  
  void DxvkGraphicsPipeline::writePipelineStateToCache(
    const DxvkGraphicsPipelineStateInfo& state) const {
    if (m_pipeMgr->m_stateCache == nullptr)
      return;
    
    DxvkStateCacheKey key;
    if (m_shaders.vs  != nullptr) key.vs = m_shaders.vs->getShaderKey();
    if (m_shaders.tcs != nullptr) key.tcs = m_shaders.tcs->getShaderKey();
    if (m_shaders.tes != nullptr) key.tes = m_shaders.tes->getShaderKey();
    if (m_shaders.gs  != nullptr) key.gs = m_shaders.gs->getShaderKey();
    if (m_shaders.fs  != nullptr) key.fs = m_shaders.fs->getShaderKey();

    m_pipeMgr->m_stateCache->addGraphicsPipeline(key, state);
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
