#pragma once

#include "../spirv/spirv_code_buffer.h"

#include "dxvk_cmdlist.h"
#include "dxvk_resource.h"

namespace dxvk {

  /**
   * \brief Packing arguments
   * 
   * Passed in as push constants
   * to the compute shader.
   */
  struct DxvkMetaPackArgs {
    VkOffset2D srcOffset;
    VkExtent2D srcExtent;
  };


  /**
   * \brief Packing pipeline
   * 
   * Stores the objects for a single pipeline
   * that is used to pack depth-stencil image
   * data into a linear buffer.
   */
  struct DxvkMetaPackPipeline {
    VkDescriptorUpdateTemplateKHR dsetTemplate;
    VkDescriptorSetLayout         dsetLayout;
    VkPipelineLayout              pipeLayout;
    VkPipeline                    pipeHandle;
  };


  /**
   * \brief Packing descriptors
   */
  struct DxvkMetaPackDescriptors {
    VkDescriptorBufferInfo  dstBuffer;
    VkDescriptorImageInfo   srcDepth;
    VkDescriptorImageInfo   srcStencil;
  };


  /**
   * \brief Depth-stencil pack objects
   *
   * Stores compute shaders and related objects
   * for depth-stencil image packing operations.
   */
  class DxvkMetaPackObjects : public RcObject {

  public:

    DxvkMetaPackObjects(const Rc<vk::DeviceFn>& vkd);
    ~DxvkMetaPackObjects();

    /**
     * \brief Retrieves pipeline for a packed format
     * 
     * \param [in] format Destination format
     * \returns Packed pipeline
     */
    DxvkMetaPackPipeline getPipeline(VkFormat format);

  private:

    Rc<vk::DeviceFn>      m_vkd;

    VkSampler             m_sampler;

    VkDescriptorSetLayout m_dsetLayout;
    VkPipelineLayout      m_pipeLayout;

    VkDescriptorUpdateTemplateKHR m_template;

    VkShaderModule        m_shaderD24S8;
    VkShaderModule        m_shaderD32S8;

    VkPipeline            m_pipeD24S8;
    VkPipeline            m_pipeD32S8;

    VkSampler createSampler();

    VkDescriptorSetLayout createDescriptorSetLayout();

    VkPipelineLayout createPipelineLayout();

    VkDescriptorUpdateTemplateKHR createDescriptorUpdateTemplate();
    
    VkPipeline createPipeline(
      const SpirvCodeBuffer&      code);
    
  };
  
}