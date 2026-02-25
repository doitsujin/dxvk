#include "dxvk_device.h"
#include "dxvk_meta_copy.h"
#include "dxvk_shader_builtin.h"
#include "dxvk_util.h"

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


  struct DxvkMetaBufferToImageCopyPushArgs {
    ir::SsaDef srcOffset  = { };
    ir::SsaDef srcExtent  = { };
    ir::SsaDef dstExtent  = { };
    ir::SsaDef layerIndex = { };
    ir::SsaDef stencilBit = { };
  };


  static DxvkMetaImageCopyPushArgs loadImageCopyPushArgs(ir::Builder& builder, DxvkBuiltInShader& helper) {
    ir::BasicType coordType(ir::ScalarType::eU32, 3u);

    DxvkMetaImageCopyPushArgs result = { };
    result.srcOffset  = helper.declarePushData(builder, coordType, offsetof(DxvkMetaImageCopy::Args, srcOffset), "src_offset");
    result.extent     = helper.declarePushData(builder, coordType, offsetof(DxvkMetaImageCopy::Args, extent), "extent");
    result.layerIndex = helper.declarePushData(builder, ir::ScalarType::eU32, offsetof(DxvkMetaImageCopy::Args, layerIndex), "layer_index");
    result.stencilBit = helper.declarePushData(builder, ir::ScalarType::eU32, offsetof(DxvkMetaImageCopy::Args, stencilBit), "stencil_bit");
    return result;
  }


  static DxvkMetaBufferToImageCopyPushArgs loadBufferToImageCopyPushArgs(ir::Builder& builder, DxvkBuiltInShader& helper) {
    ir::BasicType coordType2D(ir::ScalarType::eU32, 2u);
    ir::BasicType coordType3D(ir::ScalarType::eU32, 3u);

    DxvkMetaBufferToImageCopyPushArgs result = { };
    result.srcOffset  = helper.declarePushData(builder, coordType3D, offsetof(DxvkMetaBufferToImageCopy::Args, srcOffset), "src_offset");
    result.srcExtent  = helper.declarePushData(builder, coordType2D, offsetof(DxvkMetaBufferToImageCopy::Args, srcExtent), "src_extent");
    result.dstExtent  = helper.declarePushData(builder, coordType3D, offsetof(DxvkMetaBufferToImageCopy::Args, dstExtent), "dst_extent");
    result.layerIndex = helper.declarePushData(builder, ir::ScalarType::eU32, offsetof(DxvkMetaBufferToImageCopy::Args, layerIndex), "layer_index");
    result.stencilBit = helper.declarePushData(builder, ir::ScalarType::eU32, offsetof(DxvkMetaBufferToImageCopy::Args, stencilBit), "stencil_bit");
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


  static ir::SsaDef flattenBufferImageCoord(
          ir::Builder&            builder,
          DxvkBuiltInShader&      helper,
          ir::SsaDef              coord,
          ir::SsaDef              layer,
          ir::SsaDef              extent) {
    // Figure out how many coordinate dimensions we have to work with backwards.
    ir::BasicType type = builder.getOp(coord).getType().getBaseType(0u);
    uint32_t coordDims = type.getVectorSize();

    // Treat the layer index as a coordinate of the highest dimension.
    // This logic works because 3D images can never be layered.
    ir::SsaDef index = layer;

    for (uint32_t i = coordDims; i; i--) {
      if (index) {
        ir::SsaDef size = helper.emitExtractVector(builder, extent, i - 1u, 1u);
        index = builder.add(ir::Op::IMul(ir::ScalarType::eU32, index, size));
      }

      ir::SsaDef scalar = helper.emitExtractVector(builder, coord, i - 1u, 1u);
      index = index ? builder.add(ir::Op::IAdd(ir::ScalarType::eU32, scalar, index)) : scalar;
    }

    return index;
  }


  static std::vector<uint32_t> createCopyToImageVs(
          dxbc_spv::ir::Builder&        builder,
          DxvkBuiltInShader&            helper,
    const DxvkBuiltInVertexShader&      vertex,
          VkImageViewType               viewType,
          VkSampleCountFlagBits         samples,
          ir::SsaDef                    srcOffsetArg,
          ir::SsaDef                    dstExtentArg,
          ir::SsaDef                    layerIndexArg) {
    ir::ResourceKind srcKind = helper.determineResourceKind(viewType, samples);
    uint32_t coordDims = ir::resourceCoordComponentCount(srcKind);

    // Use instance index as layer for 3D images, otherwise use the per-draw push constant.
    ir::SsaDef layer = coordDims > 2u ? vertex.instanceIndex : layerIndexArg;

    if (ir::resourceIsLayered(srcKind) || coordDims > 2u)
      helper.exportBuiltIn(builder, ir::BuiltIn::eLayerIndex, layer);

    // Apply source offset and extent to the normalized vertex coordinate
    ir::BasicType coordType2D(ir::ScalarType::eF32, 2u);
    ir::BasicType coordType3D(ir::ScalarType::eF32, 3u);

    ir::SsaDef dstExtent = builder.add(ir::Op::ConvertItoF(coordType3D, dstExtentArg));
    ir::SsaDef srcOffset = builder.add(ir::Op::ConvertItoF(coordType3D, srcOffsetArg));

    ir::SsaDef coord = builder.add(ir::Op::FMad(coordType2D, vertex.coord,
      helper.emitExtractVector(builder, dstExtent, 0u, 2u),
      helper.emitExtractVector(builder, srcOffset, 0u, 2u)));

    // Use layer index as Z coordinate for 3D images
    ir::SsaDef zCoord = helper.emitExtractVector(builder, srcOffsetArg, 2u, 1u);
    zCoord = builder.add(ir::Op::IAdd(ir::ScalarType::eU32, zCoord, layer));

    coord = helper.emitConcatVector(builder, coord,
      builder.add(ir::Op::ConvertItoF(ir::ScalarType::eF32, zCoord)));

    // Export only the coordinate components required for the given view type
    coord = helper.emitExtractVector(builder, coord, 0u, coordDims);

    helper.exportOutput(builder, 0u, coord, "coord");
    return helper.buildShader(builder);
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

    for (const auto& p : m_bufferImageCopyPipelines)
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


  DxvkMetaBufferToImageCopy DxvkMetaCopyObjects::getPipeline(const DxvkMetaBufferToImageCopy::Key& key) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    auto entry = m_bufferImageCopyPipelines.find(key);
    if (entry != m_bufferImageCopyPipelines.end())
      return entry->second;

    DxvkMetaBufferToImageCopy pipeline = createBufferToImageCopyPipeline(key);
    m_bufferImageCopyPipelines.insert({ key, pipeline });
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


  DxvkMetaCopyPipeline DxvkMetaCopyObjects::getCopyImageToBufferPipeline(
          VkImageViewType       viewType,
          VkFormat              dstFormat) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    DxvkMetaBufferImageCopyPipelineKey key;
    key.imageViewType = viewType;
    key.imageFormat = VK_FORMAT_UNDEFINED;
    key.bufferFormat = dstFormat;
    key.imageAspects = lookupFormatInfo(dstFormat)->aspectMask;

    auto entry = m_imageToBufferPipelines.find(key);
    if (entry != m_imageToBufferPipelines.end())
      return entry->second;

    DxvkMetaCopyPipeline pipeline = createCopyImageToBufferPipeline(key);
    m_imageToBufferPipelines.insert({ key, pipeline });
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


  std::vector<uint32_t> DxvkMetaCopyObjects::createVsCopyImage(
    const DxvkPipelineLayout*           layout,
    const DxvkMetaImageCopy::Key&       key) {
    dxbc_spv::ir::Builder builder;
    DxvkBuiltInShader helper(m_device, layout, getName(key, "vs"));

    DxvkBuiltInVertexShader vertex = helper.buildFullscreenVertexShader(builder);
    DxvkMetaImageCopyPushArgs pushArgs = loadImageCopyPushArgs(builder, helper);

    return createCopyToImageVs(builder, helper, vertex, key.srcViewType,
      key.samples, pushArgs.srcOffset, pushArgs.extent, pushArgs.layerIndex);
  }


  std::vector<uint32_t> DxvkMetaCopyObjects::createVsCopyBufferToImage(
    const DxvkPipelineLayout*           layout,
    const DxvkMetaBufferToImageCopy::Key& key) {
    dxbc_spv::ir::Builder builder;
    DxvkBuiltInShader helper(m_device, layout, getName(key, "vs"));

    DxvkBuiltInVertexShader vertex = helper.buildFullscreenVertexShader(builder);
    DxvkMetaBufferToImageCopyPushArgs pushArgs = loadBufferToImageCopyPushArgs(builder, helper);

    return createCopyToImageVs(builder, helper, vertex, key.dstViewType,
      key.samples, pushArgs.srcOffset, pushArgs.dstExtent, pushArgs.layerIndex);
  }


  std::vector<uint32_t> DxvkMetaCopyObjects::createPsCopyImage(
    const DxvkPipelineLayout*           layout,
    const DxvkMetaImageCopy::Key&       key) {
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


  std::vector<uint32_t> DxvkMetaCopyObjects::createPsCopyBufferToImage(
    const DxvkPipelineLayout*           layout,
    const DxvkMetaBufferToImageCopy::Key& key) {
    dxbc_spv::ir::Builder builder;

    DxvkBuiltInShader helper(m_device, layout, getName(key, "ps"));
    helper.buildPixelShader(builder);

    DxvkMetaBufferToImageCopyPushArgs pushArgs = loadBufferToImageCopyPushArgs(builder, helper);

    ir::SsaDef srv = helper.declareTexelBufferSrv(builder, 0u, "buf", key.bufferFormat);

    // Load source coordinate and flatten it according to the
    // buffer image dimensions supplied via push data.
    ir::ResourceKind kind = helper.determineResourceKind(key.dstViewType, key.samples);
    uint32_t coordDims = ir::resourceCoordComponentCount(kind);

    ir::BasicType coordTypeF(ir::ScalarType::eF32, coordDims);
    ir::BasicType coordTypeU(ir::ScalarType::eU32, coordDims);

    ir::SsaDef coord = helper.declareInput(builder, coordTypeF, 0u, "coord");
    coord = builder.add(ir::Op::ConvertFtoI(coordTypeU, coord));

    ir::SsaDef layer = resourceIsLayered(kind) ? pushArgs.layerIndex : ir::SsaDef();

    ir::SsaDef flatIndex = flattenBufferImageCoord(
      builder, helper, coord, layer, pushArgs.srcExtent);

    // Load source texel and do some format-specific processing.
    ir::BasicType valueType(helper.determineSampledType(key.bufferFormat, VK_IMAGE_ASPECT_COLOR_BIT), 4u);
    ir::SsaDef value = builder.add(ir::Op::BufferLoad(valueType, srv, flatIndex, 0u));

    switch (key.srcFormat) {
      case VK_FORMAT_S8_UINT: {
        ir::SsaDef stencil = helper.emitExtractVector(builder, value, 0u, 1u);

        exportCopyToImagePs(builder, helper, key.dstAspects, ir::SsaDef(), stencil,
          key.bitwiseStencil ? pushArgs.stencilBit : ir::SsaDef());
      } break;

      case VK_FORMAT_D16_UNORM: {
        ir::SsaDef depth = helper.emitExtractVector(builder, value, 0u, 1u);
        depth = builder.add(ir::Op::ConvertItoF(ir::ScalarType::eF32, depth));
        depth = builder.add(ir::Op::FMul(ir::ScalarType::eF32, depth,
          builder.makeConstant(1.0f / float(0xffffu))));

        exportCopyToImagePs(builder, helper, key.dstAspects, depth, ir::SsaDef(), ir::SsaDef());
      } break;

      case VK_FORMAT_D16_UNORM_S8_UINT: {
        ir::SsaDef depth = helper.emitExtractVector(builder, value, 0u, 1u);
        ir::SsaDef stencil = helper.emitExtractVector(builder, value, 1u, 1u);

        depth = builder.add(ir::Op::ConvertItoF(ir::ScalarType::eF32, depth));
        depth = builder.add(ir::Op::FMul(ir::ScalarType::eF32, depth,
          builder.makeConstant(1.0f / float(0xffffu))));

        exportCopyToImagePs(builder, helper, key.dstAspects, depth, stencil,
          key.bitwiseStencil ? pushArgs.stencilBit : ir::SsaDef());
      } break;

      case VK_FORMAT_X8_D24_UNORM_PACK32: {
        ir::SsaDef depth = helper.emitExtractVector(builder, value, 0u, 1u);
        depth = builder.add(ir::Op::UBitExtract(ir::ScalarType::eU32, depth,
          builder.makeConstant(0u), builder.makeConstant(24u)));
        depth = builder.add(ir::Op::ConvertItoF(ir::ScalarType::eF32, depth));
        depth = builder.add(ir::Op::FMul(ir::ScalarType::eF32, depth,
          builder.makeConstant(1.0f / float(0xffffffu))));

        exportCopyToImagePs(builder, helper, key.dstAspects, depth, ir::SsaDef(), ir::SsaDef());
      } break;

      case VK_FORMAT_D24_UNORM_S8_UINT: {
        ir::SsaDef packed = helper.emitExtractVector(builder, value, 0u, 1u);

        ir::SsaDef depth = builder.add(ir::Op::UBitExtract(ir::ScalarType::eU32,
          packed, builder.makeConstant(0u), builder.makeConstant(24u)));
        depth = builder.add(ir::Op::ConvertItoF(ir::ScalarType::eF32, depth));
        depth = builder.add(ir::Op::FMul(ir::ScalarType::eF32, depth,
          builder.makeConstant(1.0f / float(0xffffffu))));

        ir::SsaDef stencil = builder.add(ir::Op::UBitExtract(ir::ScalarType::eU32,
          packed, builder.makeConstant(24u), builder.makeConstant(8u)));

        exportCopyToImagePs(builder, helper, key.dstAspects, depth, stencil,
          key.bitwiseStencil ? pushArgs.stencilBit : ir::SsaDef());
      } break;

      case VK_FORMAT_D32_SFLOAT: {
        ir::SsaDef depth = helper.emitExtractVector(builder, value, 0u, 1u);
        depth = builder.add(ir::Op::Cast(ir::ScalarType::eF32, depth));

        exportCopyToImagePs(builder, helper, key.dstAspects, depth, ir::SsaDef(), ir::SsaDef());
      } break;

      case VK_FORMAT_D32_SFLOAT_S8_UINT: {
        ir::SsaDef depth = helper.emitExtractVector(builder, value, 0u, 1u);
        ir::SsaDef stencil = helper.emitExtractVector(builder, value, 1u, 1u);

        depth = builder.add(ir::Op::Cast(ir::ScalarType::eF32, depth));

        exportCopyToImagePs(builder, helper, key.dstAspects, depth, stencil,
          key.bitwiseStencil ? pushArgs.stencilBit : ir::SsaDef());
      } break;

      case VK_FORMAT_A8_UNORM: {
        ir::SsaDef color = helper.emitExtractVector(builder, value, 0u, 1u);
        ir::SsaDef zero = builder.makeConstant(0.0f);

        color = builder.add(ir::Op::CompositeConstruct(
          ir::BasicType(ir::ScalarType::eF32, 4u), zero, zero, zero, color));

        exportCopyToImagePs(builder, helper, key.dstAspects, color, ir::SsaDef(), ir::SsaDef());
      } break;

      case VK_FORMAT_B8G8R8A8_SRGB:
      case VK_FORMAT_B8G8R8A8_UNORM:
      case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
      case VK_FORMAT_A2R10G10B10_SNORM_PACK32: {
        ir::BasicType type = builder.getOp(value).getType().getBaseType(0u);

        ir::SsaDef b = helper.emitExtractVector(builder, value, 0u, 1u);
        ir::SsaDef g = helper.emitExtractVector(builder, value, 1u, 1u);
        ir::SsaDef r = helper.emitExtractVector(builder, value, 2u, 1u);
        ir::SsaDef a = helper.emitExtractVector(builder, value, 3u, 1u);

        ir::SsaDef color = builder.add(ir::Op::CompositeConstruct(type, r, g, b, a));
        exportCopyToImagePs(builder, helper, key.dstAspects, color, ir::SsaDef(), ir::SsaDef());
      } break;

      default: {
        ir::SsaDef color = helper.emitFormatVector(builder, key.dstFormat, value);
        exportCopyToImagePs(builder, helper, key.dstAspects, color, ir::SsaDef(), ir::SsaDef());
      } break;
    }

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


  DxvkMetaBufferToImageCopy DxvkMetaCopyObjects::createBufferToImageCopyPipeline(const DxvkMetaBufferToImageCopy::Key& key) {
    static const std::array<DxvkDescriptorSetLayoutBinding, 1> bindings = {{
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};

    DxvkMetaBufferToImageCopy pipeline = { };
    pipeline.layout = m_device->createBuiltInPipelineLayout(0u,
      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
      sizeof(DxvkMetaBufferToImageCopy::Args), bindings.size(), bindings.data());

    auto vsSpirv = createVsCopyBufferToImage(pipeline.layout, key);
    auto psSpirv = createPsCopyBufferToImage(pipeline.layout, key);

    pipeline.pipeline = createCopyToImagePipeline(pipeline.layout, vsSpirv, psSpirv,
      key.dstFormat, key.dstAspects, key.samples, key.bitwiseStencil);
    return pipeline;
  }


  std::string DxvkMetaCopyObjects::getName(const DxvkMetaImageCopy::Key& key, const char* type) {
    std::stringstream name;
    name << "meta_" << type << "_copy_image";
    name << "_" << str::format(key.srcViewType).substr(std::strlen("VK_IMAGE_VIEW_TYPE_"));
    name << "_" << str::format(key.dstFormat).substr(std::strlen("VK_FORMAT_"));

    if (key.samples > VK_SAMPLE_COUNT_1_BIT)
      name << "_msx" << uint32_t(key.samples);

    return str::tolower(name.str());
  }


  std::string DxvkMetaCopyObjects::getName(const DxvkMetaBufferToImageCopy::Key& key, const char* type) {
    std::stringstream name;
    name << "meta_" << type << "_copy_buffer_to_image";
    name << "_" << str::format(key.dstViewType).substr(std::strlen("VK_IMAGE_VIEW_TYPE_"));
    name << "_" << str::format(key.srcFormat).substr(std::strlen("VK_FORMAT_"));

    if (key.samples > VK_SAMPLE_COUNT_1_BIT)
      name << "_msx" << uint32_t(key.samples);

    return str::tolower(name.str());
  }

}
