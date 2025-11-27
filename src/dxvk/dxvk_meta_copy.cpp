#include "dxvk_device.h"
#include "dxvk_meta_copy.h"
#include "dxvk_util.h"

#include <dxvk_fullscreen_geom.h>
#include <dxvk_fullscreen_vert.h>
#include <dxvk_fullscreen_layer_vert.h>

#include <dxvk_buffer_to_image_d.h>
#include <dxvk_buffer_to_image_ds_export.h>
#include <dxvk_buffer_to_image_f.h>
#include <dxvk_buffer_to_image_s_discard.h>
#include <dxvk_buffer_to_image_u.h>

#include <dxvk_image_to_buffer_ds.h>
#include <dxvk_image_to_buffer_f.h>

#include <dxvk_copy_buffer_image.h>
#include <dxvk_copy_color_1d.h>
#include <dxvk_copy_color_2d.h>
#include <dxvk_copy_color_ms.h>
#include <dxvk_copy_depth_stencil_1d.h>
#include <dxvk_copy_depth_stencil_2d.h>
#include <dxvk_copy_depth_stencil_ms.h>

namespace dxvk {

  DxvkMetaCopyViews::DxvkMetaCopyViews(
    const Rc<DxvkImage>&            dstImage,
    const VkImageSubresourceLayers& dstSubresources,
          VkFormat                  dstFormat,
    const Rc<DxvkImage>&            srcImage,
    const VkImageSubresourceLayers& srcSubresources,
          VkFormat                  srcFormat) {
    VkImageAspectFlags dstAspects = dstImage->formatInfo()->aspectMask;
    VkImageAspectFlags srcAspects = srcImage->formatInfo()->aspectMask;

    // We don't support 3D here, so we can safely ignore that case
    VkImageViewType dstViewType = dstImage->info().type == VK_IMAGE_TYPE_1D
      ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    VkImageViewType srcViewType = srcImage->info().type == VK_IMAGE_TYPE_1D
      ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    DxvkImageViewKey dstViewInfo;
    dstViewInfo.viewType = dstViewType;
    dstViewInfo.format = dstFormat;
    dstViewInfo.aspects = dstSubresources.aspectMask;
    dstViewInfo.mipIndex = dstSubresources.mipLevel;
    dstViewInfo.mipCount = 1u;
    dstViewInfo.layerIndex = dstSubresources.baseArrayLayer;
    dstViewInfo.layerCount = dstSubresources.layerCount;
    dstViewInfo.usage = (dstAspects & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
      ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
      : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    dstImageView = dstImage->createView(dstViewInfo);

    // Create source image views
    DxvkImageViewKey srcViewInfo;
    srcViewInfo.viewType = srcViewType;
    srcViewInfo.format = srcFormat;
    srcViewInfo.aspects = srcSubresources.aspectMask & ~VK_IMAGE_ASPECT_STENCIL_BIT;
    srcViewInfo.mipIndex = srcSubresources.mipLevel;
    srcViewInfo.mipCount = 1u;
    srcViewInfo.layerIndex = srcSubresources.baseArrayLayer;
    srcViewInfo.layerCount = srcSubresources.layerCount;
    srcViewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    srcImageView = srcImage->createView(srcViewInfo);

    if (srcAspects & VK_IMAGE_ASPECT_STENCIL_BIT) {
      srcViewInfo.aspects = VK_IMAGE_ASPECT_STENCIL_BIT;
      srcStencilView = srcImage->createView(srcViewInfo);
    }
  }
  

  DxvkMetaCopyViews::~DxvkMetaCopyViews() {

  }

  
  DxvkMetaCopyObjects::DxvkMetaCopyObjects(DxvkDevice* device)
  : m_device(device) {

  }


  DxvkMetaCopyObjects::~DxvkMetaCopyObjects() {
    auto vk = m_device->vkd();

    for (const auto& p : m_copyImagePipelines)
      vk->vkDestroyPipeline(vk->device(), p.second.pipeline, nullptr);

    for (const auto& p : m_bufferToImagePipelines)
      vk->vkDestroyPipeline(vk->device(), p.second.pipeline, nullptr);

    for (const auto& p : m_imageToBufferPipelines)
      vk->vkDestroyPipeline(vk->device(), p.second.pipeline, nullptr);

    vk->vkDestroyPipeline(vk->device(), m_copyBufferImagePipeline.pipeline, nullptr);
  }


  DxvkMetaCopyFormats DxvkMetaCopyObjects::getCopyImageFormats(
          VkFormat              dstFormat,
          VkImageAspectFlags    dstAspect,
          VkFormat              srcFormat,
          VkImageAspectFlags    srcAspect) const {
    if (dstAspect == srcAspect)
      return { dstFormat, srcFormat };

    if (dstAspect == VK_IMAGE_ASPECT_COLOR_BIT && srcAspect == VK_IMAGE_ASPECT_DEPTH_BIT) {
      switch (srcFormat) {
        case VK_FORMAT_D16_UNORM:  return { VK_FORMAT_R16_UNORM,  VK_FORMAT_D16_UNORM  };
        case VK_FORMAT_D32_SFLOAT: return { VK_FORMAT_R32_SFLOAT, VK_FORMAT_D32_SFLOAT };
        default:                   return { VK_FORMAT_UNDEFINED,  VK_FORMAT_UNDEFINED  };
      }
    } else if (dstAspect == VK_IMAGE_ASPECT_DEPTH_BIT && srcAspect == VK_IMAGE_ASPECT_COLOR_BIT) {
      switch (dstFormat) {
        case VK_FORMAT_D16_UNORM:  return { VK_FORMAT_D16_UNORM,  VK_FORMAT_R16_UNORM  };
        case VK_FORMAT_D32_SFLOAT: return { VK_FORMAT_D32_SFLOAT, VK_FORMAT_R32_SFLOAT };
        default:                   return { VK_FORMAT_UNDEFINED,  VK_FORMAT_UNDEFINED  };
      }
    }

    return { VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED };
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::getCopyBufferToImagePipeline(
          VkFormat              dstFormat,
          VkFormat              srcFormat,
          VkImageAspectFlags    aspects,
          VkSampleCountFlags    samples) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkMetaBufferImageCopyPipelineKey key;
    key.imageFormat = dstFormat;
    key.bufferFormat = srcFormat;
    key.imageAspects = aspects;
    key.sampleCount = VkSampleCountFlagBits(samples);

    auto entry = m_bufferToImagePipelines.find(key);
    if (entry != m_bufferToImagePipelines.end())
      return entry->second;

    DxvkMetaCopyPipeline pipeline = createCopyBufferToImagePipeline(key);
    m_bufferToImagePipelines.insert({ key, pipeline });
    return pipeline;
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::getCopyImageToBufferPipeline(
          VkImageViewType       viewType,
          VkFormat              dstFormat) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkMetaBufferImageCopyPipelineKey key;
    key.imageViewType = viewType;
    key.imageFormat = VK_FORMAT_UNDEFINED;
    key.bufferFormat = dstFormat;
    key.imageAspects = lookupFormatInfo(dstFormat)->aspectMask;

    auto entry = m_bufferToImagePipelines.find(key);
    if (entry != m_bufferToImagePipelines.end())
      return entry->second;

    DxvkMetaCopyPipeline pipeline = createCopyImageToBufferPipeline(key);
    m_bufferToImagePipelines.insert({ key, pipeline });
    return pipeline;
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::getCopyImagePipeline(
          VkImageViewType       viewType,
          VkFormat              dstFormat,
          VkSampleCountFlagBits dstSamples) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkMetaImageCopyPipelineKey key;
    key.viewType = viewType;
    key.format   = dstFormat;
    key.samples  = dstSamples;
    
    auto entry = m_copyImagePipelines.find(key);
    if (entry != m_copyImagePipelines.end())
      return entry->second;

    DxvkMetaCopyPipeline pipeline = createCopyImagePipeline(key);
    m_copyImagePipelines.insert({ key, pipeline });
    return pipeline;
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::getCopyFormattedBufferPipeline() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!m_copyBufferImagePipeline.pipeline)
      m_copyBufferImagePipeline = createCopyFormattedBufferPipeline();

    return m_copyBufferImagePipeline;
  }
  
  
  DxvkMetaCopyPipeline DxvkMetaCopyObjects::createCopyFormattedBufferPipeline() {
    static const std::array<DxvkDescriptorSetLayoutBinding, 2> bindings = {{
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
    }};

    DxvkMetaCopyPipeline pipeline;
    pipeline.layout = m_device->createBuiltInPipelineLayout(0u, VK_SHADER_STAGE_COMPUTE_BIT,
      sizeof(DxvkFormattedBufferCopyArgs), bindings.size(), bindings.data());
    pipeline.pipeline = m_device->createBuiltInComputePipeline(pipeline.layout,
      util::DxvkBuiltInShaderStage(dxvk_copy_buffer_image, nullptr));
    return pipeline;
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::createCopyImagePipeline(
    const DxvkMetaImageCopyPipelineKey& key) {
    static const std::array<DxvkDescriptorSetLayoutBinding, 2> bindings = {{
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};

    DxvkMetaCopyPipeline pipeline = { };
    pipeline.layout = m_device->createBuiltInPipelineLayout(0u, VK_SHADER_STAGE_FRAGMENT_BIT,
      sizeof(VkOffset2D), bindings.size(), bindings.data());

    VkImageAspectFlags aspect = lookupFormatInfo(key.format)->aspectMask;

    util::DxvkBuiltInGraphicsState state = { };

    if (m_device->features().vk12.shaderOutputLayer) {
      state.vs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_layer_vert, nullptr);
    } else {
      state.vs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_vert, nullptr);
      state.gs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_geom, nullptr);
    }

    bool useDepthStencil = m_device->features().extShaderStencilExport
      && (aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));

    if (useDepthStencil) {
      if (key.viewType == VK_IMAGE_VIEW_TYPE_1D)
        state.fs = util::DxvkBuiltInShaderStage(dxvk_copy_depth_stencil_1d, nullptr);
      else if (key.samples == VK_SAMPLE_COUNT_1_BIT)
        state.fs = util::DxvkBuiltInShaderStage(dxvk_copy_depth_stencil_2d, nullptr);
      else
        state.fs = util::DxvkBuiltInShaderStage(dxvk_copy_depth_stencil_ms, nullptr);
    } else {
      if (key.viewType == VK_IMAGE_VIEW_TYPE_1D)
        state.fs = util::DxvkBuiltInShaderStage(dxvk_copy_color_1d, nullptr);
      else if (key.samples == VK_SAMPLE_COUNT_1_BIT)
        state.fs = util::DxvkBuiltInShaderStage(dxvk_copy_color_2d, nullptr);
      else
        state.fs = util::DxvkBuiltInShaderStage(dxvk_copy_color_ms, nullptr);
    }

    state.sampleCount = key.samples;

    if (aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
      state.depthFormat = key.format;
    else
      state.colorFormat = key.format;

    pipeline.pipeline = m_device->createBuiltInGraphicsPipeline(pipeline.layout, state);
    return pipeline;
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::createCopyBufferToImagePipeline(
    const DxvkMetaBufferImageCopyPipelineKey& key) {
    static const std::array<DxvkDescriptorSetLayoutBinding, 1> bindings = {{
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
    }};

    DxvkMetaCopyPipeline pipeline;
    pipeline.layout = m_device->createBuiltInPipelineLayout(0u, VK_SHADER_STAGE_FRAGMENT_BIT,
      sizeof(DxvkBufferImageCopyArgs), bindings.size(), bindings.data());

    VkStencilOpState stencilOp = { };
    stencilOp.failOp      = VK_STENCIL_OP_REPLACE;
    stencilOp.passOp      = VK_STENCIL_OP_REPLACE;
    stencilOp.depthFailOp = VK_STENCIL_OP_REPLACE;
    stencilOp.compareOp   = VK_COMPARE_OP_ALWAYS;
    stencilOp.compareMask = 0xff;
    stencilOp.writeMask   = 0xff;
    stencilOp.reference   = 0xff;

    // Clear stencil when writing depth aspect
    if (!m_device->features().extShaderStencilExport && key.imageAspects != VK_IMAGE_ASPECT_STENCIL_BIT)
      stencilOp.reference = 0x00;

    VkPipelineDepthStencilStateCreateInfo dsState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    dsState.depthTestEnable   = !!(key.imageAspects & VK_IMAGE_ASPECT_DEPTH_BIT);
    dsState.depthWriteEnable  = dsState.depthTestEnable;
    dsState.depthCompareOp    = VK_COMPARE_OP_ALWAYS;
    dsState.stencilTestEnable = !!(key.imageAspects & VK_IMAGE_ASPECT_STENCIL_BIT);
    dsState.front             = stencilOp;
    dsState.back              = stencilOp;

    // Set up dynamic states. Stencil write mask is
    // only required for the stencil discard shader.
    VkDynamicState dynamicStencilWriteMask = VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;

    // Determine fragment shader to use. Always use the DS export shader
    // if possible, it can support writing to one aspect exclusively.
    VkSpecializationMapEntry specMap = { };
    specMap.size = sizeof(VkFormat);

    VkSpecializationInfo specInfo = { };
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries = &specMap;
    specInfo.dataSize = sizeof(VkFormat);
    specInfo.pData = &key.bufferFormat;

    // Set up final pipeline state
    util::DxvkBuiltInGraphicsState state = { };
    state.sampleCount = key.sampleCount;

    if (m_device->features().vk12.shaderOutputLayer) {
      state.vs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_layer_vert, nullptr);
    } else {
      state.vs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_vert, nullptr);
      state.gs = util::DxvkBuiltInShaderStage(dxvk_fullscreen_geom, nullptr);
    }

    if (key.imageAspects & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
      if (m_device->features().extShaderStencilExport) {
        state.fs = util::DxvkBuiltInShaderStage(dxvk_buffer_to_image_ds_export, &specInfo);
      } else if (key.imageAspects == VK_IMAGE_ASPECT_STENCIL_BIT) {
        state.fs = util::DxvkBuiltInShaderStage(dxvk_buffer_to_image_s_discard, &specInfo);

        state.dynamicStateCount = 1u;
        state.dynamicStates = &dynamicStencilWriteMask;
      } else {
        state.fs = util::DxvkBuiltInShaderStage(dxvk_buffer_to_image_d, &specInfo);
      }

      state.depthFormat = key.imageFormat;
      state.dsState = &dsState;
    } else {
      const auto* formatInfo = lookupFormatInfo(key.imageFormat);

      state.fs = formatInfo->flags.any(DxvkFormatFlag::SampledUInt, DxvkFormatFlag::SampledSInt)
        ? util::DxvkBuiltInShaderStage(dxvk_buffer_to_image_u, &specInfo)
        : util::DxvkBuiltInShaderStage(dxvk_buffer_to_image_f, &specInfo);

      state.colorFormat = key.imageFormat;
    }

    pipeline.pipeline = m_device->createBuiltInGraphicsPipeline(pipeline.layout, state);
    return pipeline;
  }


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::createCopyImageToBufferPipeline(
    const DxvkMetaBufferImageCopyPipelineKey& key) {
    DxvkMetaCopyPipeline pipeline = { };

    static const std::array<DxvkDescriptorSetLayoutBinding, 3> bindings = {{
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        1, VK_SHADER_STAGE_COMPUTE_BIT },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        1, VK_SHADER_STAGE_COMPUTE_BIT },
    }};

    pipeline.layout = m_device->createBuiltInPipelineLayout(0u, VK_SHADER_STAGE_COMPUTE_BIT,
      sizeof(DxvkBufferImageCopyArgs), bindings.size(), bindings.data());

    if (key.imageViewType != VK_IMAGE_VIEW_TYPE_2D_ARRAY) {
      Logger::err(str::format("DxvkMetaCopyObjects: Unsupported view type: ", key.imageViewType));
      return DxvkMetaCopyPipeline();
    }

    VkSpecializationMapEntry specMap = { };
    specMap.size = sizeof(VkFormat);

    VkSpecializationInfo specInfo = { };
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries = &specMap;
    specInfo.dataSize = sizeof(VkFormat);
    specInfo.pData = &key.bufferFormat;

    util::DxvkBuiltInShaderStage stage = (key.imageAspects & VK_IMAGE_ASPECT_STENCIL_BIT)
      ? util::DxvkBuiltInShaderStage(dxvk_image_to_buffer_ds, &specInfo)
      : util::DxvkBuiltInShaderStage(dxvk_image_to_buffer_f, &specInfo);

    pipeline.pipeline = m_device->createBuiltInComputePipeline(pipeline.layout, stage);
    return pipeline;
  }

}
