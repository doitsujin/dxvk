#include "dxvk_pipemgr.h"

namespace dxvk {
    
  DxvkPipelineManager:: DxvkPipelineManager() { }
  DxvkPipelineManager::~DxvkPipelineManager() { }
  
  
  Rc<DxvkComputePipeline> DxvkPipelineManager::getComputePipeline(
    const Rc<DxvkShader>& cs) {
    
    
    
  }
  
  
  Rc<DxvkGraphicsPipeline> DxvkPipelineManager::getGraphicsPipeline(
    const Rc<DxvkShader>& vs,
    const Rc<DxvkShader>& tcs,
    const Rc<DxvkShader>& tes,
    const Rc<DxvkShader>& gs,
    const Rc<DxvkShader>& fs) {
    
  }
  
}