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
    VkOffset2D dstOffset;
    VkExtent2D dstExtent;
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
   * \brief Unpacking descriptors
   */
  struct DxvkMetaUnpackDescriptors {
    VkBufferView            dstDepth;
    VkBufferView            dstStencil;
    VkDescriptorBufferInfo  srcBuffer;
  };


  /**
   * \brief Depth-stencil pack objects
   *
   * Stores compute shaders and related objects
   * for depth-stencil image packing operations.
   */
  class DxvkMetaPackObjects {

  public:

    DxvkMetaPackObjects(const DxvkDevice* device);
    ~DxvkMetaPackObjects();

    /**
     * \brief Retrieves depth-stencil packing pipeline
     * 
     * \param [in] format Destination format
     * \returns Data packing pipeline
     */
    DxvkMetaPackPipeline getPackPipeline(VkFormat format);

    /**
     * \brief Retrieves depth-stencil unpacking pipeline
     * 
     * \param [in] dstFormat Destination image format
     * \param [in] srcFormat Source buffer format
     * \returns Data unpacking pipeline
     */
    DxvkMetaPackPipeline getUnpackPipeline(
            VkFormat        dstFormat,
            VkFormat        srcFormat);

  private:

    Rc<vk::DeviceFn>      m_vkd;

    VkSampler             m_sampler;

    VkDescriptorSetLayout m_dsetLayoutPack;
    VkDescriptorSetLayout m_dsetLayoutUnpack;

    VkPipelineLayout      m_pipeLayoutPack;
    VkPipelineLayout      m_pipeLayoutUnpack;

    VkDescriptorUpdateTemplateKHR m_templatePack;
    VkDescriptorUpdateTemplateKHR m_templateUnpack;

    VkPipeline            m_pipePackD24S8;
    VkPipeline            m_pipePackD32S8;

    VkPipeline            m_pipeUnpackD24S8AsD32S8;
    VkPipeline            m_pipeUnpackD24S8;
    VkPipeline            m_pipeUnpackD32S8;

    VkSampler createSampler();

    VkDescriptorSetLayout createPackDescriptorSetLayout();

    VkDescriptorSetLayout createUnpackDescriptorSetLayout();

    VkPipelineLayout createPipelineLayout(
            VkDescriptorSetLayout       dsetLayout,
            size_t                      pushLayout);

    VkDescriptorUpdateTemplateKHR createPackDescriptorUpdateTemplate();

    VkDescriptorUpdateTemplateKHR createUnpackDescriptorUpdateTemplate();
    
    VkPipeline createPipeline(
            VkPipelineLayout      pipeLayout,
      const SpirvCodeBuffer&      code);
    
  };
  
}