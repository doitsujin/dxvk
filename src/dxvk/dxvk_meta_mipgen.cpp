#include "dxvk_device.h"
#include "dxvk_meta_mipgen.h"

#include <dxvk_mipgen_4.h>
#include <dxvk_mipgen_8.h>
#include <dxvk_mipgen_16.h>

namespace dxvk {

  DxvkMetaMipGenViews::DxvkMetaMipGenViews(
    const Rc<DxvkImageView>&  view)
  : m_view(view) {
    // Determine view type based on image type
    const std::array<std::pair<VkImageViewType, VkImageViewType>, 3> viewTypes = {{
      { VK_IMAGE_VIEW_TYPE_1D_ARRAY, VK_IMAGE_VIEW_TYPE_1D_ARRAY },
      { VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_VIEW_TYPE_2D_ARRAY },
      { VK_IMAGE_VIEW_TYPE_3D,       VK_IMAGE_VIEW_TYPE_2D_ARRAY },
    }};
    
    m_srcViewType = viewTypes.at(uint32_t(view->image()->info().type)).first;
    m_dstViewType = viewTypes.at(uint32_t(view->image()->info().type)).second;
    
    // Create image views and framebuffers
    m_passes.resize(view->info().mipCount - 1);
    
    for (uint32_t i = 0; i < m_passes.size(); i++)
      m_passes[i] = createViews(i);
  }
  
  
  DxvkMetaMipGenViews::~DxvkMetaMipGenViews() {

  }
  
  
  VkExtent3D DxvkMetaMipGenViews::computePassExtent(uint32_t passId) const {
    VkExtent3D extent = m_view->mipLevelExtent(passId + 1);
    
    if (m_view->image()->info().type != VK_IMAGE_TYPE_3D)
      extent.depth = m_view->info().layerCount;
    
    return extent;
  }
  
  
  DxvkMetaMipGenViews::PassViews DxvkMetaMipGenViews::createViews(uint32_t pass) const {
    PassViews result = { };

    // Source image view
    DxvkImageViewKey srcViewInfo;
    srcViewInfo.viewType = m_srcViewType;
    srcViewInfo.format = m_view->info().format;
    srcViewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    srcViewInfo.aspects = m_view->info().aspects;
    srcViewInfo.mipIndex = m_view->info().mipIndex + pass;
    srcViewInfo.mipCount = 1;
    srcViewInfo.layerIndex = m_view->info().layerIndex;
    srcViewInfo.layerCount = m_view->info().layerCount;

    result.src = m_view->image()->createView(srcViewInfo);
    
    // Create destination image view, which points
    // to the mip level we're going to render to.
    VkExtent3D dstExtent = m_view->mipLevelExtent(pass + 1);
    
    DxvkImageViewKey dstViewInfo;
    dstViewInfo.viewType = m_dstViewType;
    dstViewInfo.format = m_view->info().format;
    dstViewInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    dstViewInfo.aspects = m_view->info().aspects;
    dstViewInfo.mipIndex = m_view->info().mipIndex + pass + 1;
    dstViewInfo.mipCount = 1u;
    
    if (m_view->image()->info().type != VK_IMAGE_TYPE_3D) {
      dstViewInfo.layerIndex = m_view->info().layerIndex;
      dstViewInfo.layerCount = m_view->info().layerCount;
    } else {
      dstViewInfo.layerIndex = 0;
      dstViewInfo.layerCount = dstExtent.depth;
    }

    result.dst = m_view->image()->createView(dstViewInfo);

    return result;
  }




  DxvkMetaMipGenObjects::DxvkMetaMipGenObjects(DxvkDevice* device)
  : m_device(device) {
    createSetLayout();
    createPipelineLayout();
  }


  DxvkMetaMipGenObjects::~DxvkMetaMipGenObjects() {
    auto vk = m_device->vkd();

    for (const auto& p : m_pipelines)
      vk->vkDestroyPipeline(vk->device(), p.second.pipeline, nullptr);

    vk->vkDestroyPipelineLayout(vk->device(), m_pipelineLayout, nullptr);
    vk->vkDestroyDescriptorSetLayout(vk->device(), m_setLayout, nullptr);
  }


  VkFormat DxvkMetaMipGenObjects::getNonSrgbFormat(VkFormat format) const {
    static const std::array<std::pair<VkFormat, VkFormat>, 2> s_formats = {{
      { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM },
      { VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM },
    }};

    for (const auto& p : s_formats) {
      if (p.first == format)
        return p.second;
    }

    return format;
  }


  bool DxvkMetaMipGenObjects::supportsFormat(VkFormat format, VkImageTiling tiling) const {
    static const std::array<VkFormat, 32> s_formats = {{
      VK_FORMAT_A4R4G4B4_UNORM_PACK16,
      VK_FORMAT_A4B4G4R4_UNORM_PACK16,
      VK_FORMAT_R4G4B4A4_UNORM_PACK16,
      VK_FORMAT_B4G4R4A4_UNORM_PACK16,
      VK_FORMAT_A8_UNORM_KHR,
      VK_FORMAT_R8_UNORM,
      VK_FORMAT_R8_SNORM,
      VK_FORMAT_R8G8_UNORM,
      VK_FORMAT_R8G8_SNORM,
      VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_R8G8B8A8_SNORM,
      VK_FORMAT_R8G8B8A8_SRGB,
      VK_FORMAT_B8G8R8A8_UNORM,
      VK_FORMAT_B8G8R8A8_SNORM,
      VK_FORMAT_B8G8R8A8_SRGB,
      VK_FORMAT_A2B10G10R10_UNORM_PACK32,
      VK_FORMAT_A2B10G10R10_SNORM_PACK32,
      VK_FORMAT_A2R10G10B10_UNORM_PACK32,
      VK_FORMAT_A2R10G10B10_SNORM_PACK32,
      VK_FORMAT_B10G11R11_UFLOAT_PACK32,
      VK_FORMAT_R16_UNORM,
      VK_FORMAT_R16_SNORM,
      VK_FORMAT_R16_SFLOAT,
      VK_FORMAT_R16G16_UNORM,
      VK_FORMAT_R16G16_SNORM,
      VK_FORMAT_R16G16_SFLOAT,
      VK_FORMAT_R16G16B16A16_UNORM,
      VK_FORMAT_R16G16B16A16_SNORM,
      VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_FORMAT_R32_SFLOAT,
      VK_FORMAT_R32G32_SFLOAT,
      VK_FORMAT_R32G32B32A32_SFLOAT,
    }};

    // Check whether all relevant device features are enabled
    auto& vk11 = m_device->properties().vk11;
    auto& vk13 = m_device->features().vk13;

    if (!vk13.computeFullSubgroups || !vk13.subgroupSizeControl)
      return false;

    if (!(vk11.subgroupSupportedStages & VK_SHADER_STAGE_COMPUTE_BIT))
      return false;

    constexpr VkSubgroupFeatureFlags SubgroupFeatures =
      VK_SUBGROUP_FEATURE_BASIC_BIT |
      VK_SUBGROUP_FEATURE_BALLOT_BIT |
      VK_SUBGROUP_FEATURE_SHUFFLE_BIT;

    if ((vk11.subgroupSupportedOperations & SubgroupFeatures) != SubgroupFeatures)
      return false;

    // Scan the format list. The shader needs to be aware of
    // the exact format being used, hence the limitation.
    bool compatible = false;

    for (auto f : s_formats) {
      if ((compatible = (f == format)))
        break;
    }

    if (!compatible)
      return false;

    auto formatFeatures = m_device->getFormatFeatures(format);

    VkFormatFeatureFlags2 formatFlags = tiling == VK_IMAGE_TILING_OPTIMAL
      ? formatFeatures.optimal
      : formatFeatures.linear;

    VkFormatFeatureFlags2 requiredFlags =
      VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT |
      VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT |
      VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT |
      VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT;

    return (formatFlags & requiredFlags) == requiredFlags;
  }


  DxvkMetaMipGenPipeline DxvkMetaMipGenObjects::getPipeline(VkFormat format) {
    std::lock_guard lock(m_mutex);

    auto entry = m_pipelines.find(format);

    if (entry != m_pipelines.end())
      return entry->second;

    DxvkMetaMipGenPipeline result = createPipeline(format);
    m_pipelines.insert({ format, result });
    return result;
  }


  void DxvkMetaMipGenObjects::createSetLayout() {
    constexpr static std::array<VkDescriptorSetLayoutBinding, 3> bindings = {{
      { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  MaxDstDescriptors, VK_SHADER_STAGE_COMPUTE_BIT },
      { 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  1,                 VK_SHADER_STAGE_COMPUTE_BIT },
      { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,                 VK_SHADER_STAGE_COMPUTE_BIT },
    }};

    auto vk = m_device->vkd();

    VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.bindingCount = bindings.size();
    info.pBindings = bindings.data();

    VkResult vr = vk->vkCreateDescriptorSetLayout(vk->device(), &info, nullptr, &m_setLayout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("DxvkMetaMipGenObjects: Failed to create descriptor set layout: ", vr));
  }


  void DxvkMetaMipGenObjects::createPipelineLayout() {
    auto vk = m_device->vkd();

    VkPushConstantRange pushConstants = { };
    pushConstants.size = sizeof(DxvkMetaMipGenArgs);
    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    info.setLayoutCount = 1u;
    info.pSetLayouts = &m_setLayout;
    info.pushConstantRangeCount = 1u;
    info.pPushConstantRanges = &pushConstants;

    VkResult vr = vk->vkCreatePipelineLayout(vk->device(), &info, nullptr, &m_pipelineLayout);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("DxvkMetaMipGenObjects: Failed to create pipeline layout: ", vr));
  }


  DxvkMetaMipGenPipeline DxvkMetaMipGenObjects::createPipeline(VkFormat format) {
    auto vk = m_device->vkd();

    SpecConstants specConstants = { };
    specConstants.format = format;

    VkShaderModuleCreateInfo moduleInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };

    switch (lookupFormatInfo(format)->elementSize) {
      case 1:
      case 2:
      case 4:
        moduleInfo.codeSize = sizeof(dxvk_mipgen_4);
        moduleInfo.pCode = dxvk_mipgen_4;

        specConstants.mipsPerPass = 6u;
        break;

      case 8:
        moduleInfo.codeSize = sizeof(dxvk_mipgen_8);
        moduleInfo.pCode = dxvk_mipgen_8;

        specConstants.mipsPerPass = 5u;
        break;

      case 16:
        moduleInfo.codeSize = sizeof(dxvk_mipgen_16);
        moduleInfo.pCode = dxvk_mipgen_16;

        specConstants.mipsPerPass = 5u;
        break;

      default:
        Logger::err(str::format("DxvkMetaMipGenObjects: Unsupported format: ", format));
        return { };
    }

    std::array<VkSpecializationMapEntry, 2> specMap = {{
      { 0, offsetof(SpecConstants, format), sizeof(VkFormat) },
      { 1, offsetof(SpecConstants, mipsPerPass), sizeof(uint32_t) },
    }};

    VkSpecializationInfo specInfo = { };
    specInfo.mapEntryCount = specMap.size();
    specInfo.pMapEntries = specMap.data();
    specInfo.dataSize = sizeof(specConstants);
    specInfo.pData = &specConstants;

    VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage.flags = VK_PIPELINE_SHADER_STAGE_CREATE_ALLOW_VARYING_SUBGROUP_SIZE_BIT
                     | VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT;
    info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.pName = "main";
    info.stage.pSpecializationInfo = &specInfo;
    info.layout = m_pipelineLayout;
    info.basePipelineIndex = -1;

    VkResult vr = vk->vkCreateShaderModule(vk->device(), &moduleInfo, nullptr, &info.stage.module);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("DxvkMetaMipGenObjects: Failed to create shader module: ", vr));

    DxvkMetaMipGenPipeline result = { };
    result.setLayout = m_setLayout;
    result.pipelineLayout = m_pipelineLayout;
    result.mipsPerPass = specConstants.mipsPerPass;

    vr = vk->vkCreateComputePipelines(vk->device(),
      VK_NULL_HANDLE, 1, &info, nullptr, &result.pipeline);

    vk->vkDestroyShaderModule(vk->device(), info.stage.module, nullptr);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("DxvkMetaMipGenObjects: Failed to create pipeline: ", vr));

    return result;
  }

}
