#include "dxvk_meta_resolve.h"

namespace dxvk {
  
  DxvkMetaResolveFramebuffer::DxvkMetaResolveFramebuffer(
    const Rc<vk::DeviceFn>&         vkd,
    const Rc<DxvkImage>&            dstImage,
          VkImageSubresourceLayers  dstLayers,
    const Rc<DxvkImage>&            srcImage,
          VkImageSubresourceLayers  srcLayers,
          VkFormat                  format)
  : m_vkd(vkd) {
    // Create a render pass with one render
    // target and one resolve attachment.
    std::array<VkAttachmentDescription, 2> attachmentInfos = {{
      VkAttachmentDescription {
        0, format, dstImage->info().sampleCount,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        dstImage->info().layout },
      VkAttachmentDescription {
        0, format, srcImage->info().sampleCount,
        VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        srcImage->info().layout,
        srcImage->info().layout },
    }};
    
    // Make sure layout transitions are correctly ordered
    std::array<VkSubpassDependency, 2> subpassDeps = {{
      { VK_SUBPASS_EXTERNAL, 0,
        srcImage->info().stages | dstImage->info().stages,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        srcImage->info().access | dstImage->info().access,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0 },
      { 0, VK_SUBPASS_EXTERNAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        srcImage->info().stages | dstImage->info().stages,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        srcImage->info().access | dstImage->info().access, 0 },
    }};
    
    VkAttachmentReference dstAttachmentRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference srcAttachmentRef = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    
    VkSubpassDescription spInfo;
    spInfo.flags                   = 0;
    spInfo.inputAttachmentCount    = 0;
    spInfo.pInputAttachments       = nullptr;
    spInfo.colorAttachmentCount    = 1;
    spInfo.pColorAttachments       = &srcAttachmentRef;
    spInfo.pResolveAttachments     = &dstAttachmentRef;
    spInfo.pDepthStencilAttachment = nullptr;
    spInfo.preserveAttachmentCount = 0;
    spInfo.pPreserveAttachments    = nullptr;
    
    VkRenderPassCreateInfo rpInfo;
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.pNext             = nullptr;
    rpInfo.flags             = 0;
    rpInfo.attachmentCount   = attachmentInfos.size();
    rpInfo.pAttachments      = attachmentInfos.data();
    rpInfo.subpassCount      = 1;
    rpInfo.pSubpasses        = &spInfo;
    rpInfo.dependencyCount   = subpassDeps.size();
    rpInfo.pDependencies     = subpassDeps.data();
    
    m_vkd->vkCreateRenderPass(m_vkd->device(), &rpInfo, nullptr, &m_renderPass);
    
    // Create views for the destination and source image
    VkImageViewCreateInfo dstInfo;
    dstInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    dstInfo.pNext            = nullptr;
    dstInfo.flags            = 0;
    dstInfo.image            = dstImage->handle();
    dstInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    dstInfo.format           = format;
    dstInfo.components       = VkComponentMapping {
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    dstInfo.subresourceRange = VkImageSubresourceRange {
      dstLayers.aspectMask, dstLayers.mipLevel, 1,
      dstLayers.baseArrayLayer, dstLayers.layerCount };
    
    VkImageViewCreateInfo srcInfo;
    srcInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    srcInfo.pNext            = nullptr;
    srcInfo.flags            = 0;
    srcInfo.image            = srcImage->handle();
    srcInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    srcInfo.format           = format;
    srcInfo.components       = VkComponentMapping {
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    srcInfo.subresourceRange = VkImageSubresourceRange {
      srcLayers.aspectMask, srcLayers.mipLevel, 1,
      srcLayers.baseArrayLayer, srcLayers.layerCount };
    
    m_vkd->vkCreateImageView(m_vkd->device(), &dstInfo, nullptr, &m_dstImageView);
    m_vkd->vkCreateImageView(m_vkd->device(), &srcInfo, nullptr, &m_srcImageView);
    
    // Create a framebuffer containing the two image views
    std::array<VkImageView, 2> attachments = {{ m_dstImageView, m_srcImageView }};
    
    VkFramebufferCreateInfo fboInfo;
    fboInfo.sType            = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fboInfo.pNext            = nullptr;
    fboInfo.flags            = 0;
    fboInfo.renderPass       = m_renderPass;
    fboInfo.attachmentCount  = attachments.size();
    fboInfo.pAttachments     = attachments.data();
    fboInfo.width            = dstImage->info().extent.width;
    fboInfo.height           = dstImage->info().extent.height;
    fboInfo.layers           = dstLayers.layerCount;
    
    m_vkd->vkCreateFramebuffer(m_vkd->device(), &fboInfo, nullptr, &m_framebuffer);
  }
  
  
  DxvkMetaResolveFramebuffer::~DxvkMetaResolveFramebuffer() {
    m_vkd->vkDestroyFramebuffer(m_vkd->device(), m_framebuffer,  nullptr);
    m_vkd->vkDestroyImageView  (m_vkd->device(), m_srcImageView, nullptr);
    m_vkd->vkDestroyImageView  (m_vkd->device(), m_dstImageView, nullptr);
    m_vkd->vkDestroyRenderPass (m_vkd->device(), m_renderPass,   nullptr);
  }
  
}
