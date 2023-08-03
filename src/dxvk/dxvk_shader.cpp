#include "dxvk_device.h"
#include "dxvk_pipemanager.h"
#include "dxvk_shader.h"

#include <dxvk_dummy_frag.h>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace dxvk {
  
  bool DxvkShaderModuleCreateInfo::eq(const DxvkShaderModuleCreateInfo& other) const {
    bool eq = fsDualSrcBlend  == other.fsDualSrcBlend
           && fsFlatShading   == other.fsFlatShading
           && undefinedInputs == other.undefinedInputs;

    for (uint32_t i = 0; i < rtSwizzles.size() && eq; i++) {
      eq = rtSwizzles[i].r == other.rtSwizzles[i].r
        && rtSwizzles[i].g == other.rtSwizzles[i].g
        && rtSwizzles[i].b == other.rtSwizzles[i].b
        && rtSwizzles[i].a == other.rtSwizzles[i].a;
    }

    return eq;
  }


  size_t DxvkShaderModuleCreateInfo::hash() const {
    DxvkHashState hash;
    hash.add(uint32_t(fsDualSrcBlend));
    hash.add(uint32_t(fsFlatShading));
    hash.add(undefinedInputs);

    for (uint32_t i = 0; i < rtSwizzles.size(); i++) {
      hash.add(rtSwizzles[i].r);
      hash.add(rtSwizzles[i].g);
      hash.add(rtSwizzles[i].b);
      hash.add(rtSwizzles[i].a);
    }

    return hash;
  }


  DxvkShader::DxvkShader(
    const DxvkShaderCreateInfo&   info,
          SpirvCodeBuffer&&       spirv)
  : m_info(info), m_code(spirv), m_bindings(info.stage) {
    m_info.uniformData = nullptr;
    m_info.bindings = nullptr;

    // Copy resource binding slot infos
    for (uint32_t i = 0; i < info.bindingCount; i++) {
      DxvkBindingInfo binding = info.bindings[i];
      binding.stage = info.stage;
      m_bindings.addBinding(binding);
    }

    if (info.pushConstSize) {
      VkPushConstantRange pushConst;
      pushConst.stageFlags = info.stage;
      pushConst.offset = info.pushConstOffset;
      pushConst.size = info.pushConstSize;

      m_bindings.addPushConstantRange(pushConst);
    }

    // Copy uniform buffer data
    if (info.uniformSize) {
      m_uniformData.resize(info.uniformSize);
      std::memcpy(m_uniformData.data(), info.uniformData, info.uniformSize);
      m_info.uniformData = m_uniformData.data();
    }

    // Run an analysis pass over the SPIR-V code to gather some
    // info that we may need during pipeline compilation.
    std::vector<BindingOffsets> bindingOffsets;
    std::vector<uint32_t> varIds;
    std::vector<uint32_t> sampleMaskIds;

    SpirvCodeBuffer code = std::move(spirv);
    uint32_t o1VarId = 0;
    
    for (auto ins : code) {
      if (ins.opCode() == spv::OpDecorate) {
        if (ins.arg(2) == spv::DecorationBinding) {
          uint32_t varId = ins.arg(1);
          bindingOffsets.resize(std::max(bindingOffsets.size(), size_t(varId + 1)));
          bindingOffsets[varId].bindingId = ins.arg(3);
          bindingOffsets[varId].bindingOffset = ins.offset() + 3;
          varIds.push_back(varId);
        }

        if (ins.arg(2) == spv::DecorationBuiltIn) {
          if (ins.arg(3) == spv::BuiltInSampleMask)
            sampleMaskIds.push_back(ins.arg(1));
          if (ins.arg(3) == spv::BuiltInPosition)
            m_flags.set(DxvkShaderFlag::ExportsPosition);
        }

        if (ins.arg(2) == spv::DecorationDescriptorSet) {
          uint32_t varId = ins.arg(1);
          bindingOffsets.resize(std::max(bindingOffsets.size(), size_t(varId + 1)));
          bindingOffsets[varId].setOffset = ins.offset() + 3;
        }

        if (ins.arg(2) == spv::DecorationSpecId) {
          if (ins.arg(3) <= MaxNumSpecConstants)
            m_specConstantMask |= 1u << ins.arg(3);
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
            m_flags.set(DxvkShaderFlag::ExportsPosition);
        }
      }

      if (ins.opCode() == spv::OpExecutionMode) {
        if (ins.arg(2) == spv::ExecutionModeStencilRefReplacingEXT)
          m_flags.set(DxvkShaderFlag::ExportsStencilRef);

        if (ins.arg(2) == spv::ExecutionModeXfb)
          m_flags.set(DxvkShaderFlag::HasTransformFeedback);
      }

      if (ins.opCode() == spv::OpCapability) {
        if (ins.arg(1) == spv::CapabilitySampleRateShading)
          m_flags.set(DxvkShaderFlag::HasSampleRateShading);

        if (ins.arg(1) == spv::CapabilityShaderViewportIndex
         || ins.arg(1) == spv::CapabilityShaderLayer)
          m_flags.set(DxvkShaderFlag::ExportsViewportIndexLayerFromVertexStage);

        if (ins.arg(1) == spv::CapabilitySparseResidency)
          m_flags.set(DxvkShaderFlag::UsesSparseResidency);

        if (ins.arg(1) == spv::CapabilityFragmentFullyCoveredEXT)
          m_flags.set(DxvkShaderFlag::UsesFragmentCoverage);
      }

      if (ins.opCode() == spv::OpVariable) {
        if (ins.arg(3) == spv::StorageClassOutput) {
          if (std::find(sampleMaskIds.begin(), sampleMaskIds.end(), ins.arg(2)) != sampleMaskIds.end())
            m_flags.set(DxvkShaderFlag::ExportsSampleMask);
        }
      }

      // Ignore the actual shader code, there's nothing interesting for us in there.
      if (ins.opCode() == spv::OpFunction)
        break;
    }

    // Combine spec constant IDs with other binding info
    for (auto varId : varIds) {
      BindingOffsets info = bindingOffsets[varId];

      if (info.bindingOffset)
        m_bindingOffsets.push_back(info);
    }

    // Don't set pipeline library flag if the shader
    // doesn't actually support pipeline libraries
    m_needsLibraryCompile = canUsePipelineLibrary(true);
  }


  DxvkShader::~DxvkShader() {
    
  }
  
  
  SpirvCodeBuffer DxvkShader::getCode(
    const DxvkBindingLayoutObjects*   layout,
    const DxvkShaderModuleCreateInfo& state) const {
    SpirvCodeBuffer spirvCode = m_code.decompress();
    uint32_t* code = spirvCode.data();
    
    // Remap resource binding IDs
    for (const auto& info : m_bindingOffsets) {
      auto mappedBinding = layout->lookupBinding(m_info.stage, info.bindingId);

      if (mappedBinding) {
        code[info.bindingOffset] = mappedBinding->binding;

        if (info.setOffset)
          code[info.setOffset] = mappedBinding->set;
      }
    }

    // For dual-source blending we need to re-map
    // location 1, index 0 to location 0, index 1
    if (state.fsDualSrcBlend && m_o1IdxOffset && m_o1LocOffset)
      std::swap(code[m_o1IdxOffset], code[m_o1LocOffset]);
    
    // Replace undefined input variables with zero
    for (uint32_t u : bit::BitMask(state.undefinedInputs))
      eliminateInput(spirvCode, u);

    // Emit fragment shader swizzles as necessary
    if (m_info.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
      emitOutputSwizzles(spirvCode, m_info.outputMask, state.rtSwizzles.data());

    // Emit input decorations for flat shading as necessary
    if (m_info.stage == VK_SHADER_STAGE_FRAGMENT_BIT && state.fsFlatShading)
      emitFlatShadingDeclarations(spirvCode, m_info.flatShadingInputs);

    return spirvCode;
  }


  bool DxvkShader::canUsePipelineLibrary(bool standalone) const {
    if (standalone) {
      // Standalone pipeline libraries are unsupported for geometry
      // and tessellation stages since we'd need to compile them
      // all into one library
      if (m_info.stage != VK_SHADER_STAGE_VERTEX_BIT
       && m_info.stage != VK_SHADER_STAGE_FRAGMENT_BIT
       && m_info.stage != VK_SHADER_STAGE_COMPUTE_BIT)
        return false;

      // Standalone vertex shaders must export vertex position
      if (m_info.stage == VK_SHADER_STAGE_VERTEX_BIT
       && !m_flags.test(DxvkShaderFlag::ExportsPosition))
        return false;
    } else {
      // Tessellation control shaders must define a valid vertex count
      if (m_info.stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT
       && (m_info.patchVertexCount < 1 || m_info.patchVertexCount > 32))
        return false;

      // We don't support GPL with transform feedback right now
      if (m_flags.test(DxvkShaderFlag::HasTransformFeedback))
        return false;
    }

    // Spec constant selectors are only supported in graphics
    if (m_specConstantMask & (1u << MaxNumSpecConstants))
      return m_info.stage != VK_SHADER_STAGE_COMPUTE_BIT;

    // Always late-compile shaders with spec constants
    // that don't use the spec constant selector
    return !m_specConstantMask;
  }


  void DxvkShader::dump(std::ostream& outputStream) const {
    m_code.decompress().store(outputStream);
  }


  void DxvkShader::eliminateInput(SpirvCodeBuffer& code, uint32_t location) {
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
  

  void DxvkShader::emitOutputSwizzles(
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


  void DxvkShader::emitFlatShadingDeclarations(
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


  DxvkShaderStageInfo::DxvkShaderStageInfo(const DxvkDevice* device)
  : m_device(device) {

  }

  void DxvkShaderStageInfo::addStage(
          VkShaderStageFlagBits   stage,
          SpirvCodeBuffer&&       code,
    const VkSpecializationInfo*   specInfo) {
    // Take ownership of the SPIR-V code buffer
    auto& codeBuffer = m_codeBuffers[m_stageCount];
    codeBuffer = std::move(code);

    // For graphics pipelines, as long as graphics pipeline libraries are
    // enabled, we do not need to create a shader module object and can
    // instead chain the create info to the shader stage info struct.
    auto& moduleInfo = m_moduleInfos[m_stageCount].moduleInfo;
    moduleInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    moduleInfo.codeSize = codeBuffer.size();
    moduleInfo.pCode = codeBuffer.data();

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (!m_device->features().extGraphicsPipelineLibrary.graphicsPipelineLibrary) {
      auto vk = m_device->vkd();

      if (vk->vkCreateShaderModule(vk->device(), &moduleInfo, nullptr, &shaderModule))
        throw DxvkError("DxvkShaderStageInfo: Failed to create shader module");
    }

    // Set up shader stage info with the data provided
    auto& stageInfo = m_stageInfos[m_stageCount];
    stageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };

    if (!stageInfo.module)
      stageInfo.pNext = &moduleInfo;

    stageInfo.stage = stage;
    stageInfo.module = shaderModule;
    stageInfo.pName = "main";
    stageInfo.pSpecializationInfo = specInfo;

    m_stageCount++;
  }
  

  void DxvkShaderStageInfo::addStage(
          VkShaderStageFlagBits   stage,
    const VkShaderModuleIdentifierEXT& identifier,
    const VkSpecializationInfo*   specInfo) {
    // Copy relevant bits of the module identifier
    uint32_t identifierSize = std::min(identifier.identifierSize, VK_MAX_SHADER_MODULE_IDENTIFIER_SIZE_EXT);

    auto& moduleId = m_moduleInfos[m_stageCount].moduleIdentifier;
    moduleId.createInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_MODULE_IDENTIFIER_CREATE_INFO_EXT };
    moduleId.createInfo.identifierSize = identifierSize;
    moduleId.createInfo.pIdentifier = moduleId.data.data();
    std::memcpy(moduleId.data.data(), identifier.identifier, identifierSize);

    // Set up stage info using the module identifier
    auto& stageInfo = m_stageInfos[m_stageCount];
    stageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stageInfo.pNext = &moduleId.createInfo;
    stageInfo.stage = stage;
    stageInfo.pName = "main";
    stageInfo.pSpecializationInfo = specInfo;

    m_stageCount++;
  }


  DxvkShaderStageInfo::~DxvkShaderStageInfo() {
    auto vk = m_device->vkd();

    for (uint32_t i = 0; i < m_stageCount; i++) {
      if (m_stageInfos[i].module)
        vk->vkDestroyShaderModule(vk->device(), m_stageInfos[i].module, nullptr);
    }
  }


  DxvkShaderPipelineLibraryKey::DxvkShaderPipelineLibraryKey() {

  }


  DxvkShaderPipelineLibraryKey::~DxvkShaderPipelineLibraryKey() {

  }


  DxvkShaderSet DxvkShaderPipelineLibraryKey::getShaderSet() const {
    DxvkShaderSet result;

    for (uint32_t i = 0; i < m_shaderCount; i++) {
      auto shader = m_shaders[i].ptr();

      switch (shader->info().stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:                  result.vs = shader; break;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    result.tcs = shader; break;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: result.tes = shader; break;
        case VK_SHADER_STAGE_GEOMETRY_BIT:                result.gs = shader; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT:                result.fs = shader; break;
        case VK_SHADER_STAGE_COMPUTE_BIT:                 result.cs = shader; break;
        default: ;
      }
    }

    return result;
  }


  DxvkBindingLayout DxvkShaderPipelineLibraryKey::getBindings() const {
    DxvkBindingLayout mergedLayout(m_shaderStages);

    for (uint32_t i = 0; i < m_shaderCount; i++)
      mergedLayout.merge(m_shaders[i]->getBindings());

    return mergedLayout;
  }


  void DxvkShaderPipelineLibraryKey::addShader(
    const Rc<DxvkShader>&               shader) {
    m_shaderStages |= shader->info().stage;
    m_shaders[m_shaderCount++] = shader;
  }


  bool DxvkShaderPipelineLibraryKey::canUsePipelineLibrary() const {
    // Ensure that each individual shader can be used in a library
    bool standalone = m_shaderCount <= 1;

    for (uint32_t i = 0; i < m_shaderCount; i++) {
      if (!m_shaders[i]->canUsePipelineLibrary(standalone))
        return false;
    }

    // Ensure that stage I/O is compatible between stages
    for (uint32_t i = 0; i + 1 < m_shaderCount; i++) {
      uint32_t currStageIoMask = m_shaders[i]->info().outputMask;
      uint32_t nextStageIoMask = m_shaders[i + 1]->info().inputMask;

      if ((currStageIoMask & nextStageIoMask) != nextStageIoMask)
        return false;
    }

    return true;
  }


  bool DxvkShaderPipelineLibraryKey::eq(
    const DxvkShaderPipelineLibraryKey& other) const {
    bool eq = m_shaderStages == other.m_shaderStages;

    for (uint32_t i = 0; i < m_shaderCount && eq; i++)
      eq = m_shaders[i] == other.m_shaders[i];

    return eq;
  }


  size_t DxvkShaderPipelineLibraryKey::hash() const {
    DxvkHashState hash;
    hash.add(uint32_t(m_shaderStages));

    for (uint32_t i = 0; i < m_shaderCount; i++)
      hash.add(m_shaders[i]->getHash());

    return hash;
  }


  DxvkShaderPipelineLibrary::DxvkShaderPipelineLibrary(
    const DxvkDevice*               device,
          DxvkPipelineManager*      manager,
    const DxvkShaderPipelineLibraryKey& key,
    const DxvkBindingLayoutObjects* layout)
  : m_device      (device),
    m_stats       (&manager->m_stats),
    m_shaders     (key.getShaderSet()),
    m_layout      (layout) {

  }


  DxvkShaderPipelineLibrary::~DxvkShaderPipelineLibrary() {
    this->destroyShaderPipelinesLocked();
  }


  VkShaderModuleIdentifierEXT DxvkShaderPipelineLibrary::getModuleIdentifier(
          VkShaderStageFlagBits                 stage) {
    std::lock_guard lock(m_identifierMutex);
    auto identifier = getShaderIdentifier(stage);

    if (!identifier->identifierSize) {
      // Unfortunate, but we'll have to decode the
      // shader code here to retrieve the identifier
      SpirvCodeBuffer spirvCode = this->getShaderCode(stage);
      this->generateModuleIdentifierLocked(identifier, spirvCode);
    }

    return *identifier;
  }


  VkPipeline DxvkShaderPipelineLibrary::acquirePipelineHandle(
    const DxvkShaderPipelineLibraryCompileArgs& args) {
    std::lock_guard lock(m_mutex);

    if (m_device->mustTrackPipelineLifetime())
      m_useCount += 1;

    VkPipeline& pipeline = (m_shaders.vs && !args.depthClipEnable)
      ? m_pipelineNoDepthClip
      : m_pipeline;

    if (pipeline)
      return pipeline;

    pipeline = compileShaderPipelineLocked(args);
    return pipeline;
  }


  void DxvkShaderPipelineLibrary::releasePipelineHandle() {
    if (m_device->mustTrackPipelineLifetime()) {
      std::lock_guard lock(m_mutex);

      if (!(--m_useCount))
        this->destroyShaderPipelinesLocked();
    }
  }


  void DxvkShaderPipelineLibrary::compilePipeline() {
    std::lock_guard lock(m_mutex);

    // Skip if a pipeline has already been compiled
    if (m_compiledOnce)
      return;

    // Compile the pipeline with default args
    VkPipeline pipeline = compileShaderPipelineLocked(
      DxvkShaderPipelineLibraryCompileArgs());

    // On 32-bit, destroy the pipeline immediately in order to
    // save memory. We should hit the driver's disk cache once
    // we need to recreate the pipeline.
    if (m_device->mustTrackPipelineLifetime()) {
      auto vk = m_device->vkd();
      vk->vkDestroyPipeline(vk->device(), pipeline, nullptr);

      pipeline = VK_NULL_HANDLE;
    }

    // Write back pipeline handle for future use
    m_pipeline = pipeline;
  }


  void DxvkShaderPipelineLibrary::destroyShaderPipelinesLocked() {
    auto vk = m_device->vkd();

    vk->vkDestroyPipeline(vk->device(), m_pipeline, nullptr);
    vk->vkDestroyPipeline(vk->device(), m_pipelineNoDepthClip, nullptr);

    m_pipeline = VK_NULL_HANDLE;
    m_pipelineNoDepthClip = VK_NULL_HANDLE;
  }


  VkPipeline DxvkShaderPipelineLibrary::compileShaderPipelineLocked(
    const DxvkShaderPipelineLibraryCompileArgs& args) {
    this->notifyLibraryCompile();

    // If this is not the first time we're compiling the pipeline,
    // try to get a cache hit using the shader module identifier
    // so that we don't have to decompress our SPIR-V shader again.
    VkPipeline pipeline = VK_NULL_HANDLE;

    if (m_compiledOnce && canUsePipelineCacheControl()) {
      pipeline = this->compileShaderPipeline(args,
        VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT);
    }

    if (!pipeline)
      pipeline = this->compileShaderPipeline(args, 0);

    // Well that didn't work
    if (!pipeline)
      return VK_NULL_HANDLE;

    // Increment stat counter the first time this
    // shader pipeline gets compiled successfully
    if (!m_compiledOnce) {
      if (m_shaders.cs)
        m_stats->numComputePipelines += 1;
      else
        m_stats->numGraphicsLibraries += 1;

      m_compiledOnce = true;
    }

    return pipeline;
  }


  VkPipeline DxvkShaderPipelineLibrary::compileShaderPipeline(
    const DxvkShaderPipelineLibraryCompileArgs& args,
          VkPipelineCreateFlags                 flags) {
    DxvkShaderStageInfo stageInfo(m_device);
    VkShaderStageFlags stageMask = getShaderStages();

    { std::lock_guard lock(m_identifierMutex);
      VkShaderStageFlags stages = stageMask;

      while (stages) {
        auto stage = VkShaderStageFlagBits(stages & -stages);
        auto identifier = getShaderIdentifier(stage);

        if (flags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT) {
          // Fail if we have no idenfitier for whatever reason, caller
          // should fall back to the slow path if this happens
          if (!identifier->identifierSize)
            return VK_NULL_HANDLE;

          stageInfo.addStage(stage, *identifier, nullptr);
        } else {
          // Decompress code and generate identifier as needed
          SpirvCodeBuffer spirvCode = this->getShaderCode(stage);

          if (!identifier->identifierSize)
            this->generateModuleIdentifierLocked(identifier, spirvCode);

          stageInfo.addStage(stage, std::move(spirvCode), nullptr);
        }

        stages &= stages - 1;
      }
    }

    if (stageMask & VK_SHADER_STAGE_VERTEX_BIT)
      return compileVertexShaderPipeline(args, stageInfo, flags);

    if (stageMask & VK_SHADER_STAGE_FRAGMENT_BIT)
      return compileFragmentShaderPipeline(stageInfo, flags);

    if (stageMask & VK_SHADER_STAGE_COMPUTE_BIT)
      return compileComputeShaderPipeline(stageInfo, flags);

    // Should be unreachable
    return VK_NULL_HANDLE;
  }


  VkPipeline DxvkShaderPipelineLibrary::compileVertexShaderPipeline(
    const DxvkShaderPipelineLibraryCompileArgs& args,
    const DxvkShaderStageInfo&          stageInfo,
          VkPipelineCreateFlags         flags) {
    auto vk = m_device->vkd();

    // Set up dynamic state. We do not know any pipeline state
    // at this time, so make as much state dynamic as we can.
    uint32_t dynamicStateCount = 0;
    std::array<VkDynamicState, 7> dynamicStates;

    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BIAS;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_CULL_MODE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_FRONT_FACE;

    if (m_device->features().extExtendedDynamicState3.extendedDynamicState3DepthClipEnable)
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT;

    VkPipelineDynamicStateCreateInfo dyInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dyInfo.dynamicStateCount  = dynamicStateCount;
    dyInfo.pDynamicStates     = dynamicStates.data();

    // If a tessellation control shader is present, grab the patch vertex count
    VkPipelineTessellationStateCreateInfo tsInfo = { VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO };

    if (m_shaders.tcs)
      tsInfo.patchControlPoints = m_shaders.tcs->info().patchVertexCount;

    // All viewport state is dynamic, so we do not need to initialize this.
    VkPipelineViewportStateCreateInfo vpInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };

    // Set up rasterizer state. Depth bias, cull mode and front face are
    // all dynamic. Do not support any polygon modes other than FILL.
    VkPipelineRasterizationDepthClipStateCreateInfoEXT rsDepthClipInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT };

    VkPipelineRasterizationStateCreateInfo rsInfo = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsInfo.depthClampEnable   = VK_TRUE;
    rsInfo.rasterizerDiscardEnable = VK_FALSE;
    rsInfo.polygonMode        = VK_POLYGON_MODE_FILL;
    rsInfo.lineWidth          = 1.0f;

    if (m_device->features().extDepthClipEnable.depthClipEnable) {
      // Only use the fixed depth clip state if we can't make it dynamic
      if (!m_device->features().extExtendedDynamicState3.extendedDynamicState3DepthClipEnable) {
        rsDepthClipInfo.pNext = std::exchange(rsInfo.pNext, &rsDepthClipInfo);
        rsDepthClipInfo.depthClipEnable = args.depthClipEnable;
      }
    } else {
      rsInfo.depthClampEnable = !args.depthClipEnable;
    }

    // Only the view mask is used as input, and since we do not use MultiView, it is always 0
    VkPipelineRenderingCreateInfo rtInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

    VkGraphicsPipelineLibraryCreateInfoEXT libInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, &rtInfo };
    libInfo.flags             = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
    info.flags                = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | flags;
    info.stageCount           = stageInfo.getStageCount();
    info.pStages              = stageInfo.getStageInfos();
    info.pTessellationState   = m_shaders.tcs ? &tsInfo : nullptr;
    info.pViewportState       = &vpInfo;
    info.pRasterizationState  = &rsInfo;
    info.pDynamicState        = &dyInfo;
    info.layout               = m_layout->getPipelineLayout(true);
    info.basePipelineIndex    = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr && !(flags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT))
      Logger::err(str::format("DxvkShaderPipelineLibrary: Failed to create vertex shader pipeline: ", vr));

    return vr ? VK_NULL_HANDLE : pipeline;
  }


  VkPipeline DxvkShaderPipelineLibrary::compileFragmentShaderPipeline(
    const DxvkShaderStageInfo&          stageInfo,
          VkPipelineCreateFlags         flags) {
    auto vk = m_device->vkd();

    // Set up dynamic state. We do not know any pipeline state
    // at this time, so make as much state dynamic as we can.
    uint32_t dynamicStateCount = 0;
    std::array<VkDynamicState, 13> dynamicStates;

    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_COMPARE_OP;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_OP;

    if (m_device->features().core.features.depthBounds) {
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE;
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;
    }

    bool hasSampleRateShading = m_shaders.fs && m_shaders.fs->flags().test(DxvkShaderFlag::HasSampleRateShading);
    bool hasDynamicMultisampleState = hasSampleRateShading
      && m_device->features().extExtendedDynamicState3.extendedDynamicState3RasterizationSamples
      && m_device->features().extExtendedDynamicState3.extendedDynamicState3SampleMask;

    if (hasDynamicMultisampleState) {
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT;
      dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_SAMPLE_MASK_EXT;

      if (!m_shaders.fs || !m_shaders.fs->flags().test(DxvkShaderFlag::ExportsSampleMask)) {
        if (m_device->features().extExtendedDynamicState3.extendedDynamicState3AlphaToCoverageEnable)
          dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT;
      }
    }

    VkPipelineDynamicStateCreateInfo dyInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dyInfo.dynamicStateCount  = dynamicStateCount;
    dyInfo.pDynamicStates     = dynamicStates.data();

    // Set up multisample state. If sample shading is enabled, assume that
    // we only have one sample enabled, with a non-zero sample mask and no
    // alpha-to-coverage.
    VkSampleMask msSampleMask = 0x1;

    VkPipelineMultisampleStateCreateInfo msInfo = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msInfo.sampleShadingEnable  = VK_TRUE;
    msInfo.minSampleShading     = 1.0f;

    if (!hasDynamicMultisampleState) {
      msInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
      msInfo.pSampleMask          = &msSampleMask;
    }

    // All depth-stencil state is dynamic, so no need to initialize this.
    // Depth bounds testing is disabled on devices which don't support it.
    VkPipelineDepthStencilStateCreateInfo dsInfo = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

    // Only the view mask is used as input, and since we do not use MultiView, it is always 0
    VkPipelineRenderingCreateInfo rtInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

    VkGraphicsPipelineLibraryCreateInfoEXT libInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT, &rtInfo };
    libInfo.flags             = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

    VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &libInfo };
    info.flags                = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | flags;
    info.stageCount           = stageInfo.getStageCount();
    info.pStages              = stageInfo.getStageInfos();
    info.pDepthStencilState   = &dsInfo;
    info.pDynamicState        = &dyInfo;
    info.layout               = m_layout->getPipelineLayout(true);
    info.basePipelineIndex    = -1;

    if (hasSampleRateShading)
      info.pMultisampleState  = &msInfo;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateGraphicsPipelines(vk->device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr && !(flags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT))
      Logger::err(str::format("DxvkShaderPipelineLibrary: Failed to create fragment shader pipeline: ", vr));

    return vr ? VK_NULL_HANDLE : pipeline;
  }


  VkPipeline DxvkShaderPipelineLibrary::compileComputeShaderPipeline(
    const DxvkShaderStageInfo&          stageInfo,
          VkPipelineCreateFlags         flags) {
    auto vk = m_device->vkd();

    // Compile the compute pipeline as normal
    VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    info.flags        = flags;
    info.stage        = *stageInfo.getStageInfos();
    info.layout       = m_layout->getPipelineLayout(false);
    info.basePipelineIndex = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateComputePipelines(vk->device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr && !(flags & VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT))
      Logger::err(str::format("DxvkShaderPipelineLibrary: Failed to create compute shader pipeline: ", vr));

    return vr ? VK_NULL_HANDLE : pipeline;
  }


  SpirvCodeBuffer DxvkShaderPipelineLibrary::getShaderCode(VkShaderStageFlagBits stage) const {
    // As a special case, it is possible that we have to deal with
    // a null shader, but the pipeline library extension requires
    // us to always specify a fragment shader for fragment stages,
    // so we need to return a dummy shader in that case.
    DxvkShader* shader = getShader(stage);

    if (!shader)
      return SpirvCodeBuffer(dxvk_dummy_frag);

    return shader->getCode(m_layout, DxvkShaderModuleCreateInfo());
  }


  void DxvkShaderPipelineLibrary::generateModuleIdentifierLocked(
          VkShaderModuleIdentifierEXT*  identifier,
    const SpirvCodeBuffer&              spirvCode) {
    auto vk = m_device->vkd();

    if (!canUsePipelineCacheControl())
      return;

    VkShaderModuleCreateInfo info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    info.codeSize = spirvCode.size();
    info.pCode = spirvCode.data();

    vk->vkGetShaderModuleCreateInfoIdentifierEXT(
      vk->device(), &info, identifier);
  }


  VkShaderStageFlags DxvkShaderPipelineLibrary::getShaderStages() const {
    if (m_shaders.vs) {
      VkShaderStageFlags result = VK_SHADER_STAGE_VERTEX_BIT;

      if (m_shaders.tcs)
        result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

      if (m_shaders.tes)
        result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

      if (m_shaders.gs)
        result |= VK_SHADER_STAGE_GEOMETRY_BIT;

      return result;
    }

    if (m_shaders.cs)
      return VK_SHADER_STAGE_COMPUTE_BIT;

    // Must be a fragment shader even if fs is null
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  }


  DxvkShader* DxvkShaderPipelineLibrary::getShader(
          VkShaderStageFlagBits         stage) const {
    switch (stage) {
      case VK_SHADER_STAGE_VERTEX_BIT:
        return m_shaders.vs;

      case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return m_shaders.tcs;

      case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return m_shaders.tes;

      case VK_SHADER_STAGE_GEOMETRY_BIT:
        return m_shaders.gs;

      case VK_SHADER_STAGE_FRAGMENT_BIT:
        return m_shaders.fs;

      case VK_SHADER_STAGE_COMPUTE_BIT:
        return m_shaders.cs;

      default:
        return nullptr;
    }
  }


  VkShaderModuleIdentifierEXT* DxvkShaderPipelineLibrary::getShaderIdentifier(
          VkShaderStageFlagBits         stage) {
    switch (stage) {
      case VK_SHADER_STAGE_VERTEX_BIT:
        return &m_identifiers.vs;

      case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return &m_identifiers.tcs;

      case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return &m_identifiers.tes;

      case VK_SHADER_STAGE_GEOMETRY_BIT:
        return &m_identifiers.gs;

      case VK_SHADER_STAGE_FRAGMENT_BIT:
        return &m_identifiers.fs;

      case VK_SHADER_STAGE_COMPUTE_BIT:
        return &m_identifiers.cs;

      default:
        return nullptr;
    }
  }


  void DxvkShaderPipelineLibrary::notifyLibraryCompile() const {
    if (m_shaders.vs) {
      // Only notify the shader itself if we're actually
      // building the shader's standalone pipeline library
      if (!m_shaders.tcs && !m_shaders.tes && !m_shaders.gs)
        m_shaders.vs->notifyLibraryCompile();
    }

    if (m_shaders.fs)
      m_shaders.fs->notifyLibraryCompile();

    if (m_shaders.cs)
      m_shaders.cs->notifyLibraryCompile();
  }


  bool DxvkShaderPipelineLibrary::canUsePipelineCacheControl() const {
    const auto& features = m_device->features();

    return features.vk13.pipelineCreationCacheControl
        && features.extShaderModuleIdentifier.shaderModuleIdentifier;
  }

}