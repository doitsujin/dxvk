#include "dxvk_device.h"
#include "dxvk_meta_blit.h"
#include "dxvk_util.h"

#include <dxvk_fullscreen_geom.h>
#include <dxvk_fullscreen_vert.h>
#include <dxvk_fullscreen_layer_vert.h>

#include <dxvk_blit_frag_1d.h>
#include <dxvk_blit_frag_2d.h>
#include <dxvk_blit_frag_3d.h>
#include <dxvk_blit_frag_2d_ms.h>

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
          VkSampleCountFlagBits srcSamples,
          VkSampleCountFlagBits dstSamples) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkMetaBlitPipelineKey key;
    key.viewType   = viewType;
    key.viewFormat = viewFormat;
    key.srcSamples = srcSamples;
    key.dstSamples = dstSamples;

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
    pipeline.pipeline = createPipeline(key.viewType, key.viewFormat, key.srcSamples, key.dstSamples);
    return pipeline;
  }
  
  
  const DxvkPipelineLayout* DxvkMetaBlitObjects::createPipelineLayout() const {
    DxvkDescriptorSetLayoutBinding binding = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT };

    return m_device->createBuiltInPipelineLayout(DxvkPipelineLayoutFlag::UsesSamplerHeap,
      VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(DxvkMetaBlitPushConstants), 1, &binding);
  }


  VkPipeline DxvkMetaBlitObjects::createPipeline(
          VkImageViewType             imageViewType,
          VkFormat                    format,
          VkSampleCountFlagBits       srcSamples,
          VkSampleCountFlagBits       dstSamples) const {
    util::DxvkBuiltInGraphicsState state = { };

    VkSpecializationMapEntry specMap = { };
    specMap.size = sizeof(VkSampleCountFlagBits);

    VkSpecializationInfo specInfo = { };
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries = &specMap;
    specInfo.dataSize = sizeof(VkSampleCountFlagBits);
    specInfo.pData = &srcSamples;

    if (m_device->features().vk12.shaderOutputLayer) {
      state.vs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_layer_vert, nullptr);
    } else {
      state.vs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_vert, nullptr);
      state.gs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_geom, nullptr);
    }

    if (srcSamples != VK_SAMPLE_COUNT_1_BIT) {
      if (imageViewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY) {
        state.fs = util::DxvkBuiltInShaderStage(dxvk_blit_frag_2d_ms, &specInfo);
      } else {
        throw DxvkError("DxvkMetaBlitObjects: Invalid view type for multisampled image");
      }
    } else {
      switch (imageViewType) {
        case VK_IMAGE_VIEW_TYPE_1D_ARRAY: state.fs = util::DxvkBuiltInShaderStage(dxvk_blit_frag_1d, nullptr); break;
        case VK_IMAGE_VIEW_TYPE_2D_ARRAY: state.fs = util::DxvkBuiltInShaderStage(dxvk_blit_frag_2d, nullptr); break;
        case VK_IMAGE_VIEW_TYPE_3D:       state.fs = util::DxvkBuiltInShaderStage(dxvk_blit_frag_3d, nullptr); break;
        default: throw DxvkError("DxvkMetaBlitObjects: Invalid view type");
      }
    }

    state.colorFormat = format;
    state.sampleCount = dstSamples;

    return m_device->createBuiltInGraphicsPipeline(m_layout, state);
  }
  
}
