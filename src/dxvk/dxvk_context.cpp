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
    
    // The current state of the internal command buffer is
    // undefined, so we have to bind and set up everything
    // before any draw or dispatch command is recorded.
    m_state.flags.clr(
      DxvkContextFlag::GpRenderPassBound);
    
    m_state.flags.set(
      DxvkContextFlag::GpDirtyPipeline,
      DxvkContextFlag::GpDirtyPipelineState,
      DxvkContextFlag::GpDirtyResources,
      DxvkContextFlag::GpDirtyIndexBuffers,
      DxvkContextFlag::GpDirtyVertexBuffers,
      DxvkContextFlag::CpDirtyPipeline,
      DxvkContextFlag::CpDirtyResources);
  }
  
  
  void DxvkContext::endRecording() {
    TRACE(this);
    
    this->renderPassEnd();
    
    m_cmd->endRecording();
    m_cmd = nullptr;
  }
  
  
  void DxvkContext::bindFramebuffer(
    const Rc<DxvkFramebuffer>& fb) {
    TRACE(this, fb);
    
    if (m_state.om.framebuffer != fb) {
      m_state.om.framebuffer = fb;
      this->renderPassEnd();
    }
  }
  
  
  void DxvkContext::bindShader(
          VkShaderStageFlagBits stage,
    const Rc<DxvkShader>&       shader) {
    TRACE(this, stage, shader);
    
    DxvkShaderStageState* stageState = this->getShaderStage(stage);
    
    if (stageState->shader != shader) {
      stageState->shader = shader;
      
      if (stage == VK_SHADER_STAGE_COMPUTE_BIT) {
        m_state.flags.set(
          DxvkContextFlag::CpDirtyPipeline,
          DxvkContextFlag::CpDirtyResources);
      } else {
        m_state.flags.set(
          DxvkContextFlag::GpDirtyPipeline,
          DxvkContextFlag::GpDirtyPipelineState,
          DxvkContextFlag::GpDirtyResources,
          DxvkContextFlag::GpDirtyVertexBuffers,
          DxvkContextFlag::GpDirtyIndexBuffers);
      }
    }
  }
  
  
  void DxvkContext::clearRenderTarget(
    const VkClearAttachment&  attachment,
    const VkClearRect&        clearArea) {
    TRACE(this);
    
    // We only need the framebuffer to be bound. Flushing the
    // entire pipeline state is not required and might actually
    // cause problems if the current pipeline state is invalid.
    this->renderPassBegin();
    
    m_cmd->cmdClearAttachments(
      1, &attachment, 1, &clearArea);
  }
  
  
  void DxvkContext::draw(
          uint32_t vertexCount,
          uint32_t instanceCount,
          uint32_t firstVertex,
          uint32_t firstInstance) {
    TRACE(this, vertexCount, instanceCount,
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
  }
  
  
  void DxvkContext::renderPassBegin() {
    if (!m_state.flags.test(DxvkContextFlag::GpRenderPassBound)
     && (m_state.om.framebuffer != nullptr)) {
      m_state.flags.set(DxvkContextFlag::GpRenderPassBound);
      
      const DxvkFramebufferSize fbSize
        = m_state.om.framebuffer->size();
      
      VkRect2D renderArea;
      renderArea.offset = VkOffset2D { 0, 0 };
      renderArea.extent = VkExtent2D { fbSize.width, fbSize.height };
      
      VkRenderPassBeginInfo info;
      info.sType                = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      info.pNext                = nullptr;
      info.renderPass           = m_state.om.framebuffer->renderPass();
      info.framebuffer          = m_state.om.framebuffer->handle();
      info.renderArea           = renderArea;
      info.clearValueCount      = 0;
      info.pClearValues         = nullptr;
      
      m_cmd->cmdBeginRenderPass(&info,
        VK_SUBPASS_CONTENTS_INLINE);
    }
  }
  
  
  void DxvkContext::renderPassEnd() {
    if (m_state.flags.test(DxvkContextFlag::GpRenderPassBound)) {
      m_state.flags.clr(DxvkContextFlag::GpRenderPassBound);
      m_cmd->cmdEndRenderPass();
    }
  }
  
  
  void DxvkContext::bindGraphicsPipeline() {
    if (m_state.flags.test(DxvkContextFlag::GpDirtyPipeline)) {
      m_state.flags.clr(DxvkContextFlag::GpDirtyPipeline);
      
      m_state.activeGraphicsPipeline = m_pipeMgr->getGraphicsPipeline(
        m_state.vs.shader, m_state.tcs.shader, m_state.tes.shader,
        m_state.gs.shader, m_state.fs.shader);
    }
    
    if (m_state.flags.test(DxvkContextFlag::GpDirtyPipelineState)
     && m_state.activeGraphicsPipeline != nullptr) {
      m_state.flags.clr(DxvkContextFlag::GpDirtyPipelineState);
      
      DxvkGraphicsPipelineStateInfo gpState;
      gpState.renderPass = m_state.om.framebuffer->renderPass();
      
      m_cmd->cmdBindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_state.activeGraphicsPipeline->getPipelineHandle(gpState));
    }
  }
  
  
  void DxvkContext::flushGraphicsState() {
    this->renderPassBegin();
    this->bindGraphicsPipeline();
  }
  
  
  DxvkShaderStageState* DxvkContext::getShaderStage(VkShaderStageFlagBits stage) {
    switch (stage) {
      case VK_SHADER_STAGE_VERTEX_BIT:                  return &m_state.vs;
      case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    return &m_state.tcs;
      case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return &m_state.tes;
      case VK_SHADER_STAGE_GEOMETRY_BIT:                return &m_state.gs;
      case VK_SHADER_STAGE_FRAGMENT_BIT:                return &m_state.fs;
      case VK_SHADER_STAGE_COMPUTE_BIT:                 return &m_state.cs;
      
      default:
        throw DxvkError(str::format(
          "DxvkContext::getShaderStage: Invalid stage bit: ",
          static_cast<uint32_t>(stage)));
    }
  }
  
}