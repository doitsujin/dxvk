#include "dxvk_device.h"
#include "dxvk_meta_blit.h"

#include <dxvk_fullscreen_geom.h>
#include <dxvk_fullscreen_vert.h>
#include <dxvk_fullscreen_layer_vert.h>

#include <dxvk_blit_frag_1d.h>
#include <dxvk_blit_frag_2d.h>
#include <dxvk_blit_frag_3d.h>

namespace dxvk {
  
  DxvkMetaBlitRenderPass::DxvkMetaBlitRenderPass(
    const Rc<DxvkDevice>&       device,
    const Rc<DxvkImage>&        dstImage,
    const Rc<DxvkImage>&        srcImage,
    const VkImageBlit&          region,
    const VkComponentMapping&   mapping)
  : m_vkd         (device->vkd()),
    m_dstImage    (dstImage),
    m_srcImage    (srcImage),
    m_region      (region),
    m_dstView     (createDstView()),
    m_srcView     (createSrcView(mapping)),
    m_renderPass  (createRenderPass()),
    m_framebuffer (createFramebuffer()) {
    
  }


  DxvkMetaBlitRenderPass::~DxvkMetaBlitRenderPass() {
    m_vkd->vkDestroyImageView(m_vkd->device(), m_dstView, nullptr);
    m_vkd->vkDestroyImageView(m_vkd->device(), m_srcView, nullptr);
    m_vkd->vkDestroyRenderPass(m_vkd->device(), m_renderPass, nullptr);
    m_vkd->vkDestroyFramebuffer(m_vkd->device(), m_framebuffer, nullptr);
  }


  VkImageViewType DxvkMetaBlitRenderPass::viewType() const {
    std::array<VkImageViewType, 3> viewTypes = {{
      VK_IMAGE_VIEW_TYPE_1D_ARRAY,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      VK_IMAGE_VIEW_TYPE_3D,
    }};

    return viewTypes.at(uint32_t(m_srcImage->info().type));
  }


  uint32_t DxvkMetaBlitRenderPass::framebufferLayerIndex() const {
    uint32_t result = m_region.dstSubresource.baseArrayLayer;

    if (m_dstImage->info().type == VK_IMAGE_TYPE_3D)
      result = std::min(m_region.dstOffsets[0].z, m_region.dstOffsets[1].z);

    return result;
  }


  uint32_t DxvkMetaBlitRenderPass::framebufferLayerCount() const {
    uint32_t result = m_region.dstSubresource.layerCount;

    if (m_dstImage->info().type == VK_IMAGE_TYPE_3D) {
      uint32_t minZ = std::min(m_region.dstOffsets[0].z, m_region.dstOffsets[1].z);
      uint32_t maxZ = std::max(m_region.dstOffsets[0].z, m_region.dstOffsets[1].z);
      result = maxZ - minZ;
    }

    return result;
  }


  DxvkMetaBlitPass DxvkMetaBlitRenderPass::pass() const {
    DxvkMetaBlitPass result;
    result.srcView      = m_srcView;
    result.dstView      = m_dstView;
    result.renderPass   = m_renderPass;
    result.framebuffer  = m_framebuffer;
    return result;
  }


  VkImageView DxvkMetaBlitRenderPass::createDstView() {
    std::array<VkImageViewType, 3> viewTypes = {{
      VK_IMAGE_VIEW_TYPE_1D_ARRAY,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
    }};

    VkImageViewUsageCreateInfo usageInfo;
    usageInfo.sType       = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
    usageInfo.pNext       = nullptr;
    usageInfo.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageViewCreateInfo info;
    info.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext            = &usageInfo;
    info.flags            = 0;
    info.image            = m_dstImage->handle();
    info.viewType         = viewTypes.at(uint32_t(m_dstImage->info().type));
    info.format           = m_dstImage->info().format;
    info.components       = VkComponentMapping();
    info.subresourceRange = vk::makeSubresourceRange(m_region.dstSubresource);

    if (m_dstImage->info().type) {
      info.subresourceRange.baseArrayLayer = framebufferLayerIndex();
      info.subresourceRange.layerCount     = framebufferLayerCount();
    }

    VkImageView result;
    if (m_vkd->vkCreateImageView(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitRenderPass: Failed to create image view");
    return result;
  }


  VkImageView DxvkMetaBlitRenderPass::createSrcView(const VkComponentMapping& mapping) {
    VkImageViewUsageCreateInfo usageInfo;
    usageInfo.sType       = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
    usageInfo.pNext       = nullptr;
    usageInfo.usage       = VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageViewCreateInfo info;
    info.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext            = &usageInfo;
    info.flags            = 0;
    info.image            = m_srcImage->handle();
    info.viewType         = this->viewType();
    info.format           = m_srcImage->info().format;
    info.components       = mapping;
    info.subresourceRange = vk::makeSubresourceRange(m_region.srcSubresource);

    VkImageView result;
    if (m_vkd->vkCreateImageView(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitRenderPass: Failed to create image view");
    return result;
  }


  VkRenderPass DxvkMetaBlitRenderPass::createRenderPass() {
    VkAttachmentDescription attachment;
    attachment.flags            = 0;
    attachment.format           = m_dstImage->info().format;
    attachment.samples          = m_dstImage->info().sampleCount;
    attachment.loadOp           = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout    = m_dstImage->info().layout;
    attachment.finalLayout      = m_dstImage->info().layout;
    
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
    info.dependencyCount        = 0;
    info.pDependencies          = nullptr;
    
    VkRenderPass result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateRenderPass(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitRenderPass: Failed to create render pass");
    return result;
  }


  VkFramebuffer DxvkMetaBlitRenderPass::createFramebuffer() {
    VkExtent3D extent = m_dstImage->mipLevelExtent(m_region.dstSubresource.mipLevel);

    VkFramebufferCreateInfo fboInfo;
    fboInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fboInfo.pNext           = nullptr;
    fboInfo.flags           = 0;
    fboInfo.renderPass      = m_renderPass;
    fboInfo.attachmentCount = 1;
    fboInfo.pAttachments    = &m_dstView;
    fboInfo.width           = extent.width;
    fboInfo.height          = extent.height;
    fboInfo.layers          = framebufferLayerCount();
    
    VkFramebuffer result;
    if (m_vkd->vkCreateFramebuffer(m_vkd->device(), &fboInfo, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitRenderPass: Failed to create target framebuffer");
    return result;
  }
  


  
  DxvkMetaBlitObjects::DxvkMetaBlitObjects(const DxvkDevice* device)
  : m_vkd         (device->vkd()),
    m_samplerCopy (createSampler(VK_FILTER_NEAREST)),
    m_samplerBlit (createSampler(VK_FILTER_LINEAR)),
    m_shaderFrag1D(createShaderModule(dxvk_blit_frag_1d)),
    m_shaderFrag2D(createShaderModule(dxvk_blit_frag_2d)),
    m_shaderFrag3D(createShaderModule(dxvk_blit_frag_3d)) {
    if (device->extensions().extShaderViewportIndexLayer) {
      m_shaderVert = createShaderModule(dxvk_fullscreen_layer_vert);
    } else {
      m_shaderVert = createShaderModule(dxvk_fullscreen_vert);
      m_shaderGeom = createShaderModule(dxvk_fullscreen_geom);
    }
  }


  DxvkMetaBlitObjects::~DxvkMetaBlitObjects() {
    for (const auto& pair : m_renderPasses)
      m_vkd->vkDestroyRenderPass(m_vkd->device(), pair.second, nullptr);
    
    for (const auto& pair : m_pipelines) {
      m_vkd->vkDestroyPipeline(m_vkd->device(), pair.second.pipeHandle, nullptr);
      m_vkd->vkDestroyPipelineLayout(m_vkd->device(), pair.second.pipeLayout, nullptr);
      m_vkd->vkDestroyDescriptorSetLayout (m_vkd->device(), pair.second.dsetLayout, nullptr);
    }
    
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderFrag3D, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderFrag2D, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderFrag1D, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderGeom, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderVert, nullptr);
    
    m_vkd->vkDestroySampler(m_vkd->device(), m_samplerBlit, nullptr);
    m_vkd->vkDestroySampler(m_vkd->device(), m_samplerCopy, nullptr);
  }
  
  
  DxvkMetaBlitPipeline DxvkMetaBlitObjects::getPipeline(
          VkImageViewType       viewType,
          VkFormat              viewFormat,
          VkSampleCountFlagBits samples) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    
    DxvkMetaBlitPipelineKey key;
    key.viewType   = viewType;
    key.viewFormat = viewFormat;
    key.samples    = samples;
    
    auto entry = m_pipelines.find(key);
    if (entry != m_pipelines.end())
      return entry->second;
    
    DxvkMetaBlitPipeline pipeline = this->createPipeline(key);
    m_pipelines.insert({ key, pipeline });
    return pipeline;
  }
  
  
  VkSampler DxvkMetaBlitObjects::getSampler(VkFilter filter) {
    return filter == VK_FILTER_NEAREST
      ? m_samplerCopy
      : m_samplerBlit;
  }
  
  
  VkRenderPass DxvkMetaBlitObjects::getRenderPass(
          VkFormat                    viewFormat,
          VkSampleCountFlagBits       samples) {
    DxvkMetaBlitRenderPassKey key;
    key.viewFormat = viewFormat;
    key.samples    = samples;

    auto entry = m_renderPasses.find(key);
    if (entry != m_renderPasses.end())
      return entry->second;
    
    VkRenderPass renderPass = this->createRenderPass(viewFormat, samples);
    m_renderPasses.insert({ key, renderPass });
    return renderPass;
  }
  
  
  VkSampler DxvkMetaBlitObjects::createSampler(VkFilter filter) const {
    VkSamplerCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.magFilter              = filter;
    info.minFilter              = filter;
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
      throw DxvkError("DxvkMetaBlitObjects: Failed to create sampler");
    return result;
  }
  
  
  VkShaderModule DxvkMetaBlitObjects::createShaderModule(const SpirvCodeBuffer& code) const {
    VkShaderModuleCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.codeSize               = code.size();
    info.pCode                  = code.data();
    
    VkShaderModule result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create shader module");
    return result;
  }
  
  
  DxvkMetaBlitPipeline DxvkMetaBlitObjects::createPipeline(
    const DxvkMetaBlitPipelineKey& key) {
    DxvkMetaBlitPipeline pipe;
    pipe.dsetLayout = this->createDescriptorSetLayout(key.viewType);
    pipe.pipeLayout = this->createPipelineLayout(pipe.dsetLayout);
    pipe.pipeHandle = this->createPipeline(key.viewType, pipe.pipeLayout,
      this->getRenderPass(key.viewFormat, key.samples), key.samples);
    return pipe;
  }
  
  
  VkRenderPass DxvkMetaBlitObjects::createRenderPass(
          VkFormat                    format,
          VkSampleCountFlagBits       samples) const {
    VkAttachmentDescription attachment;
    attachment.flags            = 0;
    attachment.format           = format;
    attachment.samples          = samples;
    attachment.loadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
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
    info.dependencyCount        = 0;
    info.pDependencies          = nullptr;
    
    VkRenderPass result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateRenderPass(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create render pass");
    return result;
  }
  
  
  VkDescriptorSetLayout DxvkMetaBlitObjects::createDescriptorSetLayout(
          VkImageViewType             viewType) const {
    VkDescriptorSetLayoutBinding binding;
    binding.binding             = 0;
    binding.descriptorType      = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount     = 1;
    binding.stageFlags          = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers  = nullptr;
    
    VkDescriptorSetLayoutCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.bindingCount           = 1;
    info.pBindings              = &binding;
    
    VkDescriptorSetLayout result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateDescriptorSetLayout(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create descriptor set layout");
    return result;
  }
  
  
  VkPipelineLayout DxvkMetaBlitObjects::createPipelineLayout(
          VkDescriptorSetLayout       descriptorSetLayout) const {
    VkPushConstantRange pushRange;
    pushRange.stageFlags        = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset            = 0;
    pushRange.size              = sizeof(DxvkMetaBlitPushConstants);
    
    VkPipelineLayoutCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.setLayoutCount         = 1;
    info.pSetLayouts            = &descriptorSetLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges    = &pushRange;
    
    VkPipelineLayout result = VK_NULL_HANDLE;
    if (m_vkd->vkCreatePipelineLayout(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create pipeline layout");
    return result;
  }
  
  
  VkPipeline DxvkMetaBlitObjects::createPipeline(
          VkImageViewType             imageViewType,
          VkPipelineLayout            pipelineLayout,
          VkRenderPass                renderPass,
          VkSampleCountFlagBits       samples) const {
    std::array<VkPipelineShaderStageCreateInfo, 3> stages;
    uint32_t stageCount = 0;
    
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
    psStage.pSpecializationInfo = nullptr;
    
    switch (imageViewType) {
      case VK_IMAGE_VIEW_TYPE_1D_ARRAY: psStage.module = m_shaderFrag1D; break;
      case VK_IMAGE_VIEW_TYPE_2D_ARRAY: psStage.module = m_shaderFrag2D; break;
      case VK_IMAGE_VIEW_TYPE_3D:       psStage.module = m_shaderFrag3D; break;
      default: throw DxvkError("DxvkMetaBlitObjects: Invalid view type");
    }
    
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
    msState.sType               = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msState.pNext               = nullptr;
    msState.flags               = 0;
    msState.rasterizationSamples = samples;
    msState.sampleShadingEnable = VK_FALSE;
    msState.minSampleShading    = 1.0f;
    msState.pSampleMask         = &msMask;
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
    info.pColorBlendState       = &cbState;
    info.pDepthStencilState     = nullptr;
    info.pDynamicState          = &dynState;
    info.layout                 = pipelineLayout;
    info.renderPass             = renderPass;
    info.subpass                = 0;
    info.basePipelineHandle     = VK_NULL_HANDLE;
    info.basePipelineIndex      = -1;
    
    VkPipeline result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateGraphicsPipelines(m_vkd->device(), VK_NULL_HANDLE, 1, &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create graphics pipeline");
    return result;
  }
  
}
