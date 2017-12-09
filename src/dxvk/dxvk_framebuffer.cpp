#include "dxvk_framebuffer.h"

namespace dxvk {
  
  DxvkRenderTargets:: DxvkRenderTargets() { }
  DxvkRenderTargets::~DxvkRenderTargets() { }
  
  
  DxvkRenderPassFormat DxvkRenderTargets::renderPassFormat() const {
    DxvkRenderPassFormat result;
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_colorTargets.at(i) != nullptr) {
        result.setColorFormat(i, m_colorTargets.at(i)->info().format);
        result.setSampleCount(m_colorTargets.at(i)->image()->info().sampleCount);
      }
    }
    
    if (m_depthTarget != nullptr) {
      result.setDepthFormat(m_depthTarget->info().format);
      result.setSampleCount(m_depthTarget->image()->info().sampleCount);
    }
    
    return result;
  }
  
  
  std::vector<VkImageView> DxvkRenderTargets::getAttachments() const {
    std::vector<VkImageView> result;
    
    if (m_depthTarget != nullptr)
      result.push_back(m_depthTarget->handle());
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_colorTargets.at(i) != nullptr)
        result.push_back(m_colorTargets.at(i)->handle());
    }
    
    return result;
  }
  
  
  DxvkFramebufferSize DxvkRenderTargets::getImageSize() const {
    if (m_depthTarget != nullptr)
      return this->renderTargetSize(m_depthTarget);
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_colorTargets.at(i) != nullptr)
        return this->renderTargetSize(m_colorTargets.at(i));
    }
    
    return DxvkFramebufferSize { 0, 0, 0 };
  }
  
  
  DxvkFramebufferSize DxvkRenderTargets::renderTargetSize(
    const Rc<DxvkImageView>& renderTarget) const {
    auto extent = renderTarget->image()->info().extent;
    auto layers = renderTarget->info().numLayers;
    return DxvkFramebufferSize { extent.width, extent.height, layers };
  }
  
  
  DxvkFramebuffer::DxvkFramebuffer(
    const Rc<vk::DeviceFn>&       vkd,
    const Rc<DxvkRenderPass>&     renderPass,
    const DxvkRenderTargets&      renderTargets)
  : m_vkd             (vkd),
    m_renderPass      (renderPass),
    m_renderTargets   (renderTargets),
    m_framebufferSize (renderTargets.getImageSize()) {
    auto views = renderTargets.getAttachments();
    
    VkFramebufferCreateInfo info;
    info.sType                = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.pNext                = nullptr;
    info.flags                = 0;
    info.renderPass           = renderPass->handle();
    info.attachmentCount      = views.size();
    info.pAttachments         = views.data();
    info.width                = m_framebufferSize.width;
    info.height               = m_framebufferSize.height;
    info.layers               = m_framebufferSize.layers;
    
    if (m_vkd->vkCreateFramebuffer(m_vkd->device(), &info, nullptr, &m_framebuffer) != VK_SUCCESS)
      throw DxvkError("DxvkFramebuffer::DxvkFramebuffer: Failed to create framebuffer object");
  }
  
  
  DxvkFramebuffer::~DxvkFramebuffer() {
    m_vkd->vkDestroyFramebuffer(
      m_vkd->device(), m_framebuffer, nullptr);
  }
  
}