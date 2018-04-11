#pragma once

#include "dxvk_barrier.h"
#include "dxvk_cmdlist.h"
#include "dxvk_resource.h"

namespace dxvk {
  
  /**
   * \brief Clear args
   * 
   * The data structure that can be passed
   * to the clear shaders as push constants.
   */
  struct DxvkMetaClearArgs {
    VkClearColorValue clearValue;
    
    alignas(16) VkOffset3D offset;
    alignas(16) VkExtent3D extent;
  };
  
  
  /**
   * \brief Clear shaders and related objects
   * 
   * Creates the shaders, pipeline layouts, and
   * compute pipelines that are going to be used
   * for clear operations.
   */
  class DxvkMetaClearObjects : public RcObject {
    
  public:
    
    DxvkMetaClearObjects(const Rc<vk::DeviceFn>& vkd);
    ~DxvkMetaClearObjects();
    
  private:
    
    Rc<vk::DeviceFn> m_vkd;
    
  };
  
}
