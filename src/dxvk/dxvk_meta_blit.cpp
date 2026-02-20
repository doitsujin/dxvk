#include "dxvk_shader_builtin.h"
#include "dxvk_device.h"
#include "dxvk_meta_blit.h"
#include "dxvk_util.h"

namespace dxvk {

  struct DxvkMetaBlitPushArgs {
    ir::SsaDef srcCoord0 = { };
    ir::SsaDef srcCoord1 = { };
    ir::SsaDef sampler = { };
    ir::SsaDef layerIndex = { };
    ir::SsaDef layerCount = { };
  };


  static DxvkMetaBlitPushArgs loadBlitPushArgs(ir::Builder& builder, DxvkBuiltInShader& helper) {
    ir::BasicType coordType(ir::ScalarType::eU32, 3u);

    DxvkMetaBlitPushArgs result = { };
    result.srcCoord0 = helper.declarePushData(builder, coordType, offsetof(DxvkMetaBlit::Args, srcCoord0), "coord0");
    result.srcCoord1 = helper.declarePushData(builder, coordType, offsetof(DxvkMetaBlit::Args, srcCoord1), "coord1");
    result.sampler = helper.declareSampler(builder, offsetof(DxvkMetaBlit::Args, sampler), "filter");
    result.layerIndex = helper.declarePushData(builder, ir::ScalarType::eU32, offsetof(DxvkMetaBlit::Args, layerIndex), "layerIndex");
    result.layerCount = helper.declarePushData(builder, ir::ScalarType::eU32, offsetof(DxvkMetaBlit::Args, layerCount), "layerCount");
    return result;
  }

  
  DxvkMetaBlitObjects::DxvkMetaBlitObjects(DxvkDevice* device)
  : m_device(device) {

  }


  DxvkMetaBlitObjects::~DxvkMetaBlitObjects() {
    auto vk = m_device->vkd();

    for (const auto& p : m_pipelines)
      vk->vkDestroyPipeline(vk->device(), p.second.pipeline, nullptr);
  }


  DxvkMetaBlit DxvkMetaBlitObjects::getPipeline(const DxvkMetaBlit::Key& key) {
    std::lock_guard lock(m_mutex);

    auto entry = m_pipelines.find(key);

    if (entry != m_pipelines.end())
      return entry->second;

    auto pipeline = createPipeline(key);
    m_pipelines.insert({ key, pipeline });
    return pipeline;
  }


  const DxvkPipelineLayout* DxvkMetaBlitObjects::createPipelineLayout() const {
    DxvkDescriptorSetLayoutBinding binding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1u,
      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT);

    return m_device->createBuiltInPipelineLayout(DxvkPipelineLayoutFlag::UsesSamplerHeap,
      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
      sizeof(DxvkMetaBlit::Args), 1, &binding);
  }


  std::vector<uint32_t> DxvkMetaBlitObjects::createVs(const DxvkMetaBlit::Key& key, VkImageAspectFlagBits aspect, const DxvkPipelineLayout* layout) {
    dxbc_spv::ir::Builder builder;

    DxvkBuiltInShader helper(m_device, layout, getName(key, "vs"));
    DxvkBuiltInVertexShader vertex = helper.buildFullscreenVertexShader(builder);

    ir::SsaDef srv = helper.declareImageSrv(builder, 0u, "srcImage",
      key.srcViewType, key.dstFormat, aspect, key.srcSamples);

    ir::ResourceKind srcKind = helper.determineResourceKind(key.srcViewType, key.srcSamples);

    // 3D blits are instanced, so forward the instance ID as the layer index.
    // Arrayed images will use multiple draw calls with a push constant index
    // instead, which we will ignore in the 3D case.
    DxvkMetaBlitPushArgs pushArgs = loadBlitPushArgs(builder, helper);

    ir::SsaDef layer = srcKind == ir::ResourceKind::eImage3D
      ? vertex.instanceIndex : pushArgs.layerIndex;

    helper.exportBuiltIn(builder, ir::BuiltIn::eLayerIndex, layer);

    // Compute texture coordinates to forward to the pixel shader.
    // For 3D images, use the layer index to compute the Z coord.
    uint32_t coordDim = ir::resourceCoordComponentCount(srcKind);

    ir::BasicType coordTypeU(ir::ScalarType::eU32, coordDim);
    ir::BasicType coordTypeF(ir::ScalarType::eF32, coordDim);

    ir::SsaDef coordZ = builder.add(ir::Op::ConvertItoF(ir::ScalarType::eF32, layer));
    coordZ = builder.add(ir::Op::FAdd(ir::ScalarType::eF32, coordZ, builder.makeConstant(0.5f)));
    coordZ = builder.add(ir::Op::FDiv(ir::ScalarType::eF32, coordZ,
      builder.add(ir::Op::ConvertItoF(ir::ScalarType::eF32, pushArgs.layerCount))));

    ir::SsaDef coord = helper.emitConcatVector(builder, vertex.coord, coordZ);
    coord = helper.emitExtractVector(builder, coord, 0u, coordDim);

    // Scale normalized coordinate by the source coordinate points
    ir::SsaDef srcCoord0 = helper.emitExtractVector(builder, pushArgs.srcCoord0, 0u, coordDim);
    ir::SsaDef srcCoord1 = helper.emitExtractVector(builder, pushArgs.srcCoord1, 0u, coordDim);

    srcCoord0 = builder.add(ir::Op::ConvertItoF(coordTypeF, srcCoord0));
    srcCoord1 = builder.add(ir::Op::ConvertItoF(coordTypeF, srcCoord1));

    coord = builder.add(ir::Op::FMad(coordTypeF, coord,
      builder.add(ir::Op::FSub(coordTypeF, srcCoord1, srcCoord0)), srcCoord0));

    // The resolve shader expects raw pixel coordinates, the regular sampling
    // path requires normalized coordinates. Divide by the image size in that
    // case in order to keep the shader interface consistent.
    if (!ir::resourceIsMultisampled(srcKind)) {
      ir::Type infoType = ir::Type()
        .addStructMember(coordTypeU)
        .addStructMember(ir::ScalarType::eU32);

      ir::SsaDef size = builder.add(ir::Op::ImageQuerySize(infoType, srv, builder.makeConstant(0u)));
      size = builder.add(ir::Op::CompositeExtract(coordTypeU, size, builder.makeConstant(0u)));
      size = builder.add(ir::Op::Op::ConvertItoF(coordTypeF, size));

      coord = builder.add(ir::Op::FDiv(coordTypeF, coord, size));
    }

    helper.exportOutput(builder, 0u, coord, "coord");
    return helper.buildShader(builder);
  }


  std::vector<uint32_t> DxvkMetaBlitObjects::createPsSimple(const DxvkMetaBlit::Key& key, VkImageAspectFlagBits aspect, const DxvkPipelineLayout* layout) {
    dxbc_spv::ir::Builder builder;

    DxvkBuiltInShader helper(m_device, layout, getName(key, "ps"));
    helper.buildPixelShader(builder);

    ir::SsaDef srv = helper.declareImageSrv(builder, 0u, "srcImage",
      key.srcViewType, key.dstFormat, aspect, key.srcSamples);

    DxvkMetaBlitPushArgs pushArgs = loadBlitPushArgs(builder, helper);

    // Load texture coordinates that we can feed into the sampler operation
    // as-is. Use array layer from push data for arrayed inputs as necessary.
    ir::ResourceKind srcKind = helper.determineResourceKind(key.srcViewType, key.srcSamples);

    uint32_t coordDim = ir::resourceCoordComponentCount(srcKind);
    ir::BasicType coordType(ir::ScalarType::eF32, coordDim);

    ir::SsaDef coord = helper.declareInput(builder, coordType, 0u, "coord");

    ir::SsaDef layer = ir::resourceIsLayered(srcKind)
      ? builder.add(ir::Op::ConvertItoF(ir::ScalarType::eF32, pushArgs.layerIndex))
      : ir::SsaDef();

    // Sample input texture at LOD 0 with the appropriate component type.
    ir::BasicType pixelType(helper.determineSampledType(key.dstFormat, aspect), 4u);

    ir::SsaDef value = builder.add(ir::Op::ImageSample(pixelType,
      srv, pushArgs.sampler, layer, coord, ir::SsaDef(), builder.makeConstant(0.0f),
      ir::SsaDef(), ir::SsaDef(), ir::SsaDef(), ir::SsaDef(), ir::SsaDef()));

    value = helper.emitFormatVector(builder, key.dstFormat, value);

    if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT)
      helper.exportBuiltIn(builder, ir::BuiltIn::eDepth, value);
    else
      helper.exportOutput(builder, 0u, value, "color");

    return helper.buildShader(builder);
  }


  std::vector<uint32_t> DxvkMetaBlitObjects::createPsResolve(const DxvkMetaBlit::Key& key, VkImageAspectFlagBits aspect, const DxvkPipelineLayout* layout) {
    dxbc_spv::ir::Builder builder;

    DxvkBuiltInShader helper(m_device, layout, getName(key, "ps"));
    DxvkBuiltInPixelShader pixel = helper.buildPixelShader(builder);

    ir::SsaDef srv = helper.declareImageSrv(builder, 0u, "srcImage",
      key.srcViewType, key.dstFormat, aspect, key.srcSamples);

    DxvkMetaBlitPushArgs pushArgs = loadBlitPushArgs(builder, helper);

    // Load interpolated texture coordinates as integers
    ir::ResourceKind srcKind = helper.determineResourceKind(key.srcViewType, key.srcSamples);

    uint32_t coordDim = ir::resourceCoordComponentCount(srcKind);

    ir::BasicType coordTypeF(ir::ScalarType::eF32, coordDim);
    ir::BasicType coordTypeI(ir::ScalarType::eI32, coordDim);

    ir::SsaDef coord = helper.declareInput(builder, coordTypeF, 0u, "coord");
    coord = builder.add(ir::Op::ConvertFtoI(coordTypeI, coord));

    ir::SsaDef layer = ir::resourceIsLayered(srcKind) ? pushArgs.layerIndex : ir::SsaDef();

    // Number of samples to load in per iteration
    uint32_t sampleIterations = std::max(1u, uint32_t(key.srcSamples) / uint32_t(key.dstSamples));

    ir::SsaDef srcSamplesDef = builder.makeConstant(uint32_t(key.srcSamples));
    ir::SsaDef dstSamplesDef = builder.makeConstant(uint32_t(key.dstSamples));

    // Base sample index for the current output sample.
    // This allows partial resolves to be performed.
    ir::SsaDef baseSample = { };

    if (key.dstSamples > VK_SAMPLE_COUNT_1_BIT) {
      baseSample = builder.add(ir::Op::UDiv(ir::ScalarType::eU32,
        builder.add(ir::Op::IMul(ir::ScalarType::eU32, pixel.sampleId, srcSamplesDef)),
        dstSamplesDef));
    }

    // Emit actual sampling loop. Unrolling this should be fine.
    ir::BasicType pixelType(helper.determineSampledType(key.dstFormat, aspect), 4u);

    ir::SsaDef result = { };

    for (uint32_t i = 0u; i < sampleIterations; i++) {
      ir::SsaDef sampleIndex = builder.makeConstant(i);

      if (baseSample)
        sampleIndex = builder.add(ir::Op::IAdd(ir::ScalarType::eU32, baseSample, sampleIndex));

      ir::SsaDef sampleValue = builder.add(ir::Op::ImageLoad(pixelType,
        srv, ir::SsaDef(), layer, coord, sampleIndex, ir::SsaDef()));

      result = result ? builder.add(ir::Op::FAdd(pixelType, result, sampleValue)) : sampleValue;
    }

    // Compute average and export
    ir::SsaDef factor = helper.emitReplicateScalar(builder, pixelType,
      builder.makeConstant(1.0f / float(sampleIterations)));

    result = builder.add(ir::Op::FMul(pixelType, result, factor));
    result = helper.emitFormatVector(builder, key.dstFormat, result);

    if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT)
      helper.exportBuiltIn(builder, ir::BuiltIn::eDepth, result);
    else
      helper.exportOutput(builder, 0u, result, "color");

    return helper.buildShader(builder);
  }


  std::vector<uint32_t> DxvkMetaBlitObjects::createPsSampleMs(const DxvkMetaBlit::Key& key, VkImageAspectFlagBits aspect, const DxvkPipelineLayout* layout) {
    dxbc_spv::ir::Builder builder;

    DxvkBuiltInShader helper(m_device, layout, getName(key, "ps"));
    helper.buildPixelShader(builder);

    ir::SsaDef srv = helper.declareImageSrv(builder, 0u, "srcImage",
      key.srcViewType, key.dstFormat, aspect, key.srcSamples);

    DxvkMetaBlitPushArgs pushArgs = loadBlitPushArgs(builder, helper);

    // Load interpolated texture coordinates. Use sample interpolation
    // for the incoming coordinate to get more accurate results.
    ir::ResourceKind srcKind = helper.determineResourceKind(key.srcViewType, key.srcSamples);

    uint32_t coordDim = ir::resourceCoordComponentCount(srcKind);

    ir::BasicType coordTypeF(ir::ScalarType::eF32, coordDim);
    ir::BasicType coordTypeI(ir::ScalarType::eI32, coordDim);
    ir::BasicType coordTypeU(ir::ScalarType::eU32, coordDim);

    ir::SsaDef coord = helper.declareInput(builder, coordTypeF, 0u, "coord", ir::InterpolationMode::eSample);
    ir::SsaDef layer = ir::resourceIsLayered(srcKind) ? pushArgs.layerIndex : ir::SsaDef();

    // The idea behind this shader is to view the multisampled image as an enlarged,
    // single-sampled surface and perform linear interpolation on that surface. To
    // get reasonably accurate results, we need to take sample locations into account
    // and map each sample to the appropriate coordinate inside the pixel grid.
    static const std::array<SampleProperties, 5u> s_configs = {{
      { VK_SAMPLE_COUNT_1_BIT,  1u, 1u, 0x0000000000000000ull },
      { VK_SAMPLE_COUNT_2_BIT,  2u, 1u, 0x0000000000000001ull },
      { VK_SAMPLE_COUNT_4_BIT,  2u, 2u, 0x0000000000003210ull },
      { VK_SAMPLE_COUNT_8_BIT,  4u, 2u, 0x0000000026147035ull },
      { VK_SAMPLE_COUNT_16_BIT, 4u, 4u, 0xe58b602c3714d9afull },
    }};

    const SampleProperties* config = s_configs.data();

    for (const auto& e : s_configs) {
      if (e.samples == key.srcSamples)
        config = &e;
    }

    // Scale input coordinate by the appropriate amount for the input sample count
    coord = builder.add(ir::Op::FMul(coordTypeF, coord,
      builder.makeConstant(float(config->scaleX), float(config->scaleY))));

    // Split coordinate vector into integral and fractional part. Ignore
    // the latter when using nearest-neighbour filtering.
    ir::SsaDef coordI = builder.add(ir::Op::ConvertFtoI(coordTypeI, coord));
    ir::SsaDef coordF = { };

    if (key.resolveMode == DxvkMetaBlitResolveMode::FilterLinear)
      coordF = builder.add(ir::Op::FFract(coordTypeF, coord));

    // Initialize accumulator value
    ir::BasicType pixelType(helper.determineSampledType(key.dstFormat, aspect), 4u);

    // When using a linear filter, iterate over all four pixels in the quad,
    // otherwise only do a single iteration to handle the first pixel. The
    // loop counter uses a signed type since we feed it into signed arithmetic.
    int32_t loopIterCount = (key.resolveMode == DxvkMetaBlitResolveMode::FilterLinear) ? 4 : 1;

    // Set up blocks for the loop in reverse order
    ir::SsaDef mergeBlock = builder.add(ir::Op::Label());

    ir::SsaDef continueBlock = builder.addBefore(mergeBlock, ir::Op::Label());

    ir::SsaDef loopBody = builder.addBefore(continueBlock, ir::Op::Label());
    ir::SsaDef loopHeader = builder.addBefore(loopBody, ir::Op::LabelLoop(mergeBlock, continueBlock));

    ir::SsaDef loopBranch = builder.addBefore(loopHeader, ir::Op::Branch(loopHeader));
    ir::SsaDef loopParent = ir::findContainingBlock(builder, loopBranch);

    builder.addAfter(continueBlock, ir::Op::Branch(loopHeader));

    // Declare placeholder phis in loop header
    builder.setCursor(loopHeader);

    ir::Op countOp = ir::Op::Phi(ir::ScalarType::eI32).addPhi(loopParent, builder.makeConstant(0));
    ir::Op accumOp = ir::Op::Phi(pixelType).addPhi(loopParent, builder.makeConstantZero(pixelType));

    ir::SsaDef countPhi = builder.add(countOp);
    ir::SsaDef accumPhi = builder.add(accumOp);

    builder.add(ir::Op::Branch(loopBody));

    // Implement actual loop body.
    builder.setCursor(loopBody);

    // Offset integer pixel coordinate for the current iteration
    ir::SsaDef offsetX = builder.add(ir::Op::IAnd(ir::ScalarType::eI32, countPhi, builder.makeConstant(1)));
    ir::SsaDef offsetY = builder.add(ir::Op::SShr(ir::ScalarType::eI32, countPhi, builder.makeConstant(1)));

    coordI = builder.add(ir::Op::IAdd(coordTypeI, coordI,
      builder.add(ir::Op::CompositeConstruct(coordTypeI, offsetX, offsetY))));

    // Compute sample index from the LSB of the integer coordinate
    ir::SsaDef sampleCoord = builder.add(ir::Op::IAnd(coordTypeU,
      builder.add(ir::Op::Cast(coordTypeU, coordI)),
      builder.makeConstant(config->scaleX - 1u, config->scaleY - 1u)));

    ir::SsaDef sampleCoordX = builder.add(ir::Op::CompositeExtract(ir::ScalarType::eU32, sampleCoord, builder.makeConstant(0u)));
    ir::SsaDef sampleCoordY = builder.add(ir::Op::CompositeExtract(ir::ScalarType::eU32, sampleCoord, builder.makeConstant(1u)));

    ir::SsaDef sampleIndex = builder.add(ir::Op::IAdd(ir::ScalarType::eU32, sampleCoordX,
      builder.add(ir::Op::IMul(ir::ScalarType::eU32, sampleCoordY, builder.makeConstant(config->scaleX)))));

    // Look up actual sample index from the sample mapping for the
    // given sample count using the flattened sample index.
    ir::SsaDef sampleMap = builder.makeConstant(config->mapping);
    ir::SsaDef sampleShift = builder.add(ir::Op::IMul(ir::ScalarType::eU32, sampleIndex, builder.makeConstant(4u)));

    sampleIndex = builder.add(ir::Op::UShr(ir::ScalarType::eU64, sampleMap, sampleShift));
    sampleIndex = builder.add(ir::Op::ConvertItoI(ir::ScalarType::eU32, sampleIndex));
    sampleIndex = builder.add(ir::Op::IAnd(ir::ScalarType::eU32, sampleIndex, builder.makeConstant(0xfu)));

    // Compute pixel coordinate from the higher bits of the integer coordinate
    ir::SsaDef pixelCoord = builder.add(ir::Op::SShr(coordTypeI, coordI,
      builder.makeConstant(bit::tzcnt(config->scaleX), bit::tzcnt(config->scaleY))));

    // Read sample from the given pixel coordinate
    ir::SsaDef sampleValue = builder.add(ir::Op::ImageLoad(pixelType,
      srv, ir::SsaDef(), layer, pixelCoord, sampleIndex, ir::SsaDef()));

    // When performing linear filtering, scale the output value using the fractional
    // part of the input coordinate. The selection should be resolved at compile time.
    if (key.resolveMode == DxvkMetaBlitResolveMode::FilterLinear) {
      ir::SsaDef pickX = builder.add(ir::Op::INe(ir::ScalarType::eBool, offsetX, builder.makeConstant(0)));
      ir::SsaDef pickY = builder.add(ir::Op::INe(ir::ScalarType::eBool, offsetY, builder.makeConstant(0)));

      ir::SsaDef factorX = builder.add(ir::Op::CompositeExtract(ir::ScalarType::eF32, coordF, builder.makeConstant(0u)));
      ir::SsaDef factorY = builder.add(ir::Op::CompositeExtract(ir::ScalarType::eF32, coordF, builder.makeConstant(1u)));

      factorX = builder.add(ir::Op::Select(ir::ScalarType::eF32, pickX, factorX,
        builder.add(ir::Op::FSub(ir::ScalarType::eF32, builder.makeConstant(1.0f), factorX))));
      factorY = builder.add(ir::Op::Select(ir::ScalarType::eF32, pickY, factorY,
        builder.add(ir::Op::FSub(ir::ScalarType::eF32, builder.makeConstant(1.0f), factorY))));

      ir::SsaDef factor = builder.add(ir::Op::FMul(ir::ScalarType::eF32, factorX, factorY));
      factor = builder.add(ir::Op::CompositeConstruct(pixelType, factor, factor, factor, factor));

      sampleValue = builder.add(ir::Op::FMul(pixelType, sampleValue, factor));
    }

    // Accumulate pixel value into result
    ir::SsaDef accumDef = pixelType.isFloatType()
      ? builder.add(ir::Op::FAdd(pixelType, accumPhi, sampleValue))
      : builder.add(ir::Op::IAdd(pixelType, accumPhi, sampleValue));

    // Increment loop counter and exit loop if necessary
    ir::SsaDef countDef = builder.add(ir::Op::IAdd(ir::ScalarType::eI32, countPhi, builder.makeConstant(1)));
    ir::SsaDef loopCond = builder.add(ir::Op::SLt(ir::ScalarType::eBool, countDef, builder.makeConstant(loopIterCount)));
    builder.add(ir::Op::BranchConditional(loopCond, continueBlock, mergeBlock));

    // Rewrite phis and export the final pixel value
    builder.setCursor(mergeBlock);

    builder.rewriteOp(countPhi, countOp.addPhi(continueBlock, countDef));
    builder.rewriteOp(accumPhi, accumOp.addPhi(continueBlock, accumDef));

    ir::SsaDef result = helper.emitFormatVector(builder, key.dstFormat, accumDef);

    if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT)
      helper.exportBuiltIn(builder, ir::BuiltIn::eDepth, result);
    else
      helper.exportOutput(builder, 0u, result, "color");

    return helper.buildShader(builder);
  }


  DxvkMetaBlit DxvkMetaBlitObjects::createPipeline(const DxvkMetaBlit::Key& key) {
    DxvkMetaBlit result = { };
    result.layout = createPipelineLayout();

    auto formatInfo = lookupFormatInfo(key.dstFormat);

    VkImageAspectFlagBits aspect = VkImageAspectFlagBits(
      formatInfo->aspectMask & (VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT));

    auto vsSpirv = createVs(key, aspect, result.layout);
    auto psSpirv = (key.srcSamples == VK_SAMPLE_COUNT_1_BIT)
      ? createPsSimple(key, aspect, result.layout)
      : (key.resolveMode == DxvkMetaBlitResolveMode::ResolveAverage
        ? createPsResolve(key, aspect, result.layout)
        : createPsSampleMs(key, aspect, result.layout));

    util::DxvkBuiltInGraphicsState state = { };
    state.vs = vsSpirv;
    state.fs = psSpirv;
    state.sampleCount = key.dstSamples;
    state.colorFormats[0] = aspect == VK_IMAGE_ASPECT_COLOR_BIT ? key.dstFormat : VK_FORMAT_UNDEFINED;
    state.depthFormat = aspect == VK_IMAGE_ASPECT_DEPTH_BIT ? key.dstFormat : VK_FORMAT_UNDEFINED;

    result.pipeline = m_device->createBuiltInGraphicsPipeline(result.layout, state);
    return result;
  }


  std::string DxvkMetaBlitObjects::getName(const DxvkMetaBlit::Key& key, const char* type) {
    std::stringstream name;
    name << "meta_" << type << "_blit";
    name << "_" << str::format(key.srcViewType).substr(std::strlen("VK_IMAGE_VIEW_TYPE_"));
    name << "_" << str::format(key.dstFormat).substr(std::strlen("VK_FORMAT_"));

    if (key.dstSamples > VK_SAMPLE_COUNT_1_BIT)
      name << "_msx" << uint32_t(key.dstSamples);

    if (key.srcSamples > VK_SAMPLE_COUNT_1_BIT) {
      name << "_msx" << uint32_t(key.srcSamples);

      switch (key.resolveMode) {
        case DxvkMetaBlitResolveMode::FilterNearest:  name << "_nearest"; break;
        case DxvkMetaBlitResolveMode::FilterLinear:   name << "_linear"; break;
        case DxvkMetaBlitResolveMode::ResolveAverage: name << "_resolve"; break;
      }
    }

    return str::tolower(name.str());
  }

}
