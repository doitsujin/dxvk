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
  }
  
  
  DxvkComputePipeline::~DxvkComputePipeline() {
    this->destroyPipelines();
  }
  
  
  VkPipeline DxvkComputePipeline::getPipelineHandle(
    const DxvkComputePipelineStateInfo& state) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const PipelineStruct& pair : m_pipelines) {
      if (pair.stateVector == state)
        return pair.pipeline;
    }
    
    VkPipeline pipeline = this->compilePipeline(state, m_basePipeline);
    m_pipelines.push_back({ state, pipeline });
    
    if (m_basePipeline == VK_NULL_HANDLE)
      m_basePipeline = pipeline;
    return pipeline;
  }
  
  
  VkPipeline DxvkComputePipeline::compilePipeline(
    const DxvkComputePipelineStateInfo& state,
          VkPipeline                    baseHandle) const {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    if (Logger::logLevel() <= LogLevel::Debug) {
      Logger::debug("Compiling compute pipeline..."); 
      Logger::debug(str::format("  cs  : ", m_cs ->debugName()));
    }
    
    std::array<VkBool32,                 MaxNumActiveBindings> specData;
    std::array<VkSpecializationMapEntry, MaxNumActiveBindings> specMap;
    
    for (uint32_t i = 0; i < MaxNumActiveBindings; i++) {
      specData[i] = state.bsBindingState.isBound(i) ? VK_TRUE : VK_FALSE;
      specMap [i] = { i, static_cast<uint32_t>(sizeof(VkBool32)) * i, sizeof(VkBool32) };
    }
    
    VkSpecializationInfo specInfo;
    specInfo.mapEntryCount    = specMap.size();
    specInfo.pMapEntries      = specMap.data();
    specInfo.dataSize         = specData.size() * sizeof(VkBool32);
    specInfo.pData            = specData.data();
    
    VkComputePipelineCreateInfo info;
    info.sType                = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.pNext                = nullptr;
    info.flags                = baseHandle == VK_NULL_HANDLE
      ? VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT
      : VK_PIPELINE_CREATE_DERIVATIVE_BIT;
    info.stage                = m_cs->stageInfo(&specInfo);
    info.layout               = m_layout->pipelineLayout();
    info.basePipelineHandle   = baseHandle;
    info.basePipelineIndex    = -1;
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (m_vkd->vkCreateComputePipelines(m_vkd->device(),
          m_cache->handle(), 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
      Logger::err("DxvkComputePipeline: Failed to compile pipeline");
      return VK_NULL_HANDLE;
    }
    
    return pipeline;
  }
  
  
  void DxvkComputePipeline::destroyPipelines() {
    for (const PipelineStruct& pair : m_pipelines)
      m_vkd->vkDestroyPipeline(m_vkd->device(), pair.pipeline, nullptr);
  }
  
}