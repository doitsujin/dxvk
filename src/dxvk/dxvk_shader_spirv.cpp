#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "dxvk_device.h"
#include "dxvk_shader_spirv.h"

namespace dxvk {

  DxvkSpirvShader::DxvkSpirvShader(
    const DxvkSpirvShaderCreateInfo&  info,
          SpirvCodeBuffer&&           spirv)
  : m_info(info), m_layout(getShaderStage(spirv)) {
    m_info.bindings = nullptr;

    SpirvCodeBuffer code = std::move(spirv);
    m_metadata.stage = VkShaderStageFlagBits(m_layout.getStageMask());
    m_metadata.flatShadingInputs = info.flatShadingInputs;
    m_metadata.rasterizedStream = info.xfbRasterizedStream;
    m_metadata.patchVertexCount = info.patchVertexCount;

    // Copy resource binding slot infos
    for (uint32_t i = 0; i < info.bindingCount; i++) {
      DxvkShaderDescriptor descriptor(info.bindings[i], m_metadata.stage);
      m_layout.addBindings(1, &descriptor);
    }

    // Run an analysis pass over the SPIR-V code to gather some
    // info that we may need during pipeline compilation.
    gatherIdOffsets(code);
    gatherMetadata(code);

    if (m_info.samplerHeap.getStageMask() & m_metadata.stage) {
      m_layout.addSamplerHeap(DxvkShaderBinding(m_metadata.stage,
        info.samplerHeap.getSet(),
        info.samplerHeap.getBinding()));
    }

    if (m_debugName.empty())
      m_debugName = std::to_string(getCookie());

    m_code = SpirvCompressedBuffer(code);
  }


  DxvkSpirvShader::~DxvkSpirvShader() {

  }


  DxvkShaderMetadata DxvkSpirvShader::getShaderMetadata() {
    return m_metadata;
  }


  void DxvkSpirvShader::compile() {
    // No-op sice SPIR-V is already compiled
  }


  SpirvCodeBuffer DxvkSpirvShader::getCode(
    const DxvkShaderBindingMap*       bindings,
    const DxvkShaderLinkage*          linkage) {
    SpirvCodeBuffer spirvCode = m_code.decompress();
    patchResourceBindingsAndIoLocations(spirvCode, bindings, linkage);

    // Undefined I/O handling is coarse, and not supported for tessellation shaders.
    if (linkage) {
      uint32_t undefinedInputs = 0u;

      if (m_metadata.stage != VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
        auto producedMask = linkage->prevStageOutputs.computeMask();
        auto consumedMask = m_metadata.inputs.computeMask();

        auto definedMask = producedMask & consumedMask;
        undefinedInputs = definedMask ^ consumedMask;
      }

      // Replace undefined input variables with zero
      for (uint32_t u : bit::BitMask(undefinedInputs))
        eliminateInput(spirvCode, u);

      // Patch primitive topology as necessary
      if (m_metadata.stage == VK_SHADER_STAGE_GEOMETRY_BIT
       && linkage->inputTopology != m_metadata.inputTopology
       && linkage->inputTopology != VK_PRIMITIVE_TOPOLOGY_MAX_ENUM)
        patchInputTopology(spirvCode, linkage->inputTopology);

      // Emit fragment shader swizzles as necessary
      if (m_metadata.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
        emitOutputSwizzles(spirvCode, m_metadata.outputs.computeMask(), linkage->rtSwizzles.data());

      // Emit input decorations for flat shading as necessary
      if (m_metadata.stage == VK_SHADER_STAGE_FRAGMENT_BIT && linkage->fsFlatShading)
        emitFlatShadingDeclarations(spirvCode, m_info.flatShadingInputs);
    }

    return spirvCode;
  }


  DxvkPipelineLayoutBuilder DxvkSpirvShader::getLayout() {
    return m_layout;
  }


  void DxvkSpirvShader::dump(std::ostream& outputStream) {
    m_code.decompress().store(outputStream);
  }


  std::string DxvkSpirvShader::debugName() {
    return m_debugName;
  }


  void DxvkSpirvShader::gatherIdOffsets(
          SpirvCodeBuffer&          code) {
    for (auto i = code.begin(); i != code.end(); i++) {
      auto ins = *i;

      switch (ins.opCode()) {
        case spv::OpString:
        case spv::OpTypeVoid:
        case spv::OpTypeInt:
        case spv::OpTypeFloat:
        case spv::OpTypeBool:
        case spv::OpTypeVector:
        case spv::OpTypeMatrix:
        case spv::OpTypeImage:
        case spv::OpTypeSampler:
        case spv::OpTypeSampledImage:
        case spv::OpTypeArray:
        case spv::OpTypeRuntimeArray:
        case spv::OpTypeStruct:
        case spv::OpTypeOpaque:
        case spv::OpTypePointer:
        case spv::OpTypeFunction: {
          m_idToOffset.insert({ ins.arg(1u), ins.offset() });
        } break;

        case spv::OpConstant:
        case spv::OpVariable: {
          m_idToOffset.insert({ ins.arg(2u), ins.offset() });
        } break;

        case spv::OpFunction:
          return;

        default:
          break;
      }
    }
  }


  void DxvkSpirvShader::gatherMetadata(
          SpirvCodeBuffer&          code) {
    for (auto i = code.begin(); i != code.end(); i++) {
      auto ins = *i;

      switch (ins.opCode()) {
        case spv::OpCapability: {
          switch (spv::Capability(ins.arg(1u))) {
            case spv::CapabilitySampleRateShading: {
              m_metadata.flags.set(DxvkShaderFlag::HasSampleRateShading);
            } break;

            case spv::CapabilityShaderLayer:
            case spv::CapabilityShaderViewportIndex: {
              if (m_metadata.stage != VK_SHADER_STAGE_FRAGMENT_BIT
               && m_metadata.stage != VK_SHADER_STAGE_GEOMETRY_BIT)
                m_metadata.flags.set(DxvkShaderFlag::ExportsViewportIndexLayerFromVertexStage);
            } break;

            case spv::CapabilitySparseResidency: {
              m_metadata.flags.set(DxvkShaderFlag::UsesSparseResidency);
            } break;

            case spv::CapabilityFragmentFullyCoveredEXT: {
              m_metadata.flags.set(DxvkShaderFlag::UsesFragmentCoverage);
            } break;

            default:
              break;
          }
        } break;

        case spv::OpExecutionMode: {
          switch (spv::ExecutionMode(ins.arg(2u))) {
            case spv::ExecutionModeStencilRefReplacingEXT: {
              m_metadata.flags.set(DxvkShaderFlag::ExportsStencilRef);
            } break;

            case spv::ExecutionModeXfb: {
              m_metadata.flags.set(DxvkShaderFlag::HasTransformFeedback);
            } break;

            case spv::ExecutionModePointMode: {
              m_metadata.flags.set(DxvkShaderFlag::TessellationPoints);
            } break;

            case spv::ExecutionModeInputPoints: {
              m_metadata.inputTopology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            } break;

            case spv::ExecutionModeInputLines: {
              m_metadata.inputTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            } break;

            case spv::ExecutionModeInputLinesAdjacency: {
              m_metadata.inputTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
            } break;

            case spv::ExecutionModeInputTrianglesAdjacency: {
              m_metadata.inputTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
            } break;

            case spv::ExecutionModeIsolines: {
              m_metadata.outputTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            } break;

            case spv::ExecutionModeTriangles: {
              if (m_metadata.stage == VK_SHADER_STAGE_GEOMETRY_BIT)
                m_metadata.inputTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
              else if (m_metadata.stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
                    || m_metadata.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
                m_metadata.outputTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            } break;

            case spv::ExecutionModeQuads: {
              // Tess domain only, this will produce triangles
              m_metadata.outputTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            } break;

            case spv::ExecutionModeOutputPoints: {
              m_metadata.outputTopology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            } break;

            case spv::ExecutionModeOutputLineStrip: {
              // For metadata purposes we use list topologies everywhere
              m_metadata.outputTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            } break;

            case spv::ExecutionModeOutputTriangleStrip: {
              // For metadata purposes we use list topologies everywhere
              m_metadata.outputTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            } break;

            default:
              break;
          }
        } break;

        case spv::OpDecorate: {
          handleDecoration(ins, ins.arg(1u), -1, 2u);
        } break;

        case spv::OpMemberDecorate: {
          handleDecoration(ins, ins.arg(1u), ins.arg(2u), 3u);
        } break;

        case spv::OpSource: {
          if (ins.length() > 3u)
            handleDebugName(code, ins.arg(3u));
        } break;

        case spv::OpVariable: {
          auto varId = ins.arg(2u);
          auto storage = spv::StorageClass(ins.arg(3u));

          SpirvInstruction ptrType(code.data(), m_idToOffset.at(ins.arg(1u)), code.dwords());
          SpirvInstruction varType(code.data(), m_idToOffset.at(ptrType.arg(3u)), code.dwords());

          switch (storage) {
            case spv::StorageClassInput:
            case spv::StorageClassOutput: {
              // Tess control outputs as well as inputs, tess eval inputs and geometry inputs
              // must use array types for non-patch constant variables. Unwrap the outer array.
              if (varType.opCode() == spv::OpTypeArray && !getDecoration(varId, -1).patch) {
                bool isArrayInput = (storage == spv::StorageClassInput)
                  && (m_metadata.stage == VK_SHADER_STAGE_GEOMETRY_BIT
                   || m_metadata.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
                   || m_metadata.stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

                bool isArrayOutput = (storage == spv::StorageClassOutput) &&
                  (m_metadata.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);

                if (isArrayInput || isArrayOutput)
                  varType = SpirvInstruction(code.data(), m_idToOffset.at(varType.arg(2u)), code.dwords());
              }

              if (varType.opCode() == spv::OpTypeStruct) {
                auto structId = varType.arg(1u);

                for (uint32_t i = 2u; i < varType.length(); i++) {
                  SpirvInstruction memberType(code.data(), m_idToOffset.at(varType.arg(i)), code.dwords());
                  handleIoVariable(code, memberType, storage, structId, int32_t(i - 2u));
                }
              } else {
                handleIoVariable(code, varType, storage, varId, -1);
              }
           } break;

            case spv::StorageClassPushConstant: {
              m_pushConstantStructId = varType.arg(1u);

              if (!m_info.sharedPushData.isEmpty()) {
                auto stageMask = (m_metadata.stage & VK_SHADER_STAGE_ALL_GRAPHICS)
                  ? VK_SHADER_STAGE_ALL_GRAPHICS : VK_SHADER_STAGE_COMPUTE_BIT;

                m_layout.addPushData(DxvkPushDataBlock(stageMask,
                  m_info.sharedPushData.getOffset(),
                  m_info.sharedPushData.getSize(),
                  m_info.sharedPushData.getAlignment(),
                  m_info.sharedPushData.getResourceDwordMask()));
              }

              if (!m_info.localPushData.isEmpty()) {
                m_layout.addPushData(DxvkPushDataBlock(m_metadata.stage,
                  m_info.localPushData.getOffset(),
                  m_info.localPushData.getSize(),
                  m_info.localPushData.getAlignment(),
                  m_info.localPushData.getResourceDwordMask()));
              }
            } break;

            default:
              break;
          }
        } break;

        case spv::OpFunction:
          return;

        default:
          break;
      }
    }
  }


  void DxvkSpirvShader::handleIoVariable(
          SpirvCodeBuffer&          code,
    const SpirvInstruction&         type,
          spv::StorageClass         storage,
          uint32_t                  varId,
          int32_t                   member) {
    const auto& decoration = getDecoration(varId, member);

    if (storage == spv::StorageClassOutput) {
      if (m_metadata.stage == VK_SHADER_STAGE_GEOMETRY_BIT) {
        int32_t stream = 0;

        if (decoration.stream)
          stream = int32_t(*decoration.stream);

        if (stream != m_metadata.rasterizedStream)
          return;

        if (decoration.xfbBuffer && decoration.xfbStride)
          m_metadata.xfbStrides.at(*decoration.xfbBuffer) = *decoration.xfbStride;
      }

      if (decoration.builtIn == spv::BuiltInPosition)
        m_metadata.flags.set(DxvkShaderFlag::ExportsPosition);

      if (decoration.builtIn == spv::BuiltInSampleMask)
        m_metadata.flags.set(DxvkShaderFlag::ExportsSampleMask);
    }

    DxvkShaderIoVar varInfo = { };

    if (decoration.builtIn)
      varInfo.builtIn = *decoration.builtIn;

    if (decoration.location)
      varInfo.location = *decoration.location;

    if (decoration.index)
      varInfo.location |= (*decoration.index) << 5u;

    if (decoration.component)
      varInfo.componentIndex = *decoration.component;

    auto componentInfo = getComponentCountForType(code, type, varInfo.builtIn);

    varInfo.componentCount = componentInfo.first;
    varInfo.isPatchConstant = decoration.patch;

    for (uint32_t i = 0u; i < componentInfo.second; i++) {
      if (storage == spv::StorageClassOutput)
        m_metadata.outputs.add(varInfo);
      else
        m_metadata.inputs.add(varInfo);

      varInfo.location += 1u;
    }
  }


  void DxvkSpirvShader::handleDecoration(
    const SpirvInstruction&         ins,
          uint32_t                  id,
          int32_t                   member,
          uint32_t                  baseArg) {
    auto range = m_decorations.equal_range(id);
    auto entry = std::find_if(range.first, range.second,
      [member] (const std::pair<uint32_t, DxvkSpirvDecorations>& decoration) {
        return decoration.second.memberIndex == member;
      });

    if (entry == range.second) {
      DxvkSpirvDecorations decoration = { };
      decoration.memberIndex = member;

      entry = m_decorations.insert({ id, decoration });
    }

    switch (spv::Decoration(ins.arg(baseArg))) {
      case spv::DecorationLocation: {
        entry->second.location = ins.arg(baseArg + 1u);
      } break;

      case spv::DecorationIndex: {
        entry->second.index = ins.arg(baseArg + 1u);
      } break;

      case spv::DecorationComponent: {
        entry->second.component = ins.arg(baseArg + 1u);
      } break;

      case spv::DecorationDescriptorSet: {
        entry->second.set = ins.arg(baseArg + 1u);
      } break;

      case spv::DecorationBinding: {
        entry->second.binding = ins.arg(baseArg + 1u);
      } break;

      case spv::DecorationOffset: {
        entry->second.offset = ins.arg(baseArg + 1u);
      } break;

      case spv::DecorationArrayStride: {
        entry->second.stride = ins.arg(baseArg + 1u);
      } break;

      case spv::DecorationBuiltIn: {
        entry->second.builtIn = spv::BuiltIn(ins.arg(baseArg + 1u));
      } break;

      case spv::DecorationSpecId: {
        uint32_t specId = ins.arg(baseArg + 1u);
        m_metadata.specConstantMask |= 1u << specId;
      } break;

      case spv::DecorationPatch: {
        entry->second.patch = true;
      } break;

      case spv::DecorationStream: {
        entry->second.stream = ins.arg(baseArg + 1u);
      } break;

      case spv::DecorationXfbBuffer: {
        entry->second.xfbBuffer = ins.arg(baseArg + 1u);
      } break;

      case spv::DecorationXfbStride: {
        entry->second.xfbStride = ins.arg(baseArg + 1u);
      } break;

      default:
        break;
    }
  }


  void DxvkSpirvShader::handleDebugName(
          SpirvCodeBuffer&          code,
          uint32_t                  stringId) {
    auto e = m_idToOffset.find(stringId);

    if (e == m_idToOffset.end())
      return;

    SpirvInstruction str(code.data(), e->second, code.dwords());

    for (uint32_t i = 2u; i < str.length(); i++) {
      uint32_t arg = str.arg(i);

      for (uint32_t j = 0u; j < 4u; j++) {
        auto ch = char(arg >> (8u * j));

        if (!ch)
          return;

        m_debugName.push_back(ch);
      }
    }
  }


  const DxvkSpirvDecorations& DxvkSpirvShader::getDecoration(
          uint32_t                  id,
          int32_t                   member) const {
    static const DxvkSpirvDecorations s_defaultDecorations = { };

    auto range = m_decorations.equal_range(id);
    auto entry = std::find_if(range.first, range.second,
      [member] (const std::pair<uint32_t, DxvkSpirvDecorations>& decoration) {
        return decoration.second.memberIndex == member;
      });

    if (entry == range.second)
      return s_defaultDecorations;

    return entry->second;
  }


  std::pair<uint32_t, uint32_t> DxvkSpirvShader::getComponentCountForType(
          SpirvCodeBuffer&          code,
    const SpirvInstruction&         type,
          spv::BuiltIn              builtIn) const {
    switch (type.opCode()) {
      case spv::OpTypeVoid:
      case spv::OpTypeBool:
      case spv::OpTypeInt:
      case spv::OpTypeFloat:
        return std::make_pair(1u, 1u);

      case spv::OpTypeVector:
        return std::make_pair(type.arg(3u), 1u);

      case spv::OpTypeArray: {
        auto nested = SpirvInstruction(code.data(), m_idToOffset.at(type.arg(2u)), code.dwords());
        auto length = SpirvInstruction(code.data(), m_idToOffset.at(type.arg(3u)), code.dwords());

        if (builtIn == spv::BuiltInClipDistance
         || builtIn == spv::BuiltInCullDistance
         || builtIn == spv::BuiltInTessLevelInner
         || builtIn == spv::BuiltInTessLevelOuter
         || builtIn == spv::BuiltInSampleMask) {
          // Treat built-in arrays as a single 'location' of sorts
          return std::make_pair(length.arg(3u), 1u);
        }

        return std::make_pair(getComponentCountForType(code, nested, builtIn).first, length.arg(3u));
      }

      default:
        throw DxvkError(str::format("Unexpected I/O type: ", type.opCode()));
    }
  }


  void DxvkSpirvShader::patchResourceBindingsAndIoLocations(
          SpirvCodeBuffer&            code,
    const DxvkShaderBindingMap*       bindings,
    const DxvkShaderLinkage*          linkage) const {
    for (auto i = code.begin(); i != code.end(); i++) {
      auto ins = *i;

      switch (ins.opCode()) {
        case spv::OpDecorate: {
          auto objectId = ins.arg(1u);
          auto decorationType = spv::Decoration(ins.arg(2u));
          const auto& decoration = getDecoration(objectId, -1);

          switch (decorationType) {
            case spv::DecorationDescriptorSet:
            case spv::DecorationBinding: {
              uint32_t set = 0u;
              uint32_t binding = 0u;

              if (decoration.set)
                set = *decoration.set;

              if (decoration.binding)
                binding = *decoration.binding;

              auto mappedBinding = bindings->mapBinding(
                DxvkShaderBinding(m_metadata.stage, set, binding));

              if (mappedBinding) {
                if (decorationType == spv::DecorationDescriptorSet)
                  ins.setArg(3u, mappedBinding->getSet());

                if (decorationType == spv::DecorationBinding)
                  ins.setArg(3u, mappedBinding->getBinding());
              }
            } break;

            case spv::DecorationLocation:
            case spv::DecorationIndex: {
              if (!linkage || !linkage->fsDualSrcBlend)
                break;

              // Ensure that what we're patching is actually an output variable
              SpirvInstruction var(code.data(), m_idToOffset.at(ins.arg(1u)), code.dwords());

              if (spv::StorageClass(var.arg(3u)) != spv::StorageClassOutput)
                break;

              // Set location to 0 or index to 1 for location 1
              if (decoration.location == 1u)
                ins.setArg(3u, decorationType == spv::DecorationIndex ? 1u : 0u);
            } break;

            default:
              break;
          }
        } break;

        case spv::OpMemberDecorate: {
          auto objectId = ins.arg(1u);
          auto decorationType = spv::Decoration(ins.arg(3u));

          switch (decorationType) {
            case spv::DecorationOffset: {
              if (objectId != m_pushConstantStructId)
                break;

              uint32_t offset = bindings->mapPushData(m_metadata.stage, ins.arg(4u));

              if (offset < MaxTotalPushDataSize)
                ins.setArg(4u, offset);
            } break;

            default:
              break;
          }
        } break;

        case spv::OpFunction:
          return;

        default:
          break;
      }
    }
  }


  VkShaderStageFlagBits DxvkSpirvShader::getShaderStage(
          SpirvCodeBuffer&          code) {
    for (auto i = code.begin(); i != code.end(); i++) {
      auto ins = *i;

      if (ins.opCode() == spv::OpEntryPoint) {
        auto model = spv::ExecutionModel(ins.arg(1u));

        switch (model) {
          case spv::ExecutionModelVertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
          case spv::ExecutionModelTessellationControl:
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
          case spv::ExecutionModelTessellationEvaluation:
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
          case spv::ExecutionModelGeometry:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
          case spv::ExecutionModelFragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
          case spv::ExecutionModelGLCompute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
          default:
            throw DxvkError(str::format("Invalid execution model: ", model));
        }
      }
    }

    throw DxvkError("No OpEntryPoint found in SPIR-V shader");
  }


  void DxvkSpirvShader::eliminateInput(
          SpirvCodeBuffer&          code,
          uint32_t                  location) {
    struct SpirvTypeInfo {
      spv::Op           op            = spv::OpNop;
      uint32_t          baseTypeId    = 0;
      uint32_t          compositeSize = 0;
      spv::StorageClass storageClass  = spv::StorageClassMax;
    };

    uint32_t spirvVersion = code.data()[1];

    std::unordered_map<uint32_t, SpirvTypeInfo> types;
    std::unordered_map<uint32_t, uint32_t>      constants;
    std::unordered_set<uint32_t>                candidates;

    // Find the input variable in question
    size_t   inputVarOffset = 0;
    uint32_t inputVarTypeId = 0;
    uint32_t inputVarId     = 0;

    for (auto ins : code) {
      if (ins.opCode() == spv::OpDecorate) {
        if (ins.arg(2) == spv::DecorationLocation
         && ins.arg(3) == location)
          candidates.insert(ins.arg(1));
      }

      if (ins.opCode() == spv::OpConstant)
        constants.insert({ ins.arg(2), ins.arg(3) });

      if (ins.opCode() == spv::OpTypeFloat || ins.opCode() == spv::OpTypeInt)
        types.insert({ ins.arg(1), { ins.opCode(), 0, ins.arg(2), spv::StorageClassMax }});

      if (ins.opCode() == spv::OpTypeVector)
        types.insert({ ins.arg(1), { ins.opCode(), ins.arg(2), ins.arg(3), spv::StorageClassMax }});

      if (ins.opCode() == spv::OpTypeArray) {
        auto constant = constants.find(ins.arg(3));
        if (constant == constants.end())
          continue;
        types.insert({ ins.arg(1), { ins.opCode(), ins.arg(2), constant->second, spv::StorageClassMax }});
      }

      if (ins.opCode() == spv::OpTypePointer)
        types.insert({ ins.arg(1), { ins.opCode(), ins.arg(3), 0, spv::StorageClass(ins.arg(2)) }});

      if (ins.opCode() == spv::OpVariable && spv::StorageClass(ins.arg(3)) == spv::StorageClassInput) {
        if (candidates.find(ins.arg(2)) != candidates.end()) {
          inputVarOffset = ins.offset();
          inputVarTypeId = ins.arg(1);
          inputVarId     = ins.arg(2);
          break;
        }
      }

      if (ins.opCode() == spv::OpFunction)
        break;
    }

    if (!inputVarId)
      return;

    // Declare private pointer types
    auto pointerType = types.find(inputVarTypeId);
    if (pointerType == types.end())
      return;

    code.beginInsertion(inputVarOffset);
    std::vector<std::pair<uint32_t, SpirvTypeInfo>> privateTypes;

    for (auto p  = types.find(pointerType->second.baseTypeId);
              p != types.end();
              p  = types.find(p->second.baseTypeId)) {
      std::pair<uint32_t, SpirvTypeInfo> info = *p;
      info.first = 0;
      info.second.baseTypeId = p->first;
      info.second.storageClass = spv::StorageClassPrivate;

      for (auto t : types) {
        if (t.second.op           == info.second.op
         && t.second.baseTypeId   == info.second.baseTypeId
         && t.second.storageClass == info.second.storageClass)
          info.first = t.first;
      }

      if (!info.first) {
        info.first = code.allocId();

        code.putIns(spv::OpTypePointer, 4);
        code.putWord(info.first);
        code.putWord(info.second.storageClass);
        code.putWord(info.second.baseTypeId);
      }

      privateTypes.push_back(info);
    }

    // Define zero constants
    uint32_t constantId = 0;

    for (auto i = privateTypes.rbegin(); i != privateTypes.rend(); i++) {
      if (constantId) {
        uint32_t compositeSize = i->second.compositeSize;
        uint32_t compositeId   = code.allocId();

        code.putIns(spv::OpConstantComposite, 3 + compositeSize);
        code.putWord(i->second.baseTypeId);
        code.putWord(compositeId);

        for (uint32_t i = 0; i < compositeSize; i++)
          code.putWord(constantId);

        constantId = compositeId;
      } else {
        constantId = code.allocId();

        code.putIns(spv::OpConstant, 4);
        code.putWord(i->second.baseTypeId);
        code.putWord(constantId);
        code.putWord(0);
      }
    }

    // Erase and re-declare variable
    code.erase(4);

    code.putIns(spv::OpVariable, 5);
    code.putWord(privateTypes[0].first);
    code.putWord(inputVarId);
    code.putWord(spv::StorageClassPrivate);
    code.putWord(constantId);

    code.endInsertion();

    // Remove variable from interface list
    if (spirvVersion < spvVersion(1, 4)) {
      for (auto ins : code) {
        if (ins.opCode() == spv::OpEntryPoint) {
          uint32_t argIdx = 2 + code.strLen(ins.chr(2));

          while (argIdx < ins.length()) {
            if (ins.arg(argIdx) == inputVarId) {
              ins.setArg(0, spv::OpEntryPoint | ((ins.length() - 1) << spv::WordCountShift));

              code.beginInsertion(ins.offset() + argIdx);
              code.erase(1);
              code.endInsertion();
              break;
            }

            argIdx += 1;
          }

          break;
        }
      }
    }

    // Remove location and other declarations
    for (auto iter = code.begin(); iter != code.end(); ) {
      auto ins = *(iter++);

      if (ins.opCode() == spv::OpDecorate && ins.arg(1) == inputVarId) {
        uint32_t numWords;

        switch (ins.arg(2)) {
          case spv::DecorationLocation:
          case spv::DecorationFlat:
          case spv::DecorationNoPerspective:
          case spv::DecorationCentroid:
          case spv::DecorationPatch:
          case spv::DecorationSample:
            numWords = ins.length();
            break;

          default:
            numWords = 0;
        }

        if (numWords) {
          code.beginInsertion(ins.offset());
          code.erase(numWords);

          iter = SpirvInstructionIterator(code.data(), code.endInsertion(), code.dwords());
        }
      }

      if (ins.opCode() == spv::OpFunction)
        break;
    }

    // Fix up pointer types used in access chain instructions
    std::unordered_map<uint32_t, uint32_t> accessChainIds;

    for (auto ins : code) {
      if (ins.opCode() == spv::OpAccessChain
       || ins.opCode() == spv::OpInBoundsAccessChain) {
        uint32_t depth = ins.length() - 4;

        if (ins.arg(3) == inputVarId) {
          // Access chains accessing the variable directly
          ins.setArg(1, privateTypes.at(depth).first);
          accessChainIds.insert({ ins.arg(2), depth });
        } else {
          // Access chains derived from the variable
          auto entry = accessChainIds.find(ins.arg(2));
          if (entry != accessChainIds.end()) {
            depth += entry->second;
            ins.setArg(1, privateTypes.at(depth).first);
            accessChainIds.insert({ ins.arg(2), depth });
          }
        }
      }
    }
  }


  void DxvkSpirvShader::emitOutputSwizzles(
          SpirvCodeBuffer&          code,
          uint32_t                  outputMask,
          const VkComponentMapping* swizzles) {
    // Skip this step entirely if all relevant
    // outputs use the identity swizzle
    bool requiresEpilogue = false;

    for (auto index : bit::BitMask(outputMask))
      requiresEpilogue |= !util::isIdentityMapping(swizzles[index]);

    if (!requiresEpilogue)
      return;

    // Gather some information. We need to scan pointer types with
    // the output storage class to find the base vector type, and
    // we need to scan vector types to find the component count.
    uint32_t entryPointId = 0;
    uint32_t functionId = 0;

    size_t epilogueOffset = 0;
    size_t variableOffset = 0;

    struct VarInfo {
      uint32_t varId;
      uint32_t typeId;
      uint32_t location;
      uint32_t componentCount;
      uint32_t componentTypeId;
    };

    struct VarIdInfo {
      uint32_t location;
    };

    struct TypeIdInfo {
      uint32_t componentCount;
      uint32_t baseTypeId;
    };

    union IdInfo {
      VarIdInfo     var;
      TypeIdInfo    type;
    };

    // Stores type information depending on type category:
    // OpTypePointer:   type id -> base type id
    // OpTypeVector:    type id -> component count
    // OpTypeFloat/Int: type id -> 1
    std::unordered_map<uint32_t, IdInfo> idInfo;
    std::vector<VarInfo> varInfos;

    SpirvInstruction prev;

    for (auto ins : code) {
      switch (ins.opCode()) {
        case spv::OpEntryPoint: {
          entryPointId = ins.arg(2);
        } break;

        case spv::OpDecorate: {
          if (ins.arg(2) == spv::DecorationLocation) {
            IdInfo info;
            info.var.location = ins.arg(3);
            idInfo.insert({ ins.arg(1), info });
          }
        } break;

        case spv::OpTypeVector: {
          IdInfo info;
          info.type.componentCount = ins.arg(3);
          info.type.baseTypeId = ins.arg(2);
          idInfo.insert({ ins.arg(1), info });
        } break;

        case spv::OpTypeInt:
        case spv::OpTypeFloat: {
          IdInfo info;
          info.type.componentCount = 1;
          info.type.baseTypeId = 0;
          idInfo.insert({ ins.arg(1), info });
        } break;

        case spv::OpTypePointer: {
          if (ins.arg(2) == spv::StorageClassOutput) {
            IdInfo info;
            info.type.componentCount = 0;
            info.type.baseTypeId = ins.arg(3);
            idInfo.insert({ ins.arg(1), info });
          }
        } break;

        case spv::OpVariable: {
          if (!variableOffset)
            variableOffset = ins.offset();

          if (ins.arg(3) == spv::StorageClassOutput) {
            uint32_t ptrId = ins.arg(1);
            uint32_t varId = ins.arg(2);

            auto ptrEntry = idInfo.find(ptrId);
            auto varEntry = idInfo.find(varId);

            if (ptrEntry != idInfo.end()
             && varEntry != idInfo.end()) {
              uint32_t typeId = ptrEntry->second.type.baseTypeId;

              auto typeEntry = idInfo.find(typeId);
              if (typeEntry != idInfo.end()) {
                VarInfo info;
                info.varId = varId;
                info.typeId = typeId;
                info.location = varEntry->second.var.location;
                info.componentCount = typeEntry->second.type.componentCount;
                info.componentTypeId = (info.componentCount == 1)
                  ? typeId : typeEntry->second.type.baseTypeId;

                varInfos.push_back(info);
              }
            }
          }
        } break;

        case spv::OpFunction: {
          functionId = ins.arg(2);
        } break;

        case spv::OpFunctionEnd: {
          if (entryPointId == functionId)
            epilogueOffset = prev.offset();
        } break;

        default:
          prev = ins;
      }

      if (epilogueOffset)
        break;
    }

    // Oops, this shouldn't happen
    if (!epilogueOffset)
      return;

    code.beginInsertion(epilogueOffset);

    struct ConstInfo {
      uint32_t constId;
      uint32_t typeId;
      uint32_t value;
    };

    std::vector<ConstInfo> consts;

    for (const auto& var : varInfos) {
      uint32_t storeId = 0;

      if (var.componentCount == 1) {
        if (util::getComponentIndex(swizzles[var.location].r, 0) != 0) {
          storeId = code.allocId();

          ConstInfo constInfo;
          constInfo.constId = storeId;
          constInfo.typeId = var.componentTypeId;
          constInfo.value = 0;
          consts.push_back(constInfo);
        }
      } else {
        uint32_t constId = 0;

        std::array<uint32_t, 4> indices = {{
          util::getComponentIndex(swizzles[var.location].r, 0),
          util::getComponentIndex(swizzles[var.location].g, 1),
          util::getComponentIndex(swizzles[var.location].b, 2),
          util::getComponentIndex(swizzles[var.location].a, 3),
        }};

        bool needsSwizzle = false;

        for (uint32_t i = 0; i < var.componentCount && !constId; i++) {
          needsSwizzle |= indices[i] != i;

          if (indices[i] >= var.componentCount)
            constId = code.allocId();
        }

        if (needsSwizzle) {
          uint32_t loadId = code.allocId();
          code.putIns(spv::OpLoad, 4);
          code.putWord(var.typeId);
          code.putWord(loadId);
          code.putWord(var.varId);

          if (!constId) {
            storeId = code.allocId();
            code.putIns(spv::OpVectorShuffle, 5 + var.componentCount);
            code.putWord(var.typeId);
            code.putWord(storeId);
            code.putWord(loadId);
            code.putWord(loadId);

            for (uint32_t i = 0; i < var.componentCount; i++)
              code.putWord(indices[i]);
          } else {
            std::array<uint32_t, 4> ids = { };

            ConstInfo constInfo;
            constInfo.constId = constId;
            constInfo.typeId = var.componentTypeId;
            constInfo.value = 0;
            consts.push_back(constInfo);

            for (uint32_t i = 0; i < var.componentCount; i++) {
              if (indices[i] < var.componentCount) {
                ids[i] = code.allocId();

                code.putIns(spv::OpCompositeExtract, 5);
                code.putWord(var.componentTypeId);
                code.putWord(ids[i]);
                code.putWord(loadId);
                code.putWord(indices[i]);
              } else {
                ids[i] = constId;
              }
            }

            storeId = code.allocId();
            code.putIns(spv::OpCompositeConstruct, 3 + var.componentCount);
            code.putWord(var.typeId);
            code.putWord(storeId);

            for (uint32_t i = 0; i < var.componentCount; i++)
              code.putWord(ids[i]);
          }
        }
      }

      if (storeId) {
        code.putIns(spv::OpStore, 3);
        code.putWord(var.varId);
        code.putWord(storeId);
      }
    }

    code.endInsertion();

    // If necessary, insert constants
    if (!consts.empty()) {
      code.beginInsertion(variableOffset);

      for (const auto& c : consts) {
        code.putIns(spv::OpConstant, 4);
        code.putWord(c.typeId);
        code.putWord(c.constId);
        code.putWord(c.value);
      }

      code.endInsertion();
    }
  }


  void DxvkSpirvShader::emitFlatShadingDeclarations(
          SpirvCodeBuffer&          code,
          uint32_t                  inputMask) {
    if (!inputMask)
      return;

    struct VarInfo {
      uint32_t varId;
      size_t decorationOffset;
    };

    std::unordered_set<uint32_t> candidates;
    std::unordered_map<uint32_t, size_t> decorations;
    std::vector<VarInfo> flatVars;

    size_t decorateOffset = 0;

    for (auto ins : code) {
      switch (ins.opCode()) {
        case spv::OpDecorate: {
          decorateOffset = ins.offset() + ins.length();
          uint32_t varId = ins.arg(1);

          switch (ins.arg(2)) {
            case spv::DecorationLocation: {
              uint32_t location = ins.arg(3);

              if (inputMask & (1u << location))
                candidates.insert(varId);
            } break;

            case spv::DecorationFlat:
            case spv::DecorationCentroid:
            case spv::DecorationSample:
            case spv::DecorationNoPerspective: {
              decorations.insert({ varId, ins.offset() + 2 });
            } break;

            default: ;
          }
        } break;

        case spv::OpVariable: {
          if (ins.arg(3) == spv::StorageClassInput) {
            uint32_t varId = ins.arg(2);

            // Only consider variables that have a desired location
            if (candidates.find(varId) != candidates.end()) {
              VarInfo varInfo;
              varInfo.varId = varId;
              varInfo.decorationOffset = 0;

              auto decoration = decorations.find(varId);
              if (decoration != decorations.end())
                varInfo.decorationOffset = decoration->second;

              flatVars.push_back(varInfo);
            }
          }
        } break;

        default:
          break;
      }
    }

    // Change existing decorations as necessary
    for (const auto& var : flatVars) {
      if (var.decorationOffset) {
        uint32_t* rawCode = code.data();
        rawCode[var.decorationOffset] = spv::DecorationFlat;
      }
    }

    // Insert new decorations for remaining variables
    code.beginInsertion(decorateOffset);

    for (const auto& var : flatVars) {
      if (!var.decorationOffset) {
        code.putIns(spv::OpDecorate, 3);
        code.putWord(var.varId);
        code.putWord(spv::DecorationFlat);
      }
    }

    code.endInsertion();
  }


  void DxvkSpirvShader::patchInputTopology(
          SpirvCodeBuffer&          code,
          VkPrimitiveTopology       topology) {
    struct TopologyInfo {
      VkPrimitiveTopology topology;
      spv::ExecutionMode  mode;
      uint32_t            vertexCount;
    };

    static const std::array<TopologyInfo, 5> s_topologies = {{
      { VK_PRIMITIVE_TOPOLOGY_POINT_LIST,                   spv::ExecutionModeInputPoints,              1u },
      { VK_PRIMITIVE_TOPOLOGY_LINE_LIST,                    spv::ExecutionModeInputLines,               2u },
      { VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,     spv::ExecutionModeInputLinesAdjacency,      4u },
      { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                spv::ExecutionModeTriangles,                3u },
      { VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY, spv::ExecutionModeInputTrianglesAdjacency,  6u },
    }};

    const TopologyInfo* topologyInfo = nullptr;

    for (const auto& top : s_topologies) {
      if (top.topology == topology) {
        topologyInfo = &top;
        break;
      }
    }

    if (!topologyInfo)
      return;

    uint32_t typeUint32Id = 0u;
    uint32_t typeSint32Id = 0u;

    struct ConstantInfo {
      uint32_t typeId;
      uint32_t value;
    };

    struct ArrayTypeInfo {
      uint32_t arrayLengthId;
      uint32_t scalarTypeId;
      uint32_t replaceTypeId;
    };

    struct PointerTypeInfo {
      uint32_t objectTypeId;
    };

    std::unordered_map<uint32_t, uint32_t> nullConstantsByType;
    std::unordered_map<uint32_t, ConstantInfo> constants;
    std::unordered_map<uint32_t, uint32_t> uintConstantValueToId;
    std::unordered_map<uint32_t, ArrayTypeInfo> arrayTypes;
    std::unordered_map<uint32_t, PointerTypeInfo> pointerTypes;
    std::unordered_map<uint32_t, uint32_t> variableTypes;
    std::unordered_set<uint32_t> nullAccessChains;
    std::unordered_map<uint32_t, uint32_t> nullVarsByType;
    std::vector<std::pair<uint32_t, uint32_t>> newNullVars;

    uint32_t functionOffset = 0u;

    for (auto iter = code.begin(); iter != code.end(); ) {
      auto ins = *iter;

      switch (ins.opCode()) {
        case spv::OpExecutionMode: {
          bool isTopology = false;

          for (const auto& top : s_topologies)
            isTopology |= spv::ExecutionMode(ins.arg(2)) == top.mode;

          if (isTopology)
            ins.setArg(2, uint32_t(topologyInfo->mode));
        } break;

        case spv::OpConstant: {
          if (ins.arg(1) == typeUint32Id || ins.arg(1) == typeSint32Id) {
            ConstantInfo c = { };
            c.typeId = ins.arg(1);
            c.value = ins.arg(3);

            constants.insert({ ins.arg(2), c });
            uintConstantValueToId.insert({ ins.arg(3), ins.arg(2) });
          }
        } break;

        case spv::OpConstantNull: {
          nullConstantsByType.insert({ ins.arg(1), ins.arg(2) });
        } break;

        case spv::OpTypeInt: {
          if (ins.arg(2u) == 32u) {
            if (ins.arg(3u))
              typeSint32Id = ins.arg(1u);
            else
              typeUint32Id = ins.arg(1u);
          }
        } break;

        case spv::OpTypeArray: {
          ArrayTypeInfo t = { };
          t.arrayLengthId = ins.arg(3);
          t.scalarTypeId = ins.arg(2);
          t.replaceTypeId = 0u;

          arrayTypes.insert({ ins.arg(1), t });
        } break;

        case spv::OpTypePointer: {
          // We know that all input arrays use the vertex count as their outer
          // array size, so it is safe for us to simply replace the array type
          // of any pointer type declaration with an appropriately sized array.
          auto storageClass = spv::StorageClass(ins.arg(2));

          if (storageClass == spv::StorageClassInput) {
            uint32_t len = ins.length();

            uint32_t arrayTypeId = 0u;
            uint32_t scalarTypeId = 0u;

            PointerTypeInfo t = { };
            t.objectTypeId = ins.arg(3);

            auto entry = arrayTypes.find(t.objectTypeId);

            if (entry != arrayTypes.end()) {
              if (!entry->second.replaceTypeId) {
                arrayTypeId = code.allocId();
                scalarTypeId = entry->second.scalarTypeId;

                entry->second.replaceTypeId = arrayTypeId;
              }

              t.objectTypeId = entry->second.replaceTypeId;
              ins.setArg(3, t.objectTypeId);
            }

            pointerTypes.insert({ ins.arg(1), t });

            // If we replaced the array type, emit it before the pointer type
            // decoration as necessary. It is legal to delcare identical array
            // types multiple times.
            if (arrayTypeId) {
              code.beginInsertion(ins.offset());

              auto lengthId = uintConstantValueToId.find(topologyInfo->vertexCount);

              if (lengthId == uintConstantValueToId.end()) {
                if (!typeUint32Id) {
                  typeUint32Id = code.allocId();

                  code.putIns  (spv::OpTypeInt, 4);
                  code.putWord (typeUint32Id);
                  code.putWord (32);
                  code.putWord (0);
                }

                ConstantInfo c;
                c.typeId = typeUint32Id;
                c.value = topologyInfo->vertexCount;

                uint32_t arrayLengthId = code.allocId();

                code.putIns  (spv::OpConstant, 4);
                code.putWord (c.typeId);
                code.putWord (arrayLengthId);
                code.putWord (c.value);

                lengthId = uintConstantValueToId.insert({ c.value, arrayLengthId }).first;
                constants.insert({ arrayLengthId, c });
              }

              ArrayTypeInfo t = { };
              t.scalarTypeId = scalarTypeId;
              t.arrayLengthId = lengthId->second;

              arrayTypes.insert({ arrayTypeId, t });

              code.putIns   (spv::OpTypeArray, 4);
              code.putWord  (arrayTypeId);
              code.putWord  (t.scalarTypeId);
              code.putWord  (t.arrayLengthId);

              iter = SpirvInstructionIterator(code.data(), code.endInsertion() + len, code.dwords());
              continue;
            }
          }
        } break;

        case spv::OpVariable: {
          auto storageClass = spv::StorageClass(ins.arg(3));

          if (storageClass == spv::StorageClassInput)
            variableTypes.insert({ ins.arg(2), ins.arg(1) });
        } break;

        case spv::OpFunction: {
          if (!functionOffset)
            functionOffset = ins.offset();
        } break;

        case spv::OpAccessChain:
        case spv::OpInBoundsAccessChain: {
          bool nullChain = false;
          auto var = variableTypes.find(ins.arg(3));

          if (var == variableTypes.end()) {
            // If we're recursively loading from a null access chain, skip
            auto chain = nullAccessChains.find(ins.arg(3));
            nullChain = chain != nullAccessChains.end();
          } else {
            // If the index is out of bounds, mark the access chain as
            // dead so we can replace all loads with a null constant.
            auto c = constants.find(ins.arg(4u));

            if (c == constants.end())
              break;

            nullChain = c->second.value >= topologyInfo->vertexCount;
          }

          if (nullChain) {
            nullAccessChains.insert(ins.arg(2));

            code.beginInsertion(ins.offset());
            code.erase(ins.length());

            iter = SpirvInstructionIterator(code.data(), code.endInsertion(), code.dwords());
            continue;
          }
        } break;

        case spv::OpLoad: {
          // If we're loading from a null access chain, replace with null constant load.
          // We should never load the entire array at once, so ignore that case.
          if (nullAccessChains.find(ins.arg(3)) != nullAccessChains.end()) {
            auto var = nullVarsByType.find(ins.arg(1));

            if (var == nullVarsByType.end()) {
              var = nullVarsByType.insert({ ins.arg(1), code.allocId() }).first;
              newNullVars.push_back(std::make_pair(var->second, ins.arg(1)));
            }

            ins.setArg(3, var->second);
          }
        } break;

        default:;
      }

      iter++;
    }

    // Insert new null variables
    code.beginInsertion(functionOffset);

    for (auto v : newNullVars) {
      auto nullConst = nullConstantsByType.find(v.second);

      if (nullConst == nullConstantsByType.end()) {
        uint32_t nullConstId = code.allocId();

        code.putIns   (spv::OpConstantNull, 3u);
        code.putWord  (v.second);
        code.putWord  (nullConstId);

        nullConst = nullConstantsByType.insert({ v.second, nullConstId }).first;
      }

      uint32_t pointerTypeId = code.allocId();

      code.putIns   (spv::OpTypePointer, 4u);
      code.putWord  (pointerTypeId);
      code.putWord  (spv::StorageClassPrivate);
      code.putWord  (v.second);

      code.putIns   (spv::OpVariable, 5u);
      code.putWord  (pointerTypeId);
      code.putWord  (v.first);
      code.putWord  (spv::StorageClassPrivate);
      code.putWord  (nullConst->second);
    }

    code.endInsertion();

    // Add newly declared null variables to entry point
    for (auto ins : code) {
      if (ins.opCode() == spv::OpEntryPoint) {
        uint32_t len = ins.length();
        uint32_t token = ins.opCode() | ((len + newNullVars.size()) << 16);
        ins.setArg(0, token);

        code.beginInsertion(ins.offset() + len);

        for (auto v : newNullVars)
          code.putWord(v.first);

        code.endInsertion();
        break;
      }
    }
  }

}
