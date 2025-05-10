#include "dxvk_device.h"
#include "dxvk_meta_blit.h"
#include "dxvk_util.h"

#include <dxvk_fullscreen_geom.h>
#include <dxvk_fullscreen_vert.h>
#include <dxvk_fullscreen_layer_vert.h>

#include <dxvk_blit_frag_1d.h>
#include <dxvk_blit_frag_2d.h>
#include <dxvk_blit_frag_3d.h>

namespace dxvk {
  
  DxvkMetaBlitObjects::DxvkMetaBlitObjects(DxvkDevice* device)
  : m_device(device),
    m_layout(createPipelineLayout()){

  }


  DxvkMetaBlitObjects::~DxvkMetaBlitObjects() {
    auto vk = m_device->vkd();

    for (const auto& pair : m_pipelines)
      vk->vkDestroyPipeline(vk->device(), pair.second.pipeline, nullptr);
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


  DxvkMetaBlitPipeline DxvkMetaBlitObjects::createPipeline(
    const DxvkMetaBlitPipelineKey& key) {
    DxvkMetaBlitPipeline pipeline = { };
    pipeline.layout   = m_layout;
    pipeline.pipeline = createPipeline(key.viewType, key.viewFormat, key.samples);
    return pipeline;
  }
  
  
  const DxvkPipelineLayout* DxvkMetaBlitObjects::createPipelineLayout() const {
    DxvkDescriptorSetLayoutBinding binding = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT };

    return m_device->createBuiltInPipelineLayout(VK_SHADER_STAGE_FRAGMENT_BIT,
      sizeof(DxvkMetaBlitPushConstants), 1, &binding);
  }


  VkPipeline DxvkMetaBlitObjects::createPipeline(
          VkImageViewType             imageViewType,
          VkFormat                    format,
          VkSampleCountFlagBits       samples) const {
    util::DxvkBuiltInGraphicsState state = { };

    if (m_device->features().vk12.shaderOutputLayer) {
      state.vs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_layer_vert, nullptr);
    } else {
      state.vs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_vert, nullptr);
      state.gs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_geom, nullptr);
    }

    switch (imageViewType) {
      case VK_IMAGE_VIEW_TYPE_1D_ARRAY: state.fs = util::DxvkBuiltInShaderStage(dxvk_blit_frag_1d, nullptr); break;
      case VK_IMAGE_VIEW_TYPE_2D_ARRAY: state.fs = util::DxvkBuiltInShaderStage(dxvk_blit_frag_2d, nullptr); break;
      case VK_IMAGE_VIEW_TYPE_3D:       state.fs = util::DxvkBuiltInShaderStage(dxvk_blit_frag_3d, nullptr); break;
      default: throw DxvkError("DxvkMetaBlitObjects: Invalid view type");
    }

    state.colorFormat = format;
    state.sampleCount = samples;

    return m_device->createBuiltInGraphicsPipeline(m_layout, state);
  }
  
}
