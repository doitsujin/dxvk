#include "dxvk_device.h"
#include "dxvk_context.h"
#include "dxvk_main.h"

namespace dxvk {
  
  DxvkContext::DxvkContext(const Rc<DxvkDevice>& device)
  : m_device(device) {
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
    this->flushComputeState();
    
    m_cmd->cmdDispatch(
      wgCountX, wgCountY, wgCountZ);
  }
  
  
  void DxvkContext::draw(
          uint32_t vertexCount,
          uint32_t instanceCount,
          uint32_t firstVertex,
          uint32_t firstInstance) {
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
    this->flushGraphicsState();
    
    m_cmd->cmdDrawIndexed(
      indexCount, instanceCount,
      firstIndex, vertexOffset,
      firstInstance);
  }
  
  
  void DxvkContext::setFramebuffer(
    const Rc<DxvkFramebuffer>& fb) {
    TRACE(this, fb);
    
    if (m_state.g.fb != fb) {
      m_state.g.fb = fb;
      
      if (m_state.g.flags.test(
          DxvkGraphicsPipelineBit::RenderPassBound))
        this->endRenderPass();
    }
  }
  
  
  void DxvkContext::setShader(
          VkShaderStageFlagBits stage,
    const Rc<DxvkShader>&       shader) {
    TRACE(this, stage, shader);
    
    DxvkShaderState* state = this->getShaderState(stage);
    
    if (state->shader != shader) {
      state->shader = shader;
      
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
  }
  
  
  void DxvkContext::flushComputeState() {
    if (m_state.c.flags.test(DxvkComputePipelineBit::PipelineDirty)
     && m_state.c.pipeline != nullptr) {
      m_cmd->cmdBindPipeline(
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_state.c.pipeline->handle());
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
  
}