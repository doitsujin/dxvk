#include "dxvk_meta_pack.h"
#include "dxvk_device.h"

#include <dxvk_pack_d24s8.h>
#include <dxvk_pack_d32s8.h>

#include <dxvk_unpack_d24s8_as_d32s8.h>
#include <dxvk_unpack_d24s8.h>
#include <dxvk_unpack_d32s8.h>

namespace dxvk {

  DxvkMetaPackObjects::DxvkMetaPackObjects(const DxvkDevice* device)
  : m_vkd             (device->vkd()),
    m_sampler         (createSampler()),
    m_dsetLayoutPack  (createPackDescriptorSetLayout()),
    m_dsetLayoutUnpack(createUnpackDescriptorSetLayout()),
    m_pipeLayoutPack  (createPipelineLayout(m_dsetLayoutPack, sizeof(DxvkMetaPackArgs))),
    m_pipeLayoutUnpack(createPipelineLayout(m_dsetLayoutUnpack, sizeof(DxvkMetaPackArgs))),
    m_templatePack    (createPackDescriptorUpdateTemplate()),
    m_templateUnpack  (createUnpackDescriptorUpdateTemplate()),
    m_pipePackD24S8   (createPipeline(m_pipeLayoutPack, dxvk_pack_d24s8)),
    m_pipePackD32S8   (createPipeline(m_pipeLayoutPack, dxvk_pack_d32s8)),
    m_pipeUnpackD24S8AsD32S8(createPipeline(m_pipeLayoutUnpack, dxvk_unpack_d24s8_as_d32s8)),
    m_pipeUnpackD24S8 (createPipeline(m_pipeLayoutUnpack, dxvk_unpack_d24s8)),
    m_pipeUnpackD32S8 (createPipeline(m_pipeLayoutUnpack, dxvk_unpack_d32s8)) {
    
  }


  DxvkMetaPackObjects::~DxvkMetaPackObjects() {
    m_vkd->vkDestroyPipeline(m_vkd->device(), m_pipeUnpackD32S8, nullptr);
    m_vkd->vkDestroyPipeline(m_vkd->device(), m_pipeUnpackD24S8, nullptr);
    m_vkd->vkDestroyPipeline(m_vkd->device(), m_pipeUnpackD24S8AsD32S8, nullptr);

    m_vkd->vkDestroyPipeline(m_vkd->device(), m_pipePackD32S8, nullptr);
    m_vkd->vkDestroyPipeline(m_vkd->device(), m_pipePackD24S8, nullptr);
    
    m_vkd->vkDestroyDescriptorUpdateTemplate(m_vkd->device(), m_templatePack, nullptr);
    m_vkd->vkDestroyDescriptorUpdateTemplate(m_vkd->device(), m_templateUnpack, nullptr);
    
    m_vkd->vkDestroyPipelineLayout(m_vkd->device(), m_pipeLayoutPack, nullptr);
    m_vkd->vkDestroyPipelineLayout(m_vkd->device(), m_pipeLayoutUnpack, nullptr);
    
    m_vkd->vkDestroyDescriptorSetLayout(m_vkd->device(), m_dsetLayoutPack, nullptr);
    m_vkd->vkDestroyDescriptorSetLayout(m_vkd->device(), m_dsetLayoutUnpack, nullptr);
    
    m_vkd->vkDestroySampler(m_vkd->device(), m_sampler, nullptr);
  }


  DxvkMetaPackPipeline DxvkMetaPackObjects::getPackPipeline(VkFormat format) {
    DxvkMetaPackPipeline result;
    result.dsetTemplate = m_templatePack;
    result.dsetLayout   = m_dsetLayoutPack;
    result.pipeLayout   = m_pipeLayoutPack;
    result.pipeHandle   = VK_NULL_HANDLE;

    switch (format) {
      case VK_FORMAT_D24_UNORM_S8_UINT:  result.pipeHandle = m_pipePackD24S8; break;
      case VK_FORMAT_D32_SFLOAT_S8_UINT: result.pipeHandle = m_pipePackD32S8; break;
      default: Logger::err(str::format("DxvkMetaPackObjects: Unknown format: ", format));
    }

    return result;
  }


  DxvkMetaPackPipeline DxvkMetaPackObjects::getUnpackPipeline(
          VkFormat        dstFormat,
          VkFormat        srcFormat) {
    DxvkMetaPackPipeline result;
    result.dsetTemplate = m_templateUnpack;
    result.dsetLayout   = m_dsetLayoutUnpack;
    result.pipeLayout   = m_pipeLayoutUnpack;
    result.pipeHandle   = VK_NULL_HANDLE;

    std::array<std::tuple<VkFormat, VkFormat, VkPipeline>, 3> pipeSelector = {{
      { VK_FORMAT_D24_UNORM_S8_UINT,  VK_FORMAT_D24_UNORM_S8_UINT,  m_pipeUnpackD24S8 },
      { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,  m_pipeUnpackD24S8AsD32S8 },
      { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, m_pipeUnpackD32S8 },
    }};

    for (const auto& e : pipeSelector) {
      if (std::get<0>(e) == dstFormat
       && std::get<1>(e) == srcFormat)
        result.pipeHandle = std::get<2>(e);
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


  VkDescriptorSetLayout DxvkMetaPackObjects::createPackDescriptorSetLayout() {
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


  VkDescriptorSetLayout DxvkMetaPackObjects::createUnpackDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
      { 1, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
      { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,       1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
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


  VkPipelineLayout DxvkMetaPackObjects::createPipelineLayout(
          VkDescriptorSetLayout       dsetLayout,
          size_t                      pushLayout) {
    VkPushConstantRange push;
    push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push.offset     = 0;
    push.size       = pushLayout;

    VkPipelineLayoutCreateInfo layoutInfo;
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext                  = nullptr;
    layoutInfo.flags                  = 0;
    layoutInfo.setLayoutCount         = 1;
    layoutInfo.pSetLayouts            = &dsetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &push;

    VkPipelineLayout result = VK_NULL_HANDLE;
    if (m_vkd->vkCreatePipelineLayout(m_vkd->device(), &layoutInfo, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaPackObjects: Failed to create pipeline layout");
    return result;
  }


  VkDescriptorUpdateTemplate DxvkMetaPackObjects::createPackDescriptorUpdateTemplate() {
    std::array<VkDescriptorUpdateTemplateEntry, 3> bindings = {{
      { 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         offsetof(DxvkMetaPackDescriptors, dstBuffer),  0 },
      { 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offsetof(DxvkMetaPackDescriptors, srcDepth),   0 },
      { 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, offsetof(DxvkMetaPackDescriptors, srcStencil), 0 },
    }};

    VkDescriptorUpdateTemplateCreateInfo templateInfo;
    templateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
    templateInfo.pNext = nullptr;
    templateInfo.flags = 0;
    templateInfo.descriptorUpdateEntryCount = bindings.size();
    templateInfo.pDescriptorUpdateEntries   = bindings.data();
    templateInfo.templateType               = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
    templateInfo.descriptorSetLayout        = m_dsetLayoutPack;
    templateInfo.pipelineBindPoint          = VK_PIPELINE_BIND_POINT_COMPUTE;
    templateInfo.pipelineLayout             = m_pipeLayoutPack;
    templateInfo.set                        = 0;

    VkDescriptorUpdateTemplate result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateDescriptorUpdateTemplate(m_vkd->device(),
          &templateInfo, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaPackObjects: Failed to create descriptor update template");
    return result;
  }


  VkDescriptorUpdateTemplate DxvkMetaPackObjects::createUnpackDescriptorUpdateTemplate() {
    std::array<VkDescriptorUpdateTemplateEntry, 3> bindings = {{
      { 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, offsetof(DxvkMetaUnpackDescriptors, dstDepth),   0 },
      { 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, offsetof(DxvkMetaUnpackDescriptors, dstStencil), 0 },
      { 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,       offsetof(DxvkMetaUnpackDescriptors, srcBuffer),  0 },
    }};

    VkDescriptorUpdateTemplateCreateInfo templateInfo;
    templateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
    templateInfo.pNext = nullptr;
    templateInfo.flags = 0;
    templateInfo.descriptorUpdateEntryCount = bindings.size();
    templateInfo.pDescriptorUpdateEntries   = bindings.data();
    templateInfo.templateType               = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
    templateInfo.descriptorSetLayout        = m_dsetLayoutUnpack;
    templateInfo.pipelineBindPoint          = VK_PIPELINE_BIND_POINT_COMPUTE;
    templateInfo.pipelineLayout             = m_pipeLayoutUnpack;
    templateInfo.set                        = 0;

    VkDescriptorUpdateTemplate result = VK_NULL_HANDLE;
    if (m_vkd->vkCreateDescriptorUpdateTemplate(m_vkd->device(),
          &templateInfo, nullptr, &result) != VK_SUCCESS)
      throw DxvkError("DxvkMetaPackObjects: Failed to create descriptor update template");
    return result;
  }


  VkPipeline DxvkMetaPackObjects::createPipeline(
          VkPipelineLayout      pipeLayout,
    const SpirvCodeBuffer&      code) {
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
    pipeInfo.layout     = pipeLayout;
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