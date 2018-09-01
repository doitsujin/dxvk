#include "dxvk_shader.h"

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


  DxvkShaderModule::DxvkShaderModule(
    const Rc<vk::DeviceFn>&     vkd,
    const Rc<DxvkShader>&       shader,
    const SpirvCodeBuffer&      code)
  : m_vkd(vkd), m_shader(shader) {
    VkShaderModuleCreateInfo info;
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext    = nullptr;
    info.flags    = 0;
    info.codeSize = code.size();
    info.pCode    = code.data();
    
    if (m_vkd->vkCreateShaderModule(m_vkd->device(),
          &info, nullptr, &m_module) != VK_SUCCESS)
      throw DxvkError("DxvkComputePipeline::DxvkComputePipeline: Failed to create shader module");
  }
  
  
  DxvkShaderModule::~DxvkShaderModule() {
    m_vkd->vkDestroyShaderModule(
      m_vkd->device(), m_module, nullptr);
  }
  
  
  VkPipelineShaderStageCreateInfo DxvkShaderModule::stageInfo(const VkSpecializationInfo* specInfo) const {
    VkPipelineShaderStageCreateInfo info;
    info.sType                = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.pNext                = nullptr;
    info.flags                = 0;
    info.stage                = m_shader->stage();
    info.module               = m_module;
    info.pName                = "main";
    info.pSpecializationInfo  = specInfo;
    return info;
  }
  
  
  DxvkShader::DxvkShader(
          VkShaderStageFlagBits   stage,
          uint32_t                slotCount,
    const DxvkResourceSlot*       slotInfos,
    const DxvkInterfaceSlots&     iface,
    const SpirvCodeBuffer&        code,
          DxvkShaderConstData&&   constData)
  : m_stage(stage), m_code(code), m_interface(iface),
    m_constData(std::move(constData)) {
    // Write back resource slot infos
    for (uint32_t i = 0; i < slotCount; i++)
      m_slots.push_back(slotInfos[i]);
    
    // Gather the offsets where the binding IDs
    // are stored so we can quickly remap them.
    for (auto ins : m_code) {
      if (ins.opCode() == spv::OpDecorate
       && ((ins.arg(2) == spv::DecorationBinding)
        || (ins.arg(2) == spv::DecorationSpecId)))
        m_idOffsets.push_back(ins.offset() + 3);
    }
  }
  
  
  DxvkShader::~DxvkShader() {
    
  }
  
  
  bool DxvkShader::hasCapability(spv::Capability cap) {
    for (auto ins : m_code) {
      // OpCapability instructions come first
      if (ins.opCode() != spv::OpCapability)
        return false;
      
      if (ins.arg(1) == cap)
        return true;
    }
    
    return false;
  }
  
  
  void DxvkShader::defineResourceSlots(
          DxvkDescriptorSlotMapping& mapping) const {
    for (const auto& slot : m_slots)
      mapping.defineSlot(slot.slot, slot.type, slot.view, m_stage);
  }
  
  
  Rc<DxvkShaderModule> DxvkShader::createShaderModule(
    const Rc<vk::DeviceFn>&          vkd,
    const DxvkDescriptorSlotMapping& mapping) {
    SpirvCodeBuffer spirvCode = m_code;
    
    // Remap resource binding IDs
    uint32_t* code = spirvCode.data();
    for (uint32_t ofs : m_idOffsets) {
      if (code[ofs] < MaxNumResourceSlots)
        code[ofs] = mapping.getBindingId(code[ofs]);
    }
    
    return new DxvkShaderModule(vkd, this, spirvCode);
  }
  
  
  void DxvkShader::dump(std::ostream& outputStream) const {
    m_code.store(outputStream);
  }
  
}