#include "dxvk_device.h"
#include "dxvk_meta_copy.h"
#include "dxvk_util.h"

#include <dxvk_fullscreen_geom.h>
#include <dxvk_fullscreen_vert.h>
#include <dxvk_fullscreen_layer_vert.h>

#include <dxvk_buffer_to_image_d.h>
#include <dxvk_buffer_to_image_ds_export.h>
#include <dxvk_buffer_to_image_s_discard.h>

#include <dxvk_image_to_buffer_ds.h>
#include <dxvk_image_to_buffer_f.h>

#include <dxvk_copy_buffer_image.h>
#include <dxvk_copy_color_1d.h>
#include <dxvk_copy_color_2d.h>
#include <dxvk_copy_color_ms.h>
#include <dxvk_copy_depth_stencil_1d.h>
#include <dxvk_copy_depth_stencil_2d.h>
#include <dxvk_copy_depth_stencil_ms.h>

namespace dxvk {

  DxvkMetaCopyViews::DxvkMetaCopyViews(
    const Rc<DxvkImage>&            dstImage,
    const VkImageSubresourceLayers& dstSubresources,
          VkFormat                  dstFormat,
    const Rc<DxvkImage>&            srcImage,
    const VkImageSubresourceLayers& srcSubresources,
          VkFormat                  srcFormat) {
    VkImageAspectFlags dstAspects = dstImage->formatInfo()->aspectMask;
    VkImageAspectFlags srcAspects = srcImage->formatInfo()->aspectMask;

    // We don't support 3D here, so we can safely ignore that case
    VkImageViewType dstViewType = dstImage->info().type == VK_IMAGE_TYPE_1D
      ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    VkImageViewType srcViewType = srcImage->info().type == VK_IMAGE_TYPE_1D
      ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    DxvkImageViewKey dstViewInfo;
    dstViewInfo.viewType = dstViewType;
    dstViewInfo.format = dstFormat;
    dstViewInfo.aspects = dstSubresources.aspectMask;
    dstViewInfo.mipIndex = dstSubresources.mipLevel;
    dstViewInfo.mipCount = 1u;
    dstViewInfo.layerIndex = dstSubresources.baseArrayLayer;
    dstViewInfo.layerCount = dstSubresources.layerCount;
    dstViewInfo.usage = (dstAspects & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
      ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
      : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    dstImageView = dstImage->createView(dstViewInfo);

    // Create source image views
    DxvkImageViewKey srcViewInfo;
    srcViewInfo.viewType = srcViewType;
    srcViewInfo.format = srcFormat;
    srcViewInfo.aspects = srcSubresources.aspectMask & ~VK_IMAGE_ASPECT_STENCIL_BIT;
    srcViewInfo.mipIndex = srcSubresources.mipLevel;
    srcViewInfo.mipCount = 1u;
    srcViewInfo.layerIndex = srcSubresources.baseArrayLayer;
    srcViewInfo.layerCount = srcSubresources.layerCount;
    srcViewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    srcImageView = srcImage->createView(srcViewInfo);

    if (srcAspects & VK_IMAGE_ASPECT_STENCIL_BIT) {
      srcViewInfo.aspects = VK_IMAGE_ASPECT_STENCIL_BIT;
      srcStencilView = srcImage->createView(srcViewInfo);
    }
  }
  

  DxvkMetaCopyViews::~DxvkMetaCopyViews() {

  }

  
  DxvkMetaCopyObjects::DxvkMetaCopyObjects(DxvkDevice* device)
  : m_device(device) {

  }


  DxvkMetaCopyObjects::~DxvkMetaCopyObjects() {
    auto vk = m_device->vkd();

    for (const auto& p : m_bufferToImagePipelines)
      vk->vkDestroyPipeline(vk->device(), p.second, nullptr);

    for (const auto& p : m_imageToBufferPipelines)
      vk->vkDestroyPipeline(vk->device(), p.second, nullptr);

    vk->vkDestroyDescriptorSetLayout(vk->device(), m_bufferToImageCopySetLayout, nullptr);
    vk->vkDestroyDescriptorSetLayout(vk->device(), m_imageToBufferCopySetLayout, nullptr);

    vk->vkDestroyPipelineLayout(vk->device(), m_bufferToImageCopyPipelineLayout, nullptr);
    vk->vkDestroyPipelineLayout(vk->device(), m_imageToBufferCopyPipelineLayout, nullptr);

    vk->vkDestroyDescriptorSetLayout(vk->device(), m_copyBufferImagePipeline.dsetLayout, nullptr);
    vk->vkDestroyPipeline(vk->device(), m_copyBufferImagePipeline.pipeHandle, nullptr);
    vk->vkDestroyPipelineLayout(vk->device(), m_copyBufferImagePipeline.pipeLayout, nullptr);

    for (const auto& pair : m_pipelines) {
      vk->vkDestroyPipeline(vk->device(), pair.second.pipeHandle, nullptr);
      vk->vkDestroyPipelineLayout(vk->device(), pair.second.pipeLayout, nullptr);
      vk->vkDestroyDescriptorSetLayout(vk->device(), pair.second.dsetLayout, nullptr);
    }
  }


  DxvkMetaCopyFormats DxvkMetaCopyObjects::getCopyImageFormats(
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


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::getCopyBufferToImagePipeline(
          VkFormat              dstFormat,
          VkFormat              srcFormat,
          VkImageAspectFlags    aspects) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkMetaBufferImageCopyPipelineKey key;
    key.imageFormat = dstFormat;
    key.bufferFormat = srcFormat;
    key.imageAspects = aspects;

    auto entry = m_bufferToImagePipelines.find(key);
    if (entry != m_bufferToImagePipelines.end()) {
      DxvkMetaCopyPipeline result = { };
      result.dsetLayout = m_bufferToImageCopySetLayout;
      result.pipeLayout = m_bufferToImageCopyPipelineLayout;
      result.pipeHandle = entry->second;
      return result;
    }

    VkPipeline pipeline = createCopyBufferToImagePipeline(key);
    m_bufferToImagePipelines.insert({ key, pipeline });

    DxvkMetaCopyPipeline result = { };
    result.dsetLayout = m_bufferToImageCopySetLayout;
    result.pipeLayout = m_bufferToImageCopyPipelineLayout;
    result.pipeHandle = pipeline;
    return result;
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::getCopyImageToBufferPipeline(
          VkImageViewType       viewType,
          VkFormat              dstFormat) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    
    DxvkMetaBufferImageCopyPipelineKey key;
    key.imageViewType = viewType;
    key.imageFormat = VK_FORMAT_UNDEFINED;
    key.bufferFormat = dstFormat;
    key.imageAspects = lookupFormatInfo(dstFormat)->aspectMask;

    auto entry = m_bufferToImagePipelines.find(key);
    if (entry != m_bufferToImagePipelines.end()) {
      DxvkMetaCopyPipeline result = { };
      result.dsetLayout = m_imageToBufferCopySetLayout;
      result.pipeLayout = m_imageToBufferCopyPipelineLayout;
      result.pipeHandle = entry->second;
      return result;
    }

    VkPipeline pipeline = createCopyImageToBufferPipeline(key);
    m_bufferToImagePipelines.insert({ key, pipeline });

    DxvkMetaCopyPipeline result = { };
    result.dsetLayout = m_imageToBufferCopySetLayout;
    result.pipeLayout = m_imageToBufferCopyPipelineLayout;
    result.pipeHandle = pipeline;
    return result;
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::getCopyImagePipeline(
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


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::getCopyFormattedBufferPipeline() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!m_copyBufferImagePipeline.pipeHandle)
      m_copyBufferImagePipeline = createCopyFormattedBufferPipeline();

    return m_copyBufferImagePipeline;
  }
  
  
  DxvkMetaCopyPipeline DxvkMetaCopyObjects::createCopyFormattedBufferPipeline() {
    auto vk = m_device->vkd();

    DxvkMetaCopyPipeline pipeline;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
      { 1, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
    }};

    VkDescriptorSetLayoutCreateInfo setLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    setLayoutInfo.bindingCount = bindings.size();
    setLayoutInfo.pBindings = bindings.data();

    if (vk->vkCreateDescriptorSetLayout(vk->device(), &setLayoutInfo, nullptr, &pipeline.dsetLayout))
      throw DxvkError("DxvkMetaCopyObjects: Failed to create descriptor set layout");

    VkPushConstantRange pushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(DxvkFormattedBufferCopyArgs) };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &pipeline.dsetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    if (vk->vkCreatePipelineLayout(vk->device(), &pipelineLayoutInfo, nullptr, &pipeline.pipeLayout))
      throw DxvkError("DxvkMetaCopyObjects: Failed to create pipeline layout");

    VkShaderModuleCreateInfo moduleInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    moduleInfo.codeSize = sizeof(dxvk_copy_buffer_image);
    moduleInfo.pCode = dxvk_copy_buffer_image;

    VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    pipelineInfo.layout = pipeline.pipeLayout;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.pNext = &moduleInfo;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.basePipelineIndex = -1;

    if (vk->vkCreateComputePipelines(vk->device(), VK_NULL_HANDLE,
        1, &pipelineInfo, nullptr, &pipeline.pipeHandle) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create compute pipeline");

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


  VkPipeline DxvkMetaCopyObjects::createCopyBufferToImagePipeline(
    const DxvkMetaBufferImageCopyPipelineKey& key) {
    auto vk = m_device->vkd();

    if (!m_bufferToImageCopySetLayout) {
      std::array<VkDescriptorSetLayoutBinding, 1> bindings = {{
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
      }};

      VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
      info.bindingCount = bindings.size();
      info.pBindings = bindings.data();

      VkResult vr = vk->vkCreateDescriptorSetLayout(vk->device(),
        &info, nullptr, &m_bufferToImageCopySetLayout);

      if (vr)
        throw DxvkError(str::format("DxvkMetaCopyObjects: Failed to create descriptor set layout: ", vr));
    }

    if (!m_bufferToImageCopyPipelineLayout) {
      VkPushConstantRange pushConstants = { };
      pushConstants.offset = 0;
      pushConstants.size = sizeof(DxvkBufferImageCopyArgs);
      pushConstants.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

      VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
      info.setLayoutCount = 1;
      info.pSetLayouts = &m_bufferToImageCopySetLayout;
      info.pushConstantRangeCount = 1;
      info.pPushConstantRanges = &pushConstants;

      VkResult vr = vk->vkCreatePipelineLayout(vk->device(),
        &info, nullptr, &m_bufferToImageCopyPipelineLayout);

      if (vr)
        throw DxvkError(str::format("DxvkMetaCopyObjects: Failed to create pipeline layout: ", vr));
    }

    // We don't support color right now
    if (!(key.imageAspects & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))) {
      Logger::err(str::format("DxvkMetaCopyObjects: Color images not supported"));
      return VK_NULL_HANDLE;
    }

    // Set up geometry stages as necessary
    util::DxvkBuiltInShaderStages stages = { };

    if (m_device->features().vk12.shaderOutputLayer) {
      stages.addStage(VK_SHADER_STAGE_VERTEX_BIT, dxvk_fullscreen_layer_vert, nullptr);
    } else {
      stages.addStage(VK_SHADER_STAGE_VERTEX_BIT, dxvk_fullscreen_vert, nullptr);
      stages.addStage(VK_SHADER_STAGE_GEOMETRY_BIT, dxvk_fullscreen_geom, nullptr);
    }

    // Set up dynamic states. Stencil write mask is
    // only required for the stencil discard shader.
    std::array<VkDynamicState, 3> dynStates = {{
      VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
      VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
      VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
    }};

    VkPipelineDynamicStateCreateInfo dynState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynState.dynamicStateCount = dynStates.size();
    dynState.pDynamicStates = dynStates.data();

    // Determine fragment shader to use. Always use the DS export shader
    // if possible, it can support writing to one aspect exclusively.
    VkSpecializationMapEntry specMap = { };
    specMap.size = sizeof(VkFormat);

    VkSpecializationInfo specInfo = { };
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries = &specMap;
    specInfo.dataSize = sizeof(VkFormat);
    specInfo.pData = &key.bufferFormat;

    if (m_device->features().extShaderStencilExport) {
      stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_buffer_to_image_ds_export, &specInfo);
      dynState.dynamicStateCount -= 1u;
    } else if (key.imageAspects == VK_IMAGE_ASPECT_STENCIL_BIT) {
      stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_buffer_to_image_s_discard, &specInfo);
    } else {
      stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_buffer_to_image_d, &specInfo);
      dynState.dynamicStateCount -= 1u;
    }

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

    uint32_t msMask = 0x1;
    VkPipelineMultisampleStateCreateInfo msState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    msState.pSampleMask = &msMask;

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
    stencilOp.compareMask = 0xff;
    stencilOp.writeMask = 0xff;
    stencilOp.reference = 0xff;

    // Clear stencil for depth-only aspect
    if (!m_device->features().extShaderStencilExport && key.imageAspects != VK_IMAGE_ASPECT_STENCIL_BIT)
      stencilOp.reference = 0x00;

    VkPipelineDepthStencilStateCreateInfo dsState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    dsState.depthTestEnable = !!(key.imageAspects & VK_IMAGE_ASPECT_DEPTH_BIT);
    dsState.depthWriteEnable = dsState.depthTestEnable;
    dsState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    dsState.stencilTestEnable = !!(key.imageAspects & VK_IMAGE_ASPECT_STENCIL_BIT);
    dsState.front = stencilOp;
    dsState.back = stencilOp;

    VkPipelineRenderingCreateInfo rtState = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

    if (key.imageAspects & VK_IMAGE_ASPECT_COLOR_BIT) {
      rtState.colorAttachmentCount = 1;
      rtState.pColorAttachmentFormats = &key.imageFormat;
    } else {
      auto formatAspects = lookupFormatInfo(key.imageFormat)->aspectMask;

      if (formatAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
        rtState.depthAttachmentFormat = key.imageFormat;
      if (formatAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
        rtState.stencilAttachmentFormat = key.imageFormat;
    }

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &rtState };
    info.stageCount             = stages.count();
    info.pStages                = stages.infos();
    info.pVertexInputState      = &viState;
    info.pInputAssemblyState    = &iaState;
    info.pViewportState         = &vpState;
    info.pRasterizationState    = &rsState;
    info.pMultisampleState      = &msState;
    info.pColorBlendState       = (key.imageAspects & VK_IMAGE_ASPECT_COLOR_BIT) ? &cbState : nullptr;
    info.pDepthStencilState     = (key.imageAspects & VK_IMAGE_ASPECT_COLOR_BIT) ? nullptr : &dsState;
    info.pDynamicState          = &dynState;
    info.layout                 = m_bufferToImageCopyPipelineLayout;
    info.basePipelineIndex      = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(),
      VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("DxvkMetaCopyObjects: Failed to create graphics pipeline: ", vr));

    return pipeline;
  }


  VkPipeline DxvkMetaCopyObjects::createCopyImageToBufferPipeline(
    const DxvkMetaBufferImageCopyPipelineKey& key) {
    auto vk = m_device->vkd();

    if (!m_imageToBufferCopySetLayout) {
      std::array<VkDescriptorSetLayoutBinding, 3> bindings = {{
        { 0, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
        { 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        1, VK_SHADER_STAGE_COMPUTE_BIT },
        { 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        1, VK_SHADER_STAGE_COMPUTE_BIT },
      }};

      VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
      info.bindingCount = bindings.size();
      info.pBindings = bindings.data();

      VkResult vr = vk->vkCreateDescriptorSetLayout(vk->device(),
        &info, nullptr, &m_imageToBufferCopySetLayout);

      if (vr)
        throw DxvkError(str::format("DxvkMetaCopyObjects: Failed to create descriptor set layout: ", vr));
    }

    if (!m_imageToBufferCopyPipelineLayout) {
      VkPushConstantRange pushConstants = { };
      pushConstants.offset = 0;
      pushConstants.size = sizeof(DxvkBufferImageCopyArgs);
      pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

      VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
      info.setLayoutCount = 1;
      info.pSetLayouts = &m_imageToBufferCopySetLayout;
      info.pushConstantRangeCount = 1;
      info.pPushConstantRanges = &pushConstants;

      VkResult vr = vk->vkCreatePipelineLayout(vk->device(),
        &info, nullptr, &m_imageToBufferCopyPipelineLayout);

      if (vr)
        throw DxvkError(str::format("DxvkMetaCopyObjects: Failed to create pipeline layout: ", vr));
    }

    if (key.imageViewType != VK_IMAGE_VIEW_TYPE_2D_ARRAY) {
      Logger::err(str::format("DxvkMetaCopyObjects: Unsupported view type: ", key.imageViewType));
      return VK_NULL_HANDLE;
    }

    VkSpecializationMapEntry specMap = { };
    specMap.size = sizeof(VkFormat);

    VkSpecializationInfo specInfo = { };
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries = &specMap;
    specInfo.dataSize = sizeof(VkFormat);
    specInfo.pData = &key.bufferFormat;

    VkShaderModuleCreateInfo moduleInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };

    if (key.imageAspects & VK_IMAGE_ASPECT_STENCIL_BIT) {
      moduleInfo.codeSize = sizeof(dxvk_image_to_buffer_ds);
      moduleInfo.pCode = dxvk_image_to_buffer_ds;
    } else {
      moduleInfo.codeSize = sizeof(dxvk_image_to_buffer_f);
      moduleInfo.pCode = dxvk_image_to_buffer_f;
    }

    VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage.pNext = &moduleInfo;
    info.stage.pName = "main";
    info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.pSpecializationInfo = &specInfo;
    info.layout = m_imageToBufferCopyPipelineLayout;
    info.basePipelineIndex = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;

    VkResult vr = vk->vkCreateComputePipelines(vk->device(),
      VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr)
      throw DxvkError(str::format("DxvkMetaCopyObjects: Failed to create compute pipeline", vr));

    return pipeline;
  }


  VkDescriptorSetLayout DxvkMetaCopyObjects::createDescriptorSetLayout(
    const DxvkMetaCopyPipelineKey&  key) const {
    auto vk = m_device->vkd();

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
      { 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};
    
    VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.bindingCount = bindings.size();
    info.pBindings = bindings.data();

    VkDescriptorSetLayout result = VK_NULL_HANDLE;

    if (vk->vkCreateDescriptorSetLayout(vk->device(), &info, nullptr, &result))
      throw DxvkError("DxvkMetaCopyObjects: Failed to create descriptor set layout");

    return result;
  }

  
  VkPipelineLayout DxvkMetaCopyObjects::createPipelineLayout(
          VkDescriptorSetLayout     descriptorSetLayout) const {
    auto vk = m_device->vkd();

    VkPushConstantRange push = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VkOffset2D) };

    VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    info.setLayoutCount         = 1;
    info.pSetLayouts            = &descriptorSetLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges    = &push;
    
    VkPipelineLayout result = VK_NULL_HANDLE;

    if (vk->vkCreatePipelineLayout(vk->device(), &info, nullptr, &result))
      throw DxvkError("DxvkMetaCopyObjects: Failed to create pipeline layout");

    return result;
  }
  

  VkPipeline DxvkMetaCopyObjects::createPipelineObject(
    const DxvkMetaCopyPipelineKey&  key,
          VkPipelineLayout          pipelineLayout) {
    auto vk = m_device->vkd();
    auto aspect = lookupFormatInfo(key.format)->aspectMask;

    // Set up geometry stages as necessary
    util::DxvkBuiltInShaderStages stages = { };

    if (m_device->features().vk12.shaderOutputLayer) {
      stages.addStage(VK_SHADER_STAGE_VERTEX_BIT, dxvk_fullscreen_layer_vert, nullptr);
    } else {
      stages.addStage(VK_SHADER_STAGE_VERTEX_BIT, dxvk_fullscreen_vert, nullptr);
      stages.addStage(VK_SHADER_STAGE_GEOMETRY_BIT, dxvk_fullscreen_geom, nullptr);
    }

    bool useDepthStencil = m_device->features().extShaderStencilExport
      && (aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));

    if (useDepthStencil) {
      if (key.viewType == VK_IMAGE_VIEW_TYPE_1D)
        stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_copy_depth_stencil_1d, nullptr);
      else if (key.samples == VK_SAMPLE_COUNT_1_BIT)
        stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_copy_depth_stencil_2d, nullptr);
      else
        stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_copy_depth_stencil_ms, nullptr);
    } else {
      if (key.viewType == VK_IMAGE_VIEW_TYPE_1D)
        stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_copy_color_1d, nullptr);
      else if (key.samples == VK_SAMPLE_COUNT_1_BIT)
        stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_copy_color_2d, nullptr);
      else
        stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_copy_color_ms, nullptr);
    }

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
    info.stageCount             = stages.count();
    info.pStages                = stages.infos();
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

    if (vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE, 1, &info, nullptr, &result))
      throw DxvkError("DxvkMetaCopyObjects: Failed to create graphics pipeline");

    return result;
  }
  
}
