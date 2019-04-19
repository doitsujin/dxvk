#pragma once

#include <mutex>
#include <unordered_map>

#include "../spirv/spirv_code_buffer.h"

#include "dxvk_barrier.h"
#include "dxvk_cmdlist.h"
#include "dxvk_resource.h"

namespace dxvk {

  /**
   * \brief Resolve pipeline
   * 
   * Stores the objects for a single pipeline
   * that is used for fragment shader resolve.
   */
  struct DxvkMetaResolvePipeline {
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
  struct DxvkMetaResolvePipelineKey {
    VkFormat              format;
    VkSampleCountFlagBits samples;

    bool eq(const DxvkMetaResolvePipelineKey& other) const {
      return this->format  == other.format
          && this->samples == other.samples;
    }

    size_t hash() const {
      return (uint32_t(format)  << 4)
           ^ (uint32_t(samples) << 0);
    }
  };

  /**
   * \brief Meta resolve render pass
   * 
   * Stores a framebuffer and image view objects
   * for a meta resolve operation. Can be tracked.
   */
  class DxvkMetaResolveRenderPass : public DxvkResource {
    
  public:
    
    DxvkMetaResolveRenderPass(
      const Rc<vk::DeviceFn>&   vkd,
      const Rc<DxvkImageView>&  dstImageView,
      const Rc<DxvkImageView>&  srcImageView,
            bool                discardDst);
    
    ~DxvkMetaResolveRenderPass();
    
    VkRenderPass renderPass() const {
      return m_renderPass;
    }

    VkFramebuffer framebuffer() const {
      return m_framebuffer;
    }

  private:
    
    const Rc<vk::DeviceFn>  m_vkd;

    const Rc<DxvkImageView> m_dstImageView;
    const Rc<DxvkImageView> m_srcImageView;
    
    VkRenderPass  m_renderPass  = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;

    VkRenderPass createRenderPass(bool discard) const;

    VkFramebuffer createFramebuffer() const;

  };
  

  /**
   * \brief Meta resolve objects
   * 
   * Implements resolve operations in fragment
   * shaders when using different formats.
   */
  class DxvkMetaResolveObjects : public RcObject {

  public:

    DxvkMetaResolveObjects(DxvkDevice* device);
    ~DxvkMetaResolveObjects();

    /**
     * \brief Creates pipeline for meta copy operation
     * 
     * \param [in] format Destination image format
     * \param [in] samples Destination sample count
     * \returns Compatible pipeline for the operation
     */
    DxvkMetaResolvePipeline getPipeline(
            VkFormat              format,
            VkSampleCountFlagBits samples);

  private:

    Rc<vk::DeviceFn> m_vkd;

    VkSampler m_sampler;

    VkShaderModule m_shaderVert;
    VkShaderModule m_shaderGeom;
    VkShaderModule m_shaderFragF;
    VkShaderModule m_shaderFragU;
    VkShaderModule m_shaderFragI;

    std::mutex m_mutex;

    std::unordered_map<
      DxvkMetaResolvePipelineKey,
      DxvkMetaResolvePipeline,
      DxvkHash, DxvkEq> m_pipelines;
    
    VkSampler createSampler() const;
    
    VkShaderModule createShaderModule(
      const SpirvCodeBuffer&          code) const;
    
    DxvkMetaResolvePipeline createPipeline(
      const DxvkMetaResolvePipelineKey& key);

    VkRenderPass createRenderPass(
      const DxvkMetaResolvePipelineKey& key);
    
    VkDescriptorSetLayout createDescriptorSetLayout() const;
    
    VkPipelineLayout createPipelineLayout(
            VkDescriptorSetLayout  descriptorSetLayout) const;
    
    VkPipeline createPipelineObject(
      const DxvkMetaResolvePipelineKey& key,
            VkPipelineLayout       pipelineLayout,
            VkRenderPass           renderPass);
    
  };

}