#include "dxvk_context.h"
#include "dxvk_main.h"

namespace dxvk {
  
  DxvkContext::DxvkContext(const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd) {
    TRACE(this);
  }
  
  
  DxvkContext::~DxvkContext() {
    TRACE(this);
  }
  
  
  void DxvkContext::beginRecording(
    const Rc<DxvkCommandList>& cmdList) {
    TRACE(this, cmdList);
    m_commandList = cmdList;
    m_commandList->beginRecording();
  }
  
  
  bool DxvkContext::endRecording() {
    TRACE(this);
    
    if (m_state.fb.flags.test(DxvkFbStateFlags::InsideRenderPass))
      this->endRenderPass();
    
    // Finalize the command list
    m_commandList->endRecording();
    m_commandList = nullptr;
    return true;
  }
  
  
  void DxvkContext::clearRenderTarget(
    const VkClearAttachment&  attachment,
    const VkClearRect&        clearArea) {
    if (!m_state.fb.flags.test(DxvkFbStateFlags::InsideRenderPass))
      this->beginRenderPass();
    
    m_vkd->vkCmdClearAttachments(
      m_commandList->handle(),
      1, &attachment,
      1, &clearArea);
  }
  
  
  void DxvkContext::draw(
          uint32_t vertexCount,
          uint32_t instanceCount,
          uint32_t firstVertex,
          uint32_t firstInstance) {
    this->prepareDraw();
    m_vkd->vkCmdDraw(
      m_commandList->handle(),
      vertexCount,
      instanceCount,
      firstVertex,
      firstInstance);
  }
  
  
  void DxvkContext::drawIndexed(
          uint32_t indexCount,
          uint32_t instanceCount,
          uint32_t firstIndex,
          uint32_t vertexOffset,
          uint32_t firstInstance) {
    this->prepareDraw();
    m_vkd->vkCmdDrawIndexed(
      m_commandList->handle(),
      indexCount,
      instanceCount,
      firstIndex,
      vertexOffset,
      firstInstance);
  }
  
  
  void DxvkContext::setFramebuffer(
    const Rc<DxvkFramebuffer>& fb) {
    TRACE(this, fb);
    
    // When changing the framebuffer binding, we end the
    // current render pass, but beginning the new render
    // pass is deferred until a draw command is called.
    if (m_state.fb.framebuffer != fb) {
      if (m_state.fb.flags.test(DxvkFbStateFlags::InsideRenderPass))
        this->endRenderPass();
      
      m_state.fb.framebuffer = fb;
      m_commandList->trackResource(fb);
    }
    
  }
    
  
  void DxvkContext::setShader(
          VkShaderStageFlagBits stage,
    const Rc<DxvkShader>&       shader) {
    TRACE(this, stage, shader);
    
  }
  
  
  void DxvkContext::flushGraphicsState() {
    
  }
  
  
  void DxvkContext::prepareDraw() {
    this->flushGraphicsState();
    
    if (!m_state.fb.flags.test(DxvkFbStateFlags::InsideRenderPass))
      this->beginRenderPass();
  }
  
  
  void DxvkContext::beginRenderPass() {
    TRACE(this);
    
    const DxvkFramebufferSize fbsize
      = m_state.fb.framebuffer->size();
      
    VkRenderPassBeginInfo info;
    info.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.pNext            = nullptr;
    info.renderPass       = m_state.fb.framebuffer->renderPass();
    info.framebuffer      = m_state.fb.framebuffer->handle();
    info.renderArea       = VkRect2D { { 0, 0 }, { fbsize.width, fbsize.height } };
    info.clearValueCount  = 0;
    info.pClearValues     = nullptr;
    
    m_vkd->vkCmdBeginRenderPass(
      m_commandList->handle(),
      &info, VK_SUBPASS_CONTENTS_INLINE);
    m_state.fb.flags.set(DxvkFbStateFlags::InsideRenderPass);
  }
  
  
  void DxvkContext::endRenderPass() {
    TRACE(this);
    
    m_vkd->vkCmdEndRenderPass(m_commandList->handle());
    m_state.fb.flags.clr(DxvkFbStateFlags::InsideRenderPass);
  }
  
}