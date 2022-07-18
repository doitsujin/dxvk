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
    m_srcView     (createSrcView(mapping)) {
    
  }


  DxvkMetaBlitRenderPass::~DxvkMetaBlitRenderPass() {
    m_vkd->vkDestroyImageView(m_vkd->device(), m_dstView, nullptr);
    m_vkd->vkDestroyImageView(m_vkd->device(), m_srcView, nullptr);
  }


  VkImageViewType DxvkMetaBlitRenderPass::viewType() const {
    static const std::array<VkImageViewType, 3> viewTypes = {{
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


  VkImageView DxvkMetaBlitRenderPass::createDstView() {
    std::array<VkImageViewType, 3> viewTypes = {{
      VK_IMAGE_VIEW_TYPE_1D_ARRAY,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY,
    }};

    VkImageViewUsageCreateInfo usageInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
    usageInfo.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageViewCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, &usageInfo };
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
    VkImageViewUsageCreateInfo usageInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
    usageInfo.usage       = VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageViewCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, &usageInfo };
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




  DxvkMetaBlitObjects::DxvkMetaBlitObjects(const DxvkDevice* device)
  : m_vkd         (device->vkd()),
    m_samplerCopy (createSampler(VK_FILTER_NEAREST)),
    m_samplerBlit (createSampler(VK_FILTER_LINEAR)),
    m_shaderFrag1D(createShaderModule(dxvk_blit_frag_1d)),
    m_shaderFrag2D(createShaderModule(dxvk_blit_frag_2d)),
    m_shaderFrag3D(createShaderModule(dxvk_blit_frag_3d)) {
    if (device->features().vk12.shaderOutputLayer) {
      m_shaderVert = createShaderModule(dxvk_fullscreen_layer_vert);
    } else {
      m_shaderVert = createShaderModule(dxvk_fullscreen_vert);
      m_shaderGeom = createShaderModule(dxvk_fullscreen_geom);
    }
  }


  DxvkMetaBlitObjects::~DxvkMetaBlitObjects() {
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
  
  
  VkSampler DxvkMetaBlitObjects::createSampler(VkFilter filter) const {
    VkSamplerCreateInfo info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    info.magFilter              = filter;
    info.minFilter              = filter;
    info.mipmapMode             = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressModeU           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.borderColor            = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    
    VkSampler result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateSampler(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create sampler");
    return result;
  }
  
  
  VkShaderModule DxvkMetaBlitObjects::createShaderModule(const SpirvCodeBuffer& code) const {
    VkShaderModuleCreateInfo info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
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
    pipe.pipeHandle = this->createPipeline(pipe.pipeLayout,
      key.viewType, key.viewFormat, key.samples);
    return pipe;
  }
  
  
  VkDescriptorSetLayout DxvkMetaBlitObjects::createDescriptorSetLayout(
          VkImageViewType             viewType) const {
    VkDescriptorSetLayoutBinding binding = { 0,
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT };
    
    VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.bindingCount           = 1;
    info.pBindings              = &binding;
    
    VkDescriptorSetLayout result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateDescriptorSetLayout(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create descriptor set layout");
    return result;
  }
  
  
  VkPipelineLayout DxvkMetaBlitObjects::createPipelineLayout(
          VkDescriptorSetLayout       descriptorSetLayout) const {
    VkPushConstantRange pushRange = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DxvkMetaBlitPushConstants) };
    
    VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
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
          VkPipelineLayout            pipelineLayout,
          VkImageViewType             imageViewType,
          VkFormat                    format,
          VkSampleCountFlagBits       samples) const {
    std::array<VkPipelineShaderStageCreateInfo, 3> stages;
    uint32_t stageCount = 0;
    
    stages[stageCount++] = VkPipelineShaderStageCreateInfo {
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
      VK_SHADER_STAGE_VERTEX_BIT, m_shaderVert, "main" };
    
    if (m_shaderGeom) {
      stages[stageCount++] = VkPipelineShaderStageCreateInfo {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
        VK_SHADER_STAGE_GEOMETRY_BIT, m_shaderGeom, "main" };
    }

    VkShaderModule psModule = VK_NULL_HANDLE;

    switch (imageViewType) {
      case VK_IMAGE_VIEW_TYPE_1D_ARRAY: psModule = m_shaderFrag1D; break;
      case VK_IMAGE_VIEW_TYPE_2D_ARRAY: psModule = m_shaderFrag2D; break;
      case VK_IMAGE_VIEW_TYPE_3D:       psModule = m_shaderFrag3D; break;
      default: throw DxvkError("DxvkMetaBlitObjects: Invalid view type");
    }
    
    stages[stageCount++] = VkPipelineShaderStageCreateInfo {
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
      VK_SHADER_STAGE_FRAGMENT_BIT, psModule, "main" };
    
    std::array<VkDynamicState, 2> dynStates = {{
      VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
      VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
    }};
    
    VkPipelineDynamicStateCreateInfo dynState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynState.dynamicStateCount = dynStates.size();
    dynState.pDynamicStates = dynStates.data();
    
    VkPipelineVertexInputStateCreateInfo viState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    
    VkPipelineInputAssemblyStateCreateInfo iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    iaState.primitiveRestartEnable = VK_FALSE;
    
    VkPipelineViewportStateCreateInfo vpState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    
    VkPipelineRasterizationStateCreateInfo rsState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsState.polygonMode = VK_POLYGON_MODE_FILL;
    rsState.cullMode = VK_CULL_MODE_NONE;
    rsState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.lineWidth = 1.0f;
    
    uint32_t msMask = 0xFFFFFFFF;
    VkPipelineMultisampleStateCreateInfo msState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msState.rasterizationSamples = samples;
    msState.pSampleMask = &msMask;
    
    VkPipelineColorBlendAttachmentState cbAttachment = { };
    cbAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    VkPipelineColorBlendStateCreateInfo cbState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbState.attachmentCount     = 1;
    cbState.pAttachments        = &cbAttachment;

    VkPipelineRenderingCreateInfo rtState = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    rtState.colorAttachmentCount = 1;
    rtState.pColorAttachmentFormats = &format;

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &rtState };
    info.stageCount             = stageCount;
    info.pStages                = stages.data();
    info.pVertexInputState      = &viState;
    info.pInputAssemblyState    = &iaState;
    info.pViewportState         = &vpState;
    info.pRasterizationState    = &rsState;
    info.pMultisampleState      = &msState;
    info.pColorBlendState       = &cbState;
    info.pDynamicState          = &dynState;
    info.layout                 = pipelineLayout;
    info.basePipelineIndex      = -1;
    
    VkPipeline result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateGraphicsPipelines(m_vkd->device(), VK_NULL_HANDLE, 1, &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create graphics pipeline");
    return result;
  }
  
}
