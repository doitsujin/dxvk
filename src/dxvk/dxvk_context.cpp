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
    m_commandList->endRecording();
    m_commandList = nullptr;
    return true;
  }
    
  
  void DxvkContext::setFramebuffer(
    const Rc<DxvkFramebuffer>& fb) {
    TRACE(this, fb);
    
    const DxvkFramebufferSize fbSize = fb->size();
    // TODO implement properly
    VkRect2D renderArea;
    renderArea.offset.x       = 0;
    renderArea.offset.y       = 0;
    renderArea.extent.width   = fbSize.width;
    renderArea.extent.height  = fbSize.height;
    
    VkRenderPassBeginInfo info;
    info.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.pNext            = nullptr;
    info.renderPass       = fb->renderPass();
    info.framebuffer      = fb->handle();
    info.renderArea       = renderArea;
    info.clearValueCount  = 0;
    info.pClearValues     = nullptr;
    
    // This is for testing purposes only.
    VkClearAttachment attachment;
    attachment.aspectMask       = VK_IMAGE_ASPECT_COLOR_BIT;
    attachment.colorAttachment  = 0;
    attachment.clearValue.color.float32[0] = 1.0f;
    attachment.clearValue.color.float32[1] = 1.0f;
    attachment.clearValue.color.float32[2] = 1.0f;
    attachment.clearValue.color.float32[3] = 1.0f;
    
    VkClearRect clearRect;
    clearRect.rect           = renderArea;
    clearRect.baseArrayLayer = 0;
    clearRect.layerCount     = fbSize.layers;
    
    m_vkd->vkCmdBeginRenderPass(
      m_commandList->handle(), &info,
      VK_SUBPASS_CONTENTS_INLINE);
    m_vkd->vkCmdClearAttachments(
      m_commandList->handle(),
      1, &attachment,
      1, &clearRect);
    m_vkd->vkCmdEndRenderPass(
      m_commandList->handle());
  }
  
}