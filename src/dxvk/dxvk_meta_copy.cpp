#include "dxvk_device.h"
#include "dxvk_meta_copy.h"
#include "dxvk_shader_builtin.h"
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

namespace dxvk {

  struct DxvkMetaImageCopyPushArgs {
    ir::SsaDef srcOffset  = { };
    ir::SsaDef extent     = { };
    ir::SsaDef layerIndex = { };
    ir::SsaDef stencilBit = { };
  };


  static DxvkMetaImageCopyPushArgs loadImageCopyPushArgs(ir::Builder& builder, DxvkBuiltInShader& helper) {
    ir::BasicType coordType(ir::ScalarType::eU32, 3u);

    DxvkMetaImageCopyPushArgs result = { };
    result.srcOffset  = helper.declarePushData(builder, coordType, offsetof(DxvkMetaImageCopy::Args, srcOffset), "srcOffset");
    result.extent     = helper.declarePushData(builder, coordType, offsetof(DxvkMetaImageCopy::Args, extent), "extent");
    result.layerIndex = helper.declarePushData(builder, ir::ScalarType::eU32, offsetof(DxvkMetaImageCopy::Args, layerIndex), "layer_index");
    result.stencilBit = helper.declarePushData(builder, ir::ScalarType::eU32, offsetof(DxvkMetaImageCopy::Args, stencilBit), "stencil_bit");
    return result;
  }


  static void exportCopyToImagePs(
          ir::Builder&            builder,
          DxvkBuiltInShader&      helper,
          VkImageAspectFlags      aspects,
          ir::SsaDef              value,
          ir::SsaDef              valueStencil,
          ir::SsaDef              stencilBit) {
    if (aspects & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
      // Export depth directly through the built-in
      if (aspects & VK_IMAGE_ASPECT_DEPTH_BIT)
        helper.exportBuiltIn(builder, ir::BuiltIn::eDepth, value);

      if (aspects & VK_IMAGE_ASPECT_STENCIL_BIT) {
        if (stencilBit) {
          // Check whether stencil bit is set and discard if not
          ir::SsaDef cond = builder.add(ir::Op::IEq(ir::ScalarType::eBool,
            builder.add(ir::Op::IAnd(ir::ScalarType::eU32, valueStencil, stencilBit)),
            builder.makeConstant(0u)));

          ir::SsaDef mergeBlock = helper.emitConditionalBlock(builder, cond);

          builder.add(ir::Op::Demote());
          builder.setCursor(mergeBlock);
        } else {
          // Export stencil directly if supported by the device
          helper.exportBuiltIn(builder, ir::BuiltIn::eStencilRef, valueStencil);
        }
      }
    } else {
      // Assume color format and export data as-is
      helper.exportOutput(builder, 0u, value, "color");
    }
  }

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

    for (const auto& p : m_imageCopyPipelines)
      vk->vkDestroyPipeline(vk->device(), p.second.pipeline, nullptr);

    for (const auto& p : m_bufferToImagePipelines)
      vk->vkDestroyPipeline(vk->device(), p.second.pipeline, nullptr);

    for (const auto& p : m_imageToBufferPipelines)
      vk->vkDestroyPipeline(vk->device(), p.second.pipeline, nullptr);

    vk->vkDestroyPipeline(vk->device(), m_copyBufferImagePipeline.pipeline, nullptr);
  }


  DxvkMetaImageCopy DxvkMetaCopyObjects::getPipeline(const DxvkMetaImageCopy::Key& key) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    auto entry = m_imageCopyPipelines.find(key);
    if (entry != m_imageCopyPipelines.end())
      return entry->second;

    DxvkMetaImageCopy pipeline = createImageCopyPipeline(key);
    m_imageCopyPipelines.insert({ key, pipeline });
    return pipeline;
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


  std::vector<uint32_t> DxvkMetaCopyObjects::createVsCopyImage(const DxvkPipelineLayout* layout, const DxvkMetaImageCopy::Key& key) {
    dxbc_spv::ir::Builder builder;

    DxvkBuiltInShader helper(m_device, layout, getName(key, "vs"));
    DxvkBuiltInVertexShader vertex = helper.buildFullscreenVertexShader(builder);

    ir::ResourceKind srcKind = helper.determineResourceKind(key.srcViewType, key.samples);
    uint32_t coordDims = ir::resourceCoordComponentCount(srcKind);

    // Use instance index as layer for 3D images, otherwise use the per-draw push constant.
    DxvkMetaImageCopyPushArgs pushArgs = loadImageCopyPushArgs(builder, helper);

    ir::SsaDef layer = coordDims > 2u
      ? vertex.instanceIndex
      : pushArgs.layerIndex;

    if (ir::resourceIsLayered(srcKind) || coordDims > 2u)
      helper.exportBuiltIn(builder, ir::BuiltIn::eLayerIndex, layer);

    // Apply source offset and extent to the normalized vertex coordinate
    ir::BasicType coordType2D(ir::ScalarType::eF32, 2u);
    ir::BasicType coordType3D(ir::ScalarType::eF32, 3u);

    ir::SsaDef dstExtent = builder.add(ir::Op::ConvertItoF(coordType3D, pushArgs.extent));
    ir::SsaDef srcOffset = builder.add(ir::Op::ConvertItoF(coordType3D, pushArgs.srcOffset));

    ir::SsaDef coord = builder.add(ir::Op::FMad(coordType2D, vertex.coord,
      helper.emitExtractVector(builder, dstExtent, 0u, 2u),
      helper.emitExtractVector(builder, srcOffset, 0u, 2u)));

    // Use layer index as Z coordinate for 3D images
    ir::SsaDef zCoord = helper.emitExtractVector(builder, pushArgs.srcOffset, 2u, 1u);
    zCoord = builder.add(ir::Op::IAdd(ir::ScalarType::eU32, zCoord, layer));

    coord = helper.emitConcatVector(builder, coord,
      builder.add(ir::Op::ConvertItoF(ir::ScalarType::eF32, zCoord)));

    // Export only the coordinate components required for the given view type
    coord = helper.emitExtractVector(builder, coord, 0u, coordDims);

    helper.exportOutput(builder, 0u, coord, "coord");
    return helper.buildShader(builder);
  }


  std::vector<uint32_t> DxvkMetaCopyObjects::createPsCopyImage(const DxvkPipelineLayout* layout, const DxvkMetaImageCopy::Key& key) {
    dxbc_spv::ir::Builder builder;

    DxvkBuiltInShader helper(m_device, layout, getName(key, "ps"));
    DxvkBuiltInPixelShader pixel = helper.buildPixelShader(builder);

    DxvkMetaImageCopyPushArgs pushArgs = loadImageCopyPushArgs(builder, helper);

    ir::ResourceKind kind = helper.determineResourceKind(key.srcViewType, key.samples);

    ir::SsaDef srv0;
    ir::SsaDef srv1;

    if (key.dstAspects != VK_IMAGE_ASPECT_STENCIL_BIT) {
      VkImageAspectFlagBits aspect0 = VkImageAspectFlagBits(key.dstAspects & ~VK_IMAGE_ASPECT_STENCIL_BIT);
      srv0 = helper.declareImageSrv(builder, 0u, "src", key.srcViewType, key.dstFormat, aspect0, key.samples);
    }

    if (key.dstAspects & VK_IMAGE_ASPECT_STENCIL_BIT) {
      srv1 = helper.declareImageSrv(builder, 1u, "src_stencil", key.srcViewType,
        key.dstFormat, VK_IMAGE_ASPECT_STENCIL_BIT, key.samples);
    }

    // Load coordinate, layer and, if necessary, sample ID as integers
    uint32_t coordDims = ir::resourceCoordComponentCount(kind);

    ir::BasicType coordTypeF(ir::ScalarType::eF32, coordDims);
    ir::BasicType coordTypeU(ir::ScalarType::eU32, coordDims);

    ir::SsaDef coord = helper.declareInput(builder, coordTypeF, 0u, "coord");
    coord = builder.add(ir::Op::ConvertFtoI(coordTypeU, coord));

    ir::SsaDef layer = ir::resourceIsLayered(kind) ? pushArgs.layerIndex : ir::SsaDef();
    ir::SsaDef sample = ir::resourceIsMultisampled(kind) ? pixel.sampleId : ir::SsaDef();
    ir::SsaDef mip = ir::resourceIsMultisampled(kind) ? ir::SsaDef() : builder.makeConstant(0u);

    ir::SsaDef value0;
    ir::SsaDef value1;

    // Load raw pixel data from respective image
    if (key.dstAspects != VK_IMAGE_ASPECT_STENCIL_BIT) {
      ir::BasicType pixelType(helper.determineSampledType(key.dstFormat, VK_IMAGE_ASPECT_COLOR_BIT), 4u);

      if (key.dstAspects & VK_IMAGE_ASPECT_DEPTH_BIT)
        pixelType = ir::BasicType(ir::ScalarType::eF32, 4u);

      value0 = builder.add(ir::Op::ImageLoad(pixelType, srv0, mip, layer, coord, sample, ir::SsaDef()));
      value0 = helper.emitFormatVector(builder, key.dstFormat, value0);
    }

    if (key.dstAspects & VK_IMAGE_ASPECT_STENCIL_BIT) {
      ir::BasicType stencilType(ir::ScalarType::eU32, 4u);
      value1 = builder.add(ir::Op::ImageLoad(stencilType, srv1, mip, layer, coord, sample, ir::SsaDef()));
      value1 = helper.emitFormatVector(builder, key.dstFormat, value1);
    }

    exportCopyToImagePs(builder, helper, key.dstAspects, value0, value1,
      key.bitwiseStencil ? pushArgs.stencilBit : ir::SsaDef());
    return helper.buildShader(builder);
  }


  VkPipeline DxvkMetaCopyObjects::createCopyToImagePipeline(
    const DxvkPipelineLayout*           layout,
    const util::DxvkBuiltInShaderStage& vs,
    const util::DxvkBuiltInShaderStage& ps,
          VkFormat                      dstFormat,
          VkImageAspectFlags            dstAspects,
          VkSampleCountFlagBits         samples,
          bool                          bitwiseStencil) {
    static const std::array<VkDynamicState, 1u> dynState = {{
      VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
    }};

    VkStencilOpState stencilOp = { };
    stencilOp.failOp = VK_STENCIL_OP_REPLACE;
    stencilOp.passOp = VK_STENCIL_OP_REPLACE;
    stencilOp.depthFailOp = VK_STENCIL_OP_REPLACE;
    stencilOp.compareOp = VK_COMPARE_OP_ALWAYS;
    stencilOp.writeMask = 0xffu;
    stencilOp.reference = 0xffu;

    VkPipelineDepthStencilStateCreateInfo dsState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    dsState.depthTestEnable = bool(dstAspects & VK_IMAGE_ASPECT_DEPTH_BIT);
    dsState.depthWriteEnable = bool(dstAspects & VK_IMAGE_ASPECT_DEPTH_BIT);
    dsState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    dsState.stencilTestEnable = bool(dstAspects & VK_IMAGE_ASPECT_STENCIL_BIT);
    dsState.front = stencilOp;
    dsState.back = stencilOp;

    util::DxvkBuiltInGraphicsState state = { };
    state.vs = vs;
    state.fs = ps;
    state.sampleCount = samples;

    if (dstAspects & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
      state.depthFormat = dstFormat;
      state.dsState = &dsState;
    } else {
      state.colorFormat = dstFormat;
    }

    if ((dstAspects & VK_IMAGE_ASPECT_STENCIL_BIT) && bitwiseStencil) {
      state.dynamicStateCount = dynState.size();
      state.dynamicStates = dynState.data();
    }

    return m_device->createBuiltInGraphicsPipeline(layout, state);
  }


  DxvkMetaImageCopy DxvkMetaCopyObjects::createImageCopyPipeline(const DxvkMetaImageCopy::Key& key) {
    static const std::array<DxvkDescriptorSetLayoutBinding, 2> bindings = {{
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};

    DxvkMetaImageCopy pipeline = { };
    pipeline.layout = m_device->createBuiltInPipelineLayout(0u,
      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
      sizeof(DxvkMetaImageCopy::Args), bindings.size(), bindings.data());

    auto vsSpirv = createVsCopyImage(pipeline.layout, key);
    auto psSpirv = createPsCopyImage(pipeline.layout, key);

    pipeline.pipeline = createCopyToImagePipeline(pipeline.layout, vsSpirv, psSpirv,
      key.dstFormat, key.dstAspects, key.samples, key.bitwiseStencil);
    return pipeline;
  }


  std::string DxvkMetaCopyObjects::getName(const DxvkMetaImageCopy::Key& key, const char* type) {
    std::stringstream name;
    name << "meta_" << type << "_copy";
    name << "_" << str::format(key.srcViewType).substr(std::strlen("VK_IMAGE_VIEW_TYPE_"));
    name << "_" << str::format(key.dstFormat).substr(std::strlen("VK_FORMAT_"));

    if (key.samples > VK_SAMPLE_COUNT_1_BIT)
      name << "_msx" << uint32_t(key.samples);

    return str::tolower(name.str());
  }

}
