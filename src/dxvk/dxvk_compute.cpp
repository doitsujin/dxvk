#include <cstring>

#include "dxvk_compute.h"
#include "dxvk_device.h"

namespace dxvk {
  
  bool DxvkComputePipelineStateInfo::operator == (const DxvkComputePipelineStateInfo& other) const {
    return std::memcmp(this, &other, sizeof(DxvkComputePipelineStateInfo)) == 0;
  }
  
  
  bool DxvkComputePipelineStateInfo::operator != (const DxvkComputePipelineStateInfo& other) const {
    return std::memcmp(this, &other, sizeof(DxvkComputePipelineStateInfo)) != 0;
  }
  
  
  DxvkComputePipeline::DxvkComputePipeline(
    const DxvkDevice*             device,
    const Rc<DxvkPipelineCache>&  cache,
    const Rc<DxvkShader>&         cs)
  : m_device(device), m_vkd(device->vkd()),
    m_cache(cache) {
    DxvkDescriptorSlotMapping slotMapping;
    cs->defineResourceSlots(slotMapping);
    
    m_layout = new DxvkPipelineLayout(m_vkd,
      slotMapping.bindingCount(),
      slotMapping.bindingInfos());
    
    m_cs = cs->createShaderModule(m_vkd, slotMapping);
    
    this->compilePipeline();
  }
  
  
  DxvkComputePipeline::~DxvkComputePipeline() {
    if (m_pipeline != VK_NULL_HANDLE)
      m_vkd->vkDestroyPipeline(m_vkd->device(), m_pipeline, nullptr);
  }
  
  
  VkPipeline DxvkComputePipeline::getPipelineHandle(
    const DxvkComputePipelineStateInfo& state) const {
    // TODO take pipeine state into account
    return m_pipeline;
  }
  
  
  void DxvkComputePipeline::compilePipeline() {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    if (Logger::logLevel() <= LogLevel::Debug) {
      Logger::debug("Compiling compute pipeline..."); 
      Logger::debug(str::format("  cs  : ", m_cs ->debugName()));
    }
    
    VkComputePipelineCreateInfo info;
    info.sType                = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.pNext                = nullptr;
    info.flags                = 0;
    info.stage                = m_cs->stageInfo(nullptr);
    info.layout               = m_layout->pipelineLayout();
    info.basePipelineHandle   = VK_NULL_HANDLE;
    info.basePipelineIndex    = -1;
    
    if (m_vkd->vkCreateComputePipelines(m_vkd->device(),
          m_cache->handle(), 1, &info, nullptr, &m_pipeline) != VK_SUCCESS)
      Logger::err("DxvkComputePipeline: Failed to compile pipeline");
  }
  
}