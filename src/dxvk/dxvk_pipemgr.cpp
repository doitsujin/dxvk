#include "dxvk_pipemgr.h"

namespace dxvk {
    
  DxvkPipelineManager::DxvkPipelineManager(
    const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd) {
    
  }
  
  
  DxvkPipelineManager::~DxvkPipelineManager() {
    
  }
  
  
  Rc<DxvkComputePipeline> DxvkPipelineManager::getComputePipeline(const Rc<DxvkShader>& cs) {
    if (cs == nullptr)
      return nullptr;
    
    DxvkPipelineKey<1> key;
    key.setShader(0, cs);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto pair = m_computePipelines.find(key);
    if (pair != m_computePipelines.end())
      return pair->second;
    
    Rc<DxvkComputePipeline> pipeline = new DxvkComputePipeline(m_vkd, cs);
    m_computePipelines.insert(std::make_pair(key, pipeline));
    return pipeline;
  }
  
  
  Rc<DxvkGraphicsPipeline> DxvkPipelineManager::getGraphicsPipeline(
    const Rc<DxvkShader>& vs,
    const Rc<DxvkShader>& tcs,
    const Rc<DxvkShader>& tes,
    const Rc<DxvkShader>& gs,
    const Rc<DxvkShader>& fs) {
    if (vs == nullptr)
      return nullptr;
    
    DxvkPipelineKey<5> key;
    key.setShader(0, vs);
    key.setShader(1, tcs);
    key.setShader(2, tes);
    key.setShader(3, gs);
    key.setShader(4, fs);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto pair = m_graphicsPipelines.find(key);
    if (pair != m_graphicsPipelines.end())
      return pair->second;
    
    Rc<DxvkGraphicsPipeline> pipeline = new DxvkGraphicsPipeline(m_vkd, vs, tcs, tes, gs, fs);
    m_graphicsPipelines.insert(std::make_pair(key, pipeline));
    return pipeline;
  }
  
}