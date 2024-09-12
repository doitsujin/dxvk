#include "dxvk_hud_objects.h"

#include <dxvk_present_frag.h>
#include <dxvk_present_frag_blit.h>
#include <dxvk_present_frag_ms.h>
#include <dxvk_present_frag_ms_amd.h>
#include <dxvk_present_vert.h>

#include <hud_graph_frag.h>
#include <hud_graph_vert.h>

#include <hud_text_frag.h>
#include <hud_text_vert.h>

#include "dxvk_device.h"

namespace dxvk {

  DxvkHudObjects::DxvkHudObjects(const DxvkDevice* device)
    : m_vkd(device->vkd()) {
    this->createShaders(device);
    this->createFontSampler();
  }

  DxvkHudObjects::~DxvkHudObjects() {
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_textVs, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_textFs, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_graphVs, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_graphFs, nullptr);

    m_vkd->vkDestroySampler(m_vkd->device(), m_fontSampler, nullptr);

    for (const auto& [key, pipeline] : m_pipelines) {
      m_vkd->vkDestroyPipeline(m_vkd->device(), pipeline.textPipeHandle, nullptr);
      m_vkd->vkDestroyPipelineLayout(m_vkd->device(), pipeline.textPipeLayout, nullptr);
      m_vkd->vkDestroyDescriptorSetLayout(m_vkd->device(), pipeline.textDSetLayout, nullptr);
      m_vkd->vkDestroyPipeline(m_vkd->device(), pipeline.graphPipeHandle, nullptr);
      m_vkd->vkDestroyPipelineLayout(m_vkd->device(), pipeline.graphPipeLayout, nullptr);
      m_vkd->vkDestroyDescriptorSetLayout(m_vkd->device(), pipeline.graphDSetLayout, nullptr);
    }
  }

  void DxvkHudObjects::createShaders(const DxvkDevice* device) {
    SpirvCodeBuffer textVsCode(hud_text_vert);
    SpirvCodeBuffer textFsCode(hud_text_frag);
    SpirvCodeBuffer graphVsCode(hud_graph_vert);
    SpirvCodeBuffer graphFsCode(hud_graph_frag);

    VkShaderModuleCreateInfo info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
      info.codeSize               = textVsCode.size();
      info.pCode                  = textVsCode.data();

    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &info, nullptr, &m_textVs) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create shader module");

    info.pCode                 = textFsCode.data();
    info.codeSize              = textFsCode.size();

    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &info, nullptr, &m_textFs) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create shader module");

    info.pCode                 = graphVsCode.data();
    info.codeSize              = graphVsCode.size();

    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &info, nullptr, &m_graphVs) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create shader module");

    info.pCode                 = graphFsCode.data();
    info.codeSize              = graphFsCode.size();

    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &info, nullptr, &m_graphFs) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create shader module");
  }

  void DxvkHudObjects::createFontSampler() {
    VkSamplerCreateInfo info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    info.magFilter               = VK_FILTER_LINEAR;
    info.minFilter               = VK_FILTER_LINEAR;
    info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    info.unnormalizedCoordinates = VK_TRUE;
    info.maxAnisotropy           = 1.0f;

    if (m_vkd->vkCreateSampler(m_vkd->device(), &info, nullptr, &m_fontSampler) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create sampler");
  }

  DxvkHudPipelines DxvkHudObjects::getPipelines(
        VkSampleCountFlagBits samples,
        VkFormat              viewFormat,
        VkColorSpaceKHR       colorSpace){
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkHudPipelinesKey key;
    key.samples    = samples;
    key.viewFormat = viewFormat;
    key.colorSpace = colorSpace;

    auto entry = m_pipelines.find(key);
    if (entry != m_pipelines.end())
      return entry->second;

    DxvkHudPipelines pipeline = this->createPipelines(key);
    m_pipelines.insert({ key, pipeline });
    return pipeline;
  }

  VkDescriptorSetLayout DxvkHudObjects::createTextDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 3> bindings = { VkDescriptorSetLayoutBinding { 0,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
      VkDescriptorSetLayoutBinding { 1,
        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT
      }, VkDescriptorSetLayoutBinding { 2,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT
    } };

    VkDescriptorSetLayoutCreateInfo setLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    setLayoutInfo.bindingCount           = bindings.size();
    setLayoutInfo.pBindings              = bindings.data();

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    if (m_vkd->vkCreateDescriptorSetLayout(m_vkd->device(), &setLayoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create descriptor set layout");

    return descriptorSetLayout;
  }

  VkPipelineLayout DxvkHudObjects::createTextPipelineLayout(VkDescriptorSetLayout descriptorSetLayout) {
    VkPushConstantRange pushRange = { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(HudTextPushConstants) };

    VkPipelineLayoutCreateInfo pipeLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeLayoutInfo.setLayoutCount         = 1;
    pipeLayoutInfo.pSetLayouts            = &descriptorSetLayout;
    pipeLayoutInfo.pushConstantRangeCount = 1;
    pipeLayoutInfo.pPushConstantRanges    = &pushRange;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    if (m_vkd->vkCreatePipelineLayout(m_vkd->device(), &pipeLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create pipeline layout");

    return pipelineLayout;
  }

  VkDescriptorSetLayout DxvkHudObjects::createGraphDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 3> bindings = { VkDescriptorSetLayoutBinding { 0,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT
    } };

    VkDescriptorSetLayoutCreateInfo setLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    setLayoutInfo.bindingCount           = bindings.size();
    setLayoutInfo.pBindings              = bindings.data();

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    if (m_vkd->vkCreateDescriptorSetLayout(m_vkd->device(), &setLayoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create descriptor set layout");

    return descriptorSetLayout;
  }

  VkPipelineLayout DxvkHudObjects::createGraphPipelineLayout(VkDescriptorSetLayout descriptorSetLayout) {
    VkPushConstantRange pushRange = { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(HudGraphPushConstants) };

    VkPipelineLayoutCreateInfo pipeLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeLayoutInfo.setLayoutCount         = 1;
    pipeLayoutInfo.pSetLayouts            = &descriptorSetLayout;
    pipeLayoutInfo.pushConstantRangeCount = 1;
    pipeLayoutInfo.pPushConstantRanges    = &pushRange;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    if (m_vkd->vkCreatePipelineLayout(m_vkd->device(), &pipeLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create pipeline layout");

    return pipelineLayout;
  }

  DxvkHudPipelines DxvkHudObjects::createPipelines(
    const DxvkHudPipelinesKey& key) {

    VkDescriptorSetLayout textDescSetLayout = this->createTextDescriptorSetLayout();
    VkPipelineLayout textPipeLayout = this->createTextPipelineLayout(textDescSetLayout);

    VkPipeline textPipeline = this->createPipeline(
      m_textVs,
      m_textFs,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      key.samples,
      key.viewFormat,
      key.colorSpace,
      textPipeLayout
    );

    VkDescriptorSetLayout graphDescSetLayout = this->createTextDescriptorSetLayout();
    VkPipelineLayout graphPipeLayout = this->createTextPipelineLayout(graphDescSetLayout);

    VkPipeline graphPipeline = this->createPipeline(
      m_graphVs,
      m_graphFs,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      key.samples,
      key.viewFormat,
      key.colorSpace,
      graphPipeLayout
    );

    DxvkHudPipelines result;
    result.textPipeHandle = textPipeline;
    result.textPipeLayout = textPipeLayout;
    result.textDSetLayout = textDescSetLayout;
    result.graphPipeHandle = graphPipeline;
    result.graphPipeLayout = graphPipeLayout;
    result.graphDSetLayout = graphDescSetLayout;
    return result;
  }


  VkPipeline DxvkHudObjects::createPipeline(
          VkShaderModule        vs,
          VkShaderModule        fs,
          VkPrimitiveTopology   topology,
          VkSampleCountFlagBits samples,
          VkFormat              viewFormat,
          VkColorSpaceKHR       colorSpace,
          VkPipelineLayout      pipeLayout) {

    VkSpecializationMapEntry specEntry = VkSpecializationMapEntry {
      0, 0, sizeof(colorSpace)
    };

    VkSpecializationInfo specInfo = {
      1, &specEntry, sizeof(colorSpace), &colorSpace
    };

    std::array<VkPipelineShaderStageCreateInfo, 2> stages;
    stages[0] = VkPipelineShaderStageCreateInfo {
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
      VK_SHADER_STAGE_VERTEX_BIT, vs, "main" };

    stages[1] = VkPipelineShaderStageCreateInfo {
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
      VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", &specInfo };

    std::array<VkDynamicState, 2> dynStates = {{
      VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
      VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
    }};

    VkPipelineDynamicStateCreateInfo dynState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynState.dynamicStateCount = dynStates.size();
    dynState.pDynamicStates    = dynStates.data();

    VkPipelineVertexInputStateCreateInfo viState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    VkPipelineInputAssemblyStateCreateInfo iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    iaState.topology               = topology;
    iaState.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo vpState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };

    VkPipelineRasterizationStateCreateInfo rsState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsState.polygonMode = VK_POLYGON_MODE_FILL;
    rsState.cullMode = VK_CULL_MODE_BACK_BIT;
    rsState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rsState.lineWidth = 1.0f;

    uint32_t msMask = 0xFFFFFFFF;
    VkPipelineMultisampleStateCreateInfo msState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msState.rasterizationSamples = samples;
    msState.pSampleMask = &msMask;

    VkPipelineColorBlendAttachmentState cbAttachment = { };
    cbAttachment.blendEnable         = VK_TRUE;
    cbAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    cbAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    cbAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cbAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    cbAttachment.colorWriteMask      =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cbState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbState.attachmentCount     = 1;
    cbState.pAttachments        = &cbAttachment;

    VkPipelineRenderingCreateInfo rtState = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    rtState.colorAttachmentCount = 1;
    rtState.pColorAttachmentFormats = &viewFormat;

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &rtState };
    info.stageCount             = 2;
    info.pStages                = stages.data();
    info.pVertexInputState      = &viState;
    info.pInputAssemblyState    = &iaState;
    info.pViewportState         = &vpState;
    info.pRasterizationState    = &rsState;
    info.pMultisampleState      = &msState;
    info.pColorBlendState       = &cbState;
    info.pDynamicState          = &dynState;
    info.layout                 = pipeLayout;
    info.basePipelineIndex      = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (m_vkd->vkCreateGraphicsPipelines(m_vkd->device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create graphics pipeline");
    return pipeline;
  }
}
