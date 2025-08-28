#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "dxvk_device.h"
#include "dxvk_shader_spirv.h"

namespace dxvk {

  DxvkSpirvShader::DxvkSpirvShader(
    const DxvkShaderCreateInfo&       info,
          SpirvCodeBuffer&&           spirv)
  : m_layout(info.stage) {
    m_info = info;
    m_info.bindings = nullptr;

    // Copy resource binding slot infos
    for (uint32_t i = 0; i < info.bindingCount; i++) {
      DxvkShaderDescriptor descriptor(info.bindings[i], info.stage);
      m_layout.addBindings(1, &descriptor);
    }

    // Run an analysis pass over the SPIR-V code to gather some
    // info that we may need during pipeline compilation.
    uint32_t pushConstantStructId = 0u;

    std::vector<BindingOffsets> bindingOffsets;
    std::vector<uint32_t> varIds;
    std::vector<uint32_t> sampleMaskIds;
    std::unordered_map<uint32_t, uint32_t> pushConstantTypes;
    std::unordered_map<uint32_t, std::string> strings;

    SpirvCodeBuffer code = std::move(spirv);
    uint32_t o1VarId = 0;
    uint32_t shaderNameId = 0;

    for (auto ins : code) {
      if (ins.opCode() == spv::OpSource)
        shaderNameId = ins.arg(3u);

      if (ins.opCode() == spv::OpString) {
        small_vector<char, 64u> str;

        for (uint32_t i = 2u; i < ins.length(); i++) {
          uint32_t arg = ins.arg(i);
          str.push_back(char(arg >> 0u));
          str.push_back(char(arg >> 8u));
          str.push_back(char(arg >> 16u));
          str.push_back(char(arg >> 24u));
        }

        str.push_back('\0');

        strings.insert_or_assign(ins.arg(1u), std::string(str.data()));
      }

      if (ins.opCode() == spv::OpDecorate) {
        if (ins.arg(2) == spv::DecorationBinding) {
          uint32_t varId = ins.arg(1);
          bindingOffsets.resize(std::max(bindingOffsets.size(), size_t(varId + 1)));
          bindingOffsets[varId].bindingIndex = ins.arg(3);
          bindingOffsets[varId].bindingOffset = ins.offset() + 3;
          varIds.push_back(varId);
        }

        if (ins.arg(2) == spv::DecorationDescriptorSet) {
          uint32_t varId = ins.arg(1);
          bindingOffsets.resize(std::max(bindingOffsets.size(), size_t(varId + 1)));
          bindingOffsets[varId].setIndex = ins.arg(3);
          bindingOffsets[varId].setOffset = ins.offset() + 3;
        }

        if (ins.arg(2) == spv::DecorationBuiltIn) {
          if (ins.arg(3) == spv::BuiltInSampleMask)
            sampleMaskIds.push_back(ins.arg(1));
          if (ins.arg(3) == spv::BuiltInPosition)
            m_metadata.flags.set(DxvkShaderFlag::ExportsPosition);
        }

        if (ins.arg(2) == spv::DecorationSpecId) {
          if (ins.arg(3) <= MaxNumSpecConstants)
            m_metadata.specConstantMask |= 1u << ins.arg(3);
        }

        if (ins.arg(2) == spv::DecorationLocation && ins.arg(3) == 1) {
          m_o1LocOffset = ins.offset() + 3;
          o1VarId = ins.arg(1);
        }

        if (ins.arg(2) == spv::DecorationIndex && ins.arg(1) == o1VarId)
          m_o1IdxOffset = ins.offset() + 3;
      }

      if (ins.opCode() == spv::OpMemberDecorate) {
        if (ins.arg(3) == spv::DecorationBuiltIn) {
          if (ins.arg(4) == spv::BuiltInPosition)
            m_metadata.flags.set(DxvkShaderFlag::ExportsPosition);
        }
      }

      if (ins.opCode() == spv::OpExecutionMode) {
        if (ins.arg(2) == spv::ExecutionModeStencilRefReplacingEXT)
          m_metadata.flags.set(DxvkShaderFlag::ExportsStencilRef);

        if (ins.arg(2) == spv::ExecutionModeXfb)
          m_metadata.flags.set(DxvkShaderFlag::HasTransformFeedback);

        if (ins.arg(2) == spv::ExecutionModePointMode)
          m_metadata.flags.set(DxvkShaderFlag::TessellationPoints);
      }

      if (ins.opCode() == spv::OpCapability) {
        if (ins.arg(1) == spv::CapabilitySampleRateShading)
          m_metadata.flags.set(DxvkShaderFlag::HasSampleRateShading);

        if (ins.arg(1) == spv::CapabilityShaderViewportIndex
         || ins.arg(1) == spv::CapabilityShaderLayer)
          m_metadata.flags.set(DxvkShaderFlag::ExportsViewportIndexLayerFromVertexStage);

        if (ins.arg(1) == spv::CapabilitySparseResidency)
          m_metadata.flags.set(DxvkShaderFlag::UsesSparseResidency);

        if (ins.arg(1) == spv::CapabilityFragmentFullyCoveredEXT)
          m_metadata.flags.set(DxvkShaderFlag::UsesFragmentCoverage);
      }

      if (ins.opCode() == spv::OpVariable) {
        if (ins.arg(3) == spv::StorageClassOutput) {
          if (std::find(sampleMaskIds.begin(), sampleMaskIds.end(), ins.arg(2)) != sampleMaskIds.end())
            m_metadata.flags.set(DxvkShaderFlag::ExportsSampleMask);
        }

        if (ins.arg(3) == spv::StorageClassPushConstant) {
          auto type = pushConstantTypes.find(ins.arg(1));

          if (type != pushConstantTypes.end())
            pushConstantStructId = type->second;
        }
      }

      if (ins.opCode() == spv::OpTypePointer) {
        if (ins.arg(2) == spv::StorageClassPushConstant)
          pushConstantTypes.insert({ ins.arg(1), ins.arg(3) });
      }

      // Ignore the actual shader code, there's nothing interesting for us in there.
      if (ins.opCode() == spv::OpFunction)
        break;
    }

    for (auto ins : code) {
      if (ins.opCode() == spv::OpMemberDecorate
       && ins.arg(1) == pushConstantStructId
       && ins.arg(3) == spv::DecorationOffset) {
        auto& e = m_pushDataOffsets.emplace_back();
        e.codeOffset = ins.offset() + 4;
        e.pushOffset = ins.arg(4);
      }

      // Can exit even earlier here since decorations come up early
      if (ins.opCode() == spv::OpFunction || ins.opCode() == spv::OpTypeVoid)
        break;
    }

    // Combine spec constant IDs with other binding info
    for (auto varId : varIds) {
      BindingOffsets info = bindingOffsets[varId];

      if (info.bindingOffset)
        m_bindingOffsets.push_back(info);
    }

    if (pushConstantStructId) {
      if (!info.sharedPushData.isEmpty()) {
        auto stageMask = (info.stage & VK_SHADER_STAGE_ALL_GRAPHICS)
          ? VK_SHADER_STAGE_ALL_GRAPHICS : VK_SHADER_STAGE_COMPUTE_BIT;

        m_layout.addPushData(DxvkPushDataBlock(stageMask,
          info.sharedPushData.getOffset(),
          info.sharedPushData.getSize(),
          info.sharedPushData.getAlignment(),
          info.sharedPushData.getResourceDwordMask()));
      }

      if (!info.localPushData.isEmpty()) {
        m_layout.addPushData(DxvkPushDataBlock(info.stage,
          info.localPushData.getOffset(),
          info.localPushData.getSize(),
          info.localPushData.getAlignment(),
          info.localPushData.getResourceDwordMask()));
      }
    }

    if (info.samplerHeap.getStageMask() & info.stage) {
      m_layout.addSamplerHeap(DxvkShaderBinding(info.stage,
        info.samplerHeap.getSet(),
        info.samplerHeap.getBinding()));
    }

    if (shaderNameId) {
      auto entry = strings.find(shaderNameId);

      if (entry != strings.end())
        m_debugName = std::move(entry->second);
    }

    if (m_debugName.empty())
      m_debugName = std::to_string(getCookie());

    m_code = SpirvCompressedBuffer(code);

    // Don't set pipeline library flag if the shader
    // doesn't actually support pipeline libraries
    m_needsLibraryCompile = canUsePipelineLibrary(true);
  }


  DxvkSpirvShader::~DxvkSpirvShader() {

  }


  SpirvCodeBuffer DxvkSpirvShader::getCode(
    const DxvkShaderBindingMap*       bindings,
    const DxvkShaderModuleCreateInfo& state) const {
    SpirvCodeBuffer spirvCode = m_code.decompress();
    uint32_t* code = spirvCode.data();

    // Remap resource binding IDs
    if (bindings) {
      for (const auto& info : m_bindingOffsets) {
        auto mappedBinding = bindings->mapBinding(DxvkShaderBinding(
          m_info.stage, info.setIndex, info.bindingIndex));

        if (mappedBinding) {
          code[info.bindingOffset] = mappedBinding->getBinding();

          if (info.setOffset)
            code[info.setOffset] = mappedBinding->getSet();
        }
      }

      for (const auto& info : m_pushDataOffsets) {
        uint32_t offset = bindings->mapPushData(m_info.stage, info.pushOffset);

        if (offset < MaxTotalPushDataSize)
          code[info.codeOffset] = offset;
      }
    }

    // For dual-source blending we need to re-map
    // location 1, index 0 to location 0, index 1
    if (state.fsDualSrcBlend && m_o1IdxOffset && m_o1LocOffset)
      std::swap(code[m_o1IdxOffset], code[m_o1LocOffset]);

    // Replace undefined input variables with zero
    for (uint32_t u : bit::BitMask(state.undefinedInputs))
      eliminateInput(spirvCode, u);

    // Patch primitive topology as necessary
    if (m_info.stage == VK_SHADER_STAGE_GEOMETRY_BIT
     && state.inputTopology != m_info.inputTopology
     && state.inputTopology != VK_PRIMITIVE_TOPOLOGY_MAX_ENUM)
      patchInputTopology(spirvCode, state.inputTopology);

    // Emit fragment shader swizzles as necessary
    if (m_info.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      emitOutputSwizzles(spirvCode, m_info.outputMask, state.rtSwizzles.data());

    // Emit input decorations for flat shading as necessary
    if (m_info.stage == VK_SHADER_STAGE_FRAGMENT_BIT && state.fsFlatShading)
      emitFlatShadingDeclarations(spirvCode, m_info.flatShadingInputs);

    return spirvCode;
  }


  void DxvkSpirvShader::dump(std::ostream& outputStream) const {
    m_code.decompress().store(outputStream);
  }


  std::string DxvkSpirvShader::debugName() const {
    return m_debugName;
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
