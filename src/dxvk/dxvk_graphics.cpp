#include "dxvk_graphics.h"

namespace dxvk {
  
  template<typename T>
  size_t hashPtr(T* ptr) {
    return reinterpret_cast<size_t>(ptr);
  }
  
  size_t DxvkGraphicsPipelineStateInfo::hash() const {
    DxvkHashState state;
    state.add(hashPtr(this->inputAssemblyState.ptr()));
    state.add(hashPtr(this->inputLayout.ptr()));
    state.add(hashPtr(this->rasterizerState.ptr()));
    state.add(hashPtr(this->multisampleState.ptr()));
    state.add(hashPtr(this->depthStencilState.ptr()));
    state.add(hashPtr(this->blendState.ptr()));
    state.add(std::hash<VkRenderPass>()(this->renderPass));
    state.add(viewportCount);
    return state;
  }
  
  
  bool DxvkGraphicsPipelineStateInfo::operator == (const DxvkGraphicsPipelineStateInfo& other) const {
    return this->inputAssemblyState == other.inputAssemblyState
        && this->inputLayout        == other.inputLayout
        && this->rasterizerState    == other.rasterizerState
        && this->multisampleState   == other.multisampleState
        && this->depthStencilState  == other.depthStencilState
        && this->blendState         == other.blendState
        && this->renderPass         == other.renderPass
        && this->viewportCount      == other.viewportCount;
  }
  
  
  bool DxvkGraphicsPipelineStateInfo::operator != (const DxvkGraphicsPipelineStateInfo& other) const {
    return !this->operator == (other);
  }
  
  
  DxvkGraphicsPipeline::DxvkGraphicsPipeline(
      const Rc<vk::DeviceFn>&      vkd,
      const Rc<DxvkBindingLayout>& layout,
      const Rc<DxvkShader>&        vs,
      const Rc<DxvkShader>&        tcs,
      const Rc<DxvkShader>&        tes,
      const Rc<DxvkShader>&        gs,
      const Rc<DxvkShader>&        fs)
  : m_vkd(vkd), m_layout(layout),
    m_vs(vs), m_tcs(tcs), m_tes(tes), m_gs(gs), m_fs(fs) {
    
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
    
    VkPipelineViewportStateCreateInfo vpInfo;
    vpInfo.sType                = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpInfo.pNext                = nullptr;
    vpInfo.flags                = 0;
    vpInfo.viewportCount        = state.viewportCount;
    vpInfo.pViewports           = nullptr;
    vpInfo.scissorCount         = state.viewportCount;
    vpInfo.pViewports           = nullptr;
    
    VkPipelineDynamicStateCreateInfo dsInfo;
    dsInfo.sType                = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dsInfo.pNext                = nullptr;
    dsInfo.flags                = 0;
    dsInfo.dynamicStateCount    = dynamicStates.size();
    dsInfo.pDynamicStates       = dynamicStates.data();
    
    VkGraphicsPipelineCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.stageCount             = stages.size();
    info.pStages                = stages.data();
    info.pVertexInputState      = &state.inputLayout->info();
    info.pInputAssemblyState    = &state.inputAssemblyState->info();
    info.pTessellationState     = nullptr;  // TODO implement
    info.pViewportState         = &vpInfo;
    info.pRasterizationState    = &state.rasterizerState->info();
    info.pMultisampleState      = &state.multisampleState->info();
    info.pDepthStencilState     = &state.depthStencilState->info();
    info.pColorBlendState       = &state.blendState->info();
    info.pDynamicState          = &dsInfo;
    info.layout                 = m_layout->pipelineLayout();
    info.renderPass             = state.renderPass;
    info.subpass                = 0;
    info.basePipelineHandle     = VK_NULL_HANDLE;
    info.basePipelineIndex      = 0;
    
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