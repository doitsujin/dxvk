#pragma once

#include <mutex>
#include <unordered_map>

#include "../spirv/spirv_code_buffer.h"

#include "dxvk_barrier.h"
#include "dxvk_cmdlist.h"
#include "dxvk_hash.h"
#include "dxvk_resource.h"

namespace dxvk {

  class DxvkDevice;

  /**
   * \brief Push constants for buffer image copies
   */
  struct DxvkCopyBufferImageArgs {
    VkOffset3D dstOffset; uint32_t pad0;
    VkOffset3D srcOffset; uint32_t pad1;
    VkExtent3D extent;    uint32_t pad2;
    VkExtent2D dstSize;
    VkExtent2D srcSize;
  };

  /**
   * \brief Pair of view formats for copy operation
   */
  struct DxvkMetaCopyFormats {
    VkFormat dstFormat;
    VkFormat srcFormat;
  };

  /**
   * \brief Copy pipeline
   * 
   * Stores the objects for a single pipeline
   * that is used for fragment shader copies.
   */
  struct DxvkMetaCopyPipeline {
    VkDescriptorSetLayout dsetLayout;
    VkPipelineLayout      pipeLayout;
    VkPipeline            pipeHandle;
  };

  /**
   * \brief Copy pipeline key
   * 
   * Used to look up copy pipelines based
   * on the copy operation they support.
   */
  struct DxvkMetaCopyPipelineKey {
    VkImageViewType       viewType;
    VkFormat              format;
    VkSampleCountFlagBits samples;

    bool eq(const DxvkMetaCopyPipelineKey& other) const {
      return this->viewType == other.viewType
          && this->format   == other.format
          && this->samples  == other.samples;
    }

    size_t hash() const {
      return (uint32_t(format)  << 8)
           ^ (uint32_t(samples) << 4)
           ^ (uint32_t(viewType));
    }
  };

  /**
   * \brief Copy view objects
   * 
   * Creates and manages views used in a
   * framebuffer-based copy operations.
   */
  class DxvkMetaCopyViews : public DxvkResource {

  public:

    DxvkMetaCopyViews(
      const Rc<vk::DeviceFn>&         vkd,
      const Rc<DxvkImage>&            dstImage,
      const VkImageSubresourceLayers& dstSubresources,
            VkFormat                  dstFormat,
      const Rc<DxvkImage>&            srcImage,
      const VkImageSubresourceLayers& srcSubresources,
            VkFormat                  srcFormat);
    
    ~DxvkMetaCopyViews();

    VkImageView getDstView() const { return m_dstImageView; }
    VkImageView getSrcView() const { return m_srcImageView; }
    VkImageView getSrcStencilView() const { return m_srcStencilView; }

    VkImageViewType getSrcViewType() const {
      return m_srcViewType;
    }

  private:

    Rc<vk::DeviceFn>  m_vkd;

    VkImageViewType   m_srcViewType     = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    VkImageViewType   m_dstViewType     = VK_IMAGE_VIEW_TYPE_MAX_ENUM;

    VkImageView       m_dstImageView    = VK_NULL_HANDLE;
    VkImageView       m_srcImageView    = VK_NULL_HANDLE;
    VkImageView       m_srcStencilView  = VK_NULL_HANDLE;

  };

  /**
   * \brief Meta copy objects
   * 
   * Meta copy operations are necessary in order
   * to copy data between color and depth images.
   */
  class DxvkMetaCopyObjects {

  public:

    DxvkMetaCopyObjects(const DxvkDevice* device);
    ~DxvkMetaCopyObjects();

    /**
     * \brief Queries color format for d->c copies
     * 
     * Returns the color format that we need to use
     * as the destination image view format in case
     * of depth to color image copies.
     * \param [in] dstFormat Destination image format
     * \param [in] dstAspect Destination aspect mask
     * \param [in] srcFormat Source image format
     * \param [in] srcAspect Source aspect mask
     * \returns Corresponding color format
     */
    DxvkMetaCopyFormats getFormats(
            VkFormat              dstFormat,
            VkImageAspectFlags    dstAspect,
            VkFormat              srcFormat,
            VkImageAspectFlags    srcAspect) const;

    /**
     * \brief Creates pipeline for meta copy operation
     * 
     * \param [in] viewType Image view type
     * \param [in] dstFormat Destination image format
     * \param [in] dstSamples Destination sample count
     * \returns Compatible pipeline for the operation
     */
    DxvkMetaCopyPipeline getPipeline(
            VkImageViewType       viewType,
            VkFormat              dstFormat,
            VkSampleCountFlagBits dstSamples);

    /**
     * \brief Creates pipeline for buffer image copy
     * \returns Compute pipeline for buffer image copies
     */
    DxvkMetaCopyPipeline getCopyBufferImagePipeline();

  private:

    struct FragShaders {
      VkShaderModule frag1D = VK_NULL_HANDLE;
      VkShaderModule frag2D = VK_NULL_HANDLE;
      VkShaderModule fragMs = VK_NULL_HANDLE;
    };

    Rc<vk::DeviceFn> m_vkd;

    VkShaderModule m_shaderVert = VK_NULL_HANDLE;
    VkShaderModule m_shaderGeom = VK_NULL_HANDLE;

    FragShaders m_color;
    FragShaders m_depth;
    FragShaders m_depthStencil;

    dxvk::mutex m_mutex;

    std::unordered_map<
      DxvkMetaCopyPipelineKey,
      DxvkMetaCopyPipeline,
      DxvkHash, DxvkEq> m_pipelines;

    DxvkMetaCopyPipeline m_copyBufferImagePipeline = { };

    VkShaderModule createShaderModule(
      const SpirvCodeBuffer&          code) const;
    
    DxvkMetaCopyPipeline createCopyBufferImagePipeline();

    DxvkMetaCopyPipeline createPipeline(
      const DxvkMetaCopyPipelineKey&  key);

    VkDescriptorSetLayout createDescriptorSetLayout(
      const DxvkMetaCopyPipelineKey&  key) const;
    
    VkPipelineLayout createPipelineLayout(
            VkDescriptorSetLayout     descriptorSetLayout) const;
    
    VkPipeline createPipelineObject(
      const DxvkMetaCopyPipelineKey&  key,
            VkPipelineLayout          pipelineLayout);
    
  };
  
}