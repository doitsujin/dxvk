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
    VkFormat                  format;
    VkSampleCountFlagBits     samples;
    VkResolveModeFlagBitsKHR  modeD;
    VkResolveModeFlagBitsKHR  modeS;

    bool eq(const DxvkMetaResolvePipelineKey& other) const {
      return this->format  == other.format
          && this->samples == other.samples
          && this->modeD   == other.modeD
          && this->modeS   == other.modeS;
    }

    size_t hash() const {
      return (uint32_t(format)  << 4)
           ^ (uint32_t(samples) << 0)
           ^ (uint32_t(modeD)   << 12)
           ^ (uint32_t(modeS)   << 16);
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
      const Rc<DxvkImageView>&  srcStencilView,
            bool                discardDst);
    
    DxvkMetaResolveRenderPass(
      const Rc<vk::DeviceFn>&        vkd,
      const Rc<DxvkImageView>&       dstImageView,
      const Rc<DxvkImageView>&       srcImageView,
            VkResolveModeFlagBitsKHR modeD,
            VkResolveModeFlagBitsKHR modeS);
    
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
    const Rc<DxvkImageView> m_srcStencilView;
    
    VkRenderPass  m_renderPass  = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;

    VkRenderPass createShaderRenderPass(bool discard) const;
    
    VkRenderPass createAttachmentRenderPass(
            VkResolveModeFlagBitsKHR modeD,
            VkResolveModeFlagBitsKHR modeS) const;

    VkFramebuffer createShaderFramebuffer() const;
    
    VkFramebuffer createAttachmentFramebuffer() const;

  };
  

  /**
   * \brief Meta resolve objects
   * 
   * Implements resolve operations in fragment
   * shaders when using different formats.
   */
  class DxvkMetaResolveObjects {

  public:

    DxvkMetaResolveObjects(const DxvkDevice* device);
    ~DxvkMetaResolveObjects();

    /**
     * \brief Creates pipeline for meta copy operation
     * 
     * \param [in] format Destination image format
     * \param [in] samples Destination sample count
     * \param [in] depthResolveMode Depth resolve mode
     * \param [in] stencilResolveMode Stencil resolve mode
     * \returns Compatible pipeline for the operation
     */
    DxvkMetaResolvePipeline getPipeline(
            VkFormat                  format,
            VkSampleCountFlagBits     samples,
            VkResolveModeFlagBitsKHR  depthResolveMode,
            VkResolveModeFlagBitsKHR  stencilResolveMode);

  private:

    Rc<vk::DeviceFn> m_vkd;

    VkSampler m_sampler;

    VkShaderModule m_shaderVert  = VK_NULL_HANDLE;
    VkShaderModule m_shaderGeom  = VK_NULL_HANDLE;
    VkShaderModule m_shaderFragF = VK_NULL_HANDLE;
    VkShaderModule m_shaderFragU = VK_NULL_HANDLE;
    VkShaderModule m_shaderFragI = VK_NULL_HANDLE;
    VkShaderModule m_shaderFragD = VK_NULL_HANDLE;
    VkShaderModule m_shaderFragDS = VK_NULL_HANDLE;

    dxvk::mutex m_mutex;

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
    
    VkDescriptorSetLayout createDescriptorSetLayout(
      const DxvkMetaResolvePipelineKey& key);
    
    VkPipelineLayout createPipelineLayout(
            VkDescriptorSetLayout  descriptorSetLayout);
    
    VkPipeline createPipelineObject(
      const DxvkMetaResolvePipelineKey& key,
            VkPipelineLayout       pipelineLayout,
            VkRenderPass           renderPass);
    
  };

}