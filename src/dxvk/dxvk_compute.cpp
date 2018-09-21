#include <chrono>
#include <cstring>

#include "dxvk_compute.h"
#include "dxvk_device.h"
#include "dxvk_pipemanager.h"
#include "dxvk_spec_const.h"

namespace dxvk {
  
  bool DxvkComputePipelineStateInfo::operator == (const DxvkComputePipelineStateInfo& other) const {
    return std::memcmp(this, &other, sizeof(DxvkComputePipelineStateInfo)) == 0;
  }
  
  
  bool DxvkComputePipelineStateInfo::operator != (const DxvkComputePipelineStateInfo& other) const {
    return std::memcmp(this, &other, sizeof(DxvkComputePipelineStateInfo)) != 0;
  }
  
  
  DxvkComputePipeline::DxvkComputePipeline(
          DxvkPipelineManager*    pipeMgr,
    const Rc<DxvkShader>&         cs)
  : m_vkd(pipeMgr->m_device->vkd()),
    m_pipeMgr(pipeMgr) {
    DxvkDescriptorSlotMapping slotMapping;
    cs->defineResourceSlots(slotMapping);

    slotMapping.makeDescriptorsDynamic(
      m_pipeMgr->m_device->options().maxNumDynamicUniformBuffers,
      m_pipeMgr->m_device->options().maxNumDynamicStorageBuffers);
    
    m_layout = new DxvkPipelineLayout(m_vkd,
      slotMapping.bindingCount(),
      slotMapping.bindingInfos(),
      VK_PIPELINE_BIND_POINT_COMPUTE);
    
    m_cs = cs->createShaderModule(m_vkd, slotMapping);
  }
  
  
  DxvkComputePipeline::~DxvkComputePipeline() {
    this->destroyPipelines();
  }
  
  
  VkPipeline DxvkComputePipeline::getPipelineHandle(
    const DxvkComputePipelineStateInfo& state) {
    VkPipeline pipeline = VK_NULL_HANDLE;
    
    { std::lock_guard<sync::Spinlock> lock(m_mutex);
      
      if (this->findPipeline(state, pipeline))
        return pipeline;
    }
    
    // If no pipeline exists with the given state vector,
    // create a new one and add it to the pipeline set.
    VkPipeline newPipeline = this->compilePipeline(state, m_basePipeline);
    
    { std::lock_guard<sync::Spinlock> lock(m_mutex);
      
      // Discard the pipeline if another thread
      // was faster compiling the same pipeline
      if (this->findPipeline(state, pipeline)) {
        m_vkd->vkDestroyPipeline(m_vkd->device(), newPipeline, nullptr);
        return pipeline;
      }
      
      // Add new pipeline to the set
      m_pipelines.push_back({ state, newPipeline });
      m_pipeMgr->m_numComputePipelines += 1;
      
      if (m_basePipeline == VK_NULL_HANDLE)
        m_basePipeline = newPipeline;
      
      return newPipeline;
    }
  }
  
  
  bool DxvkComputePipeline::findPipeline(
    const DxvkComputePipelineStateInfo& state,
          VkPipeline&                   pipeline) const {
    for (const PipelineStruct& pair : m_pipelines) {
      if (pair.stateVector == state) {
        pipeline = pair.pipeline;
        return true;
      }
    }
    
    return false;
  }
  
  
  VkPipeline DxvkComputePipeline::compilePipeline(
    const DxvkComputePipelineStateInfo& state,
          VkPipeline                    baseHandle) const {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    if (Logger::logLevel() <= LogLevel::Debug) {
      Logger::debug("Compiling compute pipeline..."); 
      Logger::debug(str::format("  cs  : ", m_cs->shader()->debugName()));
    }
    
    DxvkSpecConstantData specData;
    
    for (uint32_t i = 0; i < MaxNumActiveBindings; i++)
      specData.activeBindings[i] = state.bsBindingMask.isBound(i) ? VK_TRUE : VK_FALSE;
    
    VkSpecializationInfo specInfo;
    specInfo.mapEntryCount        = g_specConstantMap.mapEntryCount();
    specInfo.pMapEntries          = g_specConstantMap.mapEntryData();
    specInfo.dataSize             = sizeof(specData);
    specInfo.pData                = &specData;
    
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
    
    // Time pipeline compilation for debugging purposes
    auto t0 = std::chrono::high_resolution_clock::now();
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (m_vkd->vkCreateComputePipelines(m_vkd->device(),
          m_pipeMgr->m_cache->handle(), 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
      Logger::err("DxvkComputePipeline: Failed to compile pipeline");
      Logger::err(str::format("  cs  : ", m_cs->shader()->debugName()));
      return VK_NULL_HANDLE;
    }
    
    auto t1 = std::chrono::high_resolution_clock::now();
    auto td = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    Logger::debug(str::format("DxvkComputePipeline: Finished in ", td.count(), " ms"));
    return pipeline;
  }
  
  
  void DxvkComputePipeline::destroyPipelines() {
    for (const PipelineStruct& pair : m_pipelines)
      m_vkd->vkDestroyPipeline(m_vkd->device(), pair.pipeline, nullptr);
  }
  
}