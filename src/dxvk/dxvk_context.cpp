#include "dxvk_device.h"
#include "dxvk_context.h"
#include "dxvk_main.h"

namespace dxvk {
  
  DxvkContext::DxvkContext(
    const Rc<DxvkDevice>&           device,
    const Rc<DxvkPipelineManager>&  pipeMgr)
  : m_device  (device),
    m_pipeMgr (pipeMgr) {
    TRACE(this, device);
  }
  
  
  DxvkContext::~DxvkContext() {
    TRACE(this);
  }
  
  
  void DxvkContext::beginRecording(
    const Rc<DxvkRecorder>& recorder) {
    TRACE(this, recorder);
    m_cmd = recorder;
    m_cmd->beginRecording();
    
    // Make sure that we apply the current context state
    // to the command buffer when recording draw commands.
    m_state.g.flags.clr(
      DxvkGraphicsPipelineBit::RenderPassBound);
    m_state.g.flags.set(
      DxvkGraphicsPipelineBit::PipelineDirty,
      DxvkGraphicsPipelineBit::PipelineStateDirty,
      DxvkGraphicsPipelineBit::DirtyResources,
      DxvkGraphicsPipelineBit::DirtyVertexBuffers,
      DxvkGraphicsPipelineBit::DirtyIndexBuffer);
    
    m_state.c.flags.set(
      DxvkComputePipelineBit::PipelineDirty,
      DxvkComputePipelineBit::DirtyResources);
  }
  
  
  bool DxvkContext::endRecording() {
    TRACE(this);
    
    // Any currently active render pass must be
    // ended before finalizing the command buffer.
    if (m_state.g.flags.test(DxvkGraphicsPipelineBit::RenderPassBound))
      this->endRenderPass();
    
    // Finalize the command list
    m_cmd->endRecording();
    m_cmd = nullptr;
    return true;
  }
  
  
  void DxvkContext::bindFramebuffer(
    const Rc<DxvkFramebuffer>& fb) {
    TRACE(this, fb);
    
    if (m_state.g.fb != fb) {
      m_state.g.fb = fb;
      
      if (m_state.g.flags.test(
          DxvkGraphicsPipelineBit::RenderPassBound))
        this->endRenderPass();
    }
  }
  
  
  void DxvkContext::bindShader(
          VkShaderStageFlagBits stage,
    const Rc<DxvkShader>&       shader) {
    TRACE(this, stage, shader);
    
    DxvkShaderState* state = this->getShaderState(stage);
    
    if (state->shader != shader) {
      state->shader = shader;
      this->setPipelineDirty(stage);
    }
  }
  
  
  void DxvkContext::bindStorageBuffer(
          VkShaderStageFlagBits stage,
          uint32_t              slot,
    const Rc<DxvkBuffer>&       buffer,
          VkDeviceSize          offset,
          VkDeviceSize          length) {
    TRACE(this, stage, slot);
    
    DxvkBufferBinding binding(buffer, offset, length);
    DxvkShaderState* state = this->getShaderState(stage);
    
    // TODO investigate whether it is worth checking whether
    // the shader actually uses the resource. However, if the
    // application is not completely retarded, always setting
    // the 'resources dirty' flag should be the best option.
    if (state->boundStorageBuffers.at(slot) != binding) {
      state->boundStorageBuffers.at(slot) = binding;
      this->setResourcesDirty(stage);
      m_cmd->trackResource(binding.resource());
    }
  }
  
  
  void DxvkContext::clearRenderTarget(
    const VkClearAttachment&  attachment,
    const VkClearRect&        clearArea) {
    this->flushGraphicsState();
    
    m_cmd->cmdClearAttachments(
      1, &attachment, 1, &clearArea);
  }
  
  
  void DxvkContext::dispatch(
          uint32_t wgCountX,
          uint32_t wgCountY,
          uint32_t wgCountZ) {
    TRACE(this, wgCountX, wgCountY, wgCountZ);
    this->endRenderPass();
    this->flushComputeState();
    
    m_cmd->cmdDispatch(
      wgCountX, wgCountY, wgCountZ);
    
    // TODO resource barriers
  }
  
  
  void DxvkContext::draw(
          uint32_t vertexCount,
          uint32_t instanceCount,
          uint32_t firstVertex,
          uint32_t firstInstance) {
    TRACE(this, vertexCount, instanceCount,
      firstVertex, firstInstance);
    this->flushGraphicsState();
    
    m_cmd->cmdDraw(
      vertexCount, instanceCount,
      firstVertex, firstInstance);
  }
  
  
  void DxvkContext::drawIndexed(
          uint32_t indexCount,
          uint32_t instanceCount,
          uint32_t firstIndex,
          uint32_t vertexOffset,
          uint32_t firstInstance) {
    TRACE(this, indexCount, instanceCount,
      firstIndex, vertexOffset, firstInstance);
    this->flushGraphicsState();
    
    m_cmd->cmdDrawIndexed(
      indexCount, instanceCount,
      firstIndex, vertexOffset,
      firstInstance);
  }
  
  
  void DxvkContext::flushComputeState() {
    if (m_state.c.flags.test(DxvkComputePipelineBit::PipelineDirty)) {
      m_state.c.pipeline = m_pipeMgr->getComputePipeline(m_state.c.cs.shader);
      
      if (m_state.c.pipeline != nullptr) {
        m_cmd->cmdBindPipeline(
          VK_PIPELINE_BIND_POINT_COMPUTE,
          m_state.c.pipeline->getPipelineHandle());
      }
    }
    
    if (m_state.c.flags.test(DxvkComputePipelineBit::DirtyResources)
     && m_state.c.pipeline != nullptr) {
      std::vector<DxvkResourceBinding> bindings;
      this->addResourceBindingInfo(bindings, m_state.c.cs);
      
      m_cmd->bindShaderResources(
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_state.c.pipeline->pipelineLayout(),
        m_state.c.pipeline->descriptorSetLayout(),
        bindings.size(), bindings.data());
    }
    
    m_state.c.flags.clr(
      DxvkComputePipelineBit::PipelineDirty,
      DxvkComputePipelineBit::DirtyResources);
  }
  
  
  void DxvkContext::flushGraphicsState() {
    if (!m_state.g.flags.test(DxvkGraphicsPipelineBit::RenderPassBound))
      this->beginRenderPass();
  }
  
  
  void DxvkContext::beginRenderPass() {
    TRACE(this);
    
    DxvkFramebufferSize fbsize
      = m_state.g.fb->size();
      
    VkRenderPassBeginInfo info;
    info.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.pNext            = nullptr;
    info.renderPass       = m_state.g.fb->renderPass();
    info.framebuffer      = m_state.g.fb->handle();
    info.renderArea       = VkRect2D { { 0, 0 }, { fbsize.width, fbsize.height } };
    info.clearValueCount  = 0;
    info.pClearValues     = nullptr;
    
    m_cmd->cmdBeginRenderPass(&info, VK_SUBPASS_CONTENTS_INLINE);
    m_state.g.flags.set(DxvkGraphicsPipelineBit::RenderPassBound);
  }
  
  
  void DxvkContext::endRenderPass() {
    TRACE(this);
    
    m_cmd->cmdEndRenderPass();
    m_state.g.flags.clr(DxvkGraphicsPipelineBit::RenderPassBound);
  }
  
  
  void DxvkContext::setPipelineDirty(VkShaderStageFlagBits stage) {
    if (stage == VK_SHADER_STAGE_COMPUTE_BIT) {
      m_state.c.flags.set(
        DxvkComputePipelineBit::PipelineDirty,
        DxvkComputePipelineBit::DirtyResources);
    } else {
      m_state.g.flags.set(
        DxvkGraphicsPipelineBit::PipelineDirty,
        DxvkGraphicsPipelineBit::DirtyResources);
    }
  }
  
  
  void DxvkContext::setResourcesDirty(VkShaderStageFlagBits stage) {
    if (stage == VK_SHADER_STAGE_COMPUTE_BIT)
      m_state.c.flags.set(DxvkComputePipelineBit::DirtyResources);
    else
      m_state.g.flags.set(DxvkGraphicsPipelineBit::DirtyResources);
  }
  
  
  DxvkShaderState* DxvkContext::getShaderState(VkShaderStageFlagBits stage) {
    switch (stage) {
      case VK_SHADER_STAGE_VERTEX_BIT:
        return &m_state.g.vs;
        
      case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return &m_state.g.tcs;
        
      case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return &m_state.g.tes;
      
      case VK_SHADER_STAGE_GEOMETRY_BIT:
        return &m_state.g.gs;
        
      case VK_SHADER_STAGE_FRAGMENT_BIT:
        return &m_state.g.fs;
        
      case VK_SHADER_STAGE_COMPUTE_BIT:
        return &m_state.c.cs;
        
      default:
        return nullptr;
    }
  }
  
  
  uint32_t DxvkContext::addResourceBindingInfo(
          std::vector<DxvkResourceBinding>& bindings,
    const DxvkShaderState&                  stageInfo) const {
    const uint32_t slotCount = stageInfo.shader->slotCount();
    
    for (uint32_t i = 0; i < slotCount; i++) {
      DxvkResourceSlot slot = stageInfo.shader->slot(i);
      DxvkResourceBinding binding;
      
      switch (slot.type) {
        case DxvkResourceType::ImageSampler:
          binding.type = VK_DESCRIPTOR_TYPE_SAMPLER;
          break;
          
        case DxvkResourceType::SampledImage:
          binding.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
          break;
          
        case DxvkResourceType::StorageImage:
          binding.type   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
          break;
          
        case DxvkResourceType::UniformBuffer:
          binding.type   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
          binding.buffer = stageInfo.boundUniformBuffers.at(slot.slot).descriptorInfo();
          break;
          
        case DxvkResourceType::StorageBuffer:
          binding.type   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
          binding.buffer = stageInfo.boundStorageBuffers.at(slot.slot).descriptorInfo();
          break;
      }
      
      bindings.push_back(binding);
    }
    
    return slotCount;
  }
  
}