#include "dxvk_renderpass.h"

namespace dxvk {
  
  DxvkRenderPassFormat::DxvkRenderPassFormat() {
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++)
      m_color.at(i) = VK_FORMAT_UNDEFINED;
    m_depth   = VK_FORMAT_UNDEFINED;
    m_samples = VK_SAMPLE_COUNT_1_BIT;
  }
  
  
  size_t DxvkRenderPassFormat::hash() const {
    DxvkHashState result;
    std::hash<VkFormat>              fhash;
    std::hash<VkSampleCountFlagBits> shash;
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++)
      result.add(fhash(m_color.at(i)));
    
    result.add(fhash(m_depth));
    result.add(shash(m_samples));
    return result;
  }
  
  
  bool DxvkRenderPassFormat::operator == (const DxvkRenderPassFormat& other) const {
    bool equal = m_depth   == other.m_depth
              && m_samples == other.m_samples;
    for (uint32_t i = 0; i < MaxNumRenderTargets && !equal; i++)
      equal = m_color.at(i) == other.m_color.at(i);
    return equal;
  }
  
  
  bool DxvkRenderPassFormat::operator != (const DxvkRenderPassFormat& other) const {
    return !this->operator == (other);
  }
  
  
  DxvkRenderPass::DxvkRenderPass(
    const Rc<vk::DeviceFn>&     vkd,
    const DxvkRenderPassFormat& fmt,
          VkImageLayout         initialLayout,
          VkImageLayout         finalLayout)
  : m_vkd(vkd), m_format(fmt) {
    TRACE(this, initialLayout, finalLayout);
    std::vector<VkAttachmentDescription> attachments;
    
    VkAttachmentReference                                  depthRef;
    std::array<VkAttachmentReference, MaxNumRenderTargets> colorRef;
    
    // Render passes may not require the previous
    // contents of the attachments to be preserved.
    VkAttachmentLoadOp  loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
    VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    
    if (initialLayout == VK_IMAGE_LAYOUT_UNDEFINED)
      loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    
    if (fmt.getDepthFormat() != VK_FORMAT_UNDEFINED) {
      VkAttachmentDescription desc;
      desc.flags          = 0;
      desc.format         = fmt.getDepthFormat();
      desc.samples        = fmt.getSampleCount();
      desc.loadOp         = loadOp;
      desc.storeOp        = storeOp;
      desc.stencilLoadOp  = loadOp;
      desc.stencilStoreOp = storeOp;
      desc.initialLayout  = initialLayout;
      desc.finalLayout    = finalLayout;
      
      depthRef.attachment = attachments.size();
      depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      
      attachments.push_back(desc);
    }
    
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      colorRef.at(i).attachment = VK_ATTACHMENT_UNUSED;
      colorRef.at(i).layout     = VK_IMAGE_LAYOUT_UNDEFINED;
      
      if (fmt.getColorFormat(i) != VK_FORMAT_UNDEFINED) {
        VkAttachmentDescription desc;
        desc.flags          = 0;
        desc.format         = fmt.getColorFormat(i);
        desc.samples        = fmt.getSampleCount();
        desc.loadOp         = loadOp;
        desc.storeOp        = storeOp;
        desc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc.initialLayout  = initialLayout;
        desc.finalLayout    = finalLayout;
        
        colorRef.at(i).attachment = attachments.size();
        colorRef.at(i).layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
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
    subpass.pDepthStencilAttachment   = fmt.getDepthFormat() != VK_FORMAT_UNDEFINED ? &depthRef : nullptr;
    subpass.preserveAttachmentCount   = 0;
    subpass.pPreserveAttachments      = nullptr;
    
    VkRenderPassCreateInfo info;
    info.sType                        = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.pNext                        = nullptr;
    info.flags                        = 0;
    info.attachmentCount              = attachments.size();
    info.pAttachments                 = attachments.data();
    info.subpassCount                 = 1;
    info.pSubpasses                   = &subpass;
    info.dependencyCount              = 0;
    info.pDependencies                = nullptr;
    
    if (m_vkd->vkCreateRenderPass(m_vkd->device(), &info, nullptr, &m_renderPass) != VK_SUCCESS)
      throw DxvkError("DxvkRenderPass::DxvkRenderPass: Failed to create render pass object");
  }
  
  
  DxvkRenderPass::~DxvkRenderPass() {
    TRACE(this);
    m_vkd->vkDestroyRenderPass(
      m_vkd->device(), m_renderPass, nullptr);
  }
  
  
  DxvkRenderPassPool::DxvkRenderPassPool(const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd) {
    TRACE(this);
  }
  
  
  DxvkRenderPassPool::~DxvkRenderPassPool() {
    TRACE(this);
  }
  
  
  Rc<DxvkRenderPass> DxvkRenderPassPool::getRenderPass(
    const DxvkRenderPassFormat& fmt) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto rp = m_renderPasses.find(fmt);
    
    if (rp != m_renderPasses.end())
      return rp->second;
    
    auto result = this->createRenderPass(fmt);
    m_renderPasses.insert(std::make_pair(fmt, result));
    return result;
  }
  
  
  Rc<DxvkRenderPass> DxvkRenderPassPool::createRenderPass(
    const DxvkRenderPassFormat& fmt) {
    return new DxvkRenderPass(m_vkd, fmt,
      VK_IMAGE_LAYOUT_GENERAL,
      VK_IMAGE_LAYOUT_GENERAL);
  }
  
}