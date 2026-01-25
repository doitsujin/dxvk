#include "dxvk_device.h"
#include "dxvk_meta_mipgen.h"

#include <dxvk_mipgen.h>

namespace dxvk {

  DxvkMetaMipGenViews::DxvkMetaMipGenViews(
    const Rc<DxvkImageView>&  view,
          VkPipelineBindPoint bindPoint)
  : m_view(view), m_bindPoint(bindPoint) {
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

    if (m_bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
      dstViewInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT;
      dstViewInfo.layout = VK_IMAGE_LAYOUT_GENERAL;
    }

    result.dst = m_view->image()->createView(dstViewInfo);

    return result;
  }
  

  DxvkMetaMipGenObjects::DxvkMetaMipGenObjects(DxvkDevice* device)
  : m_device(device), m_layout(createPipelineLayout()) {

  }


  DxvkMetaMipGenObjects::~DxvkMetaMipGenObjects() {

  }


  bool DxvkMetaMipGenObjects::checkFormatSupport(
          VkFormat              viewFormat) {
    std::lock_guard lock(m_mutex);
    auto entry = m_formatSupport.find(viewFormat);

    if (entry != m_formatSupport.end())
      return entry->second;

    bool support = queryFormatSupport(viewFormat);
    m_formatSupport.insert({ viewFormat, support });

    return support;
  }


  DxvkMetaMipGenPipeline DxvkMetaMipGenObjects::getPipeline(
          VkFormat              viewFormat) {
    std::lock_guard lock(m_mutex);

    auto entry = m_pipelines.find(viewFormat);

    if (entry != m_pipelines.end())
      return entry->second;

    DxvkMetaMipGenPipeline pipeline = createPipeline(viewFormat);
    m_pipelines.insert({ viewFormat, pipeline });
    return pipeline;
  }


  const DxvkPipelineLayout* DxvkMetaMipGenObjects::createPipelineLayout() const {
    std::array<DxvkDescriptorSetLayoutBinding, 2> bindings = {{
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1u,                  VK_SHADER_STAGE_COMPUTE_BIT },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MipCount + MipCount, VK_SHADER_STAGE_COMPUTE_BIT },
    }};

    return m_device->createBuiltInPipelineLayout(DxvkPipelineLayoutFlag::UsesSamplerHeap,
      VK_SHADER_STAGE_COMPUTE_BIT, sizeof(DxvkMetaMipGenPushConstants), bindings.size(), bindings.data());
  }


  DxvkMetaMipGenPipeline DxvkMetaMipGenObjects::createPipeline(VkFormat format) const {
    auto formatInfo = lookupFormatInfo(format);

    const std::array<VkSpecializationMapEntry, 2u> specMap = {{
      { 0u, offsetof(DxvkMetaMipGenSpecConstants, format),          sizeof(VkFormat) },
      { 1u, offsetof(DxvkMetaMipGenSpecConstants, formatDwords),    sizeof(uint32_t) },
    }};

    DxvkMetaMipGenSpecConstants specConstants = { };
    specConstants.format = format;
    specConstants.formatDwords = std::max<uint32_t>(1u,
      formatInfo->elementSize / sizeof(uint32_t));

    VkSpecializationInfo specInfo = { };
    specInfo.mapEntryCount = specMap.size();
    specInfo.pMapEntries = specMap.data();
    specInfo.dataSize = sizeof(specConstants);
    specInfo.pData = &specConstants;

    util::DxvkBuiltInShaderStage shader(dxvk_mipgen, &specInfo);

    DxvkMetaMipGenPipeline pipeline = { };
    pipeline.layout = m_layout;
    pipeline.mipsPerStep = MipCount;
    pipeline.pipeline = m_device->createBuiltInComputePipeline(m_layout, shader);

    return pipeline;
  }


  bool DxvkMetaMipGenObjects::queryFormatSupport(
          VkFormat              viewFormat) const {
    // Fixed list of formats that the shader understands
    static const std::array<VkFormat, 26> s_formats = {{
      VK_FORMAT_R8_UNORM,
      VK_FORMAT_R8_SNORM,
      VK_FORMAT_R8G8_UNORM,
      VK_FORMAT_R8G8_SNORM,
      VK_FORMAT_R16_SFLOAT,
      VK_FORMAT_R16G16_SFLOAT,
      VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_B8G8R8A8_UNORM,
      VK_FORMAT_A8B8G8R8_UNORM_PACK32,
      VK_FORMAT_R8G8B8A8_SNORM,
      VK_FORMAT_B8G8R8A8_SNORM,
      VK_FORMAT_A8B8G8R8_SNORM_PACK32,
      VK_FORMAT_A2R10G10B10_UNORM_PACK32,
      VK_FORMAT_A2B10G10R10_UNORM_PACK32,
      VK_FORMAT_A2R10G10B10_SNORM_PACK32,
      VK_FORMAT_A2B10G10R10_SNORM_PACK32,
      VK_FORMAT_B10G11R11_UFLOAT_PACK32,
      VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_FORMAT_R16_UNORM,
      VK_FORMAT_R16_SNORM,
      VK_FORMAT_R32_SFLOAT,
      VK_FORMAT_R16G16_UNORM,
      VK_FORMAT_R16G16_SNORM,
      VK_FORMAT_R32G32_SFLOAT,
      VK_FORMAT_R16G16B16A16_UNORM,
      VK_FORMAT_R16G16B16A16_SNORM,
    }};

    if (!m_device->perfHints().preferComputeMipGen)
      return false;

    // Check whether the shader actually supports the format in question
    if (std::find(s_formats.begin(), s_formats.end(), viewFormat) == s_formats.end())
      return false;

    // The shader has some feature requirements that aren't otherwise
    // needed to run DXVK, make sure everything is supported.
    if (!m_device->features().vk12.shaderInt8
     || !m_device->features().vk12.shaderFloat16)
      return false;

    // Ensure that the format can support the required usage patterns
    auto formatFeatures = m_device->adapter()->getFormatFeatures(viewFormat);

    if (!(formatFeatures.optimal & VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT)
     || !(formatFeatures.optimal & VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
      return false;

    return true;
  }

}
