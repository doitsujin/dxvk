#include "dxvk_meta_present_blit.h"

#include <dxvk_present_frag.h>
#include <dxvk_present_frag_blit.h>
#include <dxvk_present_frag_ms.h>
#include <dxvk_present_frag_ms_amd.h>
#include <dxvk_present_vert.h>

#include "dxvk_device.h"

namespace dxvk {

  DxvkMetaPresentBlitObjects::DxvkMetaPresentBlitObjects(const DxvkDevice* device)
    : m_vkd(device->vkd()) {
    this->createShaders(device);
    this->createSamplers();
  }

  DxvkMetaPresentBlitObjects::~DxvkMetaPresentBlitObjects() {
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_vs, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_fsCopy, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_fsBlit, nullptr);
    m_vkd->vkDestroyShaderModule(m_vkd->device(), m_fsResolve, nullptr);
  }

  void DxvkMetaPresentBlitObjects::createShaders(const DxvkDevice* device) {
    SpirvCodeBuffer vsCode(dxvk_present_vert);
    SpirvCodeBuffer fsCodeBlit(dxvk_present_frag_blit);
    SpirvCodeBuffer fsCodeCopy(dxvk_present_frag);
    SpirvCodeBuffer fsCodeResolve(dxvk_present_frag_ms);
    SpirvCodeBuffer fsCodeResolveAmd(dxvk_present_frag_ms_amd);

    VkShaderModuleCreateInfo info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
      info.codeSize               = vsCode.size();
      info.pCode                  = vsCode.data();

    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &info, nullptr, &m_vs) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create shader module");

    info.pCode                 = fsCodeCopy.data();
    info.codeSize              = fsCodeCopy.size();

    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &info, nullptr, &m_fsCopy) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create shader module");

    info.pCode                 = fsCodeBlit.data();
    info.codeSize              = fsCodeBlit.size();

    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &info, nullptr, &m_fsBlit) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create shader module");

    info.pCode                 = device->features().amdShaderFragmentMask ? fsCodeResolveAmd.data() : fsCodeResolve.data();
    info.codeSize              = device->features().amdShaderFragmentMask ? fsCodeResolveAmd.size() : fsCodeResolve.size();

    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &info, nullptr, &m_fsResolve) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create shader module");
  }

  void DxvkMetaPresentBlitObjects::createSamplers() {
    VkSamplerCreateInfo info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    info.magFilter              = VK_FILTER_LINEAR;
    info.minFilter              = VK_FILTER_LINEAR;
    info.mipmapMode             = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressModeU           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.borderColor            = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

    if (m_vkd->vkCreateSampler(m_vkd->device(), &info, nullptr, &m_gammaSampler) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create sampler");


    info.addressModeU           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW           = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    if (m_vkd->vkCreateSampler(m_vkd->device(), &info, nullptr, &m_srcSampler) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create sampler");
  }

  DxvkMetaPresentBlitPipeline DxvkMetaPresentBlitObjects::getPipeline(
          DxvkPresentBlitFsType   fs,
          VkSampleCountFlagBits   srcSamples,
          VkSampleCountFlagBits   dstSamples,
          VkFormat                viewFormat,
          bool                    hasGammaView) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkMetaPresentBlitPipelineKey key;
    key.fs           = fs;
    key.srcSamples   = srcSamples;
    key.dstSamples   = dstSamples;
    key.viewFormat   = viewFormat;
    key.hasGammaView = hasGammaView;

    auto entry = m_pipelines.find(key);
    if (entry != m_pipelines.end())
      return entry->second;

    DxvkMetaPresentBlitPipeline pipeline = this->createPipeline(key);
    m_pipelines.insert({ key, pipeline });
    return pipeline;
  }

  VkDescriptorSetLayout DxvkMetaPresentBlitObjects::createDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings = { VkDescriptorSetLayoutBinding { BindingIds::Image,
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
      VkDescriptorSetLayoutBinding { BindingIds::Gamma,
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

  VkPipelineLayout DxvkMetaPresentBlitObjects::createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout) {
    VkPushConstantRange pushRange = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PresenterArgs) };

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

  DxvkMetaPresentBlitPipeline DxvkMetaPresentBlitObjects::createPipeline(
    const DxvkMetaPresentBlitPipelineKey& key) {

    VkDescriptorSetLayout descSetLayout = this->createDescriptorSetLayout();

    VkPipelineLayout pipeLayout = this->createPipelineLayout(descSetLayout);

    VkPipeline pipeline = this->createPipeline(
      key.fs,
      key.srcSamples,
      key.dstSamples,
      key.viewFormat,
      key.hasGammaView,
      pipeLayout
    );

    DxvkMetaPresentBlitPipeline result;
    result.pipeHandle = pipeline;
    result.pipeLayout = pipeLayout;
    result.dsetLayout = descSetLayout;
    return result;
  }


  VkPipeline DxvkMetaPresentBlitObjects::createPipeline(
          DxvkPresentBlitFsType   fsType,
          VkSampleCountFlagBits   srcSamples,
          VkSampleCountFlagBits   dstSamples,
          VkFormat                viewFormat,
          bool                    hasGammaView,
          VkPipelineLayout        pipeLayout) {

    std::array<VkSpecializationMapEntry, 2> specMap = { VkSpecializationMapEntry {
        0, 0, 4
      }, VkSpecializationMapEntry {
        1, 4, 4
      }
    };

    std::array<uint32_t, 2> data = {
      srcSamples, hasGammaView ? 1u : 0u
    };

    VkSpecializationInfo specInfo = {
      specMap.size(), specMap.data(), 8, data.data()
    };

    std::array<VkPipelineShaderStageCreateInfo, 2> stages;
    stages[0] = VkPipelineShaderStageCreateInfo {
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
      VK_SHADER_STAGE_VERTEX_BIT, m_vs, "main" };

    VkShaderModule fs;
    switch (fsType) {
      case dxvk::DxvkPresentBlitFsType::Copy:       fs = m_fsCopy;       break;
      case dxvk::DxvkPresentBlitFsType::Blit:       fs = m_fsBlit;       break;
      case dxvk::DxvkPresentBlitFsType::Resolve:    fs = m_fsResolve;    break;
      default: throw DxvkError("DxvkPresentBlitter: Invalid FS type");
    }

    stages[1] = VkPipelineShaderStageCreateInfo {
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
      VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", &specInfo };

    std::array<VkDynamicState, 2> dynStates = {{
      VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
      VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
    }};

    VkPipelineDynamicStateCreateInfo dynState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynState.dynamicStateCount = dynStates.size();
    dynState.pDynamicStates = dynStates.data();

    VkPipelineVertexInputStateCreateInfo viState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    VkPipelineInputAssemblyStateCreateInfo iaState = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    iaState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    iaState.primitiveRestartEnable  = VK_FALSE;

    VkPipelineViewportStateCreateInfo vpState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };

    VkPipelineRasterizationStateCreateInfo rsState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsState.polygonMode = VK_POLYGON_MODE_FILL;
    rsState.cullMode = VK_CULL_MODE_NONE;
    rsState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.lineWidth = 1.0f;

    uint32_t msMask = 0xFFFFFFFF;
    VkPipelineMultisampleStateCreateInfo msState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msState.rasterizationSamples = dstSamples;
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
    rtState.pColorAttachmentFormats = &viewFormat;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = { VkDescriptorSetLayoutBinding { BindingIds::Image,
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
      VkDescriptorSetLayoutBinding { BindingIds::Gamma,
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT
    } };

    VkDescriptorSetLayoutCreateInfo setLayoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    setLayoutInfo.bindingCount           = bindings.size();
    setLayoutInfo.pBindings              = bindings.data();

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    if (m_vkd->vkCreateDescriptorSetLayout(m_vkd->device(), &setLayoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create descriptor set layout");

    VkPushConstantRange pushRange = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PresenterArgs) };

    VkPipelineLayoutCreateInfo pipeLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipeLayoutInfo.setLayoutCount         = 1;
    pipeLayoutInfo.pSetLayouts            = &descriptorSetLayout;
    pipeLayoutInfo.pushConstantRangeCount = 1;
    pipeLayoutInfo.pPushConstantRanges    = &pushRange;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    if (m_vkd->vkCreatePipelineLayout(m_vkd->device(), &pipeLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create pipeline layout");

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
    info.layout                 = pipelineLayout;
    info.basePipelineIndex      = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (m_vkd->vkCreateGraphicsPipelines(m_vkd->device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
      throw DxvkError("DxvkMetaBlitObjects: Failed to create graphics pipeline");
    return pipeline;
  }

  Rc<DxvkImageView> DxvkMetaPresentBlitObjects::createResolveImage(const Rc<DxvkDevice>& device, const DxvkImageCreateInfo& info) {
    DxvkImageCreateInfo newInfo;
    newInfo.type = VK_IMAGE_TYPE_2D;
    newInfo.format = info.format;
    newInfo.flags = 0;
    newInfo.sampleCount = VK_SAMPLE_COUNT_1_BIT;
    newInfo.extent = info.extent;
    newInfo.numLayers = 1;
    newInfo.mipLevels = 1;
    newInfo.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                   | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                   | VK_IMAGE_USAGE_SAMPLED_BIT;
    newInfo.stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                   | VK_PIPELINE_STAGE_TRANSFER_BIT
                   | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    newInfo.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                   | VK_ACCESS_TRANSFER_WRITE_BIT
                   | VK_ACCESS_SHADER_READ_BIT;
    newInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    newInfo.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Rc<DxvkImage> image = device->createImage(newInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    DxvkImageViewCreateInfo viewInfo;
    viewInfo.type = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = info.format;
    viewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    viewInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.minLevel  = 0;
    viewInfo.numLevels = 1;
    viewInfo.minLayer  = 0;
    viewInfo.numLayers = 1;
    return device->createImageView(image, viewInfo);
  }
}
