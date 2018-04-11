#pragma once

#include "dxvk_barrier.h"
#include "dxvk_cmdlist.h"
#include "dxvk_resource.h"

#include "../spirv/spirv_code_buffer.h"

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
    
    struct DxvkMetaClearPipelines {
      VkPipeline clearBuf        = VK_NULL_HANDLE;
      VkPipeline clearImg1D      = VK_NULL_HANDLE;
      VkPipeline clearImg2D      = VK_NULL_HANDLE;
      VkPipeline clearImg3D      = VK_NULL_HANDLE;
      VkPipeline clearImg1DArray = VK_NULL_HANDLE;
      VkPipeline clearImg2DArray = VK_NULL_HANDLE;
    };
    
    Rc<vk::DeviceFn> m_vkd;
    
    VkDescriptorSetLayout m_clearBufDsetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_clearImgDsetLayout = VK_NULL_HANDLE;
    
    VkPipelineLayout m_clearBufPipeLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_clearImgPipeLayout = VK_NULL_HANDLE;
    
    DxvkMetaClearPipelines m_clearPipesF32;
    DxvkMetaClearPipelines m_clearPipesU32;
    
    VkDescriptorSetLayout createDescriptorSetLayout(
            VkDescriptorType        descriptorType);
    
    VkPipelineLayout createPipelineLayout(
            VkDescriptorSetLayout   dsetLayout);
    
    VkPipeline createPipeline(
      const SpirvCodeBuffer&        spirvCode,
            VkPipelineLayout        pipeLayout);
    
  };
  
}
