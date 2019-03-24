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
    std::array<VkAttachmentDescription, 2> attachments;
    attachments[0].flags            = 0;
    attachments[0].format           = m_dstImageView->info().format;
    attachments[0].samples          = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout      = m_dstImageView->imageInfo().layout;

    attachments[1].flags            = 0;
    attachments[1].format           = m_dstImageView->info().format;
    attachments[1].samples          = m_srcImageView->imageInfo().sampleCount;
    attachments[1].loadOp           = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout    = m_srcImageView->imageInfo().layout;
    attachments[1].finalLayout      = m_srcImageView->imageInfo().layout;
    
    VkAttachmentReference dstRef;
    dstRef.attachment    = 0;
    dstRef.layout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference srcRef;
    srcRef.attachment    = 1;
    srcRef.layout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass;
    subpass.flags               = 0;
    subpass.pipelineBindPoint   = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments   = nullptr;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments   = &srcRef;
    subpass.pResolveAttachments = &dstRef;
    subpass.pDepthStencilAttachment = nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;

    VkRenderPassCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.attachmentCount        = attachments.size();
    info.pAttachments           = attachments.data();
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
    
    std::array<VkImageView, 2> viewHandles = {
      m_dstImageView->handle(),
      m_srcImageView->handle(),
    };

    VkFramebufferCreateInfo fboInfo;
    fboInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fboInfo.pNext           = nullptr;
    fboInfo.flags           = 0;
    fboInfo.renderPass      = m_renderPass;
    fboInfo.attachmentCount = viewHandles.size();
    fboInfo.pAttachments    = viewHandles.data();
    fboInfo.width           = dstExtent.width;
    fboInfo.height          = dstExtent.height;
    fboInfo.layers          = dstSubresources.layerCount;
    
    VkFramebuffer result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateFramebuffer(m_vkd->device(), &fboInfo, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaMipGenRenderPass: Failed to create target framebuffer");
    return result;
  }

}
