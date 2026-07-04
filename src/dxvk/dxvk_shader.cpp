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


  bool DxvkShaderLinkage::eq(const DxvkShaderLinkage& other) const {
    bool eq = fsDualSrcBlend  == other.fsDualSrcBlend
           && fsFlatShading   == other.fsFlatShading
           && sampleLocations == other.sampleLocations
           && semanticIo      == other.semanticIo;

    if (eq) {
      eq = prevStageOutputs.getVarCount() == other.prevStageOutputs.getVarCount();

      for (uint32_t i = 0; i < prevStageOutputs.getVarCount() && eq; i++)
        eq = prevStageOutputs.getVar(i).eq(other.prevStageOutputs.getVar(i));
    }

    for (uint32_t i = 0; i < rtSwizzles.size() && eq; i++) {
      eq = rtSwizzles[i].r == other.rtSwizzles[i].r
        && rtSwizzles[i].g == other.rtSwizzles[i].g
        && rtSwizzles[i].b == other.rtSwizzles[i].b
        && rtSwizzles[i].a == other.rtSwizzles[i].a;
    }

    return eq;
  }


  size_t DxvkShaderLinkage::hash() const {
    DxvkHashState hash;
    hash.add(uint32_t(fsDualSrcBlend));
    hash.add(uint32_t(fsFlatShading));
    hash.add(uint32_t(sampleLocations));
    hash.add(uint32_t(semanticIo));

    for (uint32_t i = 0; i < prevStageOutputs.getVarCount(); i++)
      hash.add(prevStageOutputs.getVar(i).hash());

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


  const std::string& DxvkShader::getShaderDumpPath() {
    static std::string s_path = env::getEnvVar("DXVK_SHADER_DUMP_PATH");
    return s_path;
  }


  

  DxvkShaderStageInfo::DxvkShaderStageInfo(const DxvkDevice* device, const DxvkPipelineLayout* layout)
  : m_device(device) {
    if (m_device->canUseDescriptorHeap()) {
      m_mapping = layout->getMappingInfo();
      m_next = &m_mapping;
    }
  }

  void DxvkShaderStageInfo::addStage(
          VkShaderStageFlagBits   stage,
          SpirvCodeBuffer&&       code,
    const VkSpecializationInfo*   specInfo) {
    // Take ownership of the SPIR-V code buffer
    auto& codeBuffer = m_codeBuffers[m_stageCount];
    codeBuffer = std::move(code);

    auto& moduleInfo = m_moduleInfos[m_stageCount].moduleInfo;
    moduleInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, m_next };
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
          VkShaderStageFlagBits   stage) {
    auto& stageInfo = m_stageInfos[m_stageCount++];
    stageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, m_next };
    stageInfo.stage = stage;
    stageInfo.pName = "main";
  }


  DxvkShaderStageInfo::~DxvkShaderStageInfo() {

  }


  DxvkShaderPipelineLibraryKey::DxvkShaderPipelineLibraryKey() {

  }


  DxvkShaderPipelineLibraryKey::~DxvkShaderPipelineLibraryKey() {

  }


  void DxvkShaderPipelineLibraryKey::addShader(
          Rc<DxvkShader>                shader) {
    m_shaders.push_back(std::move(shader));
  }


  bool DxvkShaderPipelineLibraryKey::eq(
    const DxvkShaderPipelineLibraryKey& other) const {
    if (m_shaders.size() != other.m_shaders.size())
      return false;

    bool eq = true;

    for (uint32_t i = 0; i < m_shaders.size() && eq; i++)
      eq = m_shaders[i] == other.m_shaders[i];

    return eq;
  }


  size_t DxvkShaderPipelineLibraryKey::hash() const {
    DxvkHashState hash;

    for (uint32_t i = 0; i < m_shaders.size(); i++)
      hash.add(m_shaders[i]->getCookie());

    return hash;
  }


  DxvkShaderPipelineLibrary::DxvkShaderPipelineLibrary(
          DxvkDevice*               device,
          DxvkPipelineManager*      manager,
    const DxvkShaderPipelineLibraryKey& key)
  : m_device      (device),
    m_manager     (manager),
    m_shaders     (key) {

  }


  DxvkShaderPipelineLibrary::~DxvkShaderPipelineLibrary() {
    this->destroyShaderPipelineLocked();
  }


  VkPipeline DxvkShaderPipelineLibrary::acquirePipelineHandle() {
    std::lock_guard lock(m_mutex);

    if (m_device->mustTrackPipelineLifetime())
      m_useCount += 1;

    if (!m_pipeline)
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
    VkPipeline pipeline = compileShaderPipelineLocked();

    if (!pipeline)
      return;

    if (m_device->mustTrackPipelineLifetime()) {
      // Destroy pipeline to save memory and rely on unmapping
      // or the driver cache to recreate it on the next use.
      auto vk = m_device->vkd();
      vk->vkDestroyPipeline(vk->device(), pipeline, nullptr);
    } else {
      // Write back pipeline handle for future use
      m_pipeline = pipeline;
    }
  }


  void DxvkShaderPipelineLibrary::destroyShaderPipelineLocked() {
    auto vk = m_device->vkd();

    if (m_pipeline)
      vk->vkDestroyPipeline(vk->device(), m_pipeline, nullptr);

    if (std::exchange(m_compiledWithBinaries, false)) {
      for (const auto& binary : m_binaries)
        m_manager->releaseBinary(binary);
    }

    m_pipeline = VK_NULL_HANDLE;
  }


  VkPipeline DxvkShaderPipelineLibrary::compileShaderPipelineLocked() {
    compileShaders();

    // Increment stat counter the first time this
    // shader pipeline gets compiled successfully
    bool compiledBefore = std::exchange(m_compiledOnce, true);

    if (!compiledBefore) {
      if (m_shaders.findShader(VK_SHADER_STAGE_COMPUTE_BIT))
        m_manager->m_stats.numComputePipelines += 1;
      else
        m_manager->m_stats.numGraphicsLibraries += 1;
    }

    if (!canCreatePipelineLibrary())
      return VK_NULL_HANDLE;

    this->notifyLibraryCompile();

    VkPipeline pipeline = VK_NULL_HANDLE;

    if (!m_binaries.empty())
      pipeline = this->compileShaderPipelineWithBinaries();

    if (!pipeline)
      pipeline = this->compileShaderPipeline(nullptr);

    return pipeline;
  }


  VkPipeline DxvkShaderPipelineLibrary::compileShaderPipelineWithBinaries() {
    auto vk = m_device->vkd();

    std::vector<VkPipelineBinaryKHR> binaries(m_binaries.size());

    bool success = true;

    for (size_t i = 0u; i < m_binaries.size() && success; i++) {
      if (!(binaries.at(i) = m_manager->acquireBinary(m_binaries.at(i))))
        success = false;
    }

    VkPipeline pipeline = VK_NULL_HANDLE;

    if (success) {
      VkPipelineBinaryInfoKHR binaryInfo = { VK_STRUCTURE_TYPE_PIPELINE_BINARY_INFO_KHR };
      binaryInfo.binaryCount = binaries.size();
      binaryInfo.pPipelineBinaries = binaries.data();

      if (!(pipeline = compileShaderPipeline(&binaryInfo)))
        Logger::warn(str::format("DXVK: Failed to create pipeline from binaries"));
    }

    // We referenced all the binaries once, make sure to
    // release them once the pipeline is no longer needed.
    m_compiledWithBinaries = true;
    return pipeline;
  }


  VkPipeline DxvkShaderPipelineLibrary::compileShaderPipeline(
    const VkPipelineBinaryInfoKHR*      binaries) {
    DxvkShaderStageInfo stageInfo(m_device, getPipelineLibraryLayout());
    VkShaderStageFlags stageMask = getShaderStages();

    for (auto stages = stageMask; stages; stages &= stages - 1u) {
      auto stage = VkShaderStageFlagBits(stages & -stages);

      if (binaries) {
        stageInfo.addStage(stage);
      } else {
        SpirvCodeBuffer spirvCode = this->getShaderCode(stage);
        stageInfo.addStage(stage, std::move(spirvCode), nullptr);
      }

      stages &= stages - 1;
    }

    VkPipeline pipeline = VK_NULL_HANDLE;

    VkPipelineCreateFlags2 flags = 0u;

    if (m_device->mustTrackPipelineLifetime()
     && m_device->features().khrPipelineBinary.pipelineBinaries
     && m_binaries.empty())
      flags |= VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR;

    if (stageMask & VK_SHADER_STAGE_VERTEX_BIT)
      pipeline = compileVertexShaderPipeline(stageInfo, flags, binaries);
    else if (stageMask & VK_SHADER_STAGE_FRAGMENT_BIT)
      pipeline = compileFragmentShaderPipeline(stageInfo, flags, binaries);
    else if (stageMask & VK_SHADER_STAGE_COMPUTE_BIT)
      pipeline = compileComputeShaderPipeline(stageInfo, flags, binaries);

    // Write pipeline binaries into unmappable memory and try to reuse
    // those for subsequent uses of the pipeline. If pipeline binaries
    // are unavaialble, pray that we hit the driver's own cache.
    if (flags & VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR)
      queryPipelineBinaries(pipeline);

    return pipeline;
  }


  VkPipeline DxvkShaderPipelineLibrary::compileVertexShaderPipeline(
    const DxvkShaderStageInfo&          stageInfo,
          VkPipelineCreateFlags2        flags,
    const VkPipelineBinaryInfoKHR*      binaries) {
    auto vk = m_device->vkd();

    // Set up dynamic state. We do not know any pipeline state
    // at this time, so make as much state dynamic as we can.
    small_vector<VkDynamicState, 16> dynamicStates;

    dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT);
    dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT);
    dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
    dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE);
    dynamicStates.push_back(VK_DYNAMIC_STATE_CULL_MODE);
    dynamicStates.push_back(VK_DYNAMIC_STATE_FRONT_FACE);

    if (m_device->features().extExtendedDynamicState3.extendedDynamicState3DepthClipEnable)
      dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT);

    VkPipelineDynamicStateCreateInfo dyInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dyInfo.dynamicStateCount  = dynamicStates.size();
    dyInfo.pDynamicStates     = dynamicStates.data();

    // If a tessellation control shader is present, grab the patch vertex count
    VkPipelineTessellationStateCreateInfo tsInfo = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };

    auto tcs = m_shaders.findShader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);

    if (tcs)
      tsInfo.patchControlPoints = tcs->metadata().patchVertexCount;

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

    // Only use the fixed depth clip state if we can't make it dynamic
    if (!m_device->features().extExtendedDynamicState3.extendedDynamicState3DepthClipEnable) {
      rsDepthClipInfo.pNext = std::exchange(rsInfo.pNext, &rsDepthClipInfo);
      rsDepthClipInfo.depthClipEnable = VK_TRUE;
    }

    // Only the view mask is used as input, and since we do not use MultiView, it is always 0
    VkPipelineRenderingCreateInfo rtInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, binaries };

    VkPipelineCreateFlags2CreateInfo flagsInfo = { VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO, &rtInfo };
    flagsInfo.flags = VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR | flags;

    if (m_device->canUseDescriptorHeap())
      flagsInfo.flags |= VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    if (m_device->canUseDescriptorBuffer())
      flagsInfo.flags |= VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT;

    VkGraphicsPipelineLibraryCreateInfoEXT libInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, &flagsInfo };
    libInfo.flags             = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
    info.stageCount           = stageInfo.getStageCount();
    info.pStages              = stageInfo.getStageInfos();
    info.pTessellationState   = m_shaders.findShader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ? &tsInfo : nullptr;
    info.pViewportState       = &vpInfo;
    info.pRasterizationState  = &rsInfo;
    info.pDynamicState        = &dyInfo;
    info.layout               = getPipelineLibraryLayout()->getPipelineLayout();
    info.basePipelineIndex    = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr)
      Logger::err(str::format("DxvkShaderPipelineLibrary: Failed to create vertex shader pipeline: ", vr));

    return vr ? VK_NULL_HANDLE : pipeline;
  }


  VkPipeline DxvkShaderPipelineLibrary::compileFragmentShaderPipeline(
    const DxvkShaderStageInfo&          stageInfo,
          VkPipelineCreateFlags2        flags,
    const VkPipelineBinaryInfoKHR*      binaries) {
    auto vk = m_device->vkd();

    // Set up dynamic state. We do not know any pipeline state
    // at this time, so make as much state dynamic as we can.
    small_vector<VkDynamicState, 16> dynamicStates;

    dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
    dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
    dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);
    dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
    dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
    dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
    dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE);
    dynamicStates.push_back(VK_DYNAMIC_STATE_STENCIL_OP);

    if (m_device->features().core.features.depthBounds) {
      dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE);
      dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
    }

    auto fs = m_shaders.findShader(VK_SHADER_STAGE_FRAGMENT_BIT);

    bool hasSampleRateShading = fs && fs->metadata().flags.test(DxvkShaderFlag::HasSampleRateShading);
    bool hasDynamicMultisampleState = hasSampleRateShading
      && m_device->features().extExtendedDynamicState3.extendedDynamicState3RasterizationSamples
      && m_device->features().extExtendedDynamicState3.extendedDynamicState3SampleMask;
    bool hasDynamicSampleLocations = m_device->canUseSampleLocations(0u);

    if (hasDynamicMultisampleState) {
      dynamicStates.push_back(VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT);
      dynamicStates.push_back(VK_DYNAMIC_STATE_SAMPLE_MASK_EXT);

      if (!fs || !fs->metadata().flags.test(DxvkShaderFlag::ExportsSampleMask)) {
        if (m_device->features().extExtendedDynamicState3.extendedDynamicState3AlphaToCoverageEnable)
          dynamicStates.push_back(VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT);
      }
    }

    if (hasDynamicSampleLocations) {
      dynamicStates.push_back(VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT);
      dynamicStates.push_back(VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT);
    }

    VkPipelineDynamicStateCreateInfo dyInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dyInfo.dynamicStateCount  = dynamicStates.size();
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
    VkPipelineRenderingCreateInfo rtInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, binaries };

    VkPipelineCreateFlags2CreateInfo flagsInfo = { VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO, &rtInfo };
    flagsInfo.flags = VK_PIPELINE_CREATE_2_LIBRARY_BIT_KHR | flags;

    if (m_device->canUseDescriptorHeap())
      flagsInfo.flags |= VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    if (m_device->canUseDescriptorBuffer())
      flagsInfo.flags |= VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT;

    VkGraphicsPipelineLibraryCreateInfoEXT libInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, &flagsInfo };
    libInfo.flags             = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
    info.stageCount           = stageInfo.getStageCount();
    info.pStages              = stageInfo.getStageInfos();
    info.pDepthStencilState   = &dsInfo;
    info.pDynamicState        = &dyInfo;
    info.layout               = m_layout->getLayout(DxvkPipelineLayoutType::Independent)->getPipelineLayout();
    info.basePipelineIndex    = -1;

    if (hasSampleRateShading)
      info.pMultisampleState  = &msInfo;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr)
      Logger::err(str::format("DxvkShaderPipelineLibrary: Failed to create fragment shader pipeline: ", vr));

    return vr ? VK_NULL_HANDLE : pipeline;
  }


  VkPipeline DxvkShaderPipelineLibrary::compileComputeShaderPipeline(
    const DxvkShaderStageInfo&          stageInfo,
          VkPipelineCreateFlags2        flags,
    const VkPipelineBinaryInfoKHR*      binaries) {
    auto vk = m_device->vkd();

    // Compile the compute pipeline as normal
    VkPipelineCreateFlags2CreateInfo flagsInfo = { VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO };
    flagsInfo.flags |= flags;

    if (m_device->canUseDescriptorHeap())
      flagsInfo.flags |= VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

    if (m_device->canUseDescriptorBuffer())
      flagsInfo.flags |= VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT;

    VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, binaries };
    info.stage        = *stageInfo.getStageInfos();
    info.layout       = getPipelineLibraryLayout()->getPipelineLayout();
    info.basePipelineIndex = -1;

    if (flagsInfo.flags)
      flagsInfo.pNext = std::exchange(info.pNext, &flagsInfo);

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateComputePipelines(vk->device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr)
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

    return shader->getCode(m_layout->getBindingMap(layoutType), nullptr);
  }


  VkShaderStageFlags DxvkShaderPipelineLibrary::getShaderStages() const {
    VkShaderStageFlags result = 0u;

    for (uint32_t i = 0u; i < m_shaders.getShaderCount(); i++)
      result |= m_shaders.getShader(i)->metadata().stage;

    // Must be a fragment shader even if fs is null
    if (!result)
      result = VK_SHADER_STAGE_FRAGMENT_BIT;

    return result;
  }


  DxvkShader* DxvkShaderPipelineLibrary::getShader(
          VkShaderStageFlagBits         stage) const {
    return m_shaders.findShader(stage);
  }


  void DxvkShaderPipelineLibrary::compileShaders() {
    if (m_layout)
      return;

    VkShaderStageFlags stages = 0u;

    for (uint32_t i = 0u; i < m_shaders.getShaderCount(); i++) {
      auto shader = m_shaders.getShader(i);
      shader->compile();
      stages |= shader->metadata().stage;
    }

    if (!stages)
      stages = VK_SHADER_STAGE_FRAGMENT_BIT;

    DxvkPipelineLayoutBuilder layoutBuilder(stages);

    for (uint32_t i = 0u; i < m_shaders.getShaderCount(); i++) {
      auto shader = m_shaders.getShader(i);
      layoutBuilder.addLayout(shader->getLayout());
    }

    m_layout.emplace(m_device, m_manager, layoutBuilder);
  }


  bool DxvkShaderPipelineLibrary::canCreatePipelineLibrary() const {
    // Check whether device supports GPL at all
    if (!m_device->canUseGraphicsPipelineLibrary())
      return false;

    // Can only create pre-raster pipelines if all shaders are present
    if ((m_shaders.findShader(VK_SHADER_STAGE_GEOMETRY_BIT)
      || m_shaders.findShader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
      || m_shaders.findShader(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
     && !m_shaders.findShader(VK_SHADER_STAGE_VERTEX_BIT))
      return false;

    // The final geometry stage must export position
    DxvkShader* lastPreRasterStage = m_shaders.findShader(VK_SHADER_STAGE_GEOMETRY_BIT);

    if (!lastPreRasterStage)
      lastPreRasterStage = m_shaders.findShader(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

    if (!lastPreRasterStage)
      lastPreRasterStage = m_shaders.findShader(VK_SHADER_STAGE_VERTEX_BIT);

    for (uint32_t i = 0u; i < m_shaders.getShaderCount(); i++) {
      auto currShader = m_shaders.getShader(i);

      if (!canCreatePipelineLibraryForShader(*currShader, currShader == lastPreRasterStage))
        return false;

      if (i) {
        // Ensure that stage I/O is compatible between stages
        auto prevShader = m_shaders.getShader(i - 1u);

        const auto& prevShaderMeta = prevShader->metadata();
        const auto& currShaderMeta = currShader->metadata();

        bool semanticIo = currShaderMeta.flags.test(DxvkShaderFlag::SemanticIo)
                       && prevShaderMeta.flags.test(DxvkShaderFlag::SemanticIo);

        if (!DxvkShaderIo::checkStageCompatibility(
            currShaderMeta.stage, currShaderMeta.inputs,
            prevShaderMeta.stage, prevShaderMeta.outputs, semanticIo))
          return false;
      }
    }

    return true;
  }


  bool DxvkShaderPipelineLibrary::canCreatePipelineLibraryForShader(DxvkShader& shader, bool needsPosition) const {
    const auto& metadata = shader.metadata();

    // Tessellation control shaders must define a valid vertex count
    if (metadata.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
      && (metadata.patchVertexCount < 1 || metadata.patchVertexCount > 32))
      return false;

    // We don't support GPL with transform feedback right now
    if (metadata.flags.test(DxvkShaderFlag::HasTransformFeedback))
      return false;

    // Pre-raster pipelines must export vertex position to be useful. This
    // stops us from compiling vertex shader libraries that are only used
    // as input for tessellation or geometry shaers.
    if (needsPosition && !metadata.flags.test(DxvkShaderFlag::ExportsPosition))
      return false;

    // Dynamic spec constants are only supported in graphics
    if (metadata.specConstantMask && metadata.stage == VK_SHADER_STAGE_COMPUTE_BIT)
      return false;

    return true;
  }


  void DxvkShaderPipelineLibrary::notifyLibraryCompile() const {
    // Only notify the shader itself if we're actually
    // building the shader's standalone pipeline library
    if (m_shaders.getShaderCount() == 1u)
      m_shaders.getShader(0u)->notifyCompile();
  }


  void DxvkShaderPipelineLibrary::queryPipelineBinaries(VkPipeline pipeline) {
    auto vk = m_device->vkd();

    VkPipelineBinaryCreateInfoKHR info = { VK_STRUCTURE_TYPE_PIPELINE_BINARY_CREATE_INFO_KHR };
    info.pipeline = pipeline;

    VkPipelineBinaryHandlesInfoKHR handles = { VK_STRUCTURE_TYPE_PIPELINE_BINARY_HANDLES_INFO_KHR };
    VkResult vr = vk->vkCreatePipelineBinariesKHR(vk->device(), &info, nullptr, &handles);

    if (vr < 0 || !handles.pipelineBinaryCount) {
      Logger::err(str::format("DXVK: Failed to create pipeline binaries: ", vr));
      return;
    }

    std::vector<VkPipelineBinaryKHR> binaries(handles.pipelineBinaryCount);
    handles.pPipelineBinaries = binaries.data();

    vr = vk->vkCreatePipelineBinariesKHR(vk->device(), &info, nullptr, &handles);

    if (vr != VK_SUCCESS) {
      Logger::err(str::format("DXVK: Failed to create pipeline binaries: ", vr));
      return;
    }

    // Retrieve actual pipeline binary data
    bool success = true;

    for (size_t i = 0u; i < handles.pipelineBinaryCount; i++) {
      auto key = m_manager->insertBinary(binaries.at(i));

      if (!(success = key.has_value())) {
        Logger::err(str::format("DXVK: Failed to retrieve pipeline binary"));
        break;
      }

      m_binaries.push_back(key.value());
    }

    // Clean up binary objects regardless of success
    for (auto binary : binaries)
      vk->vkDestroyPipelineBinaryKHR(vk->device(), binary, nullptr);

    if (!success)
      m_binaries.clear();

    // Won't need pipeline info anymore from this pipeline
    VkReleaseCapturedPipelineDataInfoKHR releaseInfo = { VK_STRUCTURE_TYPE_RELEASE_CAPTURED_PIPELINE_DATA_INFO_KHR };
    releaseInfo.pipeline = pipeline;

    vk->vkReleaseCapturedPipelineDataKHR(vk->device(), &releaseInfo, nullptr);
  }


  const DxvkPipelineLayout* DxvkShaderPipelineLibrary::getPipelineLibraryLayout() const {
    DxvkPipelineLayoutType type = m_shaders.findShader(VK_SHADER_STAGE_COMPUTE_BIT)
      ? DxvkPipelineLayoutType::Merged
      : DxvkPipelineLayoutType::Independent;

    return m_layout->getLayout(type);
  }

}
