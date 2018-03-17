#include "dxvk_framebuffer.h"

namespace dxvk {
  
  DxvkRenderTargets:: DxvkRenderTargets() { }
  DxvkRenderTargets::~DxvkRenderTargets() { }
  
  
  DxvkRenderPassFormat DxvkRenderTargets::renderPassFormat() const {
    DxvkRenderPassFormat result;
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_colorTargets.at(i).view != nullptr) {
        result.setColorFormat(i, DxvkRenderTargetFormat {
          m_colorTargets.at(i).view->info().format,
          m_colorTargets.at(i).view->imageInfo().layout,
          m_colorTargets.at(i).view->imageInfo().layout,
          m_colorTargets.at(i).layout });
        result.setSampleCount(m_colorTargets.at(i).view->imageInfo().sampleCount);
      }
    }
    
    if (m_depthTarget.view != nullptr) {
      result.setDepthFormat(DxvkRenderTargetFormat {
        m_depthTarget.view->info().format,
        m_depthTarget.view->imageInfo().layout,
        m_depthTarget.view->imageInfo().layout,
        m_depthTarget.layout });
      result.setSampleCount(m_depthTarget.view->imageInfo().sampleCount);
    }
    
    return result;
  }
  
  
  uint32_t DxvkRenderTargets::getAttachments(VkImageView* viewHandles) const {
    uint32_t numViews = 0;
    
    if (m_depthTarget.view != nullptr)
      viewHandles[numViews++] = m_depthTarget.view->handle();
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_colorTargets.at(i).view != nullptr)
        viewHandles[numViews++] = m_colorTargets.at(i).view->handle();
    }
    
    return numViews;
  }
  
  
  DxvkFramebufferSize DxvkRenderTargets::getImageSize() const {
    if (m_depthTarget.view != nullptr)
      return this->renderTargetSize(m_depthTarget.view);
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_colorTargets.at(i).view != nullptr)
        return this->renderTargetSize(m_colorTargets.at(i).view);
    }
    
    return DxvkFramebufferSize { 0, 0, 0 };
  }
  
  
  bool DxvkRenderTargets::hasAttachments() const {
    bool result = m_depthTarget.view != nullptr;
    
    for (uint32_t i = 0; (i < MaxNumRenderTargets) && !result; i++)
      result |= m_colorTargets.at(i).view != nullptr;
    
    return result;
  }
  
  
  DxvkFramebufferSize DxvkRenderTargets::renderTargetSize(
    const Rc<DxvkImageView>& renderTarget) const {
    auto extent = renderTarget->mipLevelExtent(0);
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
    std::array<VkImageView, MaxNumRenderTargets + 1> views;
    uint32_t viewCount = renderTargets.getAttachments(views.data());
    
    VkFramebufferCreateInfo info;
    info.sType                = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.pNext                = nullptr;
    info.flags                = 0;
    info.renderPass           = renderPass->handle();
    info.attachmentCount      = viewCount;
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
  
  
  uint32_t DxvkFramebuffer::findAttachment(
    const Rc<DxvkImageView>& view) const {
    if (m_renderTargets.getDepthTarget().view == view)
      return 0;
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_renderTargets.getColorTarget(i).view == view)
        return i;
    }
    
    return MaxNumRenderTargets;
  }
  
}