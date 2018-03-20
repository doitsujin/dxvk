#pragma once

#include <chrono>

#include "dxvk_include.h"

#include "../util/sha1/sha1_util.h"
#include "../util/util_env.h"

namespace dxvk {
  
  /**
   * \brief Pipeline cache
   * 
   * Allows the Vulkan implementation to
   * re-use previously compiled pipelines.
   */
  class DxvkPipelineCache : public RcObject {
    using Clock     = std::chrono::high_resolution_clock;
    using TimeDiff  = std::chrono::microseconds;
    using TimePoint = typename Clock::time_point;
    
    // 60 seconds
    constexpr static int64_t UpdateInterval = 60'000'000;

    // ~500kb
    constexpr static int64_t UpdateSize = 500'000;
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
    
    void update();
    
  private:
    
    Rc<vk::DeviceFn> m_vkd;
    VkPipelineCache  m_handle;
    std::string      m_shaderCacheFile;
    TimePoint        m_prevUpdate;
    size_t           m_prevSize;
    
    void LoadShaderCache(
      VkPipelineCacheCreateInfo&  info,
      std::vector<char>&          cache) const;
    
    void SaveShaderCache() const;
  };
  
}
