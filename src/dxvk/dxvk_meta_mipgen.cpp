#include "dxvk_meta_mipgen.h"

#include <dxvk_mipgen_vert.h>
#include <dxvk_mipgen_geom.h>
#include <dxvk_mipgen_frag_1d.h>
#include <dxvk_mipgen_frag_2d.h>
#include <dxvk_mipgen_frag_3d.h>

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
  
  
  DxvkMetaMipGenPass DxvkMetaMipGenRenderPass::createFramebuffer(uint32_t pass) const {
    DxvkMetaMipGenPass result;
    result.srcView      = VK_NULL_HANDLE;
    result.dstView      = VK_NULL_HANDLE;
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
  
  
  DxvkMetaMipGenObjects::DxvkMetaMipGenObjects(const Rc<vk::DeviceFn>& vkd)
  : m_vkd         (vkd),
    m_sampler     (createSampler()),
    m_shaderVert  (createShaderModule(dxvk_mipgen_vert)),
    m_shaderGeom  (createShaderModule(dxvk_mipgen_geom)),
    m_shaderFrag1D(createShaderModule(dxvk_mipgen_frag_1d)),
    m_shaderFrag2D(createShaderModule(dxvk_mipgen_frag_2d)),
    m_shaderFrag3D(createShaderModule(dxvk_mipgen_frag_3d)) {
    
  }
  
  
  DxvkMetaMipGenObjects::~DxvkMetaMipGenObjects() {
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
    
    m_vkd->vkDestroySampler(m_vkd->device(), m_sampler, nullptr);
  }
  
  
  DxvkMetaMipGenPipeline DxvkMetaMipGenObjects::getPipeline(
          VkImageViewType viewType,
          VkFormat        viewFormat) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    DxvkMetaMipGenPipelineKey key;
    key.viewType   = viewType;
    key.viewFormat = viewFormat;
    
    auto entry = m_pipelines.find(key);
    if (entry != m_pipelines.end())
      return entry->second;
    
    DxvkMetaMipGenPipeline pipeline = this->createPipeline(key);
    m_pipelines.insert({ key, pipeline });
    return pipeline;
  }
  
  
  VkRenderPass DxvkMetaMipGenObjects::getRenderPass(VkFormat viewFormat) {
    auto entry = m_renderPasses.find(viewFormat);
    if (entry != m_renderPasses.end())
      return entry->second;
    
    VkRenderPass renderPass = this->createRenderPass(viewFormat);
    m_renderPasses.insert({ viewFormat, renderPass });
    return renderPass;
  }
  
  
  VkSampler DxvkMetaMipGenObjects::createSampler() const {
    VkSamplerCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.magFilter              = VK_FILTER_LINEAR;
    info.minFilter              = VK_FILTER_LINEAR;
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
      throw DxvkError("DxvkMetaMipGenObjects: Failed to create sampler");
    return result;
  }
  
  
  VkShaderModule DxvkMetaMipGenObjects::createShaderModule(const SpirvCodeBuffer& code) const {
    VkShaderModuleCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.codeSize               = code.size();
    info.pCode                  = code.data();
    
    VkShaderModule result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaMipGenObjects: Failed to create shader module");
    return result;
  }
  
  
  DxvkMetaMipGenPipeline DxvkMetaMipGenObjects::createPipeline(
    const DxvkMetaMipGenPipelineKey& key) {
    DxvkMetaMipGenPipeline pipe;
    pipe.dsetLayout = this->createDescriptorSetLayout(key.viewType);
    pipe.pipeLayout = this->createPipelineLayout(pipe.dsetLayout);
    pipe.pipeHandle = this->createPipeline(key.viewType, pipe.pipeLayout,
      this->getRenderPass(key.viewFormat));
    return pipe;
  }
  
  
  VkRenderPass DxvkMetaMipGenObjects::createRenderPass(
          VkFormat                    format) const {
    VkAttachmentDescription attachment;
    attachment.flags            = 0;
    attachment.format           = format;
    attachment.samples          = VK_SAMPLE_COUNT_1_BIT;
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
      throw DxvkError("DxvkMetaMipGenObjects: Failed to create render pass");
    return result;
  }
  
  
  VkDescriptorSetLayout DxvkMetaMipGenObjects::createDescriptorSetLayout(
          VkImageViewType             viewType) const {
    VkDescriptorSetLayoutBinding binding;
    binding.binding             = 0;
    binding.descriptorType      = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount     = 1;
    binding.stageFlags          = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers  = &m_sampler;
    
    VkDescriptorSetLayoutCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.bindingCount           = 1;
    info.pBindings              = &binding;
    
    VkDescriptorSetLayout result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateDescriptorSetLayout(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaMipGenObjects: Failed to create descriptor set layout");
    return result;
  }
  
  
  VkPipelineLayout DxvkMetaMipGenObjects::createPipelineLayout(
          VkDescriptorSetLayout       descriptorSetLayout) const {
    VkPushConstantRange pushRange;
    pushRange.stageFlags        = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset            = 0;
    pushRange.size              = sizeof(DxvkMetaMipGenPushConstants);
    
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
      throw DxvkError("DxvkMetaMipGenObjects: Failed to create pipeline layout");
    return result;
  }
  
  
  VkPipeline DxvkMetaMipGenObjects::createPipeline(
          VkImageViewType             imageViewType,
          VkPipelineLayout            pipelineLayout,
          VkRenderPass                renderPass) const {
    std::array<VkPipelineShaderStageCreateInfo, 3> stages;
    
    VkPipelineShaderStageCreateInfo& vsStage = stages[0];
    vsStage.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vsStage.pNext               = nullptr;
    vsStage.flags               = 0;
    vsStage.stage               = VK_SHADER_STAGE_VERTEX_BIT;
    vsStage.module              = m_shaderVert;
    vsStage.pName               = "main";
    vsStage.pSpecializationInfo = nullptr;
    
    VkPipelineShaderStageCreateInfo& gsStage = stages[1];
    gsStage.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    gsStage.pNext               = nullptr;
    gsStage.flags               = 0;
    gsStage.stage               = VK_SHADER_STAGE_GEOMETRY_BIT;
    gsStage.module              = m_shaderGeom;
    gsStage.pName               = "main";
    gsStage.pSpecializationInfo = nullptr;
    
    VkPipelineShaderStageCreateInfo& psStage = stages[2];
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
      default: throw DxvkError("DxvkMetaMipGenObjects: Invalid view type");
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
    iaState.topology            = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
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
    msState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
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
    info.stageCount             = stages.size();
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
      throw DxvkError("DxvkMetaMipGenObjects: Failed to create graphics pipeline");
    return result;
  }
  
}
