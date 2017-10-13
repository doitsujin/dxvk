#include "dxvk_compute.h"

namespace dxvk {
  
  DxvkComputePipeline::DxvkComputePipeline(
    const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd) {
    // TODO implement
  }
  
  
  DxvkComputePipeline::~DxvkComputePipeline() {
    m_vkd->vkDestroyPipeline(
      m_vkd->device(), m_pipeline, nullptr);
  }
  
}