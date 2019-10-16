#include "dxvk_meta_mipgen.h"

namespace dxvk {

  DxvkMetaMipGenRenderPass::DxvkMetaMipGenRenderPass(
    const Rc<vk::DeviceFn>&   vkd,
    const Rc<DxvkImageView>&  view)
  : m_vkd(vkd), m_view(view), m_renderPass(createRenderPass()) {
    // Determine view type based on image type
    const std::array<std::pair<VkImageViewType, VkImageViewType>, 3> viewTypes = {{
      { VK_IMAGE_VIEW_TYPE_1D_ARRAY, VK_IMAGE_VIEW_TYPE_1D_ARRAY },
      { VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_VIEW_TYPE_2D_ARRAY },
      { VK_IMAGE_VIEW_TYPE_3D,       VK_IMAGE_VIEW_TYPE_2D_ARRAY },
    }};
    
    m_srcViewType = viewTypes.at(uint32_t(view->imageInfo().type)).first;
    m_dstViewType = viewTypes.at(uint32_t(view->imageInfo().type)).second;
    
    // Create image views and framebuffers
    m_passes.resize(view->info().numLevels - 1);
    
    for (uint32_t i = 0; i < m_passes.size(); i++)
      m_passes.at(i) = this->createFramebuffer(i);
  }
  
  
  DxvkMetaMipGenRenderPass::~DxvkMetaMipGenRenderPass() {
    for (const auto& pass : m_passes) {
      m_vkd->vkDestroyFramebuffer(m_vkd->device(), pass.framebuffer, nullptr);
      m_vkd->vkDestroyImageView(m_vkd->device(), pass.dstView, nullptr);
      m_vkd->vkDestroyImageView(m_vkd->device(), pass.srcView, nullptr);
    }
    
    m_vkd->vkDestroyRenderPass(m_vkd->device(), m_renderPass, nullptr);
  }
  
  
  VkExtent3D DxvkMetaMipGenRenderPass::passExtent(uint32_t passId) const {
    VkExtent3D extent = m_view->mipLevelExtent(passId + 1);
    
    if (m_view->imageInfo().type != VK_IMAGE_TYPE_3D)
      extent.depth = m_view->info().numLayers;
    
    return extent;
  }
  
  
  VkRenderPass DxvkMetaMipGenRenderPass::createRenderPass() const {
    std::array<VkSubpassDependency, 2> subpassDeps = {{
      { VK_SUBPASS_EXTERNAL, 0,
        m_view->imageInfo().stages,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0 },
      { 0, VK_SUBPASS_EXTERNAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        m_view->imageInfo().stages,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        m_view->imageInfo().access, 0 },
    }};
    
    VkAttachmentDescription attachment;
    attachment.flags            = 0;
    attachment.format           = m_view->info().format;
    attachment.samples          = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout      = m_view->imageInfo().layout;
    
    VkAttachmentReference attachmentRef;
    attachmentRef.attachment    = 0;
    attachmentRef.layout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass;
    subpass.flags               = 0;
    subpass.pipelineBindPoint   = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments   = nullptr;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments   = &attachmentRef;
    subpass.pResolveAttachments = nullptr;
    subpass.pDepthStencilAttachment = nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;
    
    VkRenderPassCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.attachmentCount        = 1;
    info.pAttachments           = &attachment;
    info.subpassCount           = 1;
    info.pSubpasses             = &subpass;
    info.dependencyCount        = subpassDeps.size();
    info.pDependencies          = subpassDeps.data();
    
    VkRenderPass result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateRenderPass(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaMipGenRenderPass: Failed to create render pass");
    return result;
  }
  
  
  DxvkMetaBlitPass DxvkMetaMipGenRenderPass::createFramebuffer(uint32_t pass) const {
    DxvkMetaBlitPass result;
    result.srcView      = VK_NULL_HANDLE;
    result.dstView      = VK_NULL_HANDLE;
    result.renderPass   = m_renderPass;
    result.framebuffer  = VK_NULL_HANDLE;
    
    // Common image view info
    VkImageViewCreateInfo viewInfo;
    viewInfo.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.pNext      = nullptr;
    viewInfo.flags      = 0;
    viewInfo.image      = m_view->imageHandle();
    viewInfo.format     = m_view->info().format;
    viewInfo.components = {
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    
    // Create source image view, which points to
    // the one mip level we're going to sample.
    VkImageSubresourceRange srcSubresources;
    srcSubresources.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    srcSubresources.baseMipLevel   = m_view->info().minLevel + pass;
    srcSubresources.levelCount     = 1;
    srcSubresources.baseArrayLayer = m_view->info().minLayer;
    srcSubresources.layerCount     = m_view->info().numLayers;
    
    viewInfo.viewType         = m_srcViewType;
    viewInfo.subresourceRange = srcSubresources;
    
    if (m_vkd->vkCreateImageView(m_vkd->device(), &viewInfo, nullptr, &result.srcView) != VK_SUCCESS)
      throw DxvkError("DxvkMetaMipGenRenderPass: Failed to create source image view");
    
    // Create destination image view, which points
    // to the mip level we're going to render to.
    VkExtent3D dstExtent = m_view->mipLevelExtent(pass + 1);
    
    VkImageSubresourceRange dstSubresources;
    dstSubresources.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    dstSubresources.baseMipLevel   = m_view->info().minLevel + pass + 1;
    dstSubresources.levelCount     = 1;
    
    if (m_view->imageInfo().type != VK_IMAGE_TYPE_3D) {
      dstSubresources.baseArrayLayer = m_view->info().minLayer;
      dstSubresources.layerCount     = m_view->info().numLayers;
    } else {
      dstSubresources.baseArrayLayer = 0;
      dstSubresources.layerCount     = dstExtent.depth;
    }
    
    viewInfo.viewType         = m_dstViewType;
    viewInfo.subresourceRange = dstSubresources;
    
    if (m_vkd->vkCreateImageView(m_vkd->device(), &viewInfo, nullptr, &result.dstView) != VK_SUCCESS)
      throw DxvkError("DxvkMetaMipGenRenderPass: Failed to create target image view");
    
    // Create framebuffer using the destination
    // image view as its color attachment.
    VkFramebufferCreateInfo fboInfo;
    fboInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fboInfo.pNext           = nullptr;
    fboInfo.flags           = 0;
    fboInfo.renderPass      = m_renderPass;
    fboInfo.attachmentCount = 1;
    fboInfo.pAttachments    = &result.dstView;
    fboInfo.width           = dstExtent.width;
    fboInfo.height          = dstExtent.height;
    fboInfo.layers          = dstSubresources.layerCount;
    
    if (m_vkd->vkCreateFramebuffer(m_vkd->device(), &fboInfo, nullptr, &result.framebuffer) != VK_SUCCESS)
      throw DxvkError("DxvkMetaMipGenRenderPass: Failed to create target framebuffer");
    
    return result;
  }
  
}
