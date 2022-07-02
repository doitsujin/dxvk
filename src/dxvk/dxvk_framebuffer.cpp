#include "dxvk_framebuffer.h"

namespace dxvk {
  
  DxvkFramebufferInfo::DxvkFramebufferInfo() {

  }


  DxvkFramebufferInfo::DxvkFramebufferInfo(
    const DxvkRenderTargets&      renderTargets,
    const DxvkFramebufferSize&    defaultSize,
          DxvkRenderPass*         renderPass)
  : m_renderTargets (renderTargets),
    m_renderSize    (computeRenderSize(defaultSize)),
    m_renderPass    (renderPass) {

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_renderTargets.color[i].view != nullptr) {
        m_attachments[m_attachmentCount++] = i;
        m_sampleCount = m_renderTargets.color[i].view->imageInfo().sampleCount;
      }
    }

    if (m_renderTargets.depth.view != nullptr) {
      m_attachments[m_attachmentCount++] = -1;
      m_sampleCount = m_renderTargets.depth.view->imageInfo().sampleCount;
    }
  }


  DxvkFramebufferInfo::~DxvkFramebufferInfo() {

  }


  int32_t DxvkFramebufferInfo::findAttachment(const Rc<DxvkImageView>& view) const {
    for (uint32_t i = 0; i < m_attachmentCount; i++) {
      if (getAttachment(i).view->matchesView(view))
        return int32_t(i);
    }

    return -1;
  }


  bool DxvkFramebufferInfo::hasTargets(const DxvkRenderTargets& renderTargets) {
    bool eq = m_renderTargets.depth.view   == renderTargets.depth.view
           && m_renderTargets.depth.layout == renderTargets.depth.layout;

    for (uint32_t i = 0; i < MaxNumRenderTargets && eq; i++) {
      eq &= m_renderTargets.color[i].view   == renderTargets.color[i].view
         && m_renderTargets.color[i].layout == renderTargets.color[i].layout;
    }

    return eq;
  }


  bool DxvkFramebufferInfo::isFullSize(const Rc<DxvkImageView>& view) const {
    return m_renderSize.width  == view->mipLevelExtent(0).width
        && m_renderSize.height == view->mipLevelExtent(0).height
        && m_renderSize.layers == view->info().numLayers;
  }


  bool DxvkFramebufferInfo::isWritable(uint32_t attachmentIndex, VkImageAspectFlags aspects) const {
    VkImageAspectFlags writableAspects = vk::getWritableAspectsForLayout(getAttachment(attachmentIndex).layout);
    return (writableAspects & aspects) == aspects;
  }


  DxvkFramebufferKey DxvkFramebufferInfo::key() const {
    DxvkFramebufferKey result = { };

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_renderTargets.color[i].view != nullptr)
        result.colorViews[i] = m_renderTargets.color[i].view->cookie();
    }

    if (m_renderTargets.depth.view != nullptr)
      result.depthView = m_renderTargets.depth.view->cookie();

    if (result.renderPass)
      result.renderPass = m_renderPass->getDefaultHandle();

    return result;
  }


  DxvkRtInfo DxvkFramebufferInfo::getRtInfo() const {
    VkFormat depthStencilFormat = VK_FORMAT_UNDEFINED;
    VkImageAspectFlags depthStencilReadOnlyAspects = 0;

    if (m_renderTargets.depth.view != nullptr) {
      depthStencilFormat = m_renderTargets.depth.view->info().format;
      depthStencilReadOnlyAspects = m_renderTargets.depth.view->formatInfo()->aspectMask
        & ~vk::getWritableAspectsForLayout(m_renderTargets.depth.layout);
    }

    std::array<VkFormat, MaxNumRenderTargets> colorFormats = { };
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_renderTargets.color[i].view != nullptr)
        colorFormats[i] = m_renderTargets.color[i].view->info().format;
    }

    return DxvkRtInfo(MaxNumRenderTargets, colorFormats.data(),
      depthStencilFormat, depthStencilReadOnlyAspects);
  }


  DxvkRenderPassFormat DxvkFramebufferInfo::getRenderPassFormat(const DxvkRenderTargets& renderTargets) {
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


  DxvkFramebufferSize DxvkFramebufferInfo::computeRenderSize(
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


  DxvkFramebufferSize DxvkFramebufferInfo::computeRenderTargetSize(
    const Rc<DxvkImageView>& renderTarget) const {
    auto extent = renderTarget->mipLevelExtent(0);
    auto layers = renderTarget->info().numLayers;
    return DxvkFramebufferSize { extent.width, extent.height, layers };
  }


  DxvkFramebuffer::DxvkFramebuffer(
    const Rc<vk::DeviceFn>&       vkd,
    const DxvkFramebufferInfo&    info)
  : m_vkd(vkd), m_key(info.key()) {
    std::array<VkImageView, MaxNumRenderTargets + 1> views;
    uint32_t attachmentCount = 0;
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (info.getColorTarget(i).view != nullptr)
        views[attachmentCount++] = info.getColorTarget(i).view->handle();
    }
    
    if (info.getDepthTarget().view != nullptr)
      views[attachmentCount++] = info.getDepthTarget().view->handle();
    
    VkFramebufferCreateInfo fbInfo;
    fbInfo.sType                = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.pNext                = nullptr;
    fbInfo.flags                = 0;
    fbInfo.renderPass           = info.renderPass()->getDefaultHandle();
    fbInfo.attachmentCount      = attachmentCount;
    fbInfo.pAttachments         = views.data();
    fbInfo.width                = info.size().width;
    fbInfo.height               = info.size().height;
    fbInfo.layers               = info.size().layers;
    
    if (m_vkd->vkCreateFramebuffer(m_vkd->device(), &fbInfo, nullptr, &m_handle) != VK_SUCCESS)
      Logger::err("DxvkFramebuffer: Failed to create framebuffer object");
  }
  
  
  DxvkFramebuffer::~DxvkFramebuffer() {
    m_vkd->vkDestroyFramebuffer(m_vkd->device(), m_handle, nullptr);
  }

}