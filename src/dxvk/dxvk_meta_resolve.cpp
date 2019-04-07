#include "dxvk_meta_resolve.h"

namespace dxvk {
  
  DxvkMetaResolveRenderPass::DxvkMetaResolveRenderPass(
    const Rc<vk::DeviceFn>&   vkd,
    const Rc<DxvkImageView>&  dstImageView,
    const Rc<DxvkImageView>&  srcImageView)
  : m_vkd(vkd),
    m_dstImageView(dstImageView),
    m_srcImageView(srcImageView),
    m_renderPass  (createRenderPass ()),
    m_framebuffer (createFramebuffer()) { }
  

  DxvkMetaResolveRenderPass::~DxvkMetaResolveRenderPass() {
    m_vkd->vkDestroyFramebuffer(m_vkd->device(), m_framebuffer, nullptr);
    m_vkd->vkDestroyRenderPass (m_vkd->device(), m_renderPass,  nullptr);
  }


  VkRenderPass DxvkMetaResolveRenderPass::createRenderPass() const {
    VkAttachmentDescription attachment;
    attachment.flags            = 0;
    attachment.format           = m_dstImageView->info().format;
    attachment.samples          = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout      = m_dstImageView->imageInfo().layout;
    
    VkAttachmentReference dstRef;
    dstRef.attachment    = 0;
    dstRef.layout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass;
    subpass.flags               = 0;
    subpass.pipelineBindPoint   = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments   = nullptr;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments   = &dstRef;
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
    info.dependencyCount        = 0;
    info.pDependencies          = nullptr;

    VkRenderPass result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateRenderPass(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaResolveRenderPass: Failed to create render pass");
    return result;
  }


  VkFramebuffer DxvkMetaResolveRenderPass::createFramebuffer() const {
    VkImageSubresourceRange dstSubresources = m_dstImageView->subresources();
    VkExtent3D              dstExtent       = m_dstImageView->mipLevelExtent(0);
    VkImageView             dstHandle       = m_dstImageView->handle();

    VkFramebufferCreateInfo fboInfo;
    fboInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fboInfo.pNext           = nullptr;
    fboInfo.flags           = 0;
    fboInfo.renderPass      = m_renderPass;
    fboInfo.attachmentCount = 1;
    fboInfo.pAttachments    = &dstHandle;
    fboInfo.width           = dstExtent.width;
    fboInfo.height          = dstExtent.height;
    fboInfo.layers          = dstSubresources.layerCount;
    
    VkFramebuffer result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateFramebuffer(m_vkd->device(), &fboInfo, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaMipGenRenderPass: Failed to create target framebuffer");
    return result;
  }

}
