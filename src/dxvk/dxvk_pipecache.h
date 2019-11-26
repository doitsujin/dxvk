#pragma once

#include <atomic>
#include <condition_variable>
#include <fstream>

#include "dxvk_include.h"

#include "../util/sha1/sha1_util.h"
#include "../util/util_env.h"
#include "../util/util_time.h"

namespace dxvk {
  
  /**
   * \brief Pipeline cache
   * 
   * Allows the Vulkan implementation to
   * re-use previously compiled pipelines.
   */
  class DxvkPipelineCache : public RcObject {
    
  public:
    
    DxvkPipelineCache(const Rc<vk::DeviceFn>& vkd);
    ~DxvkPipelineCache();
    
    /**
     * \brief Pipeline cache handle
     * \returns Pipeline cache handle
     */
    VkPipelineCache handle() const {
      return m_handle;
    }
    
  private:
    
    Rc<vk::DeviceFn>        m_vkd;
    VkPipelineCache         m_handle;
    
  };
  
}
