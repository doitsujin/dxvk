#include <cstring>

#include "../util/util_time.h"

#include "dxvk_compute.h"
#include "dxvk_device.h"
#include "dxvk_pipemanager.h"
#include "dxvk_spec_const.h"
#include "dxvk_state_cache.h"

namespace dxvk {
  
  DxvkComputePipeline::DxvkComputePipeline(
          DxvkPipelineManager*        pipeMgr,
          DxvkComputePipelineShaders  shaders)
  : m_vkd(pipeMgr->m_device->vkd()), m_pipeMgr(pipeMgr),
    m_shaders(std::move(shaders)) {
    m_shaders.cs->defineResourceSlots(m_slotMapping);

    m_slotMapping.makeDescriptorsDynamic(
      m_pipeMgr->m_device->options().maxNumDynamicUniformBuffers,
      m_pipeMgr->m_device->options().maxNumDynamicStorageBuffers);
    
    m_layout = new DxvkPipelineLayout(m_vkd,
      m_slotMapping, VK_PIPELINE_BIND_POINT_COMPUTE);
  }
  
  
  DxvkComputePipeline::~DxvkComputePipeline() {
    for (const auto& instance : m_pipelines)
      this->destroyPipeline(instance.pipeline());
  }
  
  
  VkPipeline DxvkComputePipeline::getPipelineHandle(
    const DxvkComputePipelineStateInfo& state) {
    DxvkComputePipelineInstance* instance = nullptr;

    { std::lock_guard<sync::Spinlock> lock(m_mutex);

      instance = this->findInstance(state);

      if (instance)
        return instance->pipeline();
    
      // If no pipeline instance exists with the given state
      // vector, create a new one and add it to the list.
      instance = this->createInstance(state);
    }
    
    if (!instance)
      return VK_NULL_HANDLE;

    this->writePipelineStateToCache(state);
    return instance->pipeline();
  }


  void DxvkComputePipeline::compilePipeline(
    const DxvkComputePipelineStateInfo& state) {
    std::lock_guard<sync::Spinlock> lock(m_mutex);

    if (!this->findInstance(state))
      this->createInstance(state);
  }
  
  
  DxvkComputePipelineInstance* DxvkComputePipeline::createInstance(
    const DxvkComputePipelineStateInfo& state) {
    VkPipeline newPipelineHandle = this->createPipeline(state);

    m_pipeMgr->m_numComputePipelines += 1;
    return &m_pipelines.emplace_back(state, newPipelineHandle);
  }

  
  DxvkComputePipelineInstance* DxvkComputePipeline::findInstance(
    const DxvkComputePipelineStateInfo& state) {
    for (auto& instance : m_pipelines) {
      if (instance.isCompatible(state))
        return &instance;
    }
    
    return nullptr;
  }
  
  
  VkPipeline DxvkComputePipeline::createPipeline(
    const DxvkComputePipelineStateInfo& state) const {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    if (Logger::logLevel() <= LogLevel::Debug) {
      Logger::debug("Compiling compute pipeline..."); 
      Logger::debug(str::format("  cs  : ", m_shaders.cs->debugName()));
    }
    
    DxvkSpecConstants specData;
    for (uint32_t i = 0; i < m_layout->bindingCount(); i++)
      specData.set(i, state.bsBindingMask.test(i), true);
    
    for (uint32_t i = 0; i < MaxNumSpecConstants; i++)
      specData.set(getSpecId(i), state.sc.specConstants[i], 0u);

    VkSpecializationInfo specInfo = specData.getSpecInfo();
    
    DxvkShaderModuleCreateInfo moduleInfo;
    moduleInfo.fsDualSrcBlend = false;

    auto csm = m_shaders.cs->createShaderModule(m_vkd, m_slotMapping, moduleInfo);

    VkComputePipelineCreateInfo info;
    info.sType                = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.pNext                = nullptr;
    info.flags                = 0;
    info.stage                = csm.stageInfo(&specInfo);
    info.layout               = m_layout->pipelineLayout();
    info.basePipelineHandle   = VK_NULL_HANDLE;
    info.basePipelineIndex    = -1;
    
    // Time pipeline compilation for debugging purposes
    dxvk::high_resolution_clock::time_point t0, t1;

    if (Logger::logLevel() <= LogLevel::Debug)
      t0 = dxvk::high_resolution_clock::now();
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (m_vkd->vkCreateComputePipelines(m_vkd->device(),
          m_pipeMgr->m_cache->handle(), 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
      Logger::err("DxvkComputePipeline: Failed to compile pipeline");
      Logger::err(str::format("  cs  : ", m_shaders.cs->debugName()));
      return VK_NULL_HANDLE;
    }
    
    if (Logger::logLevel() <= LogLevel::Debug) {
      t1 = dxvk::high_resolution_clock::now();
      auto td = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
      Logger::debug(str::format("DxvkComputePipeline: Finished in ", td.count(), " ms"));
    }

    return pipeline;
  }


  void DxvkComputePipeline::destroyPipeline(VkPipeline pipeline) {
    m_vkd->vkDestroyPipeline(m_vkd->device(), pipeline, nullptr);
  }
  
  
  void DxvkComputePipeline::writePipelineStateToCache(
    const DxvkComputePipelineStateInfo& state) const {
    if (m_pipeMgr->m_stateCache == nullptr)
      return;
    
    DxvkStateCacheKey key;

    if (m_shaders.cs != nullptr)
      key.cs = m_shaders.cs->getShaderKey();

    m_pipeMgr->m_stateCache->addComputePipeline(key, state);
  }
  
}
