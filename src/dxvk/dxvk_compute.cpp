#include <cstring>

#include "../util/util_time.h"

#include "dxvk_compute.h"
#include "dxvk_device.h"
#include "dxvk_pipemanager.h"
#include "dxvk_spec_const.h"
#include "dxvk_state_cache.h"

namespace dxvk {
  
  DxvkComputePipeline::DxvkComputePipeline(
          DxvkDevice*                 device,
          DxvkPipelineManager*        pipeMgr,
          DxvkComputePipelineShaders  shaders,
          DxvkBindingLayoutObjects*   layout)
  : m_device        (device),
    m_cache         (&pipeMgr->m_cache),
    m_stateCache    (&pipeMgr->m_stateCache),
    m_stats         (&pipeMgr->m_stats),
    m_shaders       (std::move(shaders)),
    m_bindings      (layout) {

  }
  
  
  DxvkComputePipeline::~DxvkComputePipeline() {
    for (const auto& instance : m_pipelines)
      this->destroyPipeline(instance.pipeline());
  }
  
  
  VkPipeline DxvkComputePipeline::getPipelineHandle(
    const DxvkComputePipelineStateInfo& state) {
    DxvkComputePipelineInstance* instance = this->findInstance(state);

    if (unlikely(!instance)) {
      std::lock_guard<dxvk::mutex> lock(m_mutex);
      instance = this->findInstance(state);

      if (!instance) {
        instance = this->createInstance(state);
        this->writePipelineStateToCache(state);
      }
    }

    return instance->pipeline();
  }


  void DxvkComputePipeline::compilePipeline(
    const DxvkComputePipelineStateInfo& state) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    if (!this->findInstance(state))
      this->createInstance(state);
  }
  
  
  DxvkComputePipelineInstance* DxvkComputePipeline::createInstance(
    const DxvkComputePipelineStateInfo& state) {
    VkPipeline newPipelineHandle = this->createPipeline(state);

    m_stats->numComputePipelines += 1;
    return &(*m_pipelines.emplace(state, newPipelineHandle));
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
    auto vk = m_device->vkd();

    if (Logger::logLevel() <= LogLevel::Debug) {
      Logger::debug("Compiling compute pipeline..."); 
      Logger::debug(str::format("  cs  : ", m_shaders.cs->debugName()));
    }
    
    DxvkSpecConstants specData;
    
    for (uint32_t i = 0; i < MaxNumSpecConstants; i++)
      specData.set(getSpecId(i), state.sc.specConstants[i], 0u);

    VkSpecializationInfo specInfo = specData.getSpecInfo();
    
    DxvkShaderModuleCreateInfo moduleInfo;
    moduleInfo.fsDualSrcBlend = false;

    auto csm = m_shaders.cs->createShaderModule(vk, m_bindings, moduleInfo);

    VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    info.stage                = csm.stageInfo(&specInfo);
    info.layout               = m_bindings->getPipelineLayout();
    info.basePipelineIndex    = -1;
    
    // Time pipeline compilation for debugging purposes
    dxvk::high_resolution_clock::time_point t0, t1;

    if (Logger::logLevel() <= LogLevel::Debug)
      t0 = dxvk::high_resolution_clock::now();
    
    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vk->vkCreateComputePipelines(vk->device(),
          m_cache->handle(), 1, &info, nullptr, &pipeline) != VK_SUCCESS) {
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
    auto vk = m_device->vkd();

    vk->vkDestroyPipeline(vk->device(), pipeline, nullptr);
  }
  
  
  void DxvkComputePipeline::writePipelineStateToCache(
    const DxvkComputePipelineStateInfo& state) const {
    DxvkStateCacheKey key;

    if (m_shaders.cs != nullptr)
      key.cs = m_shaders.cs->getShaderKey();

    m_stateCache->addComputePipeline(key, state);
  }
  
}
