#include "dxvk_framebuffer.h"

namespace dxvk {
  
  DxvkFramebuffer::DxvkFramebuffer(
    const Rc<vk::DeviceFn>&       vkd,
          DxvkRenderPass*         renderPass,
    const DxvkRenderTargets&      renderTargets,
    const DxvkFramebufferSize&    defaultSize)
  : m_vkd           (vkd),
    m_renderPass    (renderPass),
    m_renderTargets (renderTargets),
    m_renderSize    (computeRenderSize(defaultSize)) {
    std::array<VkImageView, MaxNumRenderTargets + 1> views;
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_renderTargets.color[i].view != nullptr) {
        views[m_attachmentCount] = m_renderTargets.color[i].view->handle();
        m_attachments[m_attachmentCount] = i;
        m_attachmentCount += 1;
      }
    }
    
    if (m_renderTargets.depth.view != nullptr) {
      views[m_attachmentCount] = m_renderTargets.depth.view->handle();
      m_attachments[m_attachmentCount] = -1;
      m_attachmentCount += 1;
    }
    
    VkFramebufferCreateInfo info;
    info.sType                = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.pNext                = nullptr;
    info.flags                = 0;
    info.renderPass           = m_renderPass->getDefaultHandle();
    info.attachmentCount      = m_attachmentCount;
    info.pAttachments         = views.data();
    info.width                = m_renderSize.width;
    info.height               = m_renderSize.height;
    info.layers               = m_renderSize.layers;
    
    if (m_vkd->vkCreateFramebuffer(m_vkd->device(), &info, nullptr, &m_handle) != VK_SUCCESS)
      Logger::err("DxvkFramebuffer: Failed to create framebuffer object");
  }
  
  
  DxvkFramebuffer::~DxvkFramebuffer() {
    m_vkd->vkDestroyFramebuffer(m_vkd->device(), m_handle, nullptr);
  }
  
  
  int32_t DxvkFramebuffer::findAttachment(const Rc<DxvkImageView>& view) const {
    for (uint32_t i = 0; i < m_attachmentCount; i++) {
      if (getAttachment(i).view->matchesView(view))
        return int32_t(i);
    }
    
    return -1;
  }
  
  
  bool DxvkFramebuffer::hasTargets(const DxvkRenderTargets& renderTargets) {
    bool eq = m_renderTargets.depth.view   == renderTargets.depth.view
           && m_renderTargets.depth.layout == renderTargets.depth.layout;
    
    for (uint32_t i = 0; i < MaxNumRenderTargets && eq; i++) {
      eq &= m_renderTargets.color[i].view   == renderTargets.color[i].view
         && m_renderTargets.color[i].layout == renderTargets.color[i].layout;
    }
    
    return eq;
  }
  
  
  bool DxvkFramebuffer::isFullSize(const Rc<DxvkImageView>& view) const {
    return m_renderSize.width  == view->mipLevelExtent(0).width
        && m_renderSize.height == view->mipLevelExtent(0).height
        && m_renderSize.layers == view->info().numLayers;
  }


  bool DxvkFramebuffer::isWritable(uint32_t attachmentIndex, VkImageAspectFlags aspects) const {
    VkImageAspectFlags writableAspects = vk::getWritableAspectsForLayout(getAttachment(attachmentIndex).layout);
    return (writableAspects & aspects) == aspects;
  }


  DxvkRenderPassFormat DxvkFramebuffer::getRenderPassFormat(const DxvkRenderTargets& renderTargets) {
    DxvkRenderPassFormat format;
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (renderTargets.color[i].view != nullptr) {
        format.sampleCount     = renderTargets.color[i].view->imageInfo().sampleCount;
        format.color[i].format = renderTargets.color[i].view->info().format;
        format.color[i].layout = renderTargets.color[i].layout;
      }
    }
    
    if (renderTargets.depth.view != nullptr) {
      format.sampleCount  = renderTargets.depth.view->imageInfo().sampleCount;
      format.depth.format = renderTargets.depth.view->info().format;
      format.depth.layout = renderTargets.depth.layout;
    }
    
    return format;
  }
  
  
  DxvkFramebufferSize DxvkFramebuffer::computeRenderSize(
    const DxvkFramebufferSize& defaultSize) const {
    // Some games bind render targets of a different size and
    // expect it to work, so we'll compute the minimum size
    DxvkFramebufferSize minSize = defaultSize;
    
    if (m_renderTargets.depth.view != nullptr) {
      DxvkFramebufferSize depthSize = this->computeRenderTargetSize(m_renderTargets.depth.view);
      minSize.width  = std::min(minSize.width,  depthSize.width);
      minSize.height = std::min(minSize.height, depthSize.height);
      minSize.layers = std::min(minSize.layers, depthSize.layers);
    }
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_renderTargets.color[i].view != nullptr) {
        DxvkFramebufferSize colorSize = this->computeRenderTargetSize(m_renderTargets.color[i].view);
        minSize.width  = std::min(minSize.width,  colorSize.width);
        minSize.height = std::min(minSize.height, colorSize.height);
        minSize.layers = std::min(minSize.layers, colorSize.layers);
      }
    }
    
    return minSize;
  }
  
  
  DxvkFramebufferSize DxvkFramebuffer::computeRenderTargetSize(
    const Rc<DxvkImageView>& renderTarget) const {
    auto extent = renderTarget->mipLevelExtent(0);
    auto layers = renderTarget->info().numLayers;
    return DxvkFramebufferSize { extent.width, extent.height, layers };
  }
  
}