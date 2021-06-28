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
   * \brief Copy pipeline
   * 
   * Stores the objects for a single pipeline
   * that is used for fragment shader copies.
   */
  struct DxvkMetaCopyPipeline {
    VkRenderPass          renderPass;
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
   * \brief Copy framebuffer and render pass
   * 
   * Creates a framebuffer and render
   * pass object for an image view.
   */
  class DxvkMetaCopyRenderPass : public DxvkResource {

  public:

    DxvkMetaCopyRenderPass(
      const Rc<vk::DeviceFn>&   vkd,
      const Rc<DxvkImageView>&  dstImageView,
      const Rc<DxvkImageView>&  srcImageView,
      const Rc<DxvkImageView>&  srcStencilView,
            bool                discardDst);
    
    ~DxvkMetaCopyRenderPass();

    VkRenderPass renderPass() const {
      return m_renderPass;
    }

    VkFramebuffer framebuffer() const {
      return m_framebuffer;
    }

  private:

    Rc<vk::DeviceFn>  m_vkd;

    Rc<DxvkImageView> m_dstImageView;
    Rc<DxvkImageView> m_srcImageView;
    Rc<DxvkImageView> m_srcStencilView;
    
    VkRenderPass  m_renderPass  = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;

    VkRenderPass createRenderPass(bool discard) const;

    VkFramebuffer createFramebuffer() const;

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
     * \param [in] format Depth format
     * \returns Corresponding color format
     */
    VkFormat getCopyDestinationFormat(
            VkImageAspectFlags    dstAspect,
            VkImageAspectFlags    srcAspect,
            VkFormat              srcFormat) const;

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

    VkSampler m_sampler;

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

    VkSampler createSampler() const;
    
    VkShaderModule createShaderModule(
      const SpirvCodeBuffer&          code) const;
    
    DxvkMetaCopyPipeline createCopyBufferImagePipeline();

    DxvkMetaCopyPipeline createPipeline(
      const DxvkMetaCopyPipelineKey&  key);

    VkRenderPass createRenderPass(
      const DxvkMetaCopyPipelineKey&  key) const;
    
    VkDescriptorSetLayout createDescriptorSetLayout(
      const DxvkMetaCopyPipelineKey&  key) const;
    
    VkPipelineLayout createPipelineLayout(
            VkDescriptorSetLayout     descriptorSetLayout) const;
    
    VkPipeline createPipelineObject(
      const DxvkMetaCopyPipelineKey&  key,
            VkPipelineLayout          pipelineLayout,
            VkRenderPass              renderPass);
    
  };
  
}