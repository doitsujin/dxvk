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
    std::unique_lock lock(m_queueLock);
    this->startWorkers();

    m_pendingTasks += 1;

    PipelineLibraryEntry e = { };
    e.pipelineLibrary = library;

    if (priority == DxvkPipelinePriority::High) {
      m_queuedLibrariesPrioritized.push(e);
      m_queueCondPrioritized.notify_one();
    } else {
      m_queuedLibraries.push(e);
    }

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

    pipeline->acquirePipeline();
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
      m_queueCondPrioritized.notify_all();
    }

    for (auto& worker : m_workers)
      worker.join();

    m_workers.clear();
  }


  void DxvkPipelineWorkers::startWorkers() {
    if (!m_workersRunning) {
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
      uint32_t npWorkerCount = m_device->canUseGraphicsPipelineLibrary()
        ? std::max(((workerCount - 1) * 5) / 7, 1u)
        : workerCount;
      uint32_t hpWorkerCount = workerCount - npWorkerCount;

      Logger::info(str::format("DXVK: Using ", npWorkerCount, " + ", hpWorkerCount, " compiler threads"));
      m_workers.resize(npWorkerCount + hpWorkerCount);

      // Set worker flag so that they don't exit immediately
      m_workersRunning = true;

      for (size_t i = 0; i < m_workers.size(); i++) {
        m_workers[i] = i >= npWorkerCount
          ? dxvk::thread([this] { runWorkerPrioritized(); })
          : dxvk::thread([this] { runWorker(); });
        m_workers[i].set_priority(ThreadPriority::Lowest);
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
              || !m_queuedLibrariesPrioritized.empty()
              || !m_queuedLibraries.empty()
              || !m_queuedPipelines.empty();
        });

        if (!m_workersRunning) {
          // Skip pending work, exiting early is
          // more important in this case.
          break;
        } else if (!m_queuedLibrariesPrioritized.empty()) {
          l = m_queuedLibrariesPrioritized.front();
          m_queuedLibrariesPrioritized.pop();
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
          l->pipelineLibrary->compilePipeline();

        m_pendingTasks -= 1;
      }

      if (p) {
        if (p->computePipeline) {
          p->computePipeline->compilePipeline(p->computeState);
        } else if (p->graphicsPipeline) {
          p->graphicsPipeline->compilePipeline(p->graphicsState);
          p->graphicsPipeline->releasePipeline();
        }

        m_pendingTasks -= 1;
      }
    }
  }


  void DxvkPipelineWorkers::runWorkerPrioritized() {
    env::setThreadName("dxvk-shader-p");

    while (true) {
      PipelineLibraryEntry l = { };

      { std::unique_lock lock(m_queueLock);

        m_queueCondPrioritized.wait(lock, [this] {
          return !m_workersRunning
              || !m_queuedLibrariesPrioritized.empty();
        });

        if (!m_workersRunning)
          break;

        l = m_queuedLibrariesPrioritized.front();
        m_queuedLibrariesPrioritized.pop();
      }

      if (l.pipelineLibrary)
        l.pipelineLibrary->compilePipeline();

      m_pendingTasks -= 1;
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

    auto layout = createPipelineLayout(shaders.cs->getBindings());
    auto library = findPipelineLibraryLocked(shaders.cs);

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

    if (shaders.tcs == nullptr && shaders.tes == nullptr && shaders.gs == nullptr) {
      vsLibrary = findPipelineLibraryLocked(shaders.vs);
      fsLibrary = findPipelineLibraryLocked(shaders.fs);
    }

    auto iter = m_graphicsPipelines.emplace(
      std::piecewise_construct,
      std::tuple(shaders),
      std::tuple(m_device, this, shaders,
        layout, vsLibrary, fsLibrary));
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
      auto library = createPipelineLibrary(shader);
      m_workers.compilePipelineLibrary(library, DxvkPipelinePriority::Normal);
    }

    m_stateCache.registerShader(shader);
  }


  void DxvkPipelineManager::requestCompileShader(
    const Rc<DxvkShader>&         shader) {
    if (!shader->needsLibraryCompile())
      return;

    // Dispatch high-priority compile job
    auto library = findPipelineLibrary(shader);

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


  DxvkShaderPipelineLibrary* DxvkPipelineManager::createPipelineLibrary(
    const Rc<DxvkShader>&     shader) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    auto layout = createPipelineLayout(shader->getBindings());

    DxvkShaderPipelineLibraryKey key;
    key.shader = shader;

    auto iter = m_shaderLibraries.emplace(
      std::piecewise_construct,
      std::tuple(key),
      std::tuple(m_device, this, shader.ptr(), layout));
    return &iter.first->second;
  }


  DxvkShaderPipelineLibrary* DxvkPipelineManager::createNullFsPipelineLibrary() {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    auto layout = createPipelineLayout(DxvkBindingLayout(
      VK_PIPELINE_LAYOUT_CREATE_INDEPENDENT_SETS_BIT_EXT));

    auto iter = m_shaderLibraries.emplace(
      std::piecewise_construct,
      std::tuple(),
      std::tuple(m_device, this, nullptr, layout));
    return &iter.first->second;
  }


  DxvkShaderPipelineLibrary* DxvkPipelineManager::findPipelineLibrary(
    const Rc<DxvkShader>&     shader) {
    std::lock_guard<dxvk::mutex> lock(m_mutex);
    return findPipelineLibraryLocked(shader);
  }


  DxvkShaderPipelineLibrary* DxvkPipelineManager::findPipelineLibraryLocked(
    const Rc<DxvkShader>&     shader) {
    DxvkShaderPipelineLibraryKey key;
    key.shader = shader;

    auto pair = m_shaderLibraries.find(key);
    if (pair == m_shaderLibraries.end())
      return nullptr;

    return &pair->second;
  }


  bool DxvkPipelineManager::canPrecompileShader(
    const Rc<DxvkShader>&     shader) const {
    if (!shader->canUsePipelineLibrary())
      return false;

    if (shader->info().stage == VK_SHADER_STAGE_COMPUTE_BIT)
      return true;

    return m_device->canUseGraphicsPipelineLibrary();
  }

}
