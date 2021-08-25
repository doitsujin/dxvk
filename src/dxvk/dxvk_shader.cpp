#include "dxvk_shader.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace dxvk {
  
  DxvkShaderConstData::DxvkShaderConstData()
  : m_size(0), m_data(nullptr) {

  }


  DxvkShaderConstData::DxvkShaderConstData(
          size_t                dwordCount,
    const uint32_t*             dwordArray)
  : m_size(dwordCount), m_data(new uint32_t[dwordCount]) {
    for (size_t i = 0; i < dwordCount; i++)
      m_data[i] = dwordArray[i];
  }


  DxvkShaderConstData::DxvkShaderConstData(DxvkShaderConstData&& other)
  : m_size(other.m_size), m_data(other.m_data) {
    other.m_size = 0;
    other.m_data = nullptr;
  }


  DxvkShaderConstData& DxvkShaderConstData::operator = (DxvkShaderConstData&& other) {
    delete[] m_data;
    this->m_size = other.m_size;
    this->m_data = other.m_data;
    other.m_size = 0;
    other.m_data = nullptr;
    return *this;
  }


  DxvkShaderConstData::~DxvkShaderConstData() {
    delete[] m_data;
  }


  DxvkShaderModule::DxvkShaderModule()
  : m_vkd(nullptr), m_stage() {

  }


  DxvkShaderModule::DxvkShaderModule(DxvkShaderModule&& other)
  : m_vkd(std::move(other.m_vkd)) {
    this->m_stage = other.m_stage;
    other.m_stage = VkPipelineShaderStageCreateInfo();
  }


  DxvkShaderModule::DxvkShaderModule(
    const Rc<vk::DeviceFn>&     vkd,
    const Rc<DxvkShader>&       shader,
    const SpirvCodeBuffer&      code)
  : m_vkd(vkd), m_stage() {
    m_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    m_stage.pNext = nullptr;
    m_stage.flags = 0;
    m_stage.stage = shader->stage();
    m_stage.module = VK_NULL_HANDLE;
    m_stage.pName = "main";
    m_stage.pSpecializationInfo = nullptr;

    VkShaderModuleCreateInfo info;
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext    = nullptr;
    info.flags    = 0;
    info.codeSize = code.size();
    info.pCode    = code.data();
    
    if (m_vkd->vkCreateShaderModule(m_vkd->device(), &info, nullptr, &m_stage.module) != VK_SUCCESS)
      throw DxvkError("DxvkComputePipeline::DxvkComputePipeline: Failed to create shader module");
  }
  
  
  DxvkShaderModule::~DxvkShaderModule() {
    if (m_vkd != nullptr) {
      m_vkd->vkDestroyShaderModule(
        m_vkd->device(), m_stage.module, nullptr);
    }
  }
  
  
  DxvkShaderModule& DxvkShaderModule::operator = (DxvkShaderModule&& other) {
    this->m_vkd   = std::move(other.m_vkd);
    this->m_stage = other.m_stage;
    other.m_stage = VkPipelineShaderStageCreateInfo();
    return *this;
  }


  DxvkShader::DxvkShader(
          VkShaderStageFlagBits   stage,
          uint32_t                slotCount,
    const DxvkResourceSlot*       slotInfos,
    const DxvkInterfaceSlots&     iface,
          SpirvCodeBuffer         code,
    const DxvkShaderOptions&      options,
          DxvkShaderConstData&&   constData)
  : m_stage(stage), m_code(code), m_interface(iface),
    m_options(options), m_constData(std::move(constData)) {
    // Write back resource slot infos
    for (uint32_t i = 0; i < slotCount; i++)
      m_slots.push_back(slotInfos[i]);
    
    // Gather the offsets where the binding IDs
    // are stored so we can quickly remap them.
    uint32_t o1VarId = 0;
    
    for (auto ins : code) {
      if (ins.opCode() == spv::OpDecorate) {
        if (ins.arg(2) == spv::DecorationBinding
         || ins.arg(2) == spv::DecorationSpecId)
          m_idOffsets.push_back(ins.offset() + 3);
        
        if (ins.arg(2) == spv::DecorationLocation && ins.arg(3) == 1) {
          m_o1LocOffset = ins.offset() + 3;
          o1VarId = ins.arg(1);
        }
        
        if (ins.arg(2) == spv::DecorationIndex && ins.arg(1) == o1VarId)
          m_o1IdxOffset = ins.offset() + 3;
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

        if (ins.arg(1) == spv::CapabilityShaderViewportIndexLayerEXT)
          m_flags.set(DxvkShaderFlag::ExportsViewportIndexLayerFromVertexStage);
      }
    }
  }
  
  
  DxvkShader::~DxvkShader() {
    
  }
  
  
  void DxvkShader::defineResourceSlots(
          DxvkDescriptorSlotMapping& mapping) const {
    for (const auto& slot : m_slots)
      mapping.defineSlot(m_stage, slot);
    
    if (m_interface.pushConstSize) {
      mapping.definePushConstRange(m_stage,
        m_interface.pushConstOffset,
        m_interface.pushConstSize);
    }
  }
  
  
  DxvkShaderModule DxvkShader::createShaderModule(
    const Rc<vk::DeviceFn>&          vkd,
    const DxvkDescriptorSlotMapping& mapping,
    const DxvkShaderModuleCreateInfo& info) {
    SpirvCodeBuffer spirvCode = m_code.decompress();
    uint32_t* code = spirvCode.data();
    
    // Remap resource binding IDs
    for (uint32_t ofs : m_idOffsets) {
      if (code[ofs] < MaxNumResourceSlots)
        code[ofs] = mapping.getBindingId(code[ofs]);
    }

    // For dual-source blending we need to re-map
    // location 1, index 0 to location 0, index 1
    if (info.fsDualSrcBlend && m_o1IdxOffset && m_o1LocOffset)
      std::swap(code[m_o1IdxOffset], code[m_o1LocOffset]);
    
    // Replace undefined input variables with zero
    for (uint32_t u : bit::BitMask(info.undefinedInputs))
      eliminateInput(spirvCode, u);

    return DxvkShaderModule(vkd, this, spirvCode);
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
      }
    }

    // Remove location declarations
    for (auto ins : code) {
      if (ins.opCode() == spv::OpDecorate
       && ins.arg(2) == spv::DecorationLocation
       && ins.arg(1) == inputVarId) {
        code.beginInsertion(ins.offset());
        code.erase(4);
        code.endInsertion();
        break;
      }
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
  
}