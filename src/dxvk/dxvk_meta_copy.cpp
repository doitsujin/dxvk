#include "dxvk_device.h"
#include "dxvk_meta_copy.h"
#include "dxvk_shader_builtin.h"
#include "dxvk_util.h"

namespace dxvk {

  struct DxvkMetaImageCopyPushArgs {
    ir::SsaDef srcOffset  = { };
    ir::SsaDef srcExtent  = { };
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


  struct DxvkMetaInputAttachmentImageCopyPushArgs {
    ir::SsaDef srcOffset  = { };
    ir::SsaDef dstOffset  = { };
    ir::SsaDef srcLayer   = { };
    ir::SsaDef dstLayer   = { };
  };


  static DxvkMetaImageCopyPushArgs loadImageCopyPushArgs(ir::Builder& builder, DxvkBuiltInShader& helper) {
    ir::BasicType coordType(ir::ScalarType::eU32, 3u);

    DxvkMetaImageCopyPushArgs result = { };
    result.srcOffset  = helper.declarePushData(builder, coordType, offsetof(DxvkMetaImageCopy::Args, srcOffset), "src_offset");
    result.srcExtent  = helper.declarePushData(builder, coordType, offsetof(DxvkMetaImageCopy::Args, srcExtent), "src_extent");
    result.layerIndex = helper.declarePushData(builder, ir::ScalarType::eU32, offsetof(DxvkMetaImageCopy::Args, layerIndex), "layer_index");
    result.stencilBit = helper.declarePushData(builder, ir::ScalarType::eU32, offsetof(DxvkMetaImageCopy::Args, stencilBit), "stencil_bit");
    return result;
  }


  static DxvkMetaInputAttachmentImageCopyPushArgs loadInputAttachmentImageCopyPushArgs(ir::Builder& builder, DxvkBuiltInShader& helper) {
    ir::BasicType coordType(ir::ScalarType::eU32, 2u);

    DxvkMetaInputAttachmentImageCopyPushArgs result = { };
    result.srcOffset  = helper.declarePushData(builder, coordType, offsetof(DxvkMetaInputAttachmentImageCopy::Args, srcOffset), "src_offset");
    result.dstOffset  = helper.declarePushData(builder, coordType, offsetof(DxvkMetaInputAttachmentImageCopy::Args, dstOffset), "dst_offset");
    result.srcLayer   = helper.declarePushData(builder, ir::ScalarType::eU32, offsetof(DxvkMetaInputAttachmentImageCopy::Args, srcLayer), "src_layer");
    result.dstLayer   = helper.declarePushData(builder, ir::ScalarType::eU32, offsetof(DxvkMetaInputAttachmentImageCopy::Args, dstLayer), "dst_layer");
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


  DxvkMetaCopyObjects::DxvkMetaCopyObjects(DxvkDevice* device)
  : m_device(device) {

  }


  DxvkMetaCopyObjects::~DxvkMetaCopyObjects() {
    auto vk = m_device->vkd();

    for (const auto& p : m_imageCopyPipelines)
      vk->vkDestroyPipeline(vk->device(), p.second.pipeline, nullptr);

    for (const auto& p : m_bufferImageCopyPipelines)
      vk->vkDestroyPipeline(vk->device(), p.second.pipeline, nullptr);

    for (const auto& p : m_imageBufferCopyPipelines)
      vk->vkDestroyPipeline(vk->device(), p.second.pipeline, nullptr);

    vk->vkDestroyPipeline(vk->device(), m_packedImageBufferCopyPipeline.pipeline, nullptr);
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


  DxvkMetaInputAttachmentImageCopy DxvkMetaCopyObjects::getPipeline(const DxvkMetaInputAttachmentImageCopy::Key& key) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    auto entry = m_inputAttachmentImageCopyPipelines.find(key);
    if (entry != m_inputAttachmentImageCopyPipelines.end())
      return entry->second;

    DxvkMetaInputAttachmentImageCopy pipeline = createInputAttachmentCopyPipeline(key);
    m_inputAttachmentImageCopyPipelines.insert({ key, pipeline });
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


  DxvkMetaImageToBufferCopy DxvkMetaCopyObjects::getPipeline(const DxvkMetaImageToBufferCopy::Key& key) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    auto entry = m_imageBufferCopyPipelines.find(key);
    if (entry != m_imageBufferCopyPipelines.end())
      return entry->second;

    DxvkMetaImageToBufferCopy pipeline = createImageToBufferCopyPipeline(key);
    m_imageBufferCopyPipelines.insert({ key, pipeline });
    return pipeline;
  }


  DxvkMetaPackedBufferImageCopy DxvkMetaCopyObjects::getPipeline(const DxvkMetaPackedBufferImageCopy::Key& key) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!m_packedImageBufferCopyPipeline.pipeline)
      m_packedImageBufferCopyPipeline = createPackedImageBufferCopyPipeline(key);

    return m_packedImageBufferCopyPipeline;
  }


  DxvkMetaCopyFormats DxvkMetaCopyObjects::getCopyImageFormats(
          VkFormat              dstFormat,
          VkImageAspectFlags    dstAspect,
          VkFormat              srcFormat,
          VkImageAspectFlags    srcAspect) const {
    auto srcFormatInfo = lookupFormatInfo(srcFormat);
    auto dstFormatInfo = lookupFormatInfo(dstFormat);

    if (dstFormatInfo->flags.test(DxvkFormatFlag::BlockCompressed)
     || srcFormatInfo->flags.test(DxvkFormatFlag::BlockCompressed)) {
      VkFormat uintFormat = dstFormatInfo->elementSize > 8u
        ? VK_FORMAT_R32G32B32A32_UINT
        : VK_FORMAT_R32G32_UINT;

      return { uintFormat, uintFormat };
    }

    if (dstAspect == srcAspect)
      return { dstFormat, dstFormat };

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


  std::vector<uint32_t> DxvkMetaCopyObjects::createVsCopyImage(
    const DxvkPipelineLayout*           layout,
    const DxvkMetaImageCopy::Key&       key) {
    dxbc_spv::ir::Builder builder;
    DxvkBuiltInShader helper(m_device, layout, getName(key, "vs"));

    DxvkBuiltInVertexShader vertex = helper.buildFullscreenVertexShader(builder);
    DxvkMetaImageCopyPushArgs pushArgs = loadImageCopyPushArgs(builder, helper);

    return createCopyToImageVs(builder, helper, vertex, key.srcViewType,
      key.samples, pushArgs.srcOffset, pushArgs.srcExtent, pushArgs.layerIndex);
  }


  std::vector<uint32_t> DxvkMetaCopyObjects::createVsCopyInputAttachment(
    const DxvkPipelineLayout*           layout,
    const DxvkMetaInputAttachmentImageCopy::Key& key) {
    dxbc_spv::ir::Builder builder;
    DxvkBuiltInShader helper(m_device, layout, getName(key, "vs"));

    helper.buildFullscreenVertexShader(builder);

    ir::ResourceKind srcKind = helper.determineResourceKind(key.srcViewType, VK_SAMPLE_COUNT_1_BIT);

    if (ir::resourceIsLayered(srcKind)) {
      DxvkMetaInputAttachmentImageCopyPushArgs pushArgs = loadInputAttachmentImageCopyPushArgs(builder, helper);
      helper.exportBuiltIn(builder, ir::BuiltIn::eLayerIndex, pushArgs.srcLayer);
    }

    return helper.buildShader(builder);
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


  std::vector<uint32_t> DxvkMetaCopyObjects::createPsCopyInputAttachment(
    const DxvkPipelineLayout*           layout,
    const DxvkMetaInputAttachmentImageCopy::Key& key) {
    dxbc_spv::ir::Builder builder;

    DxvkBuiltInShader helper(m_device, layout, getName(key, "ps"));
    helper.buildPixelShader(builder);

    DxvkMetaInputAttachmentImageCopyPushArgs pushArgs = loadInputAttachmentImageCopyPushArgs(builder, helper);

    ir::ResourceKind kind = helper.determineResourceKind(key.dstViewType, VK_SAMPLE_COUNT_1_BIT);
    uint32_t coordDims = ir::resourceCoordComponentCount(kind);

    ir::BasicType coordTypeF(ir::ScalarType::eF32, 4u);
    ir::BasicType coordTypeU(ir::ScalarType::eU32, coordDims);

    ir::SsaDef coord = helper.declareBuiltInInput(builder, coordTypeF, ir::BuiltIn::ePosition);
    coord = helper.emitExtractVector(builder, coord, 0u, coordDims);
    coord = builder.add(ir::Op::ConvertFtoI(coordTypeU, coord));
    coord = builder.add(ir::Op::ISub(coordTypeU, coord, helper.emitExtractVector(builder, pushArgs.srcOffset, 0u, coordDims)));
    coord = builder.add(ir::Op::IAdd(coordTypeU, coord, helper.emitExtractVector(builder, pushArgs.dstOffset, 0u, coordDims)));

    ir::SsaDef layer = resourceIsLayered(kind) ? pushArgs.dstLayer : ir::SsaDef();
    ir::BasicType valueType(helper.determineSampledType(key.dstFormat, VK_IMAGE_ASPECT_COLOR_BIT), 4u);

    ir::SsaDef imgSrc = helper.declareInputTarget(builder, 0u, "img_src",
      key.srcAttachment, key.colorFormats[key.srcAttachment], VK_SAMPLE_COUNT_1_BIT);
    ir::SsaDef imgDst = helper.declareImageUav(builder, 1u, "img_dst",
      key.dstViewType, key.dstFormat);

    ir::SsaDef value = builder.add(ir::Op::InputTargetLoad(valueType, imgSrc, ir::SsaDef()));
    value = helper.emitFormatVector(builder, key.dstFormat, value);

    auto resultType = builder.getOp(value).getType();

    // Emit linear to sRGB conversion as necessary
    auto srcFormatInfo = lookupFormatInfo(key.colorFormats[key.srcAttachment]);
    auto dstFormatInfo = lookupFormatInfo(key.dstFormat);

    ir::SsaDef conversionFunction = { };

    if (srcFormatInfo->flags.test(DxvkFormatFlag::ColorSpaceSrgb)
     && !dstFormatInfo->flags.test(DxvkFormatFlag::ColorSpaceSrgb))
      conversionFunction = helper.buildLinearToSrgbFn(builder, resultType);

    if (conversionFunction)
      value = builder.add(ir::Op::FunctionCall(resultType, conversionFunction).addOperand(value));

    builder.add(ir::Op::ImageStore(imgDst, layer, coord, value));
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

      case VK_FORMAT_B5G6R5_UNORM_PACK16:
      case VK_FORMAT_R5G6B5_UNORM_PACK16:
      case VK_FORMAT_A1B5G5R5_UNORM_PACK16:
      case VK_FORMAT_A1R5G5B5_UNORM_PACK16: {
        ir::SsaDef packed = helper.emitExtractVector(builder, value, 0u, 1u);

        uint32_t bitIndex = 0u;

        // Manually pack to 16-bit uint one component at a time
        bool hasAlpha = key.srcFormat == VK_FORMAT_A1B5G5R5_UNORM_PACK16
                     || key.srcFormat == VK_FORMAT_A1R5G5B5_UNORM_PACK16;

        bool isBgr = key.srcFormat == VK_FORMAT_R5G6B5_UNORM_PACK16
                  || key.srcFormat == VK_FORMAT_A1R5G5B5_UNORM_PACK16;

        small_vector<ir::SsaDef, 4u> components = { };

        for (uint32_t i = 0u; i < (hasAlpha ? 4u : 3u); i++) {
          uint32_t bitCount = 5u;

          if (i == 1u) bitCount = hasAlpha ? 5u : 6u;
          if (i == 3u) bitCount = 1u;

          ir::SsaDef scale = builder.makeConstant(1.0f / float((1u << bitCount) - 1u));

          ir::SsaDef channel = builder.add(ir::Op::UBitExtract(ir::ScalarType::eU32,
            packed, builder.makeConstant(bitIndex), builder.makeConstant(bitCount)));
          channel = builder.add(ir::Op::ConvertItoF(ir::ScalarType::eF32, channel));
          channel = builder.add(ir::Op::FMul(ir::ScalarType::eF32, channel, scale));
          components.push_back(channel);

          bitIndex += bitCount;
        }

        if (isBgr)
          std::swap(components[0u], components[2u]);

        // Pack vector in correct component order
        ir::SsaDef color = components[0u];

        for (uint32_t i = 1u; i < components.size(); i++)
          color = helper.emitConcatVector(builder, color, components[i]);

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


  std::vector<uint32_t> DxvkMetaCopyObjects::createCsCopyImageToBuffer(
    const DxvkPipelineLayout*           layout,
    const DxvkMetaImageToBufferCopy::Key& key) {
    dxbc_spv::ir::Builder builder;

    DxvkBuiltInShader helper(m_device, layout, getName(key, "cs"));
    DxvkBuiltInComputeShader compute = helper.buildComputeShader(builder, determineWorkgroupSize(key));

    ir::SsaDef uav = helper.declareTexelBufferUav(builder, 0u, "buf", key.dstFormat);

    auto srcFormatInfo = lookupFormatInfo(key.srcFormat);

    ir::SsaDef srv0 = helper.declareImageSrv(builder, 1u, "img", key.srcViewType, key.srcFormat,
      VkImageAspectFlagBits(srcFormatInfo->aspectMask & ~VK_IMAGE_ASPECT_STENCIL_BIT), VK_SAMPLE_COUNT_1_BIT);
    ir::SsaDef srv1 = { };

    if (srcFormatInfo->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) {
      srv1 = helper.declareImageSrv(builder, 2u, "img_stencil",
        key.srcViewType, key.srcFormat, VK_IMAGE_ASPECT_STENCIL_BIT, VK_SAMPLE_COUNT_1_BIT);
    }

    ir::BasicType coordType2D(ir::ScalarType::eU32, 2u);
    ir::BasicType coordType3D(ir::ScalarType::eU32, 3u);

    ir::SsaDef dstOffset = helper.declarePushData(builder, coordType3D, offsetof(DxvkMetaImageToBufferCopy::Args, dstOffset), "dst_offset");
    ir::SsaDef dstExtent = helper.declarePushData(builder, coordType2D, offsetof(DxvkMetaImageToBufferCopy::Args, dstExtent), "dst_extent");
    ir::SsaDef srcOffset = helper.declarePushData(builder, coordType3D, offsetof(DxvkMetaImageToBufferCopy::Args, srcOffset), "src_offset");
    ir::SsaDef srcExtent = helper.declarePushData(builder, coordType3D, offsetof(DxvkMetaImageToBufferCopy::Args, srcExtent), "src_extent");

    ir::ResourceKind kind = helper.determineResourceKind(key.srcViewType, VK_SAMPLE_COUNT_1_BIT);
    uint32_t coordDims = ir::resourceCoordComponentCount(kind);

    helper.emitConditionalBlock(builder, helper.emitBoundCheck(
      builder, compute.globalId, srcExtent, coordDims));

    ir::SsaDef layer = ir::resourceIsLayered(kind)
      ? helper.emitExtractVector(builder, compute.globalId, 2u, 1u)
      : ir::SsaDef();

    ir::SsaDef mip = builder.makeConstant(0u);

    ir::SsaDef srcCoord = builder.add(ir::Op::IAdd(coordType3D, srcOffset, compute.globalId));
    ir::SsaDef dstCoord = builder.add(ir::Op::IAdd(coordType3D, dstOffset, compute.globalId));

    srcCoord = helper.emitExtractVector(builder, srcCoord, 0u, coordDims);
    dstCoord = helper.emitExtractVector(builder, dstCoord, 0u, coordDims);

    // Perform actual image loads
    VkImageAspectFlagBits srcAspect = VkImageAspectFlagBits(srcFormatInfo->aspectMask & ~VK_IMAGE_ASPECT_STENCIL_BIT);

    ir::BasicType typeOut = ir::BasicType(helper.determineSampledType(key.dstFormat, VK_IMAGE_ASPECT_COLOR_BIT), 4u);
    ir::BasicType type0 = ir::BasicType(helper.determineSampledType(key.srcFormat, srcAspect), 4u);
    ir::BasicType type1 = { };

    ir::SsaDef value0 = builder.add(ir::Op::ImageLoad(type0, srv0, mip, layer, srcCoord, ir::SsaDef(), ir::SsaDef()));
    ir::SsaDef value1 = { };

    if (srv1) {
      type1 = ir::BasicType(helper.determineSampledType(key.srcFormat, VK_IMAGE_ASPECT_STENCIL_BIT), 4u);
      value1 = builder.add(ir::Op::ImageLoad(type1, srv1, mip, layer, srcCoord, ir::SsaDef(), ir::SsaDef()));
    }

    ir::SsaDef index = flattenBufferImageCoord(builder, helper, dstCoord, layer, dstExtent);
    ir::SsaDef result;

    switch (key.srcFormat) {
      case VK_FORMAT_D16_UNORM: {
        ir::SsaDef zero = builder.makeConstant(0u);

        ir::SsaDef depth = helper.emitExtractVector(builder, value0, 0u, 1u);
        depth = builder.add(ir::Op::FMul(ir::ScalarType::eF32, depth, builder.makeConstant(float(0xffffu))));
        depth = builder.add(ir::Op::ConvertFtoI(ir::ScalarType::eU32, depth));

        result = builder.add(ir::Op::CompositeConstruct(typeOut, depth, zero, zero, zero));
      } break;

      case VK_FORMAT_D16_UNORM_S8_UINT: {
        ir::SsaDef zero = builder.makeConstant(0u);

        ir::SsaDef depth = helper.emitExtractVector(builder, value0, 0u, 1u);
        depth = builder.add(ir::Op::FMul(ir::ScalarType::eF32, depth, builder.makeConstant(float(0xffffu))));
        depth = builder.add(ir::Op::ConvertFtoI(ir::ScalarType::eU32, depth));

        ir::SsaDef stencil = helper.emitExtractVector(builder, value1, 0u, 1u);
        result = builder.add(ir::Op::CompositeConstruct(typeOut, depth, stencil, zero, zero));
      } break;

      case VK_FORMAT_X8_D24_UNORM_PACK32: {
        ir::SsaDef zero = builder.makeConstant(0u);

        ir::SsaDef depth = helper.emitExtractVector(builder, value0, 0u, 1u);
        depth = builder.add(ir::Op::FMul(ir::ScalarType::eF32, depth, builder.makeConstant(float(0xffffffu))));
        depth = builder.add(ir::Op::ConvertFtoI(ir::ScalarType::eU32, depth));

        result = builder.add(ir::Op::CompositeConstruct(typeOut, depth, zero, zero, zero));
      } break;

      case VK_FORMAT_D24_UNORM_S8_UINT: {
        ir::SsaDef zero = builder.makeConstant(0u);

        ir::SsaDef depth = helper.emitExtractVector(builder, value0, 0u, 1u);
        depth = builder.add(ir::Op::FMul(ir::ScalarType::eF32, depth, builder.makeConstant(float(0xffffffu))));
        depth = builder.add(ir::Op::ConvertFtoI(ir::ScalarType::eU32, depth));

        ir::SsaDef stencil = helper.emitExtractVector(builder, value1, 0u, 1u);
        depth = builder.add(ir::Op::IBitInsert(ir::ScalarType::eU32, depth, stencil,
          builder.makeConstant(24u), builder.makeConstant(8u)));

        result = builder.add(ir::Op::CompositeConstruct(typeOut, depth, zero, zero, zero));
      } break;

      case VK_FORMAT_D32_SFLOAT: {
        ir::SsaDef zero = builder.makeConstant(0u);

        ir::SsaDef depth = helper.emitExtractVector(builder, value0, 0u, 1u);
        depth = builder.add(ir::Op::Cast(ir::ScalarType::eU32, depth));

        result = builder.add(ir::Op::CompositeConstruct(typeOut, depth, zero, zero, zero));
      } break;

      case VK_FORMAT_D32_SFLOAT_S8_UINT: {
        ir::SsaDef zero = builder.makeConstant(0u);

        ir::SsaDef depth = helper.emitExtractVector(builder, value0, 0u, 1u);
        depth = builder.add(ir::Op::Cast(ir::ScalarType::eU32, depth));

        ir::SsaDef stencil = helper.emitExtractVector(builder, value1, 0u, 1u);
        result = builder.add(ir::Op::CompositeConstruct(typeOut, depth, stencil, zero, zero));
      } break;

      case VK_FORMAT_A8_UNORM: {
        ir::SsaDef alpha = helper.emitExtractVector(builder, value0, 3u, 1u);
        ir::SsaDef zero = builder.makeConstant(0.0f);

        result = builder.add(ir::Op::CompositeConstruct(type0, alpha, zero, zero, zero));
      } break;

      case VK_FORMAT_B5G6R5_UNORM_PACK16:
      case VK_FORMAT_R5G6B5_UNORM_PACK16:
      case VK_FORMAT_A1B5G5R5_UNORM_PACK16:
      case VK_FORMAT_A1R5G5B5_UNORM_PACK16: {
        uint32_t bitIndex = 0u;

        // Manually pack to 16-bit uint one component at a time
        bool hasAlpha = key.srcFormat == VK_FORMAT_A1B5G5R5_UNORM_PACK16
                     || key.srcFormat == VK_FORMAT_A1R5G5B5_UNORM_PACK16;

        bool isBgr = key.srcFormat == VK_FORMAT_R5G6B5_UNORM_PACK16
                  || key.srcFormat == VK_FORMAT_A1R5G5B5_UNORM_PACK16;

        ir::SsaDef packed = builder.makeConstant(0u);

        for (uint32_t i = 0u; i < (hasAlpha ? 4u : 3u); i++) {
          ir::SsaDef channel = helper.emitExtractVector(builder, value0, i, 1u);

          uint32_t bitCount = 5u;

          if (i == 1u) bitCount = hasAlpha ? 5u : 6u;
          if (i == 3u) bitCount = 1u;

          uint32_t bitShift = bitIndex;

          if (isBgr && i < 3u)
            bitShift = (hasAlpha ? 10u : 11u) - bitShift;

          ir::SsaDef scale = builder.makeConstant(float((1u << bitCount) - 1u));
          channel = builder.add(ir::Op::FMul(ir::ScalarType::eF32, channel, scale));
          channel = builder.add(ir::Op::FRound(ir::ScalarType::eF32, channel, ir::RoundMode::eNearestEven));
          channel = builder.add(ir::Op::ConvertFtoI(ir::ScalarType::eU32, channel));

          packed = builder.add(ir::Op::IBitInsert(ir::ScalarType::eU32, packed, channel,
            builder.makeConstant(bitShift),
            builder.makeConstant(bitCount)));

          bitIndex += bitCount;
        }

        // Need to pad to a vec4
        ir::SsaDef zero = builder.makeConstant(0u);
        result = builder.add(ir::Op::CompositeConstruct(typeOut, packed, zero, zero, zero));
      } break;

      case VK_FORMAT_B8G8R8A8_SRGB:
      case VK_FORMAT_B8G8R8A8_UNORM:
      case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
      case VK_FORMAT_A2R10G10B10_SNORM_PACK32: {
        ir::SsaDef r = helper.emitExtractVector(builder, value0, 0u, 1u);
        ir::SsaDef g = helper.emitExtractVector(builder, value0, 1u, 1u);
        ir::SsaDef b = helper.emitExtractVector(builder, value0, 2u, 1u);
        ir::SsaDef a = helper.emitExtractVector(builder, value0, 3u, 1u);

        result = builder.add(ir::Op::CompositeConstruct(type0, b, g, r, a));
      } break;

      default: {
        // Eliminate components not part of the format
        result = helper.emitFormatVector(builder, key.srcFormat, value0);
      }
    }

    builder.add(ir::Op::ImageStore(uav, layer, index, result));
    return helper.buildShader(builder);
  }


  std::vector<uint32_t> DxvkMetaCopyObjects::createCsCopyPackedBufferImage(
    const DxvkPipelineLayout*           layout,
    const DxvkMetaPackedBufferImageCopy::Key& key) {
    dxbc_spv::ir::Builder builder;

    DxvkBuiltInShader helper(m_device, layout, getName(key, "cs"));
    DxvkBuiltInComputeShader compute = helper.buildComputeShader(builder, determineWorkgroupSize(key));

    // Declare resources. We don't know the exact format, but it's going to be something uint.
    ir::SsaDef uav = helper.declareTexelBufferUav(builder, 0u, "dst_buf", VK_FORMAT_R32G32B32A32_UINT);
    ir::SsaDef srv = helper.declareTexelBufferSrv(builder, 1u, "src_buf", VK_FORMAT_R32G32B32A32_UINT);

    // Declare push data. We always need all three components here.
    ir::BasicType coordType2D(ir::ScalarType::eU32, 2u);
    ir::BasicType coordType3D(ir::ScalarType::eU32, 3u);

    ir::SsaDef dstOffset = helper.declarePushData(builder, coordType3D, offsetof(DxvkMetaPackedBufferImageCopy::Args, dstOffset), "dst_offset");
    ir::SsaDef dstLayout = helper.declarePushData(builder, coordType2D, offsetof(DxvkMetaPackedBufferImageCopy::Args, dstLayout), "dst_layout");
    ir::SsaDef srcOffset = helper.declarePushData(builder, coordType3D, offsetof(DxvkMetaPackedBufferImageCopy::Args, srcOffset), "src_offset");
    ir::SsaDef srcLayout = helper.declarePushData(builder, coordType2D, offsetof(DxvkMetaPackedBufferImageCopy::Args, srcLayout), "src_layout");
    ir::SsaDef extent = helper.declarePushData(builder, coordType3D, offsetof(DxvkMetaPackedBufferImageCopy::Args, extent), "extent");

    helper.emitConditionalBlock(builder, helper.emitBoundCheck(builder, compute.globalId, extent, 3u));

    ir::SsaDef srcCoord = builder.add(ir::Op::IAdd(coordType3D, srcOffset, compute.globalId));
    ir::SsaDef dstCoord = builder.add(ir::Op::IAdd(coordType3D, dstOffset, compute.globalId));

    ir::SsaDef srcIndex = flattenBufferImageCoord(builder, helper, srcCoord, ir::SsaDef(), srcLayout);
    ir::SsaDef dstIndex = flattenBufferImageCoord(builder, helper, dstCoord, ir::SsaDef(), dstLayout);

    ir::BasicType pixelType(ir::ScalarType::eU32, 4u);

    ir::SsaDef value = builder.add(ir::Op::BufferLoad(pixelType, srv, srcIndex, 0u));
    builder.add(ir::Op::BufferStore(uav, dstIndex, value, 0u));

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
      state.colorFormats[0] = dstFormat;
    }

    if ((dstAspects & VK_IMAGE_ASPECT_STENCIL_BIT) && bitwiseStencil) {
      state.dynamicStateCount = dynState.size();
      state.dynamicStates = dynState.data();
    }

    return m_device->createBuiltInGraphicsPipeline(layout, state);
  }


  DxvkMetaInputAttachmentImageCopy DxvkMetaCopyObjects::createInputAttachmentCopyPipeline(const DxvkMetaInputAttachmentImageCopy::Key& key) {
    static const std::array<DxvkDescriptorSetLayoutBinding, 2> bindings = {{
      { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,  1, VK_SHADER_STAGE_FRAGMENT_BIT },
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,     1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};

    // Disable depth-stencil writes
    VkPipelineDepthStencilStateCreateInfo dsState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    dsState.depthTestEnable = VK_FALSE;
    dsState.depthWriteEnable = VK_FALSE;
    dsState.stencilTestEnable = VK_FALSE;

    DxvkMetaInputAttachmentImageCopy pipeline;
    pipeline.layout = m_device->createBuiltInPipelineLayout(0u,
      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
      sizeof(DxvkMetaInputAttachmentImageCopy::Args), bindings.size(), bindings.data());

    auto vsSpirv = createVsCopyInputAttachment(pipeline.layout, key);
    auto psSpirv = createPsCopyInputAttachment(pipeline.layout, key);

    // Disable all color writes
    std::array<VkPipelineColorBlendAttachmentState, MaxNumRenderTargets> cbAttachments = { };

    util::DxvkBuiltInGraphicsState state = { };
    state.vs = vsSpirv;
    state.fs = psSpirv;
    state.dsState = &dsState;
    state.cbAttachment = cbAttachments.data();
    state.sampleCount = VK_SAMPLE_COUNT_1_BIT;
    state.depthFormat = key.depthFormat;
    state.colorFormats = key.colorFormats;

    pipeline.pipeline = m_device->createBuiltInGraphicsPipeline(pipeline.layout, state);
    return pipeline;
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


  DxvkMetaImageToBufferCopy DxvkMetaCopyObjects::createImageToBufferCopyPipeline(
    const DxvkMetaImageToBufferCopy::Key& key) {
    static const std::array<DxvkDescriptorSetLayoutBinding, 3> bindings = {{
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        1, VK_SHADER_STAGE_COMPUTE_BIT },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,        1, VK_SHADER_STAGE_COMPUTE_BIT },
    }};

    DxvkMetaImageToBufferCopy pipeline = { };
    pipeline.layout = m_device->createBuiltInPipelineLayout(0u, VK_SHADER_STAGE_COMPUTE_BIT,
      sizeof(DxvkMetaImageToBufferCopy::Args), bindings.size(), bindings.data());

    auto csSpirv = createCsCopyImageToBuffer(pipeline.layout, key);

    pipeline.pipeline = m_device->createBuiltInComputePipeline(pipeline.layout, csSpirv);
    pipeline.workgroupSize = determineWorkgroupSize(key);
    return pipeline;
  }


  DxvkMetaPackedBufferImageCopy DxvkMetaCopyObjects::createPackedImageBufferCopyPipeline(
    const DxvkMetaPackedBufferImageCopy::Key& key) {
    static const std::array<DxvkDescriptorSetLayoutBinding, 2> bindings = {{
      { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
      { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
    }};

    DxvkMetaPackedBufferImageCopy pipeline = { };
    pipeline.layout = m_device->createBuiltInPipelineLayout(0u, VK_SHADER_STAGE_COMPUTE_BIT,
      sizeof(DxvkMetaPackedBufferImageCopy::Args), bindings.size(), bindings.data());

    auto csSpirv = createCsCopyPackedBufferImage(pipeline.layout, key);

    pipeline.pipeline = m_device->createBuiltInComputePipeline(pipeline.layout, csSpirv);
    pipeline.workgroupSize = determineWorkgroupSize(key);
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


  std::string DxvkMetaCopyObjects::getName(const DxvkMetaInputAttachmentImageCopy::Key& key, const char* type) {
    std::stringstream name;
    name << "meta_" << type << "_copy_input_attachment";
    name << "_" << str::format(key.srcViewType).substr(std::strlen("VK_IMAGE_VIEW_TYPE_"));
    name << "_to_" << str::format(key.dstViewType).substr(std::strlen("VK_IMAGE_VIEW_TYPE_"));
    name << "_" << str::format(key.colorFormats[key.srcAttachment]).substr(std::strlen("VK_FORMAT_"));
    name << "_rt" << key.srcAttachment;
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


  std::string DxvkMetaCopyObjects::getName(const DxvkMetaImageToBufferCopy::Key& key, const char* type) {
    std::stringstream name;
    name << "meta_" << type << "_copy_image_to_buffer";
    name << "_" << str::format(key.srcViewType).substr(std::strlen("VK_IMAGE_VIEW_TYPE_"));
    name << "_" << str::format(key.srcFormat).substr(std::strlen("VK_FORMAT_"));
    return str::tolower(name.str());
  }


  std::string DxvkMetaCopyObjects::getName(const DxvkMetaPackedBufferImageCopy::Key& key, const char* type) {
    std::stringstream name;
    name << "meta_" << type << "_copy_packed_image_buffer";
    return str::tolower(name.str());
  }


  VkExtent3D DxvkMetaCopyObjects::determineWorkgroupSize(const DxvkMetaImageToBufferCopy::Key& key) {
    VkExtent3D workgroupSize = { 64u, 1u, 1u };

    switch (key.srcViewType) {
      case VK_IMAGE_VIEW_TYPE_1D:
      case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
        workgroupSize.width = 64u;
        break;

      case VK_IMAGE_VIEW_TYPE_2D:
      case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
        workgroupSize.width = 8u;
        workgroupSize.height = 8u;
        break;

      case VK_IMAGE_VIEW_TYPE_3D:
        workgroupSize.width = 4u;
        workgroupSize.height = 4u;
        workgroupSize.depth = 4u;
        break;

      default:
        Logger::err(str::format("DxvkMetaClearObjects: Unhandled view type: ", key.srcViewType));
        break;
    }

    return workgroupSize;
  }


  VkExtent3D DxvkMetaCopyObjects::determineWorkgroupSize(const DxvkMetaPackedBufferImageCopy::Key& key) {
    // Use linear workgroup setup for this pipeline since
    // memory is laid out linearly in buffers anyway.
    return VkExtent3D { 64u, 1u, 1u };
  }

}
