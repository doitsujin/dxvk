#include <cstring>

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
  
  
  size_t DxvkGraphicsPipelineStateHash::operator () (
    const DxvkGraphicsPipelineStateInfo& state) const {
    // TODO implement hash
    return 0;
  }
  
  
  size_t DxvkGraphicsPipelineStateEq::operator () (
    const DxvkGraphicsPipelineStateInfo& a,
    const DxvkGraphicsPipelineStateInfo& b) const {
    return std::memcmp(&a, &b, sizeof(DxvkGraphicsPipelineStateInfo)) == 0;
  }
  
  
  DxvkGraphicsPipeline::DxvkGraphicsPipeline(
      const Rc<vk::DeviceFn>& vkd,
      const Rc<DxvkShader>&   vs,
      const Rc<DxvkShader>&   tcs,
      const Rc<DxvkShader>&   tes,
      const Rc<DxvkShader>&   gs,
      const Rc<DxvkShader>&   fs)
  : m_vkd(vkd) {
    DxvkDescriptorSlotMapping slotMapping;
    if (vs  != nullptr) vs ->defineResourceSlots(slotMapping);
    if (tcs != nullptr) tcs->defineResourceSlots(slotMapping);
    if (tes != nullptr) tes->defineResourceSlots(slotMapping);
    if (gs  != nullptr) gs ->defineResourceSlots(slotMapping);
    if (fs  != nullptr) fs ->defineResourceSlots(slotMapping);
    
    m_layout = new DxvkBindingLayout(vkd,
      slotMapping.bindingCount(),
      slotMapping.bindingInfos());
    
    if (vs  != nullptr) m_vs  = vs ->createShaderModule(vkd, slotMapping);
    if (tcs != nullptr) m_tcs = tcs->createShaderModule(vkd, slotMapping);
    if (tes != nullptr) m_tes = tes->createShaderModule(vkd, slotMapping);
    if (gs  != nullptr) m_gs  = gs ->createShaderModule(vkd, slotMapping);
    if (fs  != nullptr) m_fs  = fs ->createShaderModule(vkd, slotMapping);
  }
  
  
  DxvkGraphicsPipeline::~DxvkGraphicsPipeline() {
    this->destroyPipelines();
  }
  
  
  VkPipeline DxvkGraphicsPipeline::getPipelineHandle(
    const DxvkGraphicsPipelineStateInfo& state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto pair = m_pipelines.find(state);
    if (pair != m_pipelines.end())
      return pair->second;
    
    VkPipeline pipeline = this->compilePipeline(state);
    m_pipelines.insert(std::make_pair(state, pipeline));
    return pipeline;
  }
  
  
  VkPipeline DxvkGraphicsPipeline::compilePipeline(
    const DxvkGraphicsPipelineStateInfo& state) const {
    std::array<VkDynamicState, 4> dynamicStates = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_BLEND_CONSTANTS,
      VK_DYNAMIC_STATE_STENCIL_REFERENCE,
    };
    
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    
    if (m_vs  != nullptr) stages.push_back(m_vs->stageInfo());
    if (m_tcs != nullptr) stages.push_back(m_tcs->stageInfo());
    if (m_tes != nullptr) stages.push_back(m_tes->stageInfo());
    if (m_gs  != nullptr) stages.push_back(m_gs->stageInfo());
    if (m_fs  != nullptr) stages.push_back(m_fs->stageInfo());
    
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
    info.flags                    = 0;
    info.stageCount               = stages.size();
    info.pStages                  = stages.data();
    info.pVertexInputState        = &viInfo;
    info.pInputAssemblyState      = &iaInfo;
    info.pTessellationState       = nullptr;  // TODO implement
    info.pViewportState           = &vpInfo;
    info.pRasterizationState      = &rsInfo;
    info.pMultisampleState        = &msInfo;
    info.pDepthStencilState       = &dsInfo;
    info.pColorBlendState         = &cbInfo;
    info.pDynamicState            = &dyInfo;
    info.layout                   = m_layout->pipelineLayout();
    info.renderPass               = state.omRenderPass;
    info.subpass                  = 0;
    info.basePipelineHandle       = VK_NULL_HANDLE; // TODO use this
    info.basePipelineIndex        = 0;
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (m_vkd->vkCreateGraphicsPipelines(m_vkd->device(),
          VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
      throw DxvkError("DxvkGraphicsPipeline::DxvkGraphicsPipeline: Failed to compile pipeline");
    return pipeline;
  }
  
  
  void DxvkGraphicsPipeline::destroyPipelines() {
    for (const auto& pair : m_pipelines) {
      m_vkd->vkDestroyPipeline(
        m_vkd->device(), pair.second, nullptr);
    }
  }
  
}