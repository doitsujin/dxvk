#include "dxvk_device.h"
#include "dxvk_meta_resolve.h"
#include "dxvk_shader_builtin.h"
#include "dxvk_util.h"

namespace dxvk {

  struct DxvkMetaResolvePushArgs {
    ir::SsaDef srcOffset    = { };
    ir::SsaDef extent       = { };
    ir::SsaDef layer        = { };
    ir::SsaDef stencilBit   = { };
  };


  static DxvkMetaResolvePushArgs loadResolvePushArgs(ir::Builder& builder, DxvkBuiltInShader& helper) {
    ir::BasicType coordType(ir::ScalarType::eU32, 2u);

    DxvkMetaResolvePushArgs result = { };
    result.srcOffset = helper.declarePushData(builder, coordType, offsetof(DxvkMetaResolve::Args, srcOffset), "srcOffset");
    result.extent = helper.declarePushData(builder, coordType, offsetof(DxvkMetaResolve::Args, extent), "extent");
    result.layer = helper.declarePushData(builder, ir::ScalarType::eU32, offsetof(DxvkMetaResolve::Args, layer), "layer");
    result.stencilBit = helper.declarePushData(builder, ir::ScalarType::eU32, offsetof(DxvkMetaResolve::Args, stencilBit), "mask");
    return result;
  }


  static ir::SsaDef buildResolveFunction(
    const DxvkMetaResolve::Key&     key,
          ir::Builder&              builder,
          DxvkBuiltInShader&        helper,
          ir::SsaDef                coordArg,
          ir::SsaDef                layerArg,
          VkImageAspectFlagBits     aspect) {
    // Function return type. Return only one component for depth-stencil
    // so that the result can be exported as-is.
    ir::BasicType resultType(helper.determineSampledType(key.format, aspect),
      aspect == VK_IMAGE_ASPECT_COLOR_BIT ? 4u : 1u);

    VkResolveModeFlagBits mode = (aspect == VK_IMAGE_ASPECT_STENCIL_BIT) ? key.modeStencil : key.mode;

    // Declare new function at the start of the shader
    ir::SsaDef codeStart = { };

    if (builder.getCode().first != builder.getCode().second)
      codeStart = builder.getCode().first->getDef();

    ir::Op functionOp = ir::Op::Function(resultType);

    // Declare coordinate and, if necessary, layer parameters
    ir::SsaDef coord = builder.add(ir::Op::DclParam(ir::BasicType(ir::ScalarType::eU32, 2u)));
    builder.add(ir::Op::DebugName(coord, "coord"));
    functionOp.addOperand(coord);

    ir::SsaDef layer = { };

    if (layerArg) {
      layer = builder.add(ir::Op::DclParam(ir::ScalarType::eU32));
      builder.add(ir::Op::DebugName(layer, "layer"));
      functionOp.addOperand(layer);
    }

    ir::SsaDef function = builder.addBefore(codeStart, functionOp);
    ir::SsaDef cursor = builder.setCursor(function);

    // Debug name for readability
    const char* aspectName = "color";

    if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT)   aspectName = "depth";
    if (aspect == VK_IMAGE_ASPECT_STENCIL_BIT) aspectName = "stencil";

    builder.add(ir::Op::DebugName(function, str::format("resolve_", aspectName).c_str()));
    builder.add(ir::Op::Label());

    // Load function parameters
    coord = builder.add(ir::Op::ParamLoad(builder.getOp(coord).getType(), function, coord));

    if (layer)
      layer = builder.add(ir::Op::ParamLoad(builder.getOp(layer).getType(), function, layer));

    // Declare resource. Stencil uses binding index 1, everything else is 0.
    uint32_t binding = aspect == VK_IMAGE_ASPECT_STENCIL_BIT ? 1u : 0u;

    ir::SsaDef resource = helper.declareImageSrv(builder, binding,
      str::format(aspectName, "_ms").c_str(), key.viewType, key.format, aspect, key.samples);

    // Load first sample as-is, we're going to need it regardless of the resolve mode.
    ir::BasicType pixelType(resultType.getBaseType(), 4u);

    ir::SsaDef accum = builder.add(ir::Op::ImageLoad(pixelType, resource,
      ir::SsaDef(), layer, coord, builder.makeConstant(0u), ir::SsaDef()));

    for (uint32_t i = 1u; i < uint32_t(key.samples); i++) {
      ir::SsaDef sample = builder.add(ir::Op::ImageLoad(pixelType, resource,
        ir::SsaDef(), layer, coord, builder.makeConstant(i), ir::SsaDef()));

      switch (mode) {
        case VK_RESOLVE_MODE_AVERAGE_BIT: {
          accum = builder.add(ir::Op::FAdd(pixelType, sample, accum));
        } break;

        case VK_RESOLVE_MODE_MIN_BIT:
        case VK_RESOLVE_MODE_MAX_BIT: {
          using MinMaxOp = std::pair<ir::ScalarType, std::pair<ir::OpCode, ir::OpCode>>;

          static const std::array<MinMaxOp, 3u> s_ops = {{
            { ir::ScalarType::eF32, { ir::OpCode::eFMin, ir::OpCode::eFMax } },
            { ir::ScalarType::eU32, { ir::OpCode::eUMin, ir::OpCode::eUMax } },
            { ir::ScalarType::eI32, { ir::OpCode::eSMin, ir::OpCode::eSMax } },
          }};

          auto e = std::find_if(s_ops.begin(), s_ops.end(), [pixelType] (const MinMaxOp& op) {
            return op.first == pixelType.getBaseType();
          });

          if (e == s_ops.end()) {
            Logger::err(str::format("DxvkMetaResolveObjects: Unhandled type: ", pixelType));
            break;
          }

          auto opCode = (mode == VK_RESOLVE_MODE_MIN_BIT)
            ? e->second.first : e->second.second;

          accum = builder.add(ir::Op(opCode, pixelType).addOperands(sample, accum));
        } break;

        case VK_RESOLVE_MODE_SAMPLE_ZERO_BIT:
          break;

        default:
          Logger::err(str::format("DxvkMetaResolveObjects: Unhandled resolve mode: ", mode));
          break;
      }
    }

    // For AVERAGE resolves, divide by sample count right away
    if (mode == VK_RESOLVE_MODE_AVERAGE_BIT) {
      float rcpSamples = 1.0f / float(uint32_t(key.samples));

      accum = builder.add(ir::Op::FMul(pixelType, accum,
        builder.makeConstant(rcpSamples, rcpSamples, rcpSamples, rcpSamples)));
    }

    // Only return components that are needed for the format
    accum = helper.emitFormatVector(builder, key.format, accum);
    builder.add(ir::Op::Return(resultType, accum));
    builder.add(ir::Op::FunctionEnd());

    // Emit function call
    builder.setCursor(cursor);

    ir::Op callOp = ir::Op::FunctionCall(resultType, function).addOperand(coordArg);

    if (layerArg)
      callOp.addOperand(layerArg);

    return builder.add(std::move(callOp));
  }


  
  DxvkMetaResolveViews::DxvkMetaResolveViews(
    const Rc<DxvkImage>&            dstImage,
    const VkImageSubresourceLayers& dstSubresources,
          VkFormat                  dstFormat,
    const Rc<DxvkImage>&            srcImage,
    const VkImageSubresourceLayers& srcSubresources,
          VkFormat                  srcFormat,
          VkShaderStageFlags        shaderStage) {
    DxvkImageViewKey viewInfo;
    viewInfo.format = dstFormat;
    viewInfo.aspects = dstSubresources.aspectMask;
    viewInfo.mipIndex = dstSubresources.mipLevel;
    viewInfo.mipCount = 1u;
    viewInfo.layerIndex = dstSubresources.baseArrayLayer;
    viewInfo.layerCount = dstSubresources.layerCount;
    viewInfo.usage = (lookupFormatInfo(dstFormat)->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
      ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
      : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (shaderStage & VK_SHADER_STAGE_COMPUTE_BIT)
      viewInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT;

    viewInfo.viewType = viewType(*dstImage, dstSubresources, viewInfo.usage);

    dstView = dstImage->createView(viewInfo);

    viewInfo.format = srcFormat;
    viewInfo.aspects = srcSubresources.aspectMask;
    viewInfo.mipIndex = srcSubresources.mipLevel;
    viewInfo.layerIndex = srcSubresources.baseArrayLayer;
    viewInfo.layerCount = srcSubresources.layerCount;

    if (shaderStage) {
      viewInfo.aspects &= ~VK_IMAGE_ASPECT_STENCIL_BIT;
      viewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    }

    viewInfo.viewType = viewType(*srcImage, srcSubresources, viewInfo.usage);

    srcView = srcImage->createView(viewInfo);

    if (shaderStage && (srcSubresources.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)) {
      viewInfo.aspects = VK_IMAGE_ASPECT_STENCIL_BIT;
      viewInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

      srcStencilView = srcImage->createView(viewInfo);
    }
  }


  DxvkMetaResolveViews::~DxvkMetaResolveViews() {

  }


  VkImageViewType DxvkMetaResolveViews::viewType(
    const DxvkImage&                image,
    const VkImageSubresourceLayers& subresources,
          VkImageUsageFlags         usage) {
    VkImageUsageFlags rtFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                              | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    bool isLayered = subresources.layerCount > 1u;
    bool isRenderTarget = bool(usage & rtFlags);

    switch (image.info().type) {
      case VK_IMAGE_TYPE_1D: return isLayered ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
      case VK_IMAGE_TYPE_2D: return isLayered ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
      case VK_IMAGE_TYPE_3D: return isRenderTarget ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_3D;
      default: return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    }
  }




  DxvkMetaResolveObjects::DxvkMetaResolveObjects(DxvkDevice* device)
  : m_device(device) {

  }


  DxvkMetaResolveObjects::~DxvkMetaResolveObjects() {
    auto vk = m_device->vkd();

    for (const auto& p : m_pipelines)
      vk->vkDestroyPipeline(vk->device(), p.second.pipeline, nullptr);
  }


  DxvkMetaResolve DxvkMetaResolveObjects::getPipeline(const DxvkMetaResolve::Key& key) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    auto entry = m_pipelines.find(key);

    if (entry != m_pipelines.end())
      return entry->second;

    DxvkMetaResolve pipeline = createPipeline(key);
    m_pipelines.insert({ key, pipeline });
    return pipeline;
  }


  std::vector<uint32_t> DxvkMetaResolveObjects::createVs(
    const DxvkMetaResolve::Key&       key,
    const DxvkPipelineLayout*         layout) {
    dxbc_spv::ir::Builder builder;

    DxvkBuiltInShader helper(m_device, layout, getName(key, "vs"));
    DxvkBuiltInVertexShader vertex = helper.buildFullscreenVertexShader(builder);

    // Only export layer if the resolve is actually layered. The destination
    // layer count must match the source layer count for this to work anyway.
    ir::ResourceKind srcKind = helper.determineResourceKind(key.viewType, key.samples);

    DxvkMetaResolvePushArgs pushArgs = loadResolvePushArgs(builder, helper);

    if (ir::resourceIsLayered(srcKind))
      helper.exportBuiltIn(builder, ir::BuiltIn::eLayerIndex, pushArgs.layer);

    // Apply source offset and extent before exporting image coordinate
    ir::BasicType coordTypeF(ir::ScalarType::eF32, 2u);

    ir::SsaDef coord = builder.add(ir::Op::FMad(coordTypeF, vertex.coord,
      builder.add(ir::Op::ConvertItoF(coordTypeF, pushArgs.extent)),
      builder.add(ir::Op::ConvertItoF(coordTypeF, pushArgs.srcOffset))));

    helper.exportOutput(builder, 0u, coord, "coord");
    return helper.buildShader(builder);
  }


  std::vector<uint32_t> DxvkMetaResolveObjects::createPs(
    const DxvkMetaResolve::Key&       key,
    const DxvkPipelineLayout*         layout) {
    dxbc_spv::ir::Builder builder;

    DxvkBuiltInShader helper(m_device, layout, getName(key, "ps"));
    helper.buildPixelShader(builder);

    DxvkMetaResolvePushArgs pushArgs = loadResolvePushArgs(builder, helper);

    // Load source image coordinate as integers
    ir::ResourceKind kind = helper.determineResourceKind(key.viewType, key.samples);

    ir::BasicType coordTypeF(ir::ScalarType::eF32, 2u);
    ir::BasicType coordTypeU(ir::ScalarType::eU32, 2u);

    ir::SsaDef coord = helper.declareInput(builder, coordTypeF, 0u, "coord");
    coord = builder.add(ir::Op::ConvertFtoI(coordTypeU, coord));

    ir::SsaDef layer = ir::resourceIsLayered(kind) ? pushArgs.layer : ir::SsaDef();

    // Emit code for the actual resolves and export to the respective
    // output, depending on the aspects being resolved.
    auto formatInfo = lookupFormatInfo(key.format);

    if (formatInfo->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) {
      ir::SsaDef value = buildResolveFunction(key, builder, helper, coord, layer, VK_IMAGE_ASPECT_COLOR_BIT);
      helper.exportOutput(builder, 0u, value, "color");
    } else {
      if ((formatInfo->aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) && key.mode != VK_RESOLVE_MODE_NONE) {
        ir::SsaDef value = buildResolveFunction(key, builder, helper, coord, layer, VK_IMAGE_ASPECT_DEPTH_BIT);
        helper.exportBuiltIn(builder, ir::BuiltIn::eDepth, value);
      }

      if ((formatInfo->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) && key.modeStencil != VK_RESOLVE_MODE_NONE) {
        ir::SsaDef value = buildResolveFunction(key, builder, helper, coord, layer, VK_IMAGE_ASPECT_STENCIL_BIT);

        if (key.bitwiseStencil) {
          // In discard mode, check if the current bit is set in the returned stencil mask
          // and discard the fragment if not. This requires that stencil is resolved separately
          // from depth, otherwise this will break the depth resolve.
          ir::SsaDef discardCond = builder.add(ir::Op::IEq(ir::ScalarType::eBool,
            builder.add(ir::Op::IAnd(ir::ScalarType::eU32, value, pushArgs.stencilBit)),
            builder.makeConstant(0u)));

          ir::SsaDef mergeBlock = helper.emitConditionalBlock(builder, discardCond);

          builder.add(ir::Op::Demote());
          builder.setCursor(mergeBlock);
        } else {
          // Export stencil ref directly on devices that support this.
          helper.exportBuiltIn(builder, ir::BuiltIn::eStencilRef, value);
        }
      }
    }

    return helper.buildShader(builder);
  }


  DxvkMetaResolve DxvkMetaResolveObjects::createPipeline(const DxvkMetaResolve::Key& key) {
    static const std::array<DxvkDescriptorSetLayoutBinding, 2> bindings = {{
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
      { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    }};

    static const std::array<VkDynamicState, 1u> dynState = {{
      VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
    }};

    auto formatInfo = lookupFormatInfo(key.format);

    DxvkMetaResolve pipeline = { };
    pipeline.layout = m_device->createBuiltInPipelineLayout(0u,
      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
      sizeof(DxvkMetaResolve::Args), bindings.size(), bindings.data());

    VkStencilOpState stencilOp = { };
    stencilOp.failOp = VK_STENCIL_OP_REPLACE;
    stencilOp.passOp = VK_STENCIL_OP_REPLACE;
    stencilOp.depthFailOp = VK_STENCIL_OP_REPLACE;
    stencilOp.compareOp = VK_COMPARE_OP_ALWAYS;
    stencilOp.writeMask = 0xffu;
    stencilOp.reference = 0xffu;

    VkPipelineDepthStencilStateCreateInfo dsState = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    dsState.depthTestEnable = key.mode != VK_RESOLVE_MODE_NONE;
    dsState.depthWriteEnable = key.mode != VK_RESOLVE_MODE_NONE;
    dsState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    dsState.stencilTestEnable = key.modeStencil != VK_RESOLVE_MODE_NONE;
    dsState.front = stencilOp;
    dsState.back = stencilOp;

    auto vsSpirv = createVs(key, pipeline.layout);
    auto psSpirv = createPs(key, pipeline.layout);

    util::DxvkBuiltInGraphicsState state = { };
    state.vs = vsSpirv;
    state.fs = psSpirv;

    if (formatInfo->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
      state.colorFormats[0] = key.format;

    if (formatInfo->aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
      state.depthFormat = key.format;
      state.dsState = &dsState;
    }

    if ((formatInfo->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) && key.modeStencil != VK_RESOLVE_MODE_NONE && key.bitwiseStencil) {
      // When resolving stencil in discard mode, we essentially write one bit at
      // a time by adjusting the write mask. Add the corresponding dynamic state.
      state.dynamicStateCount = dynState.size();
      state.dynamicStates = dynState.data();
    } else {
      state.colorFormats[0] = key.format;
    }

    pipeline.pipeline = m_device->createBuiltInGraphicsPipeline(pipeline.layout, state);
    return pipeline;
  }


  std::string DxvkMetaResolveObjects::getName(const DxvkMetaResolve::Key& key, const char* type) {
    auto formatInfo = lookupFormatInfo(key.format);

    std::stringstream name;
    name << "meta_" << type << "_resolve";
    name << "_" << str::format(key.viewType).substr(std::strlen("VK_IMAGE_VIEW_TYPE_"));
    name << "_" << str::format(key.format).substr(std::strlen("VK_FORMAT_"));
    name << "_x" << uint32_t(key.samples);

    if (formatInfo->aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
      name << "_color_" << getModeName(key.mode);

    if (formatInfo->aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT)
      name << "_depth_" << getModeName(key.mode);

    if (formatInfo->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
      name << "_stencil_" << getModeName(key.modeStencil);

    return str::tolower(name.str());
  }


  const char* DxvkMetaResolveObjects::getModeName(VkResolveModeFlagBits mode) {
    switch (mode) {
      case VK_RESOLVE_MODE_NONE: return "none";
      case VK_RESOLVE_MODE_SAMPLE_ZERO_BIT: return "sz";
      case VK_RESOLVE_MODE_MIN_BIT: return "min";
      case VK_RESOLVE_MODE_MAX_BIT: return "max";
      case VK_RESOLVE_MODE_AVERAGE_BIT: return "avg";
      default: return "unknown";
    }
  }

}
