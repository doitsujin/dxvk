#include <algorithm>

#include "dxvk_renderpass.h"

namespace dxvk {
  
  bool DxvkRenderPassFormat::matchesFormat(const DxvkRenderPassFormat& other) const {
    bool equal = m_samples == other.m_samples;
    
    equal =  m_depth.format         == other.m_depth.format
          && m_depth.initialLayout  == other.m_depth.initialLayout
          && m_depth.finalLayout    == other.m_depth.finalLayout
          && m_depth.renderLayout   == other.m_depth.renderLayout;
    
    for (uint32_t i = 0; i < MaxNumRenderTargets && equal; i++) {
      equal &= m_color[i].format         == other.m_color[i].format
            && m_color[i].initialLayout  == other.m_color[i].initialLayout
            && m_color[i].finalLayout    == other.m_color[i].finalLayout
            && m_color[i].renderLayout   == other.m_color[i].renderLayout;
    }
    
    return equal;
  }
  
  
  DxvkRenderPass::DxvkRenderPass(
    const Rc<vk::DeviceFn>&     vkd,
    const DxvkRenderPassFormat& fmt)
  : m_vkd(vkd), m_format(fmt) {
    std::vector<VkAttachmentDescription> attachments;
    
    VkAttachmentReference                                  depthRef;
    std::array<VkAttachmentReference, MaxNumRenderTargets> colorRef;
    
    // Render passes may not require the previous
    // contents of the attachments to be preserved.
    const DxvkRenderTargetFormat depthFmt = fmt.getDepthFormat();
    
    if (depthFmt.format != VK_FORMAT_UNDEFINED) {
      VkAttachmentDescription desc;
      desc.flags          = 0;
      desc.format         = depthFmt.format;
      desc.samples        = fmt.getSampleCount();
      desc.loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
      desc.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
      desc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
      desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
      desc.initialLayout  = depthFmt.initialLayout;
      desc.finalLayout    = depthFmt.finalLayout;
      
      depthRef.attachment = attachments.size();
      depthRef.layout     = depthFmt.renderLayout;
      
      attachments.push_back(desc);
    }
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      const DxvkRenderTargetFormat colorFmt = fmt.getColorFormat(i);
      
      colorRef[i].attachment = VK_ATTACHMENT_UNUSED;
      colorRef[i].layout     = VK_IMAGE_LAYOUT_UNDEFINED;
      
      if (colorFmt.format != VK_FORMAT_UNDEFINED) {
        VkAttachmentDescription desc;
        desc.flags            = 0;
        desc.format           = colorFmt.format;
        desc.samples          = fmt.getSampleCount();
        desc.loadOp           = VK_ATTACHMENT_LOAD_OP_LOAD;
        desc.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
        desc.stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        desc.stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc.initialLayout    = colorFmt.initialLayout;
        desc.finalLayout      = colorFmt.finalLayout;
        
        colorRef[i].attachment = attachments.size();
        colorRef[i].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        attachments.push_back(desc);
      }
    }
    
    VkSubpassDescription subpass;
    subpass.flags                     = 0;
    subpass.pipelineBindPoint         = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount      = 0;
    subpass.pInputAttachments         = nullptr;
    subpass.colorAttachmentCount      = colorRef.size();
    subpass.pColorAttachments         = colorRef.data();
    subpass.pResolveAttachments       = nullptr;
    subpass.pDepthStencilAttachment   = depthFmt.format != VK_FORMAT_UNDEFINED ? &depthRef : nullptr;
    subpass.preserveAttachmentCount   = 0;
    subpass.pPreserveAttachments      = nullptr;
    
    std::array<VkSubpassDependency, 2> subpassDeps = {{
      { VK_SUBPASS_EXTERNAL, 0,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT    |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_SHADER_READ_BIT                     |
        VK_ACCESS_SHADER_WRITE_BIT                    |
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT           |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT          |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT   |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT  |
        VK_ACCESS_TRANSFER_READ_BIT                   |
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT           |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT          |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT   |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0 },
      { 0, VK_SUBPASS_EXTERNAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT    |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT           |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT          |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT   |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT                     |
        VK_ACCESS_SHADER_WRITE_BIT                    |
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT           |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT          |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT   |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT  |
        VK_ACCESS_TRANSFER_READ_BIT                   |
        VK_ACCESS_TRANSFER_WRITE_BIT, 0 },
    }};
    
    VkRenderPassCreateInfo info;
    info.sType                        = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.pNext                        = nullptr;
    info.flags                        = 0;
    info.attachmentCount              = attachments.size();
    info.pAttachments                 = attachments.data();
    info.subpassCount                 = 1;
    info.pSubpasses                   = &subpass;
    info.dependencyCount              = subpassDeps.size();
    info.pDependencies                = subpassDeps.data();
    
    if (m_vkd->vkCreateRenderPass(m_vkd->device(), &info, nullptr, &m_renderPass) != VK_SUCCESS)
      throw DxvkError("DxvkRenderPass::DxvkRenderPass: Failed to create render pass object");
  }
  
  
  DxvkRenderPass::~DxvkRenderPass() {
    m_vkd->vkDestroyRenderPass(
      m_vkd->device(), m_renderPass, nullptr);
  }
  
  
  DxvkRenderPassPool::DxvkRenderPassPool(const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd) {
    
  }
  
  
  DxvkRenderPassPool::~DxvkRenderPassPool() {
    
  }
  
  
  Rc<DxvkRenderPass> DxvkRenderPassPool::getRenderPass(
    const DxvkRenderPassFormat& fmt) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Rc<DxvkRenderPass> renderPass = nullptr;
    
    for (uint32_t i = 0; i < m_renderPasses.size() && renderPass == nullptr; i++) {
      if (m_renderPasses[i]->matchesFormat(fmt))
        renderPass = m_renderPasses[i];
    }
    
    if (renderPass != nullptr)
      return renderPass;
    
    renderPass = this->createRenderPass(fmt);
    m_renderPasses.push_back(renderPass);
    return renderPass;
  }
  
  
  Rc<DxvkRenderPass> DxvkRenderPassPool::createRenderPass(
    const DxvkRenderPassFormat& fmt) {
    return new DxvkRenderPass(m_vkd, fmt);
  }
  
}