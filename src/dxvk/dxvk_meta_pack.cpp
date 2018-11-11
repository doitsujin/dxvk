#include "dxvk_meta_pack.h"

#include <dxvk_pack_d24s8.h>
#include <dxvk_pack_d32s8.h>

namespace dxvk {

  DxvkMetaPackObjects::DxvkMetaPackObjects(const Rc<vk::DeviceFn>& vkd)
  : m_vkd         (vkd),
    m_sampler     (createSampler()),
    m_dsetLayout  (createDescriptorSetLayout()),
    m_pipeLayout  (createPipelineLayout()),
    m_template    (createDescriptorUpdateTemplate()),
    m_pipeD24S8   (createPipeline(dxvk_pack_d24s8)),
    m_pipeD32S8   (createPipeline(dxvk_pack_d32s8)) {
    
  }


  DxvkMetaPackObjects::~DxvkMetaPackObjects() {
    m_vkd->vkDestroyPipeline(m_vkd->device(), m_pipeD32S8, nullptr);
    m_vkd->vkDestroyPipeline(m_vkd->device(), m_pipeD24S8, nullptr);
    
    m_vkd->vkDestroyDescriptorUpdateTemplateKHR(
      m_vkd->device(), m_template, nullptr);
    
    m_vkd->vkDestroyPipelineLayout(
      m_vkd->device(), m_pipeLayout, nullptr);
    
    m_vkd->vkDestroyDescriptorSetLayout(
      m_vkd->device(), m_dsetLayout, nullptr);
    
    m_vkd->vkDestroySampler(
      m_vkd->device(), m_sampler, nullptr);
  }


  DxvkMetaPackPipeline DxvkMetaPackObjects::getPipeline(VkFormat format) {
    DxvkMetaPackPipeline result;
    result.dsetTemplate = m_template;
    result.dsetLayout   = m_dsetLayout;
    result.pipeLayout   = m_pipeLayout;
    result.pipeHandle   = VK_NULL_HANDLE;

    switch (format) {
      case VK_FORMAT_D24_UNORM_S8_UINT:  result.pipeHandle = m_pipeD24S8; break;
      case VK_FORMAT_D32_SFLOAT_S8_UINT: result.pipeHandle = m_pipeD32S8; break;
      default: Logger::err(str::format("DxvkMetaPackObjects: Unknown format: ", format));
    }

    return result;
  }


  VkSampler DxvkMetaPackObjects::createSampler() {
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
      throw DxvkError("DxvkMetaPackObjects: Failed to create sampler");
    return result;
  }


  VkDescriptorSetLayout DxvkMetaPackObjects::createDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
      { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &m_sampler },
      { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &m_sampler },
    }};

    VkDescriptorSetLayoutCreateInfo dsetInfo;
    dsetInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsetInfo.pNext        = nullptr;
    dsetInfo.flags        = 0;
    dsetInfo.bindingCount = bindings.size();
    dsetInfo.pBindings    = bindings.data();

    VkDescriptorSetLayout result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateDescriptorSetLayout(m_vkd->device(), &dsetInfo, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaPackObjects: Failed to create descriptor set layout");
    return result;
  }


  VkPipelineLayout DxvkMetaPackObjects::createPipelineLayout() {
    VkPushConstantRange push;
    push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push.offset     = 0;
    push.size       = sizeof(DxvkMetaPackArgs);

    VkPipelineLayoutCreateInfo layoutInfo;
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext                  = nullptr;
    layoutInfo.flags                  = 0;
    layoutInfo.setLayoutCount         = 1;
    layoutInfo.pSetLayouts            = &m_dsetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &push;

    VkPipelineLayout result = VK_NULL_HANDLE;
    if (m_vkd->vkCreatePipelineLayout(m_vkd->device(), &layoutInfo, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaPackObjects: Failed to create pipeline layout");
    return result;
  }


  VkDescriptorUpdateTemplateKHR DxvkMetaPackObjects::createDescriptorUpdateTemplate() {
    std::array<VkDescriptorUpdateTemplateEntryKHR, 3> bindings = {{
      { 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         offsetof(DxvkMetaPackDescriptors, dstBuffer),  0 },
      { 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offsetof(DxvkMetaPackDescriptors, srcDepth),   0 },
      { 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offsetof(DxvkMetaPackDescriptors, srcStencil), 0 },
    }};

    VkDescriptorUpdateTemplateCreateInfoKHR templateInfo;
    templateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR;
    templateInfo.pNext = nullptr;
    templateInfo.flags = 0;
    templateInfo.descriptorUpdateEntryCount = bindings.size();
    templateInfo.pDescriptorUpdateEntries   = bindings.data();
    templateInfo.templateType               = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR;
    templateInfo.descriptorSetLayout        = m_dsetLayout;
    templateInfo.pipelineBindPoint          = VK_PIPELINE_BIND_POINT_COMPUTE;
    templateInfo.pipelineLayout             = m_pipeLayout;
    templateInfo.set                        = 0;

    VkDescriptorUpdateTemplateKHR result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateDescriptorUpdateTemplateKHR(m_vkd->device(),
          &templateInfo, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaPackObjects: Failed to create descriptor update template");
    return result;
  }


  VkPipeline DxvkMetaPackObjects::createPipeline(const SpirvCodeBuffer& code) {
    VkShaderModuleCreateInfo shaderInfo;
    shaderInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.pNext    = nullptr;
    shaderInfo.flags    = 0;
    shaderInfo.codeSize = code.size();
    shaderInfo.pCode    = code.data();

    VkShaderModule module = VK_NULL_HANDLE;

    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &shaderInfo, nullptr, &module) != VK_SUCCESS)
      throw DxvkError("DxvkMetaPackObjects: Failed to create shader module");
    
    VkPipelineShaderStageCreateInfo stageInfo;
    stageInfo.sType     = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext     = nullptr;
    stageInfo.flags     = 0;
    stageInfo.stage     = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module    = module;
    stageInfo.pName     = "main";
    stageInfo.pSpecializationInfo = nullptr;
    
    VkComputePipelineCreateInfo pipeInfo;
    pipeInfo.sType      = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeInfo.pNext      = nullptr;
    pipeInfo.flags      = 0;
    pipeInfo.stage      = stageInfo;
    pipeInfo.layout     = m_pipeLayout;
    pipeInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipeInfo.basePipelineIndex  = -1;

    VkPipeline result = VK_NULL_HANDLE;

    VkResult status = m_vkd->vkCreateComputePipelines(
      m_vkd->device(), VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &result);
    
    m_vkd->vkDestroyShaderModule(m_vkd->device(), module, nullptr);

    if (status != VK_SUCCESS)
      throw DxvkError("DxvkMetaPackObjects: Failed to create pipeline");
    return result;
  }
  
}