#include <cstring>
#include <iomanip>
#include <sstream>

#include "../util/util_time.h"

#include "dxvk_compute.h"
#include "dxvk_device.h"
#include "dxvk_graphics.h"
#include "dxvk_pipemanager.h"
#include "dxvk_state_cache.h"

namespace dxvk {
  
  DxvkComputePipeline::DxvkComputePipeline(
          DxvkDevice*                 device,
          DxvkPipelineManager*        pipeMgr,
          DxvkComputePipelineShaders  shaders,
          DxvkBindingLayoutObjects*   layout,
          DxvkShaderPipelineLibrary*  library)
  : m_device        (device),
    m_stateCache    (&pipeMgr->m_stateCache),
    m_stats         (&pipeMgr->m_stats),
    m_library       (library),
    m_libraryHandle (VK_NULL_HANDLE),
    m_shaders       (std::move(shaders)),
    m_bindings      (layout) {

  }
  
  
  DxvkComputePipeline::~DxvkComputePipeline() {
    if (m_libraryHandle)
      m_library->releasePipelineHandle();

    for (const auto& instance : m_pipelines)
      this->destroyPipeline(instance.handle);
  }
  
  
  VkPipeline DxvkComputePipeline::getPipelineHandle(
    const DxvkComputePipelineStateInfo& state) {
    if (m_libraryHandle) {
      // Compute pipelines without spec constants are always
      // pre-compiled, so we'll almost always hit this path
      return m_libraryHandle;
    } else if (m_library) {
      // Retrieve actual pipeline handle on first use. This
      // may wait for an ongoing compile job to finish, or
      // compile the pipeline immediately on the calling thread.
      m_libraryHandle = m_library->acquirePipelineHandle(
        DxvkShaderPipelineLibraryCompileArgs());

      return m_libraryHandle;
    } else {
      // Slow path for compute shaders that do use spec constants
      DxvkComputePipelineInstance* instance = this->findInstance(state);

      if (unlikely(!instance)) {
        std::lock_guard<dxvk::mutex> lock(m_mutex);
        instance = this->findInstance(state);

        if (!instance)
          instance = this->createInstance(state);
      }

      return instance->handle;
    }
  }


  void DxvkComputePipeline::compilePipeline(
    const DxvkComputePipelineStateInfo& state) {
    if (!m_library) {
      std::lock_guard<dxvk::mutex> lock(m_mutex);

      if (!this->findInstance(state))
        this->createInstance(state);
    }
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
      if (instance.state == state)
        return &instance;
    }
    
    return nullptr;
  }
  
  
  VkPipeline DxvkComputePipeline::createPipeline(
    const DxvkComputePipelineStateInfo& state) const {
    auto vk = m_device->vkd();

    DxvkPipelineSpecConstantState scState(m_shaders.cs->getSpecConstantMask(), state.sc);
    
    DxvkShaderStageInfo stageInfo(m_device);
    stageInfo.addStage(VK_SHADER_STAGE_COMPUTE_BIT, 
      m_shaders.cs->getCode(m_bindings, DxvkShaderModuleCreateInfo()),
      &scState.scInfo);

    VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    info.stage                = *stageInfo.getStageInfos();
    info.layout               = m_bindings->getPipelineLayout(false);
    info.basePipelineIndex    = -1;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult vr = vk->vkCreateComputePipelines(vk->device(),
          VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);

    if (vr != VK_SUCCESS) {
      Logger::err(str::format("DxvkComputePipeline: Failed to compile pipeline: ", vr));
      this->logPipelineState(LogLevel::Error, state);
      return VK_NULL_HANDLE;
    }

    return pipeline;
  }


  void DxvkComputePipeline::destroyPipeline(VkPipeline pipeline) {
    auto vk = m_device->vkd();

    vk->vkDestroyPipeline(vk->device(), pipeline, nullptr);
  }
  
  
  void DxvkComputePipeline::logPipelineState(
          LogLevel                      level,
    const DxvkComputePipelineStateInfo& state) const {
    std::stringstream sstr;
    sstr << "  cs  : " << m_shaders.cs->debugName() << std::endl;

    bool hasSpecConstants = false;

    for (uint32_t i = 0; i < MaxNumSpecConstants; i++) {
      if (state.sc.specConstants[i]) {
        if (!hasSpecConstants) {
          sstr << "Specialization constants:" << std::endl;
          hasSpecConstants = true;
        }

        sstr << "  " << i << ": 0x" << std::hex << std::setw(8) << std::setfill('0') << state.sc.specConstants[i] << std::dec << std::endl;
      }
    }

    Logger::log(level, sstr.str());
  }

}
