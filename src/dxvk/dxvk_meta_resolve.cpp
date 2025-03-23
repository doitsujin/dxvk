#include "dxvk_device.h"
#include "dxvk_meta_resolve.h"
#include "dxvk_util.h"

#include <dxvk_fullscreen_geom.h>
#include <dxvk_fullscreen_vert.h>
#include <dxvk_fullscreen_layer_vert.h>

#include <dxvk_resolve_frag_d.h>
#include <dxvk_resolve_frag_ds.h>
#include <dxvk_resolve_frag_f.h>
#include <dxvk_resolve_frag_u.h>
#include <dxvk_resolve_frag_i.h>

namespace dxvk {
  
  DxvkMetaResolveViews::DxvkMetaResolveViews(
    const Rc<DxvkImage>&            dstImage,
    const VkImageSubresourceLayers& dstSubresources,
    const Rc<DxvkImage>&            srcImage,
    const VkImageSubresourceLayers& srcSubresources,
          VkFormat                  format) {
    DxvkImageViewKey viewInfo;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = format;
    viewInfo.aspects = dstSubresources.aspectMask;
    viewInfo.mipIndex = dstSubresources.mipLevel;
    viewInfo.mipCount = 1u;
    viewInfo.layerIndex = dstSubresources.baseArrayLayer;
    viewInfo.layerCount = dstSubresources.layerCount;
    viewInfo.usage = (lookupFormatInfo(format)->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
      ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
      : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    dstView = dstImage->createView(viewInfo);

    viewInfo.aspects = srcSubresources.aspectMask;
    viewInfo.mipIndex = srcSubresources.mipLevel;
    viewInfo.layerIndex = srcSubresources.baseArrayLayer;
    viewInfo.layerCount = srcSubresources.layerCount;

    srcView = srcImage->createView(viewInfo);
  }


  DxvkMetaResolveViews::~DxvkMetaResolveViews() {

  }




  DxvkMetaResolveObjects::DxvkMetaResolveObjects(DxvkDevice* device)
  : m_device(device) {

  }


  DxvkMetaResolveObjects::~DxvkMetaResolveObjects() {
    auto vk = m_device->vkd();

    for (const auto& pair : m_pipelines) {
      vk->vkDestroyPipeline(vk->device(), pair.second.pipeHandle, nullptr);
      vk->vkDestroyPipelineLayout(vk->device(), pair.second.pipeLayout, nullptr);
      vk->vkDestroyDescriptorSetLayout(vk->device(), pair.second.dsetLayout, nullptr);
    }
  }


  DxvkMetaResolvePipeline DxvkMetaResolveObjects::getPipeline(
          VkFormat                  format,
          VkSampleCountFlagBits     samples,
          VkResolveModeFlagBits     depthResolveMode,
          VkResolveModeFlagBits     stencilResolveMode) {
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


  DxvkMetaResolvePipeline DxvkMetaResolveObjects::createPipeline(
    const DxvkMetaResolvePipelineKey& key) {
    DxvkMetaResolvePipeline pipeline;
    pipeline.dsetLayout = this->createDescriptorSetLayout(key);
    pipeline.pipeLayout = this->createPipelineLayout(pipeline.dsetLayout);
    pipeline.pipeHandle = this->createPipelineObject(key, pipeline.pipeLayout);
    return pipeline;
  }


  VkDescriptorSetLayout DxvkMetaResolveObjects::createDescriptorSetLayout(
    const DxvkMetaResolvePipelineKey& key) {
    auto vk = m_device->vkd();

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
      { 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
    }};

    VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.bindingCount = bindings.size();
    info.pBindings = bindings.data();

    VkDescriptorSetLayout result = VK_NULL_HANDLE;

    if (vk->vkCreateDescriptorSetLayout(vk->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaResolveObjects: Failed to create descriptor set layout");

    return result;
  }
  

  VkPipelineLayout DxvkMetaResolveObjects::createPipelineLayout(
          VkDescriptorSetLayout  descriptorSetLayout) {
    auto vk = m_device->vkd();

    VkPushConstantRange push = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VkOffset2D) };

    VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    info.setLayoutCount         = 1;
    info.pSetLayouts            = &descriptorSetLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges    = &push;
    
    VkPipelineLayout result = VK_NULL_HANDLE;

    if (vk->vkCreatePipelineLayout(vk->device(), &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create pipeline layout");

    return result;
  }
  

  VkPipeline DxvkMetaResolveObjects::createPipelineObject(
    const DxvkMetaResolvePipelineKey& key,
          VkPipelineLayout       pipelineLayout) {
    auto vk = m_device->vkd();
    auto formatInfo = lookupFormatInfo(key.format);

    util::DxvkBuiltInShaderStages stages = { };

    std::array<VkSpecializationMapEntry, 3> specEntries = {{
      { 0, offsetof(DxvkMetaResolvePipelineKey, samples), sizeof(VkSampleCountFlagBits) },
      { 1, offsetof(DxvkMetaResolvePipelineKey, modeD),   sizeof(VkResolveModeFlagBits) },
      { 2, offsetof(DxvkMetaResolvePipelineKey, modeS),   sizeof(VkResolveModeFlagBits) },
    }};

    VkSpecializationInfo specInfo = { };
    specInfo.mapEntryCount      = specEntries.size();
    specInfo.pMapEntries        = specEntries.data();
    specInfo.dataSize           = sizeof(key);
    specInfo.pData              = &key;

    if (m_device->features().vk12.shaderOutputLayer) {
      stages.addStage(VK_SHADER_STAGE_VERTEX_BIT, dxvk_fullscreen_layer_vert, nullptr);
    } else {
      stages.addStage(VK_SHADER_STAGE_VERTEX_BIT, dxvk_fullscreen_vert, nullptr);
      stages.addStage(VK_SHADER_STAGE_GEOMETRY_BIT, dxvk_fullscreen_geom, nullptr);
    }

    if (key.modeS && (formatInfo->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)) {
      if (m_device->features().extShaderStencilExport) {
        stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_resolve_frag_ds, &specInfo);
      } else {
        stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_resolve_frag_d, &specInfo);
        Logger::warn("DXVK: Stencil export not supported by device, skipping stencil resolve");
      }
    } else if (formatInfo->aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) {
      stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_resolve_frag_d, &specInfo);
    } else if (formatInfo->flags.test(DxvkFormatFlag::SampledUInt)) {
      stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_resolve_frag_u, &specInfo);
    } else if (formatInfo->flags.test(DxvkFormatFlag::SampledSInt)) {
      stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_resolve_frag_i, &specInfo);
    } else {
      stages.addStage(VK_SHADER_STAGE_FRAGMENT_BIT, dxvk_resolve_frag_f, &specInfo);
    }

    std::array<VkDynamicState, 2> dynStates = {{
      VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
      VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
    }};
    
    VkPipelineDynamicStateCreateInfo dynState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynState.dynamicStateCount  = dynStates.size();
    dynState.pDynamicStates     = dynStates.data();
    
    VkPipelineVertexInputStateCreateInfo viState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    
    VkPipelineInputAssemblyStateCreateInfo iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    iaState.topology            = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    
    VkPipelineViewportStateCreateInfo vpState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    
    VkPipelineRasterizationStateCreateInfo rsState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsState.depthClampEnable    = VK_TRUE;
    rsState.polygonMode         = VK_POLYGON_MODE_FILL;
    rsState.cullMode            = VK_CULL_MODE_NONE;
    rsState.frontFace           = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.lineWidth           = 1.0f;
    
    uint32_t msMask = 0xFFFFFFFF;
    VkPipelineMultisampleStateCreateInfo msState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msState.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    msState.pSampleMask           = &msMask;

    VkPipelineColorBlendAttachmentState cbAttachment = { };
    cbAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    VkPipelineColorBlendStateCreateInfo cbState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbState.attachmentCount     = 1;
    cbState.pAttachments        = &cbAttachment;
    
    VkStencilOpState stencilOp = { };
    stencilOp.failOp            = VK_STENCIL_OP_REPLACE;
    stencilOp.passOp            = VK_STENCIL_OP_REPLACE;
    stencilOp.depthFailOp       = VK_STENCIL_OP_REPLACE;
    stencilOp.compareOp         = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask       = 0xFFFFFFFF;
    stencilOp.writeMask         = 0xFFFFFFFF;

    VkPipelineDepthStencilStateCreateInfo dsState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    dsState.depthTestEnable     = key.modeD != VK_RESOLVE_MODE_NONE;
    dsState.depthWriteEnable    = key.modeD != VK_RESOLVE_MODE_NONE;
    dsState.depthCompareOp      = VK_COMPARE_OP_ALWAYS;
    dsState.stencilTestEnable   = key.modeS != VK_RESOLVE_MODE_NONE;
    dsState.front               = stencilOp;
    dsState.back                = stencilOp;

    VkPipelineRenderingCreateInfo rtState = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

    if (formatInfo->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
      rtState.colorAttachmentCount = 1;
      rtState.pColorAttachmentFormats = &key.format;
    } else {
      if (formatInfo->aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
        rtState.depthAttachmentFormat = key.format;
      if (formatInfo->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
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
    info.pColorBlendState       = (formatInfo->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) ? &cbState : nullptr;
    info.pDepthStencilState     = (formatInfo->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) ? nullptr : &dsState;
    info.pDynamicState          = &dynState;
    info.layout                 = pipelineLayout;
    info.basePipelineIndex      = -1;
    
    VkPipeline result = VK_NULL_HANDLE;

    if (vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE, 1, &info, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaCopyObjects: Failed to create graphics pipeline");

    return result;
  }

}
