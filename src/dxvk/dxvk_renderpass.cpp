#include <algorithm>

#include "dxvk_renderpass.h"

namespace dxvk {
  
  bool DxvkRenderPassFormat::matches(const DxvkRenderPassFormat& fmt) const {
    bool eq = sampleCount == fmt.sampleCount;
    
    for (uint32_t i = 0; i < MaxNumRenderTargets && eq; i++) {
      eq &= color[i].format == fmt.color[i].format
         && color[i].layout == fmt.color[i].layout;
    }
    
    eq &= depth.format == fmt.depth.format
       && depth.layout == fmt.depth.layout;
    
    return eq;
  }
  
  
  DxvkRenderPass::DxvkRenderPass(
    const Rc<vk::DeviceFn>&       vkd,
    const DxvkRenderPassFormat&   fmt)
  : m_vkd(vkd), m_format(fmt),
    m_default(createRenderPass(DxvkRenderPassOps())) {
    
  }
  
  
  DxvkRenderPass::~DxvkRenderPass() {
    m_vkd->vkDestroyRenderPass(m_vkd->device(), m_default, nullptr);
    
    for (const auto& i : m_instances) {
      m_vkd->vkDestroyRenderPass(
        m_vkd->device(), i.handle, nullptr);
    }
  }
  
  
  bool DxvkRenderPass::hasCompatibleFormat(const DxvkRenderPassFormat& fmt) const {
    return m_format.matches(fmt);
  }
  
  
  VkRenderPass DxvkRenderPass::getHandle(const DxvkRenderPassOps& ops) {
    std::lock_guard<sync::Spinlock> lock(m_mutex);
    
    for (const auto& i : m_instances) {
      if (compareOps(i.ops, ops))
        return i.handle;
    }
    
    VkRenderPass handle = this->createRenderPass(ops);
    m_instances.push_back({ ops, handle });
    return handle;
  }
  
  
  VkRenderPass DxvkRenderPass::createRenderPass(const DxvkRenderPassOps& ops) {
    std::vector<VkAttachmentDescription> attachments;
    
    VkAttachmentReference                                  depthRef;
    std::array<VkAttachmentReference, MaxNumRenderTargets> colorRef;
    
    // Render passes may not require the previous
    // contents of the attachments to be preserved.
    for (uint32_t i = 0; i < MaxNumRenderTargets; i++) {
      colorRef[i].attachment = VK_ATTACHMENT_UNUSED;
      colorRef[i].layout     = VK_IMAGE_LAYOUT_UNDEFINED;
      
      if (m_format.color[i].format != VK_FORMAT_UNDEFINED) {
        VkAttachmentDescription desc;
        desc.flags            = 0;
        desc.format           = m_format.color[i].format;
        desc.samples          = m_format.sampleCount;
        desc.loadOp           = ops.colorOps[i].loadOp;
        desc.storeOp          = ops.colorOps[i].storeOp;
        desc.stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        desc.stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc.initialLayout    = ops.colorOps[i].loadLayout;
        desc.finalLayout      = ops.colorOps[i].storeLayout;
        
        colorRef[i].attachment = attachments.size();
        colorRef[i].layout     = m_format.color[i].layout;
        
        attachments.push_back(desc);
      }
    }
    
    if (m_format.depth.format != VK_FORMAT_UNDEFINED) {
      VkAttachmentDescription desc;
      desc.flags          = 0;
      desc.format         = m_format.depth.format;
      desc.samples        = m_format.sampleCount;
      desc.loadOp         = ops.depthOps.loadOpD;
      desc.storeOp        = ops.depthOps.storeOpD;
      desc.stencilLoadOp  = ops.depthOps.loadOpS;
      desc.stencilStoreOp = ops.depthOps.storeOpS;
      desc.initialLayout  = ops.depthOps.loadLayout;
      desc.finalLayout    = ops.depthOps.storeLayout;
      
      depthRef.attachment = attachments.size();
      depthRef.layout     = m_format.depth.layout;
      
      attachments.push_back(desc);
    }
    
    VkSubpassDescription subpass;
    subpass.flags                     = 0;
    subpass.pipelineBindPoint         = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount      = 0;
    subpass.pInputAttachments         = nullptr;
    subpass.colorAttachmentCount      = colorRef.size();
    subpass.pColorAttachments         = colorRef.data();
    subpass.pResolveAttachments       = nullptr;
    subpass.pDepthStencilAttachment   = &depthRef;
    subpass.preserveAttachmentCount   = 0;
    subpass.pPreserveAttachments      = nullptr;
    
    if (m_format.depth.format == VK_FORMAT_UNDEFINED)
      subpass.pDepthStencilAttachment = nullptr;
    
    std::array<VkSubpassDependency, 3> subpassDeps;
    uint32_t                           subpassDepCount = 0;

    if (ops.barrier.srcStages & (
          VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT |
          VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT |
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)) {
      subpassDeps[subpassDepCount++] = { 0, 0,
        VK_PIPELINE_STAGE_TRANSFORM_FEEDBACK_BIT_EXT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, /* XXX */
        VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
        VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT, 0 };
    }

    if (ops.barrier.srcStages & (
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
          VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT |
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)) {
      subpassDeps[subpassDepCount++] = { 0, 0,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT, 0 };
    }

    if (ops.barrier.srcStages && ops.barrier.dstStages) {
      subpassDeps[subpassDepCount++] = {
        0, VK_SUBPASS_EXTERNAL,
        ops.barrier.srcStages,
        ops.barrier.dstStages,
        ops.barrier.srcAccess,
        ops.barrier.dstAccess, 0 };
    }
    
    VkRenderPassCreateInfo info;
    info.sType                        = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.pNext                        = nullptr;
    info.flags                        = 0;
    info.attachmentCount              = attachments.size();
    info.pAttachments                 = attachments.data();
    info.subpassCount                 = 1;
    info.pSubpasses                   = &subpass;
    info.dependencyCount              = subpassDepCount;
    info.pDependencies                = subpassDepCount ? subpassDeps.data() : nullptr;
    
    VkRenderPass renderPass = VK_NULL_HANDLE;
    
    if (m_vkd->vkCreateRenderPass(m_vkd->device(), &info, nullptr, &renderPass) != VK_SUCCESS) {
      Logger::err("DxvkRenderPass: Failed to create render pass object");
      return VK_NULL_HANDLE;
    }
    
    return renderPass;
  }
  
  
  bool DxvkRenderPass::compareOps(
    const DxvkRenderPassOps& a,
    const DxvkRenderPassOps& b) {
    bool eq = a.barrier.srcStages == b.barrier.srcStages
           && a.barrier.srcAccess == b.barrier.srcAccess
           && a.barrier.dstStages == b.barrier.dstStages
           && a.barrier.dstAccess == b.barrier.dstAccess;
    
    if (eq) {
      eq &= a.depthOps.loadOpD     == b.depthOps.loadOpD
         && a.depthOps.loadOpS     == b.depthOps.loadOpS
         && a.depthOps.loadLayout  == b.depthOps.loadLayout
         && a.depthOps.storeOpD    == b.depthOps.storeOpD
         && a.depthOps.storeOpS    == b.depthOps.storeOpS
         && a.depthOps.storeLayout == b.depthOps.storeLayout;
    }
    
    for (uint32_t i = 0; i < MaxNumRenderTargets && eq; i++) {
      eq &= a.colorOps[i].loadOp      == b.colorOps[i].loadOp
         && a.colorOps[i].loadLayout  == b.colorOps[i].loadLayout
         && a.colorOps[i].storeOp     == b.colorOps[i].storeOp
         && a.colorOps[i].storeLayout == b.colorOps[i].storeLayout;
    }
    
    return eq;
  }
  
  
  DxvkRenderPassPool::DxvkRenderPassPool(const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd) {
    
  }
  
  
  DxvkRenderPassPool::~DxvkRenderPassPool() {
    
  }
  
  
  Rc<DxvkRenderPass> DxvkRenderPassPool::getRenderPass(const DxvkRenderPassFormat& fmt) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& r : m_renderPasses) {
      if (r->hasCompatibleFormat(fmt))
        return r;
    }
    
    Rc<DxvkRenderPass> rp = new DxvkRenderPass(m_vkd, fmt);
    m_renderPasses.push_back(rp);
    return rp;
  }
  
}