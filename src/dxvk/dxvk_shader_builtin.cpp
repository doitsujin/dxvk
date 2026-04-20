#include <ir/ir_disasm.h>

#include <ir/passes/ir_pass_cse.h>
#include <ir/passes/ir_pass_remove_unused.h>
#include <ir/passes/ir_pass_scalarize.h>

#include "dxvk_shader_builtin.h"

namespace dxvk {

  DxvkBuiltInResourceMapping::DxvkBuiltInResourceMapping(const DxvkPipelineLayout* layout)
  : m_setIndex(layout->usesSamplerHeap() ? 1u : 0u) {

  }


  DxvkBuiltInResourceMapping::~DxvkBuiltInResourceMapping() {

  }


  spirv::DescriptorBinding DxvkBuiltInResourceMapping::mapDescriptor(
        ir::ScalarType          type,
        uint32_t                regSpace,
        uint32_t                regIndex) {
    spirv::DescriptorBinding result = { };
    result.set = type == ir::ScalarType::eSampler ? 0u : m_setIndex;
    result.binding = regIndex;
    return result;
  }


  uint32_t DxvkBuiltInResourceMapping::mapPushData(ir::ShaderStageMask stages) {
    return 0u;
  }



  DxvkBuiltInShader::DxvkBuiltInShader(
          DxvkDevice*           device,
    const DxvkPipelineLayout*   layout,
    const std::string&          name)
  : m_options(device->getShaderCompileOptions()),
    m_resourceMapping(layout),
    m_name(name) {

  }


  DxvkBuiltInShader::~DxvkBuiltInShader() {

  }


  DxvkBuiltInComputeShader DxvkBuiltInShader::buildComputeShader(
          ir::Builder&          builder,
          VkExtent3D            groupSize) {
    auto entryPoint = declareEntryPoint(builder, ir::ShaderStage::eCompute);

    builder.add(ir::Op::SetCsWorkgroupSize(entryPoint,
      groupSize.width, groupSize.height, groupSize.depth));

    DxvkBuiltInComputeShader context = { };
    context.globalId = declareBuiltInInput(builder, ir::BasicType(ir::ScalarType::eU32, 3u), ir::BuiltIn::eGlobalThreadId);
    context.groupId = declareBuiltInInput(builder, ir::BasicType(ir::ScalarType::eU32, 3u), ir::BuiltIn::eWorkgroupId);
    context.localId = declareBuiltInInput(builder, ir::BasicType(ir::ScalarType::eU32, 3u), ir::BuiltIn::eLocalThreadId);
    context.localIndex = declareBuiltInInput(builder, ir::ScalarType::eU32, ir::BuiltIn::eLocalThreadIndex);
    return context;
  }


  DxvkBuiltInPixelShader DxvkBuiltInShader::buildPixelShader(
          ir::Builder&          builder) {
    declareEntryPoint(builder, ir::ShaderStage::ePixel);

    DxvkBuiltInPixelShader context = { };
    context.sampleId = declareBuiltInInput(builder, ir::ScalarType::eU32, ir::BuiltIn::eSampleId);
    return context;
  }


  DxvkBuiltInVertexShader DxvkBuiltInShader::buildFullscreenVertexShader(
          ir::Builder&          builder) {
    declareEntryPoint(builder, ir::ShaderStage::eVertex);

    auto vertexIndex = declareBuiltInInput(builder, ir::ScalarType::eU32, ir::BuiltIn::eVertexId);
    auto instanceIndex = declareBuiltInInput(builder, ir::ScalarType::eU32, ir::BuiltIn::eInstanceId);

    auto coordX = builder.add(ir::Op::IAnd(ir::ScalarType::eU32, vertexIndex, builder.makeConstant(2u)));
    auto coordY = builder.add(ir::Op::IAnd(ir::ScalarType::eU32, vertexIndex, builder.makeConstant(1u)));

    coordX = builder.add(ir::Op::ConvertItoF(ir::ScalarType::eF32, coordX));
    coordY = builder.add(ir::Op::ConvertItoF(ir::ScalarType::eF32, coordY));
    coordY = builder.add(ir::Op::FMul(ir::ScalarType::eF32, coordY, builder.makeConstant(2.0f)));

    auto position = builder.add(ir::Op::CompositeConstruct(ir::BasicType(ir::ScalarType::eF32, 4u),
      builder.add(ir::Op::FMad(ir::ScalarType::eF32, coordX, builder.makeConstant(2.0f), builder.makeConstant(-1.0f))),
      builder.add(ir::Op::FMad(ir::ScalarType::eF32, coordY, builder.makeConstant(2.0f), builder.makeConstant(-1.0f))),
      builder.makeConstant(0.0f),
      builder.makeConstant(1.0f)));
    exportBuiltIn(builder, ir::BuiltIn::ePosition, position);

    DxvkBuiltInVertexShader context = { };
    context.vertexIndex = vertexIndex;
    context.instanceIndex = instanceIndex;
    context.coord = builder.add(ir::Op::CompositeConstruct(
      ir::BasicType(ir::ScalarType::eF32, 2u), coordX, coordY));
    return context;
  }


  ir::SsaDef DxvkBuiltInShader::declareImageSrv(
          ir::Builder&          builder,
          uint32_t              binding,
    const char*                 name,
          VkImageViewType       viewType,
          VkFormat              viewFormat,
          VkImageAspectFlagBits viewAspect,
          VkSampleCountFlagBits samples) {
    auto entry = findEntryPoint(builder);
    dxbc_spv_assert(entry);

    auto type = determineSampledType(viewFormat, viewAspect);
    auto kind = determineResourceKind(viewType, samples);

    auto resource = builder.add(ir::Op::DclSrv(type, entry, 0u, binding, 1u, kind));
    builder.add(ir::Op::DebugName(resource, name));

    return builder.add(ir::Op::DescriptorLoad(ir::ScalarType::eSrv, resource, ir::SsaDef()));
  }


  ir::SsaDef DxvkBuiltInShader::declareTexelBufferSrv(
          ir::Builder&          builder,
          uint32_t              binding,
    const char*                 name,
          VkFormat              viewFormat) {
    auto entry = findEntryPoint(builder);
    dxbc_spv_assert(entry);

    auto type = determineSampledType(viewFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    auto resource = builder.add(ir::Op::DclSrv(type, entry, 0u, binding, 1u, ir::ResourceKind::eBufferTyped));
    builder.add(ir::Op::DebugName(resource, name));

    return builder.add(ir::Op::DescriptorLoad(ir::ScalarType::eSrv, resource, ir::SsaDef()));
  }


  ir::SsaDef DxvkBuiltInShader::declareBufferSrv(
          ir::Builder&          builder,
          uint32_t              binding,
    const char*                 name,
          ir::BasicType         elementType) {
    auto entry = findEntryPoint(builder);
    dxbc_spv_assert(entry);

    auto type = ir::Type(elementType).addArrayDimension(0u);

    auto resource = builder.add(ir::Op::DclSrv(type, entry, 0u, binding, 1u, ir::ResourceKind::eBufferStructured));
    builder.add(ir::Op::DebugName(resource, name));

    return builder.add(ir::Op::DescriptorLoad(ir::ScalarType::eSrv, resource, ir::SsaDef()));
  }


  ir::SsaDef DxvkBuiltInShader::declareImageUav(
          ir::Builder&          builder,
          uint32_t              binding,
    const char*                 name,
          VkImageViewType       viewType,
          VkFormat              viewFormat) {
    auto entry = findEntryPoint(builder);
    dxbc_spv_assert(entry);

    auto type = determineSampledType(viewFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    auto kind = determineResourceKind(viewType, VK_SAMPLE_COUNT_1_BIT);

    auto resource = builder.add(ir::Op::DclUav(type, entry,
      0u, binding, 1u, kind, ir::UavFlag::eWriteOnly));
    builder.add(ir::Op::DebugName(resource, name));

    return builder.add(ir::Op::DescriptorLoad(ir::ScalarType::eUav, resource, ir::SsaDef()));
  }


  ir::SsaDef DxvkBuiltInShader::declareTexelBufferUav(
          ir::Builder&          builder,
          uint32_t              binding,
    const char*                 name,
          VkFormat              viewFormat) {
    auto entry = findEntryPoint(builder);
    dxbc_spv_assert(entry);

    auto type = determineSampledType(viewFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    auto resource = builder.add(ir::Op::DclUav(type, entry,
      0u, binding, 1u, ir::ResourceKind::eBufferTyped, ir::UavFlag::eWriteOnly));
    builder.add(ir::Op::DebugName(resource, name));

    return builder.add(ir::Op::DescriptorLoad(ir::ScalarType::eUav, resource, ir::SsaDef()));
  }


  ir::SsaDef DxvkBuiltInShader::declareBufferUav(
          ir::Builder&          builder,
          uint32_t              binding,
    const char*                 name,
          ir::BasicType         elementType) {
    auto entry = findEntryPoint(builder);
    dxbc_spv_assert(entry);

    auto type = ir::Type(elementType).addArrayDimension(0u);

    auto resource = builder.add(ir::Op::DclUav(type, entry,
      0u, binding, 1u, ir::ResourceKind::eBufferStructured, ir::UavFlag::eWriteOnly));
    builder.add(ir::Op::DebugName(resource, name));

    return builder.add(ir::Op::DescriptorLoad(ir::ScalarType::eUav, resource, ir::SsaDef()));
  }


  ir::SsaDef DxvkBuiltInShader::declareSampler(
          ir::Builder&          builder,
          uint32_t              pushDataOffset,
    const char*                 name) {
    // Declare sampler index as regular push data
    ir::SsaDef index = declarePushData(builder, ir::ScalarType::eU32, pushDataOffset, name);

    // Declare actual sampler heap as an unsized descriptor
    // array, if we don't have one already
    ir::SsaDef samplerHeap = { };

    auto decl = builder.getDeclarations();

    for (auto i = decl.first; i != decl.second && !samplerHeap; i++) {
      if (i->getOpCode() == ir::OpCode::eDclSampler)
        samplerHeap = i->getDef();
    }

    if (!samplerHeap) {
      auto entry = findEntryPoint(builder);
      samplerHeap = builder.add(ir::Op::DclSampler(entry, 0u, 0u, 0u));
    }

    // Emit descriptor load using the loaded index
    return builder.add(ir::Op::DescriptorLoad(ir::ScalarType::eSampler, samplerHeap, index));
  }


  ir::SsaDef DxvkBuiltInShader::declarePushData(
          ir::Builder&          builder,
          ir::BasicType         type,
          uint32_t              pushDataOffset,
    const char*                 name) {
    auto entry = builder.getOp(findEntryPoint(builder));
    auto stage = ir::ShaderStageMask(ir::ShaderStage(entry.getOperand(1u)));

    if (stage != ir::ShaderStage::eCompute)
      stage |= ir::ShaderStage::eVertex | ir::ShaderStage::ePixel;

    auto pushVar = builder.add(ir::Op::DclPushData(type, entry.getDef(), pushDataOffset, stage));
    builder.add(ir::Op::DebugName(pushVar, name));

    return splitLoad(builder, ir::OpCode::ePushDataLoad, pushVar);
  }


  ir::SsaDef DxvkBuiltInShader::declareBuiltInInput(
          ir::Builder&          builder,
          ir::BasicType         type,
          ir::BuiltIn           builtIn,
          ir::InterpolationModes interpolation) {
    auto entry = builder.getOp(findEntryPoint(builder));
    dxbc_spv_assert(entry);

    std::stringstream name;
    name << builtIn;

    ir::SsaDef inputVar = { };

    if (ir::ShaderStage(entry.getOperand(1u)) == ir::ShaderStage::ePixel) {
      if (!type.isFloatType())
        interpolation = ir::InterpolationMode::eFlat;

      inputVar = builder.add(ir::Op::DclInputBuiltIn(type, entry.getDef(), builtIn, interpolation));
    } else {
      inputVar = builder.add(ir::Op::DclInputBuiltIn(type, entry.getDef(), builtIn));
    }

    builder.add(ir::Op::DebugName(inputVar, name.str().c_str()));
    return splitLoad(builder, ir::OpCode::eInputLoad, inputVar);
  }


  ir::SsaDef DxvkBuiltInShader::declareInput(
          ir::Builder&          builder,
          ir::BasicType         type,
          uint32_t              location,
    const char*                 name,
          ir::InterpolationModes interpolation) {
    auto entry = builder.getOp(findEntryPoint(builder));
    dxbc_spv_assert(entry);

    ir::SsaDef inputVar = { };

    if (ir::ShaderStage(entry.getOperand(1u)) == ir::ShaderStage::ePixel) {
      if (!type.isFloatType())
        interpolation = ir::InterpolationMode::eFlat;

      inputVar = builder.add(ir::Op::DclInput(type, entry.getDef(), location, 0u, interpolation));
    } else {
      inputVar = builder.add(ir::Op::DclInput(type, entry.getDef(), location, 0u));
    }

    builder.add(ir::Op::DebugName(inputVar, name));
    return splitLoad(builder, ir::OpCode::eInputLoad, inputVar);
  }


  void DxvkBuiltInShader::exportBuiltIn(
          ir::Builder&          builder,
          ir::BuiltIn           builtIn,
          ir::SsaDef            value) {
    auto entry = findEntryPoint(builder);
    auto type = builder.getOp(value).getType();

    std::stringstream name;
    name << builtIn;

    auto outputVar = builder.add(ir::Op::DclOutputBuiltIn(type, entry, builtIn));
    builder.add(ir::Op::DebugName(outputVar, name.str().c_str()));

    splitStore(builder, ir::OpCode::eOutputStore, outputVar, value);
  }


  void DxvkBuiltInShader::exportOutput(
          ir::Builder&          builder,
          uint32_t              location,
          ir::SsaDef            value,
    const char*                 name) {
    auto entry = findEntryPoint(builder);
    auto type = builder.getOp(value).getType();

    auto outputVar = builder.add(ir::Op::DclOutput(type, entry, location, 0u));
    builder.add(ir::Op::DebugName(outputVar, name));

    splitStore(builder, ir::OpCode::eOutputStore, outputVar, value);
  }


  ir::SsaDef DxvkBuiltInShader::emitBoundCheck(
          ir::Builder&          builder,
          ir::SsaDef            coord,
          ir::SsaDef            size,
          uint32_t              dims) {
    auto coordType = builder.getOp(coord).getType().getBaseType(0u);
    auto sizeType = builder.getOp(size).getType().getBaseType(0u);

    dxbc_spv_assert(dims <= coordType.getVectorSize());
    dxbc_spv_assert(dims <= sizeType.getVectorSize());

    ir::SsaDef cond = { };

    for (uint32_t i = 0u; i < dims; i++) {
      auto scalarCoord = coord;
      auto scalarSize = size;

      if (coordType.isVector()) {
        scalarCoord = builder.add(ir::Op::CompositeExtract(
          coordType.getBaseType(), coord, builder.makeConstant(i)));
      }

      if (sizeType.isVector()) {
        scalarSize = builder.add(ir::Op::CompositeExtract(
          sizeType.getBaseType(), size, builder.makeConstant(i)));
      }

      auto scalarCond = builder.add(ir::Op::ULt(ir::ScalarType::eBool, scalarCoord, scalarSize));
      cond = cond ? builder.add(ir::Op::BAnd(ir::ScalarType::eBool, cond, scalarCond)) : scalarCond;
    }

    return cond;
  }


  ir::SsaDef DxvkBuiltInShader::emitConditionalBlock(
          ir::Builder&          builder,
          ir::SsaDef            cond) {
    // Build everything upside down since it's easier that way
    auto mergeBlock = builder.add(ir::Op::Label());
    auto mergeBranch = builder.addBefore(mergeBlock, ir::Op::Branch(mergeBlock));

    auto condBlock = builder.addBefore(mergeBranch, ir::Op::Label());
    auto condBranch = builder.addBefore(condBlock,
      ir::Op::BranchConditional(cond, condBlock, mergeBlock));

    // Find current label to rewrite it as a structured selection later
    auto currBlock = ir::findContainingBlock(builder, condBranch);
    builder.rewriteOp(currBlock, ir::Op::LabelSelection(mergeBlock));

    builder.setCursor(condBlock);
    return mergeBlock;
  }


  ir::SsaDef DxvkBuiltInShader::emitExtractVector(
          ir::Builder&          builder,
          ir::SsaDef            vector,
          uint32_t              first,
          uint32_t              count) {
    auto vectorType = builder.getOp(vector).getType().getBaseType(0u);
    auto scalarType = vectorType.getBaseType();

    dxbc_spv_assert(first + count <= vectorType.getVectorSize());

    if (count == vectorType.getVectorSize())
      return vector;

    ir::Op compositeOp(ir::Op::CompositeConstruct(ir::BasicType(scalarType, count)));

    for (uint32_t i = first; i < first + count; i++) {
      compositeOp.addOperand(builder.add(ir::Op::CompositeExtract(
        scalarType, vector, builder.makeConstant(i))));
    }

    if (count == 1u)
      return ir::SsaDef(compositeOp.getOperand(0u));

    return builder.add(std::move(compositeOp));
  }


  ir::SsaDef DxvkBuiltInShader::emitConcatVector(
          ir::Builder&          builder,
          ir::SsaDef            a,
          ir::SsaDef            b) {
    auto aType = builder.getOp(a).getType().getBaseType(0u);
    auto bType = builder.getOp(b).getType().getBaseType(0u);

    dxbc_spv_assert(aType.getVectorSize() + bType.getVectorSize() <= 4u);

    auto type = ir::BasicType(aType.getBaseType(),
      aType.getVectorSize() + bType.getVectorSize());

    ir::Op compositeOp(ir::OpCode::eCompositeConstruct, type);

    std::array<ir::SsaDef, 2u> defs = { a, b };

    for (auto def : defs) {
      auto ty = builder.getOp(def).getType().getBaseType(0u);

      if (ty.isVector()) {
        for (uint32_t i = 0u; i < ty.getVectorSize(); i++) {
          compositeOp.addOperand(builder.add(ir::Op::CompositeExtract(
            ty.getBaseType(), def, builder.makeConstant(i))));
        }
      } else {
        compositeOp.addOperand(def);
      }
    }

    return builder.add(std::move(compositeOp));
  }


  ir::SsaDef DxvkBuiltInShader::emitReplicateScalar(
          ir::Builder&          builder,
          ir::BasicType         type,
          ir::SsaDef            value) {
    if (type.isScalar())
      return value;

    ir::Op compositeOp(ir::OpCode::eCompositeConstruct, type);

    for (uint32_t i = 0u; i < type.getVectorSize(); i++)
      compositeOp.addOperand(value);

    return builder.add(std::move(compositeOp));
  }


  ir::SsaDef DxvkBuiltInShader::emitFormatVector(
          ir::Builder&          builder,
          VkFormat              format,
          ir::SsaDef            a) {
    auto formatInfo = lookupFormatInfo(format);

    if (formatInfo->aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
      return emitExtractVector(builder, a, 0u, 1u);

    ir::BasicType type = builder.getOp(a).getType().getBaseType(0u);

    ir::Op compositeOp(ir::OpCode::eCompositeConstruct,
      ir::BasicType(type.getBaseType(), 4u));

    for (uint32_t i = 0u; i < 4u; i++) {
      if (i < type.getVectorSize() && (formatInfo->componentMask & (1u << i)))
        compositeOp.addOperand(emitExtractVector(builder, a, i, 1u));
      else
        compositeOp.addOperand(builder.makeConstantZero(type.getBaseType()));
    }

    return builder.add(std::move(compositeOp));
  }


  ir::SsaDef DxvkBuiltInShader::buildLinearToSrgbFn(
          ir::Builder&          builder,
    const ir::Type&             type) {
    dxbc_spv_assert(type.isBasicType());
    ir::BasicType vectorType = type.getBaseType(0u);

    // Declare input parameter and the actual function
    ir::SsaDef param = builder.add(ir::Op::DclParam(vectorType));
    builder.add(ir::Op::DebugName(param, "lin"));

    dxbc_spv_assert(builder.getCode().first != builder.end());
    ir::SsaDef function = builder.addBefore(builder.getCode().first->getDef(),
      ir::Op::Function(vectorType).addOperand(param));
    builder.add(ir::Op::DebugName(function, str::format("linear_to_srgb_", vectorType).c_str()));

    ir::SsaDef cursor = builder.setCursor(function);
    builder.add(ir::Op::Label());

    // Extract color and alpha from given parameters
    ir::SsaDef color = builder.add(ir::Op::ParamLoad(vectorType, function, param));
    ir::SsaDef alpha = ir::SsaDef();

    if (vectorType.getVectorSize() > 3u) {
      alpha = emitExtractVector(builder, color, 3u, 1u);
      color = emitExtractVector(builder, color, 0u, 3u);
    }

    ir::BasicType colorType = builder.getOp(color).getType().getBaseType(0u);
    ir::BasicType condType(ir::ScalarType::eBool, colorType.getVectorSize());

    ir::SsaDef isLo = builder.add(ir::Op::FLe(condType, color,
      makeConstantVector(builder, builder.makeConstant(0.0031308f), colorType)));

    ir::SsaDef loPart = builder.add(ir::Op::FMul(colorType, color,
      makeConstantVector(builder, builder.makeConstant(12.92f), colorType)));

    ir::SsaDef hiPart = builder.add(ir::Op::FMad(colorType,
      builder.add(ir::Op::FPow(colorType, color,
        makeConstantVector(builder, builder.makeConstant(5.0f / 12.0f), colorType))),
      makeConstantVector(builder, builder.makeConstant(1.055f), colorType),
      makeConstantVector(builder, builder.makeConstant(-0.055f), colorType)));

    ir::SsaDef value = builder.add(ir::Op::Select(colorType, isLo, loPart, hiPart));

    if (alpha)
      value = emitConcatVector(builder, value, alpha);

    builder.add(ir::Op::Return(vectorType, value));
    builder.add(ir::Op::FunctionEnd());

    builder.setCursor(cursor);
    return function;
  }


  void DxvkBuiltInShader::printShader(
          LogLevel              level,
          ir::Builder&          builder) {
    if (level < Logger::logLevel())
      return;

    ir::Disassembler::Options options = { };
    options.useDebugNames = true;
    options.useEnumNames = true;
    options.resolveConstants = true;
    options.showConstants = false;
    options.showDebugNames = false;
    options.sortDeclarative = true;
    options.showDivergence = true;
    options.coloredOutput = false;

    ir::Disassembler disasm(builder, options);

    for (const auto& op : builder)
      Logger::log(level, disasm.disassembleOp(op));
  }


  std::vector<uint32_t> DxvkBuiltInShader::buildShader(
          ir::Builder&          builder) {
    // Run minimal set of passes to clean up vectors and unused I/O
    ir::ScalarizePass::runPass(builder, ir::ScalarizePass::Options());

    ir::CsePass::Options cseOptions = { };
    cseOptions.relocateDescriptorLoad = true;

    do {
      ir::RemoveUnusedPass::runPass(builder);
    } while (ir::CsePass::runPass(builder, cseOptions));

    ir::RemoveUnusedPass::runRemoveUnusedFloatModePass(builder);

    // Ignore float control stuff for built-in shaders,
    // these should generally not be necessary.
    spirv::SpirvBuilder::Options spirvOptions = { };
    spirvOptions.includeDebugNames = true;

    spirv::SpirvBuilder spirvBuilder(builder, m_resourceMapping, spirvOptions);
    spirvBuilder.buildSpirvBinary();

    auto spirvBinary = spirvBuilder.getSpirvBinary();

    dumpShader(spirvBinary.size(), spirvBinary.data());
    printShader(LogLevel::Debug, builder);
    return spirvBinary;
  }


  ir::ScalarType DxvkBuiltInShader::determineSampledType(
          VkFormat              format,
          VkImageAspectFlagBits aspect) {
    auto formatInfo = lookupFormatInfo(format);
    dxbc_spv_assert(formatInfo);

    if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT)
      return ir::ScalarType::eF32;

    if (aspect == VK_IMAGE_ASPECT_STENCIL_BIT)
      return ir::ScalarType::eU32;

    if (formatInfo->flags.test(DxvkFormatFlag::SampledSInt))
      return ir::ScalarType::eI32;

    if (formatInfo->flags.test(DxvkFormatFlag::SampledUInt))
      return ir::ScalarType::eU32;

    return ir::ScalarType::eF32;
  }


  ir::ResourceKind DxvkBuiltInShader::determineResourceKind(
          VkImageViewType       viewType,
          VkSampleCountFlagBits samples) {
    switch (viewType) {
      case VK_IMAGE_VIEW_TYPE_1D:
        return ir::ResourceKind::eImage1D;

      case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
        return ir::ResourceKind::eImage1DArray;

      case VK_IMAGE_VIEW_TYPE_2D:
        return samples > VK_SAMPLE_COUNT_1_BIT
          ? ir::ResourceKind::eImage2DMS
          : ir::ResourceKind::eImage2D;

      case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
        return samples > VK_SAMPLE_COUNT_1_BIT
          ? ir::ResourceKind::eImage2DMSArray
          : ir::ResourceKind::eImage2DArray;

      case VK_IMAGE_VIEW_TYPE_CUBE:
        return ir::ResourceKind::eImageCube;

      case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
        return ir::ResourceKind::eImageCubeArray;

      case VK_IMAGE_VIEW_TYPE_3D:
        return ir::ResourceKind::eImage3D;

      default:
        dxbc_spv_unreachable();
        return ir::ResourceKind();
    }
  }


  ir::SsaDef DxvkBuiltInShader::findEntryPoint(
          ir::Builder&          builder) {
    auto decl = builder.getDeclarations();

    for (auto i = decl.first; i != decl.second; i++) {
      if (i->getOpCode() == ir::OpCode::eEntryPoint)
        return i->getDef();
    }

    return ir::SsaDef();
  }


  ir::SsaDef DxvkBuiltInShader::findEntryPointFunction(
          ir::Builder&          builder) {
    auto entry = builder.getOp(findEntryPoint(builder));
    dxbc_spv_assert(entry);

    return ir::SsaDef(entry.getOperand(0u));
  }


  ir::SsaDef DxvkBuiltInShader::declareEntryPoint(
          ir::Builder&          builder,
          ir::ShaderStage       stage) {
    auto function = builder.add(ir::Op::Function(ir::Type()));
    builder.add(ir::Op::DebugName(function, "main"));

    auto label = builder.add(ir::Op::Label());
    builder.add(ir::Op::Return());
    builder.add(ir::Op::FunctionEnd());

    auto entryPoint = builder.add(ir::Op::EntryPoint(function, stage));
    builder.add(ir::Op::DebugName(entryPoint, m_name.c_str()));
    builder.add(ir::Op::SetFpMode(entryPoint, ir::ScalarType::eF32,
      ir::OpFlags(), ir::RoundMode::eNearestEven, ir::DenormMode::eFlush));

    builder.setCursor(label);
    return entryPoint;
  }


  ir::SsaDef DxvkBuiltInShader::splitLoad(
          ir::Builder&          builder,
          ir::OpCode            opCode,
          ir::SsaDef            def) {
    auto type = builder.getOp(def).getType().getBaseType(0u);

    if (!type.isVector())
      return builder.add(ir::Op(opCode, type).addOperands(def, ir::SsaDef()));

    ir::Op compositeOp(ir::OpCode::eCompositeConstruct, type);

    for (uint32_t i = 0u; i < type.getVectorSize(); i++) {
      compositeOp.addOperand(builder.add(ir::Op(opCode, type.getBaseType())
        .addOperands(def, builder.makeConstant(i))));
    }

    return builder.add(std::move(compositeOp));
  }


  void DxvkBuiltInShader::splitStore(
          ir::Builder&          builder,
          ir::OpCode            opCode,
          ir::SsaDef            var,
          ir::SsaDef            value) {
    auto type = builder.getOp(var).getType().getBaseType(0u);
    dxbc_spv_assert(type == builder.getOp(value).getType().getBaseType(0u));

    if (!type.isVector()) {
      builder.add(ir::Op(opCode, ir::Type()).addOperands(var, ir::SsaDef(), value));
    } else {
      for (uint32_t i = 0u; i < type.getVectorSize(); i++) {
        auto index = builder.makeConstant(i);

        auto scalar = builder.add(ir::Op::CompositeExtract(
          type.getBaseType(), value, index));

        builder.add(ir::Op(opCode, ir::Type()).addOperands(var, index, scalar));
      }
    }
  }


  ir::SsaDef DxvkBuiltInShader::makeConstantVector(
          ir::Builder&          builder,
          ir::SsaDef            constant,
          ir::BasicType         type) {
    const ir::Op& op = builder.getOp(constant);

    dxbc_spv_assert(op.isConstant());
    dxbc_spv_assert(op.getType().getBaseType(0u) == type);

    ir::Op vector(ir::OpCode::eConstant, type);

    for (uint32_t i = 0u; i < type.getVectorSize(); i++)
      vector.addOperand(op.getOperand(0u));

    return builder.add(std::move(vector));
  }


  void DxvkBuiltInShader::dumpShader(
          size_t                size,
    const uint32_t*             dwords) {
    const auto& dumpPath = DxvkShader::getShaderDumpPath();

    if (m_name.empty() || dumpPath.empty())
      return;

    std::ofstream file(str::topath(str::format(dumpPath, "/", m_name, ".spv").c_str()).c_str(), std::ios_base::trunc | std::ios_base::binary);
    file.write(reinterpret_cast<const char*>(dwords), size * sizeof(*dwords));
  }

}
