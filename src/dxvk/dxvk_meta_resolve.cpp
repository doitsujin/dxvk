#include "dxvk_device.h"
#include "dxvk_meta_resolve.h"

#include <dxvk_fullscreen_geom.h>
#include <dxvk_fullscreen_vert.h>
#include <dxvk_fullscreen_layer_vert.h>

#include <dxvk_resolve_frag_d.h>
#include <dxvk_resolve_frag_ds.h>
#include <dxvk_resolve_frag_f.h>
#include <dxvk_resolve_frag_f_amd.h>
#include <dxvk_resolve_frag_u.h>
#include <dxvk_resolve_frag_i.h>

namespace dxvk {
  
  DxvkMetaResolveRenderPass::DxvkMetaResolveRenderPass(
    const Rc<vk::DeviceFn>&   vkd,
    const Rc<DxvkImageView>&  dstImageView,
    const Rc<DxvkImageView>&  srcImageView,
    const Rc<DxvkImageView>&  srcStencilView,
          bool                discardDst)
  : m_vkd(vkd),
    m_dstImageView(dstImageView),
    m_srcImageView(srcImageView),
    m_srcStencilView(srcStencilView),
    m_renderPass  (createShaderRenderPass(discardDst)),
    m_framebuffer (createShaderFramebuffer()) { }


  DxvkMetaResolveRenderPass::DxvkMetaResolveRenderPass(
    const Rc<vk::DeviceFn>&        vkd,
    const Rc<DxvkImageView>&       dstImageView,
    const Rc<DxvkImageView>&       srcImageView,
          VkResolveModeFlagBitsKHR modeD,
          VkResolveModeFlagBitsKHR modeS)
  : m_vkd(vkd),
    m_dstImageView(dstImageView),
    m_srcImageView(srcImageView),
    m_renderPass  (createAttachmentRenderPass(modeD, modeS)),
    m_framebuffer (createAttachmentFramebuffer()) { }
  

  DxvkMetaResolveRenderPass::~DxvkMetaResolveRenderPass() {
    m_vkd->vkDestroyFramebuffer(m_vkd->device(), m_framebuffer, nullptr);
    m_vkd->vkDestroyRenderPass (m_vkd->device(), m_renderPass,  nullptr);
  }


  VkRenderPass DxvkMetaResolveRenderPass::createShaderRenderPass(bool discard) const {
    auto formatInfo = m_dstImageView->formatInfo();
    bool isColorImage = (formatInfo->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT);

    VkAttachmentDescription attachment;
    attachment.flags            = 0;
    attachment.format           = m_dstImageView->info().format;
    attachment.samples          = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp           = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.stencilStoreOp   = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.initialLayout    = m_dstImageView->imageInfo().layout;
    attachment.finalLayout      = m_dstImageView->imageInfo().layout;

    if (discard) {
      attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VkImageLayout layout = isColorImage
      ? m_dstImageView->pickLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
      : m_dstImageView->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    
    VkAttachmentReference dstRef;
    dstRef.attachment    = 0;
    dstRef.layout        = layout;
    
    VkSubpassDescription subpass;
    subpass.flags                   = 0;
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount    = 0;
    subpass.pInputAttachments       = nullptr;
    subpass.colorAttachmentCount    = isColorImage ? 1 : 0;
    subpass.pColorAttachments       = isColorImage ? &dstRef : nullptr;
    subpass.pResolveAttachments     = nullptr;
    subpass.pDepthStencilAttachment = isColorImage ? nullptr : &dstRef;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments    = nullptr;

    VkPipelineStageFlags cpyStages = 0;
    VkAccessFlags        cpyAccess = 0;

    if (isColorImage) {
      cpyStages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      cpyAccess |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

      if (!discard)
        cpyAccess |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    } else {
      cpyStages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                |  VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      cpyAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      if (!discard)
        cpyAccess |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    }

    // Resolve targets are required to be render targets
    VkPipelineStageFlags extStages = m_dstImageView->imageInfo().stages | m_srcImageView->imageInfo().stages;
    VkAccessFlags        extAccess = m_dstImageView->imageInfo().access;

    std::array<VkSubpassDependency, 2> dependencies = {{
      { VK_SUBPASS_EXTERNAL, 0, cpyStages, cpyStages, 0,         cpyAccess, 0 },
      { 0, VK_SUBPASS_EXTERNAL, cpyStages, extStages, cpyAccess, extAccess, 0 },
    }};

    VkRenderPassCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.attachmentCount        = 1;
    info.pAttachments           = &attachment;
    info.subpassCount           = 1;
    info.pSubpasses             = &subpass;
    info.dependencyCount        = dependencies.size();
    info.pDependencies          = dependencies.data();

    VkRenderPass result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateRenderPass(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaResolveRenderPass: Failed to create render pass");
    return result;
  }


  VkRenderPass DxvkMetaResolveRenderPass::createAttachmentRenderPass(
          VkResolveModeFlagBitsKHR modeD,
          VkResolveModeFlagBitsKHR modeS) const {
    std::array<VkAttachmentDescription2KHR, 2> attachments;
    attachments[0].sType          = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR;
    attachments[0].pNext          = nullptr;
    attachments[0].flags          = 0;
    attachments[0].format         = m_srcImageView->info().format;
    attachments[0].samples        = m_srcImageView->imageInfo().sampleCount;
    attachments[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout  = m_srcImageView->imageInfo().layout;
    attachments[0].finalLayout    = m_srcImageView->imageInfo().layout;

    attachments[1].sType          = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR;
    attachments[1].pNext          = nullptr;
    attachments[1].flags          = 0;
    attachments[1].format         = m_dstImageView->info().format;
    attachments[1].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].initialLayout  = m_dstImageView->imageInfo().layout;
    attachments[1].finalLayout    = m_dstImageView->imageInfo().layout;

    if (modeD != VK_RESOLVE_MODE_NONE_KHR && modeS != VK_RESOLVE_MODE_NONE_KHR) {
      attachments[1].loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachments[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachments[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    VkAttachmentReference2KHR srcRef;
    srcRef.sType                = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
    srcRef.pNext                = nullptr;
    srcRef.attachment           = 0;
    srcRef.layout               = m_srcImageView->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    srcRef.aspectMask           = m_srcImageView->formatInfo()->aspectMask;

    VkAttachmentReference2KHR dstRef;
    dstRef.sType                = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
    dstRef.pNext                = nullptr;
    dstRef.attachment           = 1;
    dstRef.layout               = m_dstImageView->pickLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    dstRef.aspectMask           = m_dstImageView->formatInfo()->aspectMask;

    VkSubpassDescriptionDepthStencilResolveKHR subpassResolve;
    subpassResolve.sType        = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE_KHR;
    subpassResolve.pNext        = nullptr;
    subpassResolve.depthResolveMode   = modeD;
    subpassResolve.stencilResolveMode = modeS;
    subpassResolve.pDepthStencilResolveAttachment = &dstRef;
    
    VkSubpassDescription2KHR subpass;
    subpass.sType               = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR;
    subpass.pNext               = &subpassResolve;
    subpass.flags               = 0;
    subpass.pipelineBindPoint   = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.viewMask            = 0;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments   = nullptr;
    subpass.colorAttachmentCount = 0;
    subpass.pColorAttachments   = nullptr;
    subpass.pResolveAttachments = nullptr;
    subpass.pDepthStencilAttachment = &srcRef;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;

    VkPipelineStageFlags cpyStages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    VkPipelineStageFlags extStages = m_dstImageView->imageInfo().stages | m_srcImageView->imageInfo().stages;
    VkAccessFlags cpyAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    VkAccessFlags extAccess = m_dstImageView->imageInfo().access;

    std::array<VkSubpassDependency2KHR, 2> dependencies = {{
      { VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2_KHR, nullptr, VK_SUBPASS_EXTERNAL, 0, cpyStages, cpyStages, 0,         cpyAccess, 0 },
      { VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2_KHR, nullptr, 0, VK_SUBPASS_EXTERNAL, cpyStages, extStages, cpyAccess, extAccess, 0 },
    }};

    VkRenderPassCreateInfo2KHR info;
    info.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.attachmentCount        = attachments.size();
    info.pAttachments           = attachments.data();
    info.subpassCount           = 1;
    info.pSubpasses             = &subpass;
    info.dependencyCount        = dependencies.size();
    info.pDependencies          = dependencies.data();
    info.correlatedViewMaskCount = 0;
    info.pCorrelatedViewMasks   = nullptr;

    VkRenderPass result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateRenderPass2KHR(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaResolveRenderPass: Failed to create render pass");
    return result;
  }


  VkFramebuffer DxvkMetaResolveRenderPass::createShaderFramebuffer() const {
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
      throw DxvkError("DxvkMetaResolveRenderPass: Failed to create target framebuffer");
    return result;
  }


  VkFramebuffer DxvkMetaResolveRenderPass::createAttachmentFramebuffer() const {
    VkImageSubresourceRange dstSubresources = m_dstImageView->subresources();
    VkExtent3D              dstExtent       = m_dstImageView->mipLevelExtent(0);

    std::array<VkImageView, 2> handles = {{
      m_srcImageView->handle(),
      m_dstImageView->handle(),
    }};

    VkFramebufferCreateInfo fboInfo;
    fboInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fboInfo.pNext           = nullptr;
    fboInfo.flags           = 0;
    fboInfo.renderPass      = m_renderPass;
    fboInfo.attachmentCount = handles.size();
    fboInfo.pAttachments    = handles.data();
    fboInfo.width           = dstExtent.width;
    fboInfo.height          = dstExtent.height;
    fboInfo.layers          = dstSubresources.layerCount;
    
    VkFramebuffer result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateFramebuffer(m_vkd->device(), &fboInfo, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaResolveRenderPass: Failed to create target framebuffer");
    return result;
  }



  DxvkMetaResolveObjects::DxvkMetaResolveObjects(const DxvkDevice* device)
  : m_vkd         (device->vkd()),
    m_sampler     (createSampler()),
    m_shaderFragF (device->extensions().amdShaderFragmentMask
      ? createShaderModule(dxvk_resolve_frag_f_amd)
      : createShaderModule(dxvk_resolve_frag_f)),
    m_shaderFragU (createShaderModule(dxvk_resolve_frag_u)),
    m_shaderFragI (createShaderModule(dxvk_resolve_frag_i)),
    m_shaderFragD (createShaderModule(dxvk_resolve_frag_d)) {
    if (device->extensions().extShaderStencilExport)
      m_shaderFragDS = createShaderModule(dxvk_resolve_frag_ds);

    if (device->extensions().extShaderViewportIndexLayer) {
      m_shaderVert = createShaderModule(dxvk_fullscreen_layer_vert);
    } else {
      m_shaderVert = createShaderModule(dxvk_fullscreen_vert);
      m_shaderGeom = createShaderModule(dxvk_fullscreen_geom);
    }
  }


  DxvkMetaResolveObjects::~DxvkMetaResolveObjects() {
    for (const auto& pair : m_pipelines) {
      m_vkd->vkDestroyPipeline(m_vkd->device(), pair.second.pipeHandle, nullptr);
      m_vkd->vkDestroyPipelineLayout(m_vkd->device(), pair.second.pipeLayout, nullptr);
      m_vkd->vkDestroyDescriptorSetLayout(m_vkd->device(), pair.second.dsetLayout, nullptr);
      m_vkd->vkDestroyRenderPass(m_vkd->device(), pair.second.renderPass, nullptr);
    }

    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderFragDS, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderFragD, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderFragF, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderFragI, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderFragU, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderGeom, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderVert, nullptr);

    m_vkd->vkDestroySampler(m_vkd->device(), m_sampler, nullptr);
  }


  DxvkMetaResolvePipeline DxvkMetaResolveObjects::getPipeline(
          VkFormat                  format,
          VkSampleCountFlagBits     samples,
          VkResolveModeFlagBitsKHR  depthResolveMode,
          VkResolveModeFlagBitsKHR  stencilResolveMode) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkMetaResolvePipelineKey key;
    key.format  = format;
    key.samples = samples;
    key.modeD   = depthResolveMode;
    key.modeS   = stencilResolveMode;
    
    auto entry = m_pipelines.find(key);
    if (entry != m_pipelines.end())
      return entry->second;

    DxvkMetaResolvePipeline pipeline = createPipeline(key);
    m_pipelines.insert({ key, pipeline });
    return pipeline;
  }
  
  
  VkSampler DxvkMetaResolveObjects::createSampler() const {
    VkSamplerCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.magFilter              = VK_FILTER_NEAREST;
    info.minFilter              = VK_FILTER_NEAREST;
    info.mipmapMode             = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressModeU           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.mipLodBias             = 0.0f;
    info.anisotropyEnable       = VK_FALSE;
    info.maxAnisotropy          = 1.0f;
    info.compareEnable          = VK_FALSE;
    info.compareOp              = VK_COMPARE_OP_ALWAYS;
    info.minLod                 = 0.0f;
    info.maxLod                 = 0.0f;
    info.borderColor            = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;
    
    VkSampler result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateSampler(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaResolveObjects: Failed to create sampler");
    return result;
  }

  
  VkShaderModule DxvkMetaResolveObjects::createShaderModule(
    const SpirvCodeBuffer&       code) const {
    VkShaderModuleCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.codeSize               = code.size();
    info.pCode                  = code.data();
    
    VkShaderModule result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create shader module");
    return result;
  }

  
  DxvkMetaResolvePipeline DxvkMetaResolveObjects::createPipeline(
    const DxvkMetaResolvePipelineKey& key) {
    DxvkMetaResolvePipeline pipeline;
    pipeline.renderPass = this->createRenderPass(key);
    pipeline.dsetLayout = this->createDescriptorSetLayout(key);
    pipeline.pipeLayout = this->createPipelineLayout(pipeline.dsetLayout);
    pipeline.pipeHandle = this->createPipelineObject(key, pipeline.pipeLayout, pipeline.renderPass);
    return pipeline;
  }


  VkRenderPass DxvkMetaResolveObjects::createRenderPass(
    const DxvkMetaResolvePipelineKey& key) {
    auto formatInfo = imageFormatInfo(key.format);
    bool isColorImage = (formatInfo->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT);

    VkAttachmentDescription attachment;
    attachment.flags            = 0;
    attachment.format           = key.format;
    attachment.samples          = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp           = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.stencilStoreOp   = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.initialLayout    = VK_IMAGE_LAYOUT_GENERAL;
    attachment.finalLayout      = VK_IMAGE_LAYOUT_GENERAL;

    VkImageLayout layout = isColorImage
      ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
      : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference attachmentRef;
    attachmentRef.attachment    = 0;
    attachmentRef.layout        = layout;
    
    VkSubpassDescription subpass;
    subpass.flags                   = 0;
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount    = 0;
    subpass.pInputAttachments       = nullptr;
    subpass.colorAttachmentCount    = isColorImage ? 1 : 0;
    subpass.pColorAttachments       = isColorImage ? &attachmentRef : nullptr;
    subpass.pResolveAttachments     = nullptr;
    subpass.pDepthStencilAttachment = isColorImage ? nullptr : &attachmentRef;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments    = nullptr;

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
      throw DxvkError("DxvkMetaResolveObjects: Failed to create render pass");
    return result;
  }

  
  VkDescriptorSetLayout DxvkMetaResolveObjects::createDescriptorSetLayout(
    const DxvkMetaResolvePipelineKey& key) {
    auto formatInfo = imageFormatInfo(key.format);

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &m_sampler },
      { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &m_sampler },
    }};
    
    VkDescriptorSetLayoutCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.bindingCount           = 1;
    info.pBindings              = bindings.data();

    if ((formatInfo->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) && key.modeS != VK_RESOLVE_MODE_NONE_KHR)
      info.bindingCount = 2;
    
    VkDescriptorSetLayout result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateDescriptorSetLayout(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaResolveObjects: Failed to create descriptor set layout");
    return result;
  }
  

  VkPipelineLayout DxvkMetaResolveObjects::createPipelineLayout(
          VkDescriptorSetLayout  descriptorSetLayout) {
    VkPushConstantRange push;
    push.stageFlags             = VK_SHADER_STAGE_FRAGMENT_BIT;
    push.offset                 = 0;
    push.size                   = sizeof(VkOffset2D);

    VkPipelineLayoutCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.setLayoutCount         = 1;
    info.pSetLayouts            = &descriptorSetLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges    = &push;
    
    VkPipelineLayout result = VK_NULL_HANDLE;
    if (m_vkd->vkCreatePipelineLayout(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create pipeline layout");
    return result;
  }
  

  VkPipeline DxvkMetaResolveObjects::createPipelineObject(
    const DxvkMetaResolvePipelineKey& key,
          VkPipelineLayout       pipelineLayout,
          VkRenderPass           renderPass) {
    auto formatInfo = imageFormatInfo(key.format);
    bool isColorImage = formatInfo->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT;

    std::array<VkPipelineShaderStageCreateInfo, 3> stages;
    uint32_t stageCount = 0;

    std::array<VkSpecializationMapEntry, 3> specEntries = {{
      { 0, offsetof(DxvkMetaResolvePipelineKey, samples), sizeof(VkSampleCountFlagBits) },
      { 1, offsetof(DxvkMetaResolvePipelineKey, modeD),   sizeof(VkResolveModeFlagBitsKHR) },
      { 2, offsetof(DxvkMetaResolvePipelineKey, modeS),   sizeof(VkResolveModeFlagBitsKHR) },
    }};

    VkSpecializationInfo specInfo;
    specInfo.mapEntryCount      = specEntries.size();
    specInfo.pMapEntries        = specEntries.data();
    specInfo.dataSize           = sizeof(key);
    specInfo.pData              = &key;
    
    VkPipelineShaderStageCreateInfo& vsStage = stages[stageCount++];
    vsStage.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vsStage.pNext               = nullptr;
    vsStage.flags               = 0;
    vsStage.stage               = VK_SHADER_STAGE_VERTEX_BIT;
    vsStage.module              = m_shaderVert;
    vsStage.pName               = "main";
    vsStage.pSpecializationInfo = nullptr;
    
    if (m_shaderGeom) {
      VkPipelineShaderStageCreateInfo& gsStage = stages[stageCount++];
      gsStage.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      gsStage.pNext               = nullptr;
      gsStage.flags               = 0;
      gsStage.stage               = VK_SHADER_STAGE_GEOMETRY_BIT;
      gsStage.module              = m_shaderGeom;
      gsStage.pName               = "main";
      gsStage.pSpecializationInfo = nullptr;
    }
    
    VkPipelineShaderStageCreateInfo& psStage = stages[stageCount++];
    psStage.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    psStage.pNext               = nullptr;
    psStage.flags               = 0;
    psStage.stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
    psStage.module              = VK_NULL_HANDLE;
    psStage.pName               = "main";
    psStage.pSpecializationInfo = &specInfo;

    if ((formatInfo->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) && key.modeS != VK_RESOLVE_MODE_NONE_KHR) {
      if (m_shaderFragDS) {
        psStage.module = m_shaderFragDS;
      } else {
        psStage.module = m_shaderFragD;
        Logger::err("DXVK: Stencil export not supported by device, skipping stencil resolve");
      }
    } else if (formatInfo->aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
      psStage.module = m_shaderFragD;
    else if (formatInfo->flags.test(DxvkFormatFlag::SampledUInt))
      psStage.module = m_shaderFragU;
    else if (formatInfo->flags.test(DxvkFormatFlag::SampledSInt))
      psStage.module = m_shaderFragI;
    else
      psStage.module = m_shaderFragF;
    
    std::array<VkDynamicState, 2> dynStates = {{
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
    }};
    
    VkPipelineDynamicStateCreateInfo dynState;
    dynState.sType              = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynState.pNext              = nullptr;
    dynState.flags              = 0;
    dynState.dynamicStateCount  = dynStates.size();
    dynState.pDynamicStates     = dynStates.data();
    
    VkPipelineVertexInputStateCreateInfo viState;
    viState.sType               = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    viState.pNext               = nullptr;
    viState.flags               = 0;
    viState.vertexBindingDescriptionCount   = 0;
    viState.pVertexBindingDescriptions      = nullptr;
    viState.vertexAttributeDescriptionCount = 0;
    viState.pVertexAttributeDescriptions    = nullptr;
    
    VkPipelineInputAssemblyStateCreateInfo iaState;
    iaState.sType               = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaState.pNext               = nullptr;
    iaState.flags               = 0;
    iaState.topology            = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    iaState.primitiveRestartEnable = VK_FALSE;
    
    VkPipelineViewportStateCreateInfo vpState;
    vpState.sType               = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpState.pNext               = nullptr;
    vpState.flags               = 0;
    vpState.viewportCount       = 1;
    vpState.pViewports          = nullptr;
    vpState.scissorCount        = 1;
    vpState.pScissors           = nullptr;
    
    VkPipelineRasterizationStateCreateInfo rsState;
    rsState.sType               = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rsState.pNext               = nullptr;
    rsState.flags               = 0;
    rsState.depthClampEnable    = VK_TRUE;
    rsState.rasterizerDiscardEnable = VK_FALSE;
    rsState.polygonMode         = VK_POLYGON_MODE_FILL;
    rsState.cullMode            = VK_CULL_MODE_NONE;
    rsState.frontFace           = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.depthBiasEnable     = VK_FALSE;
    rsState.depthBiasConstantFactor = 0.0f;
    rsState.depthBiasClamp          = 0.0f;
    rsState.depthBiasSlopeFactor    = 0.0f;
    rsState.lineWidth           = 1.0f;
    
    uint32_t msMask = 0xFFFFFFFF;
    VkPipelineMultisampleStateCreateInfo msState;
    msState.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msState.pNext                 = nullptr;
    msState.flags                 = 0;
    msState.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    msState.sampleShadingEnable   = VK_FALSE;
    msState.minSampleShading      = 1.0f;
    msState.pSampleMask           = &msMask;
    msState.alphaToCoverageEnable = VK_FALSE;
    msState.alphaToOneEnable      = VK_FALSE;

    VkPipelineColorBlendAttachmentState cbAttachment;
    cbAttachment.blendEnable         = VK_FALSE;
    cbAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    cbAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    cbAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    cbAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cbAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cbAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    cbAttachment.colorWriteMask      =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    VkPipelineColorBlendStateCreateInfo cbState;
    cbState.sType               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbState.pNext               = nullptr;
    cbState.flags               = 0;
    cbState.logicOpEnable       = VK_FALSE;
    cbState.logicOp             = VK_LOGIC_OP_NO_OP;
    cbState.attachmentCount     = 1;
    cbState.pAttachments        = &cbAttachment;
    
    for (uint32_t i = 0; i < 4; i++)
      cbState.blendConstants[i] = 0.0f;
    
    VkStencilOpState stencilOp;
    stencilOp.failOp            = VK_STENCIL_OP_REPLACE;
    stencilOp.passOp            = VK_STENCIL_OP_REPLACE;
    stencilOp.depthFailOp       = VK_STENCIL_OP_REPLACE;
    stencilOp.compareOp         = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask       = 0xFFFFFFFF;
    stencilOp.writeMask         = 0xFFFFFFFF;
    stencilOp.reference         = 0;

    VkPipelineDepthStencilStateCreateInfo dsState;
    dsState.sType               = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dsState.pNext               = nullptr;
    dsState.flags               = 0;
    dsState.depthTestEnable     = key.modeD != VK_RESOLVE_MODE_NONE_KHR;
    dsState.depthWriteEnable    = key.modeD != VK_RESOLVE_MODE_NONE_KHR;
    dsState.depthCompareOp      = VK_COMPARE_OP_ALWAYS;
    dsState.depthBoundsTestEnable = VK_FALSE;
    dsState.stencilTestEnable   = key.modeS != VK_RESOLVE_MODE_NONE_KHR;
    dsState.front               = stencilOp;
    dsState.back                = stencilOp;
    dsState.minDepthBounds      = 0.0f;
    dsState.maxDepthBounds      = 1.0f;

    VkGraphicsPipelineCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.stageCount             = stageCount;
    info.pStages                = stages.data();
    info.pVertexInputState      = &viState;
    info.pInputAssemblyState    = &iaState;
    info.pTessellationState     = nullptr;
    info.pViewportState         = &vpState;
    info.pRasterizationState    = &rsState;
    info.pMultisampleState      = &msState;
    info.pColorBlendState       = isColorImage ? &cbState : nullptr;
    info.pDepthStencilState     = isColorImage ? nullptr : &dsState;
    info.pDynamicState          = &dynState;
    info.layout                 = pipelineLayout;
    info.renderPass             = renderPass;
    info.subpass                = 0;
    info.basePipelineHandle     = VK_NULL_HANDLE;
    info.basePipelineIndex      = -1;
    
    VkPipeline result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateGraphicsPipelines(m_vkd->device(), VK_NULL_HANDLE, 1, &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create graphics pipeline");
    return result;
  }

}
