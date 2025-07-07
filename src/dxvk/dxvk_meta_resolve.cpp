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

    for (const auto& pair : m_pipelines)
      vk->vkDestroyPipeline(vk->device(), pair.second.pipeline, nullptr);
  }


  DxvkMetaResolvePipeline DxvkMetaResolveObjects::getPipeline(
          VkFormat                  format,
          VkSampleCountFlagBits     samples,
          VkResolveModeFlagBits     depthResolveMode,
          VkResolveModeFlagBits     stencilResolveMode) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkMetaResolvePipelineKey key = { };
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
    static const std::array<DxvkDescriptorSetLayoutBinding, 2> bindings = {{
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};

    DxvkMetaResolvePipeline pipeline = { };
    pipeline.layout = m_device->createBuiltInPipelineLayout(0u, VK_SHADER_STAGE_FRAGMENT_BIT,
      sizeof(VkOffset2D), bindings.size(), bindings.data());

    auto formatInfo = lookupFormatInfo(key.format);

    VkStencilOpState stencilOp = { };
    stencilOp.failOp            = VK_STENCIL_OP_REPLACE;
    stencilOp.passOp            = VK_STENCIL_OP_REPLACE;
    stencilOp.depthFailOp       = VK_STENCIL_OP_REPLACE;
    stencilOp.compareOp         = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask       = 0xffu;
    stencilOp.writeMask         = 0xffu;

    VkPipelineDepthStencilStateCreateInfo dsState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    dsState.depthTestEnable     = key.modeD != VK_RESOLVE_MODE_NONE;
    dsState.depthWriteEnable    = key.modeD != VK_RESOLVE_MODE_NONE;
    dsState.depthCompareOp      = VK_COMPARE_OP_ALWAYS;
    dsState.stencilTestEnable   = key.modeS != VK_RESOLVE_MODE_NONE;
    dsState.front               = stencilOp;
    dsState.back                = stencilOp;

    static const std::array<VkSpecializationMapEntry, 3> specEntries = {{
      { 0, offsetof(DxvkMetaResolvePipelineKey, samples), sizeof(VkSampleCountFlagBits) },
      { 1, offsetof(DxvkMetaResolvePipelineKey, modeD),   sizeof(VkResolveModeFlagBits) },
      { 2, offsetof(DxvkMetaResolvePipelineKey, modeS),   sizeof(VkResolveModeFlagBits) },
    }};

    VkSpecializationInfo specInfo = { };
    specInfo.mapEntryCount      = specEntries.size();
    specInfo.pMapEntries        = specEntries.data();
    specInfo.dataSize           = sizeof(key);
    specInfo.pData              = &key;

    util::DxvkBuiltInGraphicsState state = { };

    if (m_device->features().vk12.shaderOutputLayer) {
      state.vs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_layer_vert, nullptr);
    } else {
      state.vs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_vert, nullptr);
      state.gs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_geom, nullptr);
    }

    if (key.modeS && (formatInfo->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)) {
      if (m_device->features().extShaderStencilExport) {
        state.fs = util::DxvkBuiltInShaderStage(dxvk_resolve_frag_ds, &specInfo);
      } else {
        state.fs = util::DxvkBuiltInShaderStage(dxvk_resolve_frag_d, &specInfo);
        Logger::warn("DXVK: Stencil export not supported by device, skipping stencil resolve");
      }
    } else if (formatInfo->aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) {
      state.fs = util::DxvkBuiltInShaderStage(dxvk_resolve_frag_d, &specInfo);
    } else if (formatInfo->flags.test(DxvkFormatFlag::SampledUInt)) {
      state.fs = util::DxvkBuiltInShaderStage(dxvk_resolve_frag_u, &specInfo);
    } else if (formatInfo->flags.test(DxvkFormatFlag::SampledSInt)) {
      state.fs = util::DxvkBuiltInShaderStage(dxvk_resolve_frag_i, &specInfo);
    } else {
      state.fs = util::DxvkBuiltInShaderStage(dxvk_resolve_frag_f, &specInfo);
    }

    if (formatInfo->aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
      state.depthFormat = key.format;
      state.dsState = &dsState;
    } else {
      state.colorFormat = key.format;
    }

    pipeline.pipeline = m_device->createBuiltInGraphicsPipeline(pipeline.layout, state);
    return pipeline;
  }

}
