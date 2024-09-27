#include "dxvk_framebuffer.h"

namespace dxvk {
  
  DxvkFramebufferInfo::DxvkFramebufferInfo() {

  }


  DxvkFramebufferInfo::DxvkFramebufferInfo(
    const DxvkRenderTargets&      renderTargets,
    const DxvkFramebufferSize&    defaultSize)
  : m_renderTargets (renderTargets),
    m_renderSize    (computeRenderSize(defaultSize)) {

    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      if (m_renderTargets.color[i].view != nullptr) {
        m_attachments[m_attachmentCount++] = i;
        m_sampleCount = m_renderTargets.color[i].view->image()->info().sampleCount;
      }
    }

    if (m_renderTargets.depth.view != nullptr) {
      m_attachments[m_attachmentCount++] = -1;
      m_sampleCount = m_renderTargets.depth.view->image()->info().sampleCount;
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
        && m_renderSize.layers == view->info().layerCount;
  }


  bool DxvkFramebufferInfo::isWritable(uint32_t attachmentIndex, VkImageAspectFlags aspects) const {
    VkImageAspectFlags writableAspects = vk::getWritableAspectsForLayout(getAttachment(attachmentIndex).layout);
    return (writableAspects & aspects) == aspects;
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
    auto layers = renderTarget->info().layerCount;
    return DxvkFramebufferSize { extent.width, extent.height, layers };
  }

}