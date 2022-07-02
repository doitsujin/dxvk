#include "dxvk_device.h"
#include "dxvk_pipemanager.h"
#include "dxvk_state_cache.h"

namespace dxvk {
  
  DxvkPipelineManager::DxvkPipelineManager(
          DxvkDevice*         device)
  : m_device    (device),
    m_cache     (new DxvkPipelineCache(device->vkd())) {
    std::string useStateCache = env::getEnvVar("DXVK_STATE_CACHE");
    
    if (useStateCache != "0" && device->config().enableStateCache)
      m_stateCache = new DxvkStateCache(device, this);
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
      std::tuple(this, shaders, layout));
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

    DxvkBindingLayout mergedLayout;
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
      std::tuple(this, shaders, layout));
    return &iter.first->second;
  }

  
  void DxvkPipelineManager::registerShader(
    const Rc<DxvkShader>&         shader) {
    if (m_stateCache != nullptr)
      m_stateCache->registerShader(shader);
  }


  DxvkPipelineCount DxvkPipelineManager::getPipelineCount() const {
    DxvkPipelineCount result;
    result.numComputePipelines  = m_numComputePipelines.load();
    result.numGraphicsPipelines = m_numGraphicsPipelines.load();
    return result;
  }


  bool DxvkPipelineManager::isCompilingShaders() const {
    return m_stateCache != nullptr
        && m_stateCache->isCompilingShaders();
  }


  void DxvkPipelineManager::stopWorkerThreads() const {
    if (m_stateCache != nullptr)
      m_stateCache->stopWorkerThreads();
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

    for (uint32_t i = 0; i < setLayouts.size(); i++)
      setLayouts[i] = createDescriptorSetLayout(layout.getBindingList(i));

    auto iter = m_pipelineLayouts.emplace(
      std::piecewise_construct,
      std::tuple(layout),
      std::tuple(m_device, layout, setLayouts.data()));
    return &iter.first->second;
  }
  
}
