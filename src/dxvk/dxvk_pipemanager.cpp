#include <optional>

#include "dxvk_device.h"
#include "dxvk_pipemanager.h"
#include "dxvk_state_cache.h"

namespace dxvk {
  
  DxvkPipelineWorkers::DxvkPipelineWorkers(
          DxvkDevice*                     device)
  : m_device(device) {

  }


  DxvkPipelineWorkers::~DxvkPipelineWorkers() {
    this->stopWorkers();
  }


  void DxvkPipelineWorkers::compilePipelineLibrary(
          DxvkShaderPipelineLibrary*      library,
          DxvkPipelinePriority            priority) {
    std::unique_lock lock(m_lock);
    this->startWorkers();

    m_tasksTotal += 1;

    m_buckets[uint32_t(priority)].queue.emplace(library);
    notifyWorkers(priority);
  }


  void DxvkPipelineWorkers::compileGraphicsPipeline(
          DxvkGraphicsPipeline*           pipeline,
    const DxvkGraphicsPipelineStateInfo&  state,
          DxvkPipelinePriority            priority) {
    std::unique_lock lock(m_lock);
    this->startWorkers();

    pipeline->acquirePipeline();
    m_tasksTotal += 1;

    m_buckets[uint32_t(priority)].queue.emplace(pipeline, state);
    notifyWorkers(priority);
  }


  void DxvkPipelineWorkers::stopWorkers() {
    { std::unique_lock lock(m_lock);

      if (!m_workersRunning)
        return;

      m_workersRunning = false;

      for (uint32_t i = 0; i < m_buckets.size(); i++)
        m_buckets[i].cond.notify_all();
    }

    for (auto& worker : m_workers)
      worker.join();

    m_workers.clear();
  }


  void DxvkPipelineWorkers::notifyWorkers(DxvkPipelinePriority priority) {
    uint32_t index = uint32_t(priority);

    // If any workers are idle in a suitable set, notify the corresponding
    // condition variable. If all workers are busy anyway, we know that the
    // job is going to be picked up at some point anyway.
    for (uint32_t i = index; i < m_buckets.size(); i++) {
      if (m_buckets[i].idleWorkers) {
        m_buckets[i].cond.notify_one();
        break;
      }
    }
  }


  void DxvkPipelineWorkers::startWorkers() {
    if (!std::exchange(m_workersRunning, true)) {
      // Use all available cores by default
      uint32_t workerCount = dxvk::thread::hardware_concurrency();

      if (workerCount <  1) workerCount =  1;
      if (workerCount > 64) workerCount = 64;

      // Reduce worker count on 32-bit to save adderss space
      if (env::is32BitHostPlatform())
        workerCount = std::min(workerCount, 16u);

      if (m_device->config().numCompilerThreads > 0)
        workerCount = m_device->config().numCompilerThreads;

      // Number of workers that can process pipeline pipelines with normal
      // priority. Any other workers can only build high-priority pipelines.
      uint32_t npWorkerCount = std::max(((workerCount - 1) * 5) / 7, 1u);
      uint32_t lpWorkerCount = std::max(((workerCount - 1) * 2) / 7, 1u);

      m_workers.reserve(workerCount);

      for (size_t i = 0; i < workerCount; i++) {
        DxvkPipelinePriority priority = DxvkPipelinePriority::Normal;

        if (m_device->canUseGraphicsPipelineLibrary()) {
          if (i >= npWorkerCount)
            priority = DxvkPipelinePriority::High;
          else if (i < lpWorkerCount)
            priority = DxvkPipelinePriority::Low;
        }

        auto& worker = m_workers.emplace_back([this, priority] {
          runWorker(priority);
        });
        
        worker.set_priority(ThreadPriority::Lowest);
      }

      Logger::info(str::format("DXVK: Using ", workerCount, " compiler threads"));
    }
  }


  void DxvkPipelineWorkers::runWorker(DxvkPipelinePriority maxPriority) {
    static const std::array<char, 3> suffixes = { 'h', 'n', 'l' };

    const uint32_t maxPriorityIndex = uint32_t(maxPriority);
    env::setThreadName(str::format("dxvk-shader-", suffixes.at(maxPriorityIndex)));

    while (true) {
      PipelineEntry entry;

      { std::unique_lock lock(m_lock);
        auto& bucket = m_buckets[maxPriorityIndex];

        bucket.idleWorkers += 1;
        bucket.cond.wait(lock, [this, maxPriorityIndex, &entry] {
          // Attempt to fetch a work item from the
          // highest-priority queue that is not empty
          for (uint32_t i = 0; i <= maxPriorityIndex; i++) {
            if (!m_buckets[i].queue.empty()) {
              entry = m_buckets[i].queue.front();
              m_buckets[i].queue.pop();
              return true;
            }
          }

          return !m_workersRunning;
        });

        bucket.idleWorkers -= 1;

        // Skip pending work, exiting early is
        // more important in this case.
        if (!m_workersRunning)
          break;
      }

      if (entry.pipelineLibrary) {
        entry.pipelineLibrary->compilePipeline();
      } else if (entry.graphicsPipeline) {
        entry.graphicsPipeline->compilePipeline(entry.graphicsState);
        entry.graphicsPipeline->releasePipeline();
      }

      m_tasksCompleted += 1;
    }
  }


  DxvkPipelineManager::DxvkPipelineManager(
          DxvkDevice*         device)
  : m_device    (device),
    m_workers   (device),
    m_stateCache(device, this, &m_workers) {
    Logger::info(str::format("DXVK: Graphics pipeline libraries ",
      (m_device->canUseGraphicsPipelineLibrary() ? "supported" : "not supported")));

    if (m_device->canUseGraphicsPipelineLibrary()) {
      auto library = createNullFsPipelineLibrary();
      library->compilePipeline();
    }
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

    DxvkShaderPipelineLibraryKey key;
    key.addShader(shaders.cs);

    auto layout = createPipelineLayout(shaders.cs->getBindings());
    auto library = findPipelineLibraryLocked(key);

    auto iter = m_computePipelines.emplace(
      std::piecewise_construct,
      std::tuple(shaders),
      std::tuple(m_device, this, shaders, layout, library));
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

    DxvkShaderPipelineLibrary* vsLibrary = nullptr;
    DxvkShaderPipelineLibrary* fsLibrary = nullptr;

    if (m_device->canUseGraphicsPipelineLibrary()) {
      DxvkShaderPipelineLibraryKey vsKey;
      vsKey.addShader(shaders.vs);

      if (shaders.tcs != nullptr) vsKey.addShader(shaders.tcs);
      if (shaders.tes != nullptr) vsKey.addShader(shaders.tes);
      if (shaders.gs  != nullptr) vsKey.addShader(shaders.gs);

      if (vsKey.canUsePipelineLibrary()) {
        vsLibrary = findPipelineLibraryLocked(vsKey);

        if (!vsLibrary) {
          // If multiple shader stages are participating, create a
          // pipeline library so that it can potentially be reused.
          // Don't dispatch the pipeline library to a worker thread
          // since it should be compiled on demand anyway.
          vsLibrary = createPipelineLibraryLocked(vsKey);

          // Register the pipeline library with the state cache
          // so that subsequent runs can still compile it early
          DxvkStateCacheKey shaderKeys;
          shaderKeys.vs = shaders.vs->getShaderKey();

          if (shaders.tcs != nullptr) shaderKeys.tcs = shaders.tcs->getShaderKey();
          if (shaders.tes != nullptr) shaderKeys.tes = shaders.tes->getShaderKey();
          if (shaders.gs  != nullptr) shaderKeys.gs  = shaders.gs->getShaderKey();

          m_stateCache.addPipelineLibrary(shaderKeys);
        }
      }

      if (vsLibrary) {
        DxvkShaderPipelineLibraryKey fsKey;

        if (shaders.fs != nullptr)
          fsKey.addShader(shaders.fs);

        fsLibrary = findPipelineLibraryLocked(fsKey);
      }
    }

    auto iter = m_graphicsPipelines.emplace(
      std::piecewise_construct,
      std::tuple(shaders),
      std::tuple(m_device, this, shaders,
        layout, vsLibrary, fsLibrary));
    return &iter.first->second;
  }

  
  DxvkShaderPipelineLibrary* DxvkPipelineManager::createShaderPipelineLibrary(
    const DxvkShaderPipelineLibraryKey& key) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    return createPipelineLibraryLocked(key);
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
      std::tuple(m_device, state));
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
      std::tuple(m_device, state));
    return &iter.first->second;
  }
  
  
  void DxvkPipelineManager::registerShader(
    const Rc<DxvkShader>&         shader) {
    if (canPrecompileShader(shader)) {
      DxvkShaderPipelineLibraryKey key;
      key.addShader(shader);

      auto library = createShaderPipelineLibrary(key);
      m_workers.compilePipelineLibrary(library, DxvkPipelinePriority::Normal);
    }

    m_stateCache.registerShader(shader);
  }


  void DxvkPipelineManager::requestCompileShader(
    const Rc<DxvkShader>&         shader) {
    if (!shader->needsLibraryCompile())
      return;

    // Dispatch high-priority compile job
    DxvkShaderPipelineLibraryKey key;
    key.addShader(shader);

    auto library = findPipelineLibrary(key);

    if (library)
      m_workers.compilePipelineLibrary(library, DxvkPipelinePriority::High);

    // Notify immediately so that this only gets called
    // once, even if compilation does ot start immediately
    shader->notifyLibraryCompile();
  }


  DxvkPipelineCount DxvkPipelineManager::getPipelineCount() const {
    DxvkPipelineCount result;
    result.numGraphicsPipelines = m_stats.numGraphicsPipelines.load();
    result.numGraphicsLibraries = m_stats.numGraphicsLibraries.load();
    result.numComputePipelines  = m_stats.numComputePipelines.load();
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


  DxvkShaderPipelineLibrary* DxvkPipelineManager::createPipelineLibraryLocked(
    const DxvkShaderPipelineLibraryKey& key) {
    auto bindings = key.getBindings();
    auto layout = createPipelineLayout(bindings);

    auto iter = m_shaderLibraries.emplace(
      std::piecewise_construct,
      std::tuple(key),
      std::tuple(m_device, this, key, layout));
    return &iter.first->second;
  }


  DxvkShaderPipelineLibrary* DxvkPipelineManager::createNullFsPipelineLibrary() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    DxvkShaderPipelineLibraryKey key;

    DxvkBindingLayout bindings(VK_SHADER_STAGE_FRAGMENT_BIT);
    auto layout = createPipelineLayout(bindings);

    auto iter = m_shaderLibraries.emplace(
      std::piecewise_construct,
      std::tuple(),
      std::tuple(m_device, this, key, layout));
    return &iter.first->second;
  }


  DxvkShaderPipelineLibrary* DxvkPipelineManager::findPipelineLibrary(
    const DxvkShaderPipelineLibraryKey& key) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    return findPipelineLibraryLocked(key);
  }


  DxvkShaderPipelineLibrary* DxvkPipelineManager::findPipelineLibraryLocked(
    const DxvkShaderPipelineLibraryKey& key) {
    auto pair = m_shaderLibraries.find(key);
    if (pair == m_shaderLibraries.end())
      return nullptr;

    return &pair->second;
  }


  bool DxvkPipelineManager::canPrecompileShader(
    const Rc<DxvkShader>& shader) const {
    if (!shader->canUsePipelineLibrary(true))
      return false;

    if (shader->info().stage == VK_SHADER_STAGE_COMPUTE_BIT)
      return true;

    return m_device->canUseGraphicsPipelineLibrary();
  }

}
