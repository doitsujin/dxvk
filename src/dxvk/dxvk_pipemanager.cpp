#include <optional>

#include "dxvk_device.h"
#include "dxvk_pipemanager.h"
#include "dxvk_state_cache.h"

namespace dxvk {
  
  DxvkPipelineWorkers::DxvkPipelineWorkers(
          DxvkDevice*                     device,
          DxvkPipelineCache*              cache)
  : m_cache(cache) {
    // Use a reasonably large number of threads for compiling, but
    // leave some cores to the application to avoid excessive stutter
    uint32_t numCpuCores = dxvk::thread::hardware_concurrency();
    m_workerCount = ((std::max(1u, numCpuCores) - 1) * 5) / 7;

    if (m_workerCount <  1) m_workerCount =  1;
    if (m_workerCount > 32) m_workerCount = 32;

    if (device->config().numCompilerThreads > 0)
      m_workerCount = device->config().numCompilerThreads;
  }


  DxvkPipelineWorkers::~DxvkPipelineWorkers() {
    this->stopWorkers();
  }


  void DxvkPipelineWorkers::compilePipelineLibrary(
          DxvkShaderPipelineLibrary*      library) {
    std::unique_lock lock(m_queueLock);
    this->startWorkers();

    m_pendingTasks += 1;

    PipelineLibraryEntry e = { };
    e.pipelineLibrary = library;

    m_queuedLibraries.push(e);
    m_queueCond.notify_one();
  }


  void DxvkPipelineWorkers::compileComputePipeline(
          DxvkComputePipeline*            pipeline,
    const DxvkComputePipelineStateInfo&   state) {
    std::unique_lock lock(m_queueLock);
    this->startWorkers();

    m_pendingTasks += 1;

    PipelineEntry e = { };
    e.computePipeline = pipeline;
    e.computeState = state;

    m_queuedPipelines.push(e);
    m_queueCond.notify_one();
  }


  void DxvkPipelineWorkers::compileGraphicsPipeline(
          DxvkGraphicsPipeline*           pipeline,
    const DxvkGraphicsPipelineStateInfo&  state) {
    std::unique_lock lock(m_queueLock);
    this->startWorkers();

    m_pendingTasks += 1;

    PipelineEntry e = { };
    e.graphicsPipeline = pipeline;
    e.graphicsState = state;

    m_queuedPipelines.push(e);
    m_queueCond.notify_one();
  }


  bool DxvkPipelineWorkers::isBusy() const {
    return m_pendingTasks.load() != 0ull;
  }


  void DxvkPipelineWorkers::stopWorkers() {
    { std::unique_lock lock(m_queueLock);

      if (!m_workersRunning)
        return;

      m_workersRunning = false;
      m_queueCond.notify_all();
    }

    for (auto& worker : m_workers)
      worker.join();

    m_workers.clear();
  }


  void DxvkPipelineWorkers::startWorkers() {
    if (!m_workersRunning) {
      m_workersRunning = true;

      Logger::info(str::format("DXVK: Using ", m_workerCount, " compiler threads"));
      m_workers.resize(m_workerCount);

      for (auto& worker : m_workers) {
        worker = dxvk::thread([this] { runWorker(); });
        worker.set_priority(ThreadPriority::Lowest);
      }
    }
  }


  void DxvkPipelineWorkers::runWorker() {
    env::setThreadName("dxvk-shader");

    while (true) {
      std::optional<PipelineEntry> p;
      std::optional<PipelineLibraryEntry> l;

      { std::unique_lock lock(m_queueLock);

        m_queueCond.wait(lock, [this] {
          return !m_workersRunning
              || !m_queuedLibraries.empty()
              || !m_queuedPipelines.empty();
        });

        if (!m_workersRunning) {
          // Skip pending work, exiting early is
          // more important in this case.
          break;
        } else if (!m_queuedLibraries.empty()) {
          l = m_queuedLibraries.front();
          m_queuedLibraries.pop();
        } else if (!m_queuedPipelines.empty()) {
          p = m_queuedPipelines.front();
          m_queuedPipelines.pop();
        }
      }

      if (l) {
        if (l->pipelineLibrary)
          l->pipelineLibrary->compilePipeline(m_cache->handle());

        m_pendingTasks -= 1;
      }

      if (p) {
        if (p->computePipeline)
          p->computePipeline->compilePipeline(p->computeState);
        else if (p->graphicsPipeline)
          p->graphicsPipeline->compilePipeline(p->graphicsState);

        m_pendingTasks -= 1;
      }
    }
  }


  DxvkPipelineManager::DxvkPipelineManager(
          DxvkDevice*         device)
  : m_device    (device),
    m_cache     (device),
    m_workers   (device, &m_cache),
    m_stateCache(device, this, &m_workers) {
    Logger::info(str::format("DXVK: Graphics pipeline libraries ",
      (m_device->canUseGraphicsPipelineLibrary() ? "supported" : "not supported")));
  }
  
  
  DxvkPipelineManager::~DxvkPipelineManager() {
    
  }
  
  
  DxvkComputePipeline* DxvkPipelineManager::createComputePipeline(
    const DxvkComputePipelineShaders& shaders) {
    if (shaders.cs == nullptr)
      return nullptr;
    
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    
    auto pair = m_computePipelines.find(shaders);
    if (pair != m_computePipelines.end())
      return &pair->second;

    auto layout = createPipelineLayout(shaders.cs->getBindings());

    auto iter = m_computePipelines.emplace(
      std::piecewise_construct,
      std::tuple(shaders),
      std::tuple(m_device, this, shaders, layout));
    return &iter.first->second;
  }
  
  
  DxvkGraphicsPipeline* DxvkPipelineManager::createGraphicsPipeline(
    const DxvkGraphicsPipelineShaders& shaders) {
    if (shaders.vs == nullptr)
      return nullptr;
    
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    
    auto pair = m_graphicsPipelines.find(shaders);
    if (pair != m_graphicsPipelines.end())
      return &pair->second;

    DxvkBindingLayout mergedLayout(VK_SHADER_STAGE_ALL_GRAPHICS);
    mergedLayout.merge(shaders.vs->getBindings());

    if (shaders.tcs != nullptr)
      mergedLayout.merge(shaders.tcs->getBindings());

    if (shaders.tes != nullptr)
      mergedLayout.merge(shaders.tes->getBindings());

    if (shaders.gs != nullptr)
      mergedLayout.merge(shaders.gs->getBindings());

    if (shaders.fs != nullptr)
      mergedLayout.merge(shaders.fs->getBindings());

    auto layout = createPipelineLayout(mergedLayout);

    auto iter = m_graphicsPipelines.emplace(
      std::piecewise_construct,
      std::tuple(shaders),
      std::tuple(m_device, this, shaders, layout));
    return &iter.first->second;
  }

  
  DxvkGraphicsPipelineVertexInputLibrary* DxvkPipelineManager::createVertexInputLibrary(
    const DxvkGraphicsPipelineVertexInputState& state) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    auto pair = m_vertexInputLibraries.find(state);
    if (pair != m_vertexInputLibraries.end())
      return &pair->second;

    auto iter = m_vertexInputLibraries.emplace(
      std::piecewise_construct,
      std::tuple(state),
      std::tuple(m_device, state, m_cache.handle()));
    return &iter.first->second;
  }


  DxvkGraphicsPipelineFragmentOutputLibrary* DxvkPipelineManager::createFragmentOutputLibrary(
    const DxvkGraphicsPipelineFragmentOutputState& state) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);

    auto pair = m_fragmentOutputLibraries.find(state);
    if (pair != m_fragmentOutputLibraries.end())
      return &pair->second;

    auto iter = m_fragmentOutputLibraries.emplace(
      std::piecewise_construct,
      std::tuple(state),
      std::tuple(m_device, state, m_cache.handle()));
    return &iter.first->second;
  }
  
  
  void DxvkPipelineManager::registerShader(
    const Rc<DxvkShader>&         shader) {
    if (m_device->canUseGraphicsPipelineLibrary() && shader->canUsePipelineLibrary()) {
      auto library = createPipelineLibrary(shader);
      m_workers.compilePipelineLibrary(library);
    }

    m_stateCache.registerShader(shader);
  }


  DxvkPipelineCount DxvkPipelineManager::getPipelineCount() const {
    DxvkPipelineCount result;
    result.numComputePipelines  = m_stats.numComputePipelines.load();
    result.numGraphicsPipelines = m_stats.numGraphicsPipelines.load();
    return result;
  }


  void DxvkPipelineManager::stopWorkerThreads() {
    m_workers.stopWorkers();
    m_stateCache.stopWorkers();
  }


  DxvkBindingSetLayout* DxvkPipelineManager::createDescriptorSetLayout(
    const DxvkBindingSetLayoutKey& key) {
    auto pair = m_descriptorSetLayouts.find(key);
    if (pair != m_descriptorSetLayouts.end())
      return &pair->second;

    auto iter = m_descriptorSetLayouts.emplace(
      std::piecewise_construct,
      std::tuple(key),
      std::tuple(m_device, key));
    return &iter.first->second;
  }


  DxvkBindingLayoutObjects* DxvkPipelineManager::createPipelineLayout(
    const DxvkBindingLayout& layout) {
    auto pair = m_pipelineLayouts.find(layout);
    if (pair != m_pipelineLayouts.end())
      return &pair->second;

    std::array<const DxvkBindingSetLayout*, DxvkDescriptorSets::SetCount> setLayouts = { };
    uint32_t setMask = layout.getSetMask();

    for (uint32_t i = 0; i < setLayouts.size(); i++) {
      if (setMask & (1u << i))
        setLayouts[i] = createDescriptorSetLayout(layout.getBindingList(i));
    }

    auto iter = m_pipelineLayouts.emplace(
      std::piecewise_construct,
      std::tuple(layout),
      std::tuple(m_device, layout, setLayouts.data()));
    return &iter.first->second;
  }


  DxvkShaderPipelineLibrary* DxvkPipelineManager::createPipelineLibrary(
    const Rc<DxvkShader>&     shader) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    auto layout = createPipelineLayout(shader->getBindings());

    DxvkShaderPipelineLibraryKey key;
    key.shader = shader;

    auto iter = m_shaderLibraries.emplace(
      std::piecewise_construct,
      std::tuple(key),
      std::tuple(m_device, shader.ptr(), layout));
    return &iter.first->second;
  }
  
}
