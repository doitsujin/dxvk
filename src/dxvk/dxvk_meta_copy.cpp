#include "dxvk_device.h"
#include "dxvk_meta_copy.h"

#include <dxvk_fullscreen_geom.h>
#include <dxvk_fullscreen_vert.h>
#include <dxvk_fullscreen_layer_vert.h>

#include <dxvk_copy_buffer_image.h>
#include <dxvk_copy_color_1d.h>
#include <dxvk_copy_color_2d.h>
#include <dxvk_copy_color_ms.h>
#include <dxvk_copy_depth_stencil_1d.h>
#include <dxvk_copy_depth_stencil_2d.h>
#include <dxvk_copy_depth_stencil_ms.h>

namespace dxvk {

  DxvkMetaCopyViews::DxvkMetaCopyViews(
    const Rc<vk::DeviceFn>&         vkd,
    const Rc<DxvkImage>&            dstImage,
    const VkImageSubresourceLayers& dstSubresources,
          VkFormat                  dstFormat,
    const Rc<DxvkImage>&            srcImage,
    const VkImageSubresourceLayers& srcSubresources,
          VkFormat                  srcFormat)
  : m_vkd(vkd) {
    VkImageAspectFlags dstAspects = dstImage->formatInfo()->aspectMask;
    VkImageAspectFlags srcAspects = srcImage->formatInfo()->aspectMask;

    // We don't support 3D here, so we can safely ignore that case
    m_dstViewType = dstImage->info().type == VK_IMAGE_TYPE_1D
      ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    m_srcViewType = srcImage->info().type == VK_IMAGE_TYPE_1D
      ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    VkImageViewUsageCreateInfo usageInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
    usageInfo.usage = (dstAspects & VK_IMAGE_ASPECT_COLOR_BIT)
      ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
      : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    // Create destination view
    VkImageViewCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, &usageInfo };
    info.image = dstImage->handle();
    info.viewType = m_dstViewType;
    info.format = dstFormat;
    info.subresourceRange = vk::makeSubresourceRange(dstSubresources);

    if ((m_vkd->vkCreateImageView(m_vkd->device(), &info, nullptr, &m_dstImageView)))
      throw DxvkError("DxvkMetaCopyViews: Failed to create destination image view");

    // Create source image views
    usageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    info.image = srcImage->handle();
    info.viewType = m_srcViewType;
    info.format = srcFormat;
    info.subresourceRange = vk::makeSubresourceRange(srcSubresources);
    info.subresourceRange.aspectMask = srcAspects & (VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT);

    if ((m_vkd->vkCreateImageView(m_vkd->device(), &info, nullptr, &m_srcImageView)))
      throw DxvkError("DxvkMetaCopyViews: Failed to create source image view");

    if (srcAspects & VK_IMAGE_ASPECT_STENCIL_BIT) {
      info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

      if ((m_vkd->vkCreateImageView(m_vkd->device(), &info, nullptr, &m_srcStencilView)))
        throw DxvkError("DxvkMetaCopyViews: Failed to create source stencil view");
    }
  }
  

  DxvkMetaCopyViews::~DxvkMetaCopyViews() {
    m_vkd->vkDestroyImageView(m_vkd->device(), m_dstImageView, nullptr);
    m_vkd->vkDestroyImageView(m_vkd->device(), m_srcImageView, nullptr);
    m_vkd->vkDestroyImageView(m_vkd->device(), m_srcStencilView, nullptr);
  }

  
  DxvkMetaCopyObjects::DxvkMetaCopyObjects(const DxvkDevice* device)
  : m_vkd         (device->vkd()),
    m_color {
      createShaderModule(dxvk_copy_color_1d),
      createShaderModule(dxvk_copy_color_2d),
      createShaderModule(dxvk_copy_color_ms) } {
    if (device->features().vk12.shaderOutputLayer) {
      m_shaderVert = createShaderModule(dxvk_fullscreen_layer_vert);
    } else {
      m_shaderVert = createShaderModule(dxvk_fullscreen_vert);
      m_shaderGeom = createShaderModule(dxvk_fullscreen_geom);
    }
    
    if (device->features().extShaderStencilExport) {
      m_depthStencil = {
        createShaderModule(dxvk_copy_depth_stencil_1d),
        createShaderModule(dxvk_copy_depth_stencil_2d),
        createShaderModule(dxvk_copy_depth_stencil_ms) };
    }
  }


  DxvkMetaCopyObjects::~DxvkMetaCopyObjects() {
    m_vkd->vkDestroyPipeline(m_vkd->device(), m_copyBufferImagePipeline.pipeHandle, nullptr);
    m_vkd->vkDestroyPipelineLayout(m_vkd->device(), m_copyBufferImagePipeline.pipeLayout, nullptr);
    m_vkd->vkDestroyDescriptorSetLayout(m_vkd->device(), m_copyBufferImagePipeline.dsetLayout, nullptr);

    for (const auto& pair : m_pipelines) {
      m_vkd->vkDestroyPipeline(m_vkd->device(), pair.second.pipeHandle, nullptr);
      m_vkd->vkDestroyPipelineLayout(m_vkd->device(), pair.second.pipeLayout, nullptr);
      m_vkd->vkDestroyDescriptorSetLayout (m_vkd->device(), pair.second.dsetLayout, nullptr);
    }

    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_depthStencil.fragMs, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_depthStencil.frag2D, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_depthStencil.frag1D, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_color.fragMs, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_color.frag2D, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_color.frag1D, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderGeom,   nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_shaderVert,   nullptr);
  }


  DxvkMetaCopyFormats DxvkMetaCopyObjects::getFormats(
          VkFormat              dstFormat,
          VkImageAspectFlags    dstAspect,
          VkFormat              srcFormat,
          VkImageAspectFlags    srcAspect) const {
    if (dstAspect == srcAspect)
      return { dstFormat, srcFormat };

    if (dstAspect == VK_IMAGE_ASPECT_COLOR_BIT && srcAspect == VK_IMAGE_ASPECT_DEPTH_BIT) {
      switch (srcFormat) {
        case VK_FORMAT_D16_UNORM:  return { VK_FORMAT_R16_UNORM,  VK_FORMAT_D16_UNORM  };
        case VK_FORMAT_D32_SFLOAT: return { VK_FORMAT_R32_SFLOAT, VK_FORMAT_D32_SFLOAT };
        default:                   return { VK_FORMAT_UNDEFINED,  VK_FORMAT_UNDEFINED  };
      }
    } else if (dstAspect == VK_IMAGE_ASPECT_DEPTH_BIT && srcAspect == VK_IMAGE_ASPECT_COLOR_BIT) {
      switch (dstFormat) {
        case VK_FORMAT_D16_UNORM:  return { VK_FORMAT_D16_UNORM,  VK_FORMAT_R16_UNORM  };
        case VK_FORMAT_D32_SFLOAT: return { VK_FORMAT_D32_SFLOAT, VK_FORMAT_R32_SFLOAT };
        default:                   return { VK_FORMAT_UNDEFINED,  VK_FORMAT_UNDEFINED  };
      }
    }

    return { VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED };
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::getPipeline(
          VkImageViewType       viewType,
          VkFormat              dstFormat,
          VkSampleCountFlagBits dstSamples) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkMetaCopyPipelineKey key;
    key.viewType = viewType;
    key.format   = dstFormat;
    key.samples  = dstSamples;
    
    auto entry = m_pipelines.find(key);
    if (entry != m_pipelines.end())
      return entry->second;

    DxvkMetaCopyPipeline pipeline = createPipeline(key);
    m_pipelines.insert({ key, pipeline });
    return pipeline;
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::getCopyBufferImagePipeline() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!m_copyBufferImagePipeline.pipeHandle)
      m_copyBufferImagePipeline = createCopyBufferImagePipeline();

    return m_copyBufferImagePipeline;
  }
  
  
  VkShaderModule DxvkMetaCopyObjects::createShaderModule(
    const SpirvCodeBuffer&          code) const {
    VkShaderModuleCreateInfo info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    info.codeSize               = code.size();
    info.pCode                  = code.data();
    
    VkShaderModule result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create shader module");
    return result;
  }

  
  DxvkMetaCopyPipeline DxvkMetaCopyObjects::createCopyBufferImagePipeline() {
    DxvkMetaCopyPipeline pipeline;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
      { 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    }};

    VkDescriptorSetLayoutCreateInfo setLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    setLayoutInfo.bindingCount = bindings.size();
    setLayoutInfo.pBindings = bindings.data();

    if (m_vkd->vkCreateDescriptorSetLayout(m_vkd->device(), &setLayoutInfo, nullptr, &pipeline.dsetLayout) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create descriptor set layout");

    VkPushConstantRange pushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(DxvkCopyBufferImageArgs) };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &pipeline.dsetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    if (m_vkd->vkCreatePipelineLayout(m_vkd->device(), &pipelineLayoutInfo, nullptr, &pipeline.pipeLayout) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create pipeline layout");

    VkShaderModule shaderModule = createShaderModule(dxvk_copy_buffer_image);

    VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    pipelineInfo.layout = pipeline.pipeLayout;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.basePipelineIndex = -1;

    if (m_vkd->vkCreateComputePipelines(m_vkd->device(), VK_NULL_HANDLE,
        1, &pipelineInfo, nullptr, &pipeline.pipeHandle) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create compute pipeline");

    m_vkd->vkDestroyShaderModule(m_vkd->device(), shaderModule, nullptr);
    return pipeline;
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::createPipeline(
    const DxvkMetaCopyPipelineKey&  key) {
    DxvkMetaCopyPipeline pipeline;
    pipeline.dsetLayout = this->createDescriptorSetLayout(key);
    pipeline.pipeLayout = this->createPipelineLayout(pipeline.dsetLayout);
    pipeline.pipeHandle = this->createPipelineObject(key, pipeline.pipeLayout);
    return pipeline;
  }


  VkDescriptorSetLayout DxvkMetaCopyObjects::createDescriptorSetLayout(
    const DxvkMetaCopyPipelineKey&  key) const {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
      { 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};
    
    VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.bindingCount = bindings.size();
    info.pBindings = bindings.data();

    VkDescriptorSetLayout result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateDescriptorSetLayout(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create descriptor set layout");
    return result;
  }

  
  VkPipelineLayout DxvkMetaCopyObjects::createPipelineLayout(
          VkDescriptorSetLayout     descriptorSetLayout) const {
    VkPushConstantRange push = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VkOffset2D) };

    VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    info.setLayoutCount         = 1;
    info.pSetLayouts            = &descriptorSetLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges    = &push;
    
    VkPipelineLayout result = VK_NULL_HANDLE;
    if (m_vkd->vkCreatePipelineLayout(m_vkd->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create pipeline layout");
    return result;
  }
  

  VkPipeline DxvkMetaCopyObjects::createPipelineObject(
    const DxvkMetaCopyPipelineKey&  key,
          VkPipelineLayout          pipelineLayout) {
    auto aspect = lookupFormatInfo(key.format)->aspectMask;

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
    
    std::array<std::pair<const FragShaders*, VkImageAspectFlags>, 3> shaderSets = {{
      { &m_color,        VK_IMAGE_ASPECT_COLOR_BIT },
      { &m_color,        VK_IMAGE_ASPECT_DEPTH_BIT },
      { &m_depthStencil, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT },
    }};

    const FragShaders* shaderSet = nullptr;
    
    for (const auto& pair : shaderSets) {
      if (pair.second == aspect)
        shaderSet = pair.first;
    }

    if (!shaderSet)
      throw DxvkError(str::format("DxvkMetaCopyObjects: Unsupported aspect mask: ", aspect));

    VkShaderModule psModule = VK_NULL_HANDLE;

    if (key.viewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY)
      psModule = shaderSet->frag1D;
    else if (key.samples == VK_SAMPLE_COUNT_1_BIT)
      psModule = shaderSet->frag2D;
    else
      psModule = shaderSet->fragMs;
    
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
    
    VkPipelineViewportStateCreateInfo vpState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    
    VkPipelineRasterizationStateCreateInfo rsState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsState.depthClampEnable = VK_TRUE;
    rsState.polygonMode = VK_POLYGON_MODE_FILL;
    rsState.cullMode = VK_CULL_MODE_NONE;
    rsState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.lineWidth = 1.0f;
    
    uint32_t msMask = 0xFFFFFFFF;
    VkPipelineMultisampleStateCreateInfo msState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msState.rasterizationSamples = key.samples;
    msState.sampleShadingEnable = key.samples != VK_SAMPLE_COUNT_1_BIT;
    msState.minSampleShading  = 1.0f;
    msState.pSampleMask       = &msMask;
    
    VkPipelineColorBlendAttachmentState cbAttachment = { };
    cbAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    VkPipelineColorBlendStateCreateInfo cbState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbState.attachmentCount = 1;
    cbState.pAttachments = &cbAttachment;
    
    VkStencilOpState stencilOp = { };
    stencilOp.failOp = VK_STENCIL_OP_REPLACE;
    stencilOp.passOp = VK_STENCIL_OP_REPLACE;
    stencilOp.depthFailOp = VK_STENCIL_OP_REPLACE;
    stencilOp.compareOp = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask = 0xFFFFFFFF;
    stencilOp.writeMask = 0xFFFFFFFF;
    
    VkPipelineDepthStencilStateCreateInfo dsState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    dsState.depthTestEnable = VK_TRUE;
    dsState.depthWriteEnable = VK_TRUE;
    dsState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    dsState.stencilTestEnable = VK_TRUE;
    dsState.front = stencilOp;
    dsState.back = stencilOp;
    
    VkPipelineRenderingCreateInfo rtState = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

    if (aspect & VK_IMAGE_ASPECT_COLOR_BIT) {
      rtState.colorAttachmentCount = 1;
      rtState.pColorAttachmentFormats = &key.format;
    } else {
      if (aspect & VK_IMAGE_ASPECT_DEPTH_BIT)
        rtState.depthAttachmentFormat = key.format;
      if (aspect & VK_IMAGE_ASPECT_STENCIL_BIT)
        rtState.stencilAttachmentFormat = key.format;
    }

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &rtState };
    info.stageCount             = stageCount;
    info.pStages                = stages.data();
    info.pVertexInputState      = &viState;
    info.pInputAssemblyState    = &iaState;
    info.pViewportState         = &vpState;
    info.pRasterizationState    = &rsState;
    info.pMultisampleState      = &msState;
    info.pColorBlendState       = (aspect & VK_IMAGE_ASPECT_COLOR_BIT) ? &cbState : nullptr;
    info.pDepthStencilState     = (aspect & VK_IMAGE_ASPECT_COLOR_BIT) ? nullptr : &dsState;
    info.pDynamicState          = &dynState;
    info.layout                 = pipelineLayout;
    info.basePipelineIndex      = -1;
    
    VkPipeline result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateGraphicsPipelines(m_vkd->device(), VK_NULL_HANDLE, 1, &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create graphics pipeline");
    return result;
  }
  
}