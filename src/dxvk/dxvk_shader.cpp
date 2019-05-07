#include "dxvk_shader.h"

#include <algorithm>

namespace dxvk {
  
  DxvkShaderConstData::DxvkShaderConstData() {

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

      if (ins.opCode() == spv::OpCapability)
        m_capabilities.push_back(spv::Capability(ins.arg(1)));
    }
  }
  
  
  DxvkShader::~DxvkShader() {
    
  }
  
  
  bool DxvkShader::hasCapability(spv::Capability cap) {
    auto entry = std::find(
      m_capabilities.begin(),
      m_capabilities.end(), cap);
    
    return entry != m_capabilities.end();
  }
  
  
  void DxvkShader::defineResourceSlots(
          DxvkDescriptorSlotMapping& mapping) const {
    for (const auto& slot : m_slots)
      mapping.defineSlot(slot.slot, slot.type, slot.view, m_stage, slot.access);
    
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
    
    return DxvkShaderModule(vkd, this, spirvCode);
  }
  
  
  void DxvkShader::dump(std::ostream& outputStream) const {
    m_code.decompress().store(outputStream);
  }
  
}