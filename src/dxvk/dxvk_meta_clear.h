#pragma once

#include "dxvk_format.h"
#include "dxvk_include.h"
#include "dxvk_pipelayout.h"

#include "../spirv/spirv_code_buffer.h"

namespace dxvk {

  class DxvkDevice;
  
  /**
   * \brief Clear args
   * 
   * The data structure that can be passed
   * to the clear shaders as push constants.
   */
  struct DxvkMetaClearArgs {
    VkClearColorValue clearValue;
    VkOffset3D offset; uint32_t pad1;
    VkExtent3D extent; uint32_t pad2;
  };
  
  
  /**
   * \brief Pipeline-related objects
   * 
   * Use this to bind the pipeline
   * and allocate a descriptor set.
   */
  struct DxvkMetaClearPipeline {
    const DxvkPipelineLayout* layout = nullptr;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkExtent3D workgroupSize = { };
  };
  
  
  /**
   * \brief Clear shaders and related objects
   * 
   * Creates the shaders, pipeline layouts, and
   * compute pipelines that are going to be used
   * for clear operations.
   */
  class DxvkMetaClearObjects {
    
  public:
    
    DxvkMetaClearObjects(DxvkDevice* device);
    ~DxvkMetaClearObjects();
    
    /**
     * \brief Retrieves objects to use for buffers
     * 
     * Returns the pipeline, pipeline layout and descriptor
     * set layout which are required to perform a meta clear
     * operation on a buffer resource with the given format.
     * \param [in] viewType The image virw type
     */
    DxvkMetaClearPipeline getClearBufferPipeline(
            DxvkFormatFlags       formatFlags) const;
    
    /**
     * \brief Retrieves objects for a given image view type
     * 
     * Returns the pipeline, pipeline layout and descriptor
     * set layout which are required to perform a meta clear
     * operation on a resource with the given view type.
     * \param [in] viewType The image virw type
     * \returns The pipeline-related objects to use
     */
    DxvkMetaClearPipeline getClearImagePipeline(
            VkImageViewType       viewType,
            DxvkFormatFlags       formatFlags) const;
    
  private:
    
    struct DxvkMetaClearPipelines {
      VkPipeline clearBuf        = VK_NULL_HANDLE;
      VkPipeline clearImg1D      = VK_NULL_HANDLE;
      VkPipeline clearImg2D      = VK_NULL_HANDLE;
      VkPipeline clearImg3D      = VK_NULL_HANDLE;
      VkPipeline clearImg1DArray = VK_NULL_HANDLE;
      VkPipeline clearImg2DArray = VK_NULL_HANDLE;
    };
    
    DxvkDevice* m_device = nullptr;
    
    const DxvkPipelineLayout* m_clearBufPipeLayout = VK_NULL_HANDLE;
    const DxvkPipelineLayout* m_clearImgPipeLayout = VK_NULL_HANDLE;
    
    DxvkMetaClearPipelines m_clearPipesF32;
    DxvkMetaClearPipelines m_clearPipesU32;
    
    const DxvkPipelineLayout* createPipelineLayout(
            VkDescriptorType        descriptorType);
    
    VkPipeline createPipeline(
            size_t                  size,
      const uint32_t*               code,
      const DxvkPipelineLayout*     layout);

    template<size_t N>
    VkPipeline createPipeline(
      const uint32_t                (&code)[N],
      const DxvkPipelineLayout*     layout) {
      return createPipeline(sizeof(uint32_t) * N, &code[0], layout);
    }

  };
  
}
