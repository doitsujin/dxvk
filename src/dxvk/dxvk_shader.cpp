#include "dxvk_device.h"
#include "dxvk_pipemanager.h"
#include "dxvk_shader.h"
#include "dxvk_shader_io.h"

#include <dxvk_dummy_frag.h>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace dxvk {

  std::atomic<uint32_t> DxvkShader::s_cookie = { 0u };


  bool DxvkShaderModuleCreateInfo::eq(const DxvkShaderModuleCreateInfo& other) const {
    bool eq = fsDualSrcBlend == other.fsDualSrcBlend
           && fsFlatShading == other.fsFlatShading
           && !prevStageOutputs == !other.prevStageOutputs;

    if (prevStageOutputs && eq) {
      eq = prevStageOutputs->getVarCount() == other.prevStageOutputs->getVarCount();

      for (uint32_t i = 0; i < prevStageOutputs->getVarCount() && eq; i++)
        eq = prevStageOutputs->getVar(i).eq(other.prevStageOutputs->getVar(i));
    }

    for (uint32_t i = 0; i < rtSwizzles.size() && eq; i++) {
      eq = rtSwizzles[i].r == other.rtSwizzles[i].r
        && rtSwizzles[i].g == other.rtSwizzles[i].g
        && rtSwizzles[i].b == other.rtSwizzles[i].b
        && rtSwizzles[i].a == other.rtSwizzles[i].a;
    }

    return eq;
  }


  size_t DxvkShaderModuleCreateInfo::hash() const {
    DxvkHashState hash;
    hash.add(uint32_t(fsDualSrcBlend));
    hash.add(uint32_t(fsFlatShading));

    if (prevStageOutputs) {
      for (uint32_t i = 0; i < prevStageOutputs->getVarCount(); i++)
        hash.add(prevStageOutputs->getVar(i).hash());
    }

    for (uint32_t i = 0; i < rtSwizzles.size(); i++) {
      hash.add(rtSwizzles[i].r);
      hash.add(rtSwizzles[i].g);
      hash.add(rtSwizzles[i].b);
      hash.add(rtSwizzles[i].a);
    }

    return hash;
  }


  DxvkShader::DxvkShader()
  : m_cookie(++s_cookie) {

  }


  DxvkShader::~DxvkShader() {
    
  }
  
  
  bool DxvkShader::canUsePipelineLibrary(bool standalone) const {
    if (standalone) {
      // Standalone pipeline libraries are unsupported for geometry
      // and tessellation stages since we'd need to compile them
      // all into one library
      if (m_metadata.stage != VK_SHADER_STAGE_VERTEX_BIT
       && m_metadata.stage != VK_SHADER_STAGE_FRAGMENT_BIT
       && m_metadata.stage != VK_SHADER_STAGE_COMPUTE_BIT)
        return false;

      // Standalone vertex shaders must export vertex position
      if (m_metadata.stage == VK_SHADER_STAGE_VERTEX_BIT
       && !m_metadata.flags.test(DxvkShaderFlag::ExportsPosition))
        return false;
    } else {
      // Tessellation control shaders must define a valid vertex count
      if (m_metadata.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
       && (m_metadata.patchVertexCount < 1 || m_metadata.patchVertexCount > 32))
        return false;

      // We don't support GPL with transform feedback right now
      if (m_metadata.flags.test(DxvkShaderFlag::HasTransformFeedback))
        return false;
    }

    // Spec constant selectors are only supported in graphics
    if (m_metadata.specConstantMask & (1u << MaxNumSpecConstants))
      return m_metadata.stage != VK_SHADER_STAGE_COMPUTE_BIT;

    // Always late-compile shaders with spec constants
    // that don't use the spec constant selector
    return !m_metadata.specConstantMask;
  }




  DxvkShaderStageInfo::DxvkShaderStageInfo(const DxvkDevice* device)
  : m_device(device) {

  }

  void DxvkShaderStageInfo::addStage(
          VkShaderStageFlagBits   stage,
          SpirvCodeBuffer&&       code,
    const VkSpecializationInfo*   specInfo) {
    // Take ownership of the SPIR-V code buffer
    auto& codeBuffer = m_codeBuffers[m_stageCount];
    codeBuffer = std::move(code);

    auto& moduleInfo = m_moduleInfos[m_stageCount].moduleInfo;
    moduleInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    moduleInfo.codeSize = codeBuffer.size();
    moduleInfo.pCode = codeBuffer.data();

    auto& stageInfo = m_stageInfos[m_stageCount];
    stageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, &moduleInfo };
    stageInfo.stage = stage;
    stageInfo.pName = "main";
    stageInfo.pSpecializationInfo = specInfo;

    m_stageCount++;
  }
  

  void DxvkShaderStageInfo::addStage(
          VkShaderStageFlagBits   stage,
    const VkShaderModuleIdentifierEXT& identifier,
    const VkSpecializationInfo*   specInfo) {
    // Copy relevant bits of the module identifier
    uint32_t identifierSize = std::min(identifier.identifierSize, VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT);

    auto& moduleId = m_moduleInfos[m_stageCount].moduleIdentifier;
    moduleId.createInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT };
    moduleId.createInfo.identifierSize = identifierSize;
    moduleId.createInfo.pIdentifier = moduleId.data.data();
    std::memcpy(moduleId.data.data(), identifier.identifier, identifierSize);

    // Set up stage info using the module identifier
    auto& stageInfo = m_stageInfos[m_stageCount];
    stageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stageInfo.pNext = &moduleId.createInfo;
    stageInfo.stage = stage;
    stageInfo.pName = "main";
    stageInfo.pSpecializationInfo = specInfo;

    m_stageCount++;
  }


  DxvkShaderStageInfo::~DxvkShaderStageInfo() {

  }


  DxvkShaderPipelineLibraryKey::DxvkShaderPipelineLibraryKey() {

  }


  DxvkShaderPipelineLibraryKey::~DxvkShaderPipelineLibraryKey() {

  }


  DxvkShaderSet DxvkShaderPipelineLibraryKey::getShaderSet() const {
    DxvkShaderSet result;

    for (uint32_t i = 0; i < m_shaderCount; i++) {
      auto shader = m_shaders[i].ptr();

      switch (shader->metadata().stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:                  result.vs = shader; break;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    result.tcs = shader; break;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: result.tes = shader; break;
        case VK_SHADER_STAGE_GEOMETRY_BIT:                result.gs = shader; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT:                result.fs = shader; break;
        case VK_SHADER_STAGE_COMPUTE_BIT:                 result.cs = shader; break;
        default: ;
      }
    }

    return result;
  }


  DxvkPipelineLayoutBuilder DxvkShaderPipelineLibraryKey::getLayout() const {
    // If no shader is defined, this is a null fragment shader library
    VkShaderStageFlags stages = m_shaderStages;

    if (!stages)
      stages = VK_SHADER_STAGE_FRAGMENT_BIT;

    DxvkPipelineLayoutBuilder result(stages);

    for (uint32_t i = 0u; i < m_shaderCount; i++)
      result.addLayout(m_shaders[i]->getLayout());

    return result;
  }


  void DxvkShaderPipelineLibraryKey::addShader(
    const Rc<DxvkShader>&               shader) {
    m_shaderStages |= shader->metadata().stage;
    m_shaders[m_shaderCount++] = shader;
  }


  bool DxvkShaderPipelineLibraryKey::canUsePipelineLibrary() const {
    // Ensure that each individual shader can be used in a library
    bool standalone = m_shaderCount <= 1;

    for (uint32_t i = 0; i < m_shaderCount; i++) {
      if (!m_shaders[i]->canUsePipelineLibrary(standalone))
        return false;
    }

    // Ensure that stage I/O is compatible between stages
    for (uint32_t i = 0; i + 1 < m_shaderCount; i++) {
      const auto& currShaderMeta = m_shaders[i]->metadata();
      const auto& nextShaderMeta = m_shaders[i + 1u]->metadata();

      if (!DxvkShaderIo::checkStageCompatibility(
          nextShaderMeta.stage, nextShaderMeta.inputs,
          currShaderMeta.stage, currShaderMeta.outputs))
        return false;
    }

    return true;
  }


  bool DxvkShaderPipelineLibraryKey::eq(
    const DxvkShaderPipelineLibraryKey& other) const {
    bool eq = m_shaderStages == other.m_shaderStages;

    for (uint32_t i = 0; i < m_shaderCount && eq; i++)
      eq = m_shaders[i] == other.m_shaders[i];

    return eq;
  }


  size_t DxvkShaderPipelineLibraryKey::hash() const {
    DxvkHashState hash;
    hash.add(uint32_t(m_shaderStages));

    for (uint32_t i = 0; i < m_shaderCount; i++)
      hash.add(m_shaders[i]->getCookie());

    return hash;
  }


  DxvkShaderPipelineLibrary::DxvkShaderPipelineLibrary(
          DxvkDevice*               device,
          DxvkPipelineManager*      manager,
    const DxvkShaderPipelineLibraryKey& key)
  : m_device      (device),
    m_stats       (&manager->m_stats),
    m_shaders     (key.getShaderSet()),
    m_layout      (device, manager, key.getLayout()) {

  }


  DxvkShaderPipelineLibrary::~DxvkShaderPipelineLibrary() {
    this->destroyShaderPipelineLocked();
  }


  VkShaderModuleIdentifierEXT DxvkShaderPipelineLibrary::getModuleIdentifier(
          VkShaderStageFlagBits                 stage) {
    std::lock_guard lock(m_identifierMutex);
    auto identifier = getShaderIdentifier(stage);

    if (!identifier->identifierSize) {
      // Unfortunate, but we'll have to decode the
      // shader code here to retrieve the identifier
      SpirvCodeBuffer spirvCode = this->getShaderCode(stage);
      this->generateModuleIdentifierLocked(identifier, spirvCode);
    }

    return *identifier;
  }


  DxvkShaderPipelineLibraryHandle DxvkShaderPipelineLibrary::acquirePipelineHandle() {
    std::lock_guard lock(m_mutex);

    if (m_device->mustTrackPipelineLifetime())
      m_useCount += 1;

    if (m_pipeline.handle)
      return m_pipeline;

    m_pipeline = compileShaderPipelineLocked();
    return m_pipeline;
  }


  void DxvkShaderPipelineLibrary::releasePipelineHandle() {
    if (m_device->mustTrackPipelineLifetime()) {
      std::lock_guard lock(m_mutex);

      if (!(--m_useCount))
        this->destroyShaderPipelineLocked();
    }
  }


  void DxvkShaderPipelineLibrary::compilePipeline() {
    std::lock_guard lock(m_mutex);

    // Skip if a pipeline has already been compiled
    if (m_compiledOnce)
      return;

    // Compile the pipeline with default args
    DxvkShaderPipelineLibraryHandle pipeline = compileShaderPipelineLocked();

    // On 32-bit, destroy the pipeline immediately in order to
    // save memory. We should hit the driver's disk cache once
    // we need to recreate the pipeline.
    if (m_device->mustTrackPipelineLifetime()) {
      auto vk = m_device->vkd();
      vk->vkDestroyPipeline(vk->device(), pipeline.handle, nullptr);

      pipeline.handle = VK_NULL_HANDLE;
    }

    // Write back pipeline handle for future use
    m_pipeline = pipeline;
  }


  void DxvkShaderPipelineLibrary::destroyShaderPipelineLocked() {
    auto vk = m_device->vkd();

    vk->vkDestroyPipeline(vk->device(), m_pipeline.handle, nullptr);

    m_pipeline.handle = VK_NULL_HANDLE;
  }


  DxvkShaderPipelineLibraryHandle DxvkShaderPipelineLibrary::compileShaderPipelineLocked() {
    this->notifyLibraryCompile();

    // If this is not the first time we're compiling the pipeline,
    // try to get a cache hit using the shader module identifier
    // so that we don't have to decompress our SPIR-V shader again.
    DxvkShaderPipelineLibraryHandle pipeline = { VK_NULL_HANDLE, 0 };

    if (m_compiledOnce && canUsePipelineCacheControl())
      pipeline = this->compileShaderPipeline(VK_PIPELINE_CREATE_2_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT);

    if (!pipeline.handle)
      pipeline = this->compileShaderPipeline(0);

    // Well that didn't work
    if (!pipeline.handle)
      return { VK_NULL_HANDLE, 0 };

    // Increment stat counter the first time this
    // shader pipeline gets compiled successfully
    if (!m_compiledOnce) {
      if (m_shaders.cs)
        m_stats->numComputePipelines += 1;
      else
        m_stats->numGraphicsLibraries += 1;

      m_compiledOnce = true;
    }

    return pipeline;
  }


  DxvkShaderPipelineLibraryHandle DxvkShaderPipelineLibrary::compileShaderPipeline(
          VkPipelineCreateFlags2                flags) {
    DxvkShaderStageInfo stageInfo(m_device);
    VkShaderStageFlags stageMask = getShaderStages();

    { std::lock_guard lock(m_identifierMutex);
      VkShaderStageFlags stages = stageMask;

      while (stages) {
        auto stage = VkShaderStageFlagBits(stages & -stages);
        auto identifier = getShaderIdentifier(stage);

        if (flags & VK_PIPELINE_CREATE_2_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT) {
          // Fail if we have no idenfitier for whatever reason, caller
          // should fall back to the slow path if this happens
          if (!identifier->identifierSize)
            return { VK_NULL_HANDLE, 0 };

          stageInfo.addStage(stage, *identifier, nullptr);
        } else {
          // Decompress code and generate identifier as needed
          SpirvCodeBuffer spirvCode = this->getShaderCode(stage);

          if (!identifier->identifierSize)
            this->generateModuleIdentifierLocked(identifier, spirvCode);

          stageInfo.addStage(stage, std::move(spirvCode), nullptr);
        }

        stages &= stages - 1;
      }
    }

    VkPipeline pipeline = VK_NULL_HANDLE;

    if (stageMask & VK_SHADER_STAGE_VERTEX_BIT)
      pipeline = compileVertexShaderPipeline(stageInfo, flags);
    else if (stageMask & VK_SHADER_STAGE_FRAGMENT_BIT)
      pipeline = compileFragmentShaderPipeline(stageInfo, flags);
    else if (stageMask & VK_SHADER_STAGE_COMPUTE_BIT)
      pipeline = compileComputeShaderPipeline(stageInfo, flags);

    // Should be unreachable
    return { pipeline, flags };
  }


  VkPipeline DxvkShaderPipelineLibrary::compileVertexShaderPipeline(
    const DxvkShaderStageInfo&          stageInfo,
          VkPipelineCreateFlags2        flags) {
    auto vk = m_device->vkd();

    // Set up dynamic state. We do not know any pipeline state
    // at this time, so make as much state dynamic as we can.
    uint32_t dynamicStateCount = 0;
    std::array<VkDynamicState, 7> dynamicStates;

    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BIAS;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_CULL_MODE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_FRONT_FACE;

    if (m_device->features().extExtendedDynamicState3.extendedDynamicState3DepthClipEnable)
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT;

    VkPipelineDynamicStateCreateInfo dyInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dyInfo.dynamicStateCount  = dynamicStateCount;
    dyInfo.pDynamicStates     = dynamicStates.data();

    // If a tessellation control shader is present, grab the patch vertex count
    VkPipelineTessellationStateCreateInfo tsInfo = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };

    if (m_shaders.tcs)
      tsInfo.patchControlPoints = m_shaders.tcs->metadata().patchVertexCount;

    // All viewport state is dynamic, so we do not need to initialize this.
    VkPipelineViewportStateCreateInfo vpInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };

    // Set up rasterizer state. Depth bias, cull mode and front face are
    // all dynamic. Do not support any polygon modes other than FILL.
    VkPipelineRasterizationDepthClipStateCreateInfoEXT rsDepthClipInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT };

    VkPipelineRasterizationStateCreateInfo rsInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsInfo.depthClampEnable   = VK_TRUE;
    rsInfo.rasterizerDiscardEnable = VK_FALSE;
    rsInfo.polygonMode        = VK_POLYGON_MODE_FILL;
    rsInfo.lineWidth          = 1.0f;

    if (m_device->features().extDepthClipEnable.depthClipEnable) {
      // Only use the fixed depth clip state if we can't make it dynamic
      if (!m_device->features().extExtendedDynamicState3.extendedDynamicState3DepthClipEnable) {
        rsDepthClipInfo.pNext = std::exchange(rsInfo.pNext, &rsDepthClipInfo);
        rsDepthClipInfo.depthClipEnable = VK_TRUE;
      }
    } else {
      rsInfo.depthClampEnable = VK_FALSE;
    }

    // Only the view mask is used as input, and since we do not use MultiView, it is always 0
    VkPipelineRenderingCreateInfo rtInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

    VkPipelineCreateFlags2CreateInfo flagsInfo = { VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO, &rtInfo };
    flagsInfo.flags = VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR | flags;

    if (m_device->canUseDescriptorBuffer())
      flagsInfo.flags |= VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT;

    VkGraphicsPipelineLibraryCreateInfoEXT libInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, &flagsInfo };
    libInfo.flags             = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
    info.stageCount           = stageInfo.getStageCount();
    info.pStages              = stageInfo.getStageInfos();
    info.pTessellationState   = m_shaders.tcs ? &tsInfo : nullptr;
    info.pViewportState       = &vpInfo;
    info.pRasterizationState  = &rsInfo;
    info.pDynamicState        = &dyInfo;
    info.layout               = m_layout.getLayout(DxvkPipelineLayoutType::Independent)->getPipelineLayout();
    info.basePipelineIndex    = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr && vr != VK_PIPELINE_COMPILE_REQUIRED_EXT)
      Logger::err(str::format("DxvkShaderPipelineLibrary: Failed to create vertex shader pipeline: ", vr));

    return vr ? VK_NULL_HANDLE : pipeline;
  }


  VkPipeline DxvkShaderPipelineLibrary::compileFragmentShaderPipeline(
    const DxvkShaderStageInfo&          stageInfo,
          VkPipelineCreateFlags2        flags) {
    auto vk = m_device->vkd();

    // Set up dynamic state. We do not know any pipeline state
    // at this time, so make as much state dynamic as we can.
    uint32_t dynamicStateCount = 0;
    std::array<VkDynamicState, 13> dynamicStates;

    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_COMPARE_OP;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_OP;

    if (m_device->features().core.features.depthBounds) {
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE;
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;
    }

    bool hasSampleRateShading = m_shaders.fs && m_shaders.fs->metadata().flags.test(DxvkShaderFlag::HasSampleRateShading);
    bool hasDynamicMultisampleState = hasSampleRateShading
      && m_device->features().extExtendedDynamicState3.extendedDynamicState3RasterizationSamples
      && m_device->features().extExtendedDynamicState3.extendedDynamicState3SampleMask;

    if (hasDynamicMultisampleState) {
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT;
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_SAMPLE_MASK_EXT;

      if (!m_shaders.fs || !m_shaders.fs->metadata().flags.test(DxvkShaderFlag::ExportsSampleMask)) {
        if (m_device->features().extExtendedDynamicState3.extendedDynamicState3AlphaToCoverageEnable)
          dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT;
      }
    }

    VkPipelineDynamicStateCreateInfo dyInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dyInfo.dynamicStateCount  = dynamicStateCount;
    dyInfo.pDynamicStates     = dynamicStates.data();

    // Set up multisample state. If sample shading is enabled, assume that
    // we only have one sample enabled, with a non-zero sample mask and no
    // alpha-to-coverage.
    VkSampleMask msSampleMask = 0x1;

    VkPipelineMultisampleStateCreateInfo msInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msInfo.sampleShadingEnable  = VK_TRUE;
    msInfo.minSampleShading     = 1.0f;

    if (!hasDynamicMultisampleState) {
      msInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
      msInfo.pSampleMask          = &msSampleMask;
    }

    // All depth-stencil state is dynamic, so no need to initialize this.
    // Depth bounds testing is disabled on devices which don't support it.
    VkPipelineDepthStencilStateCreateInfo dsInfo = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

    // Only the view mask is used as input, and since we do not use MultiView, it is always 0
    VkPipelineRenderingCreateInfo rtInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

    VkPipelineCreateFlags2CreateInfo flagsInfo = { VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO, &rtInfo };
    flagsInfo.flags = VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR | flags;

    if (m_device->canUseDescriptorBuffer())
      flagsInfo.flags |= VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT;

    VkGraphicsPipelineLibraryCreateInfoEXT libInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, &flagsInfo };
    libInfo.flags             = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
    info.stageCount           = stageInfo.getStageCount();
    info.pStages              = stageInfo.getStageInfos();
    info.pDepthStencilState   = &dsInfo;
    info.pDynamicState        = &dyInfo;
    info.layout               = m_layout.getLayout(DxvkPipelineLayoutType::Independent)->getPipelineLayout();
    info.basePipelineIndex    = -1;

    if (hasSampleRateShading)
      info.pMultisampleState  = &msInfo;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr && !(flags & VK_PIPELINE_CREATE_2_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT))
      Logger::err(str::format("DxvkShaderPipelineLibrary: Failed to create fragment shader pipeline: ", vr));

    return vr ? VK_NULL_HANDLE : pipeline;
  }


  VkPipeline DxvkShaderPipelineLibrary::compileComputeShaderPipeline(
    const DxvkShaderStageInfo&          stageInfo,
          VkPipelineCreateFlags2        flags) {
    auto vk = m_device->vkd();

    // Compile the compute pipeline as normal
    VkPipelineCreateFlags2CreateInfo flagsInfo = { VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO };
    flagsInfo.flags = flags;

    if (m_device->canUseDescriptorBuffer())
      flagsInfo.flags |= VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT;

    VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, &flagsInfo };
    info.stage        = *stageInfo.getStageInfos();
    info.layout       = m_layout.getLayout(DxvkPipelineLayoutType::Merged)->getPipelineLayout();
    info.basePipelineIndex = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateComputePipelines(vk->device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr && vr != VK_PIPELINE_COMPILE_REQUIRED_EXT)
      Logger::err(str::format("DxvkShaderPipelineLibrary: Failed to create compute shader pipeline: ", vr));

    return vr ? VK_NULL_HANDLE : pipeline;
  }


  SpirvCodeBuffer DxvkShaderPipelineLibrary::getShaderCode(VkShaderStageFlagBits stage) const {
    // As a special case, it is possible that we have to deal with
    // a null shader, but the pipeline library extension requires
    // us to always specify a fragment shader for fragment stages,
    // so we need to return a dummy shader in that case.
    DxvkShader* shader = getShader(stage);

    if (!shader)
      return SpirvCodeBuffer(dxvk_dummy_frag);

    DxvkPipelineLayoutType layoutType = stage == VK_SHADER_STAGE_COMPUTE_BIT
      ? DxvkPipelineLayoutType::Merged
      : DxvkPipelineLayoutType::Independent;

    return shader->getCode(m_layout.getBindingMap(layoutType), DxvkShaderModuleCreateInfo());
  }


  void DxvkShaderPipelineLibrary::generateModuleIdentifierLocked(
          VkShaderModuleIdentifierEXT*  identifier,
    const SpirvCodeBuffer&              spirvCode) {
    auto vk = m_device->vkd();

    if (!canUsePipelineCacheControl())
      return;

    VkShaderModuleCreateInfo info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    info.codeSize = spirvCode.size();
    info.pCode = spirvCode.data();

    vk->vkGetShaderModuleCreateInfoIdentifierEXT(
      vk->device(), &info, identifier);
  }


  VkShaderStageFlags DxvkShaderPipelineLibrary::getShaderStages() const {
    if (m_shaders.vs) {
      VkShaderStageFlags result = VK_SHADER_STAGE_VERTEX_BIT;

      if (m_shaders.tcs)
        result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

      if (m_shaders.tes)
        result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

      if (m_shaders.gs)
        result |= VK_SHADER_STAGE_GEOMETRY_BIT;

      return result;
    }

    if (m_shaders.cs)
      return VK_SHADER_STAGE_COMPUTE_BIT;

    // Must be a fragment shader even if fs is null
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  }


  DxvkShader* DxvkShaderPipelineLibrary::getShader(
          VkShaderStageFlagBits         stage) const {
    switch (stage) {
      case VK_SHADER_STAGE_VERTEX_BIT:
        return m_shaders.vs;

      case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return m_shaders.tcs;

      case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return m_shaders.tes;

      case VK_SHADER_STAGE_GEOMETRY_BIT:
        return m_shaders.gs;

      case VK_SHADER_STAGE_FRAGMENT_BIT:
        return m_shaders.fs;

      case VK_SHADER_STAGE_COMPUTE_BIT:
        return m_shaders.cs;

      default:
        return nullptr;
    }
  }


  VkShaderModuleIdentifierEXT* DxvkShaderPipelineLibrary::getShaderIdentifier(
          VkShaderStageFlagBits         stage) {
    switch (stage) {
      case VK_SHADER_STAGE_VERTEX_BIT:
        return &m_identifiers.vs;

      case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return &m_identifiers.tcs;

      case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return &m_identifiers.tes;

      case VK_SHADER_STAGE_GEOMETRY_BIT:
        return &m_identifiers.gs;

      case VK_SHADER_STAGE_FRAGMENT_BIT:
        return &m_identifiers.fs;

      case VK_SHADER_STAGE_COMPUTE_BIT:
        return &m_identifiers.cs;

      default:
        return nullptr;
    }
  }


  void DxvkShaderPipelineLibrary::notifyLibraryCompile() const {
    if (m_shaders.vs) {
      // Only notify the shader itself if we're actually
      // building the shader's standalone pipeline library
      if (!m_shaders.tcs && !m_shaders.tes && !m_shaders.gs)
        m_shaders.vs->notifyCompile();
    }

    if (m_shaders.fs)
      m_shaders.fs->notifyCompile();

    if (m_shaders.cs)
      m_shaders.cs->notifyCompile();
  }


  bool DxvkShaderPipelineLibrary::canUsePipelineCacheControl() const {
    const auto& features = m_device->features();

    return features.vk13.pipelineCreationCacheControl
        && features.extShaderModuleIdentifier.shaderModuleIdentifier;
  }

}
