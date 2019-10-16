#pragma once

#include <mutex>
#include <unordered_map>

#include "../spirv/spirv_code_buffer.h"

#include "dxvk_hash.h"
#include "dxvk_image.h"

namespace dxvk {
  
  /**
   * \brief Push constant data
   */
  struct DxvkMetaBlitPushConstants {
    uint32_t layerCount;
  };
  
  /**
   * \brief Blit pipeline key
   * 
   * We have to create pipelines for each
   * combination of source image view type
   * and image format.
   */
  struct DxvkMetaBlitPipelineKey {
    VkImageViewType viewType;
    VkFormat        viewFormat;
    
    bool eq(const DxvkMetaBlitPipelineKey& other) const {
      return this->viewType   == other.viewType
          && this->viewFormat == other.viewFormat;
    }
    
    size_t hash() const {
      DxvkHashState result;
      result.add(uint32_t(this->viewType));
      result.add(uint32_t(this->viewFormat));
      return result;
    }
  };
  
  
  /**
   * \brief Blit pipeline
   * 
   * Stores the objects for a single pipeline
   * that is used for blitting.
   */
  struct DxvkMetaBlitPipeline {
    VkDescriptorSetLayout dsetLayout;
    VkPipelineLayout      pipeLayout;
    VkPipeline            pipeHandle;
  };
  
  
  /**
   * \brief Blit framebuffer
   * 
   * Stores the image views and framebuffer
   * handle used to generate one mip level.
   */
  struct DxvkMetaBlitPass {
    VkImageView   srcView;
    VkImageView   dstView;
    VkFramebuffer framebuffer;
  };
  
  
  /**
   * \brief Blitter objects
   * 
   * Stores render pass objects and pipelines used
   * to generate mip maps. Due to Vulkan API design
   * decisions, we have to create one render pass
   * and pipeline object per image format used.
   */
  class DxvkMetaBlitObjects {
    
  public:
    
    DxvkMetaBlitObjects(const DxvkDevice* device);
    ~DxvkMetaBlitObjects();
    
    /**
     * \brief Creates a blit pipeline
     * 
     * \param [in] viewType Source image view type
     * \param [in] viewFormat Image view format
     * \returns The blit pipeline
     */
    DxvkMetaBlitPipeline getPipeline(
            VkImageViewType viewType,
            VkFormat        viewFormat);
    
    /**
     * \brief Retrieves sampler with a given filter
     *
     * \param [in] filter The desired filter
     * \returns Sampler object with the given filter
     */
    VkSampler getSampler(
            VkFilter        filter);
    
  private:
    
    Rc<vk::DeviceFn>  m_vkd;
    
    VkSampler m_samplerCopy;
    VkSampler m_samplerBlit;
    
    VkShaderModule m_shaderVert   = VK_NULL_HANDLE;
    VkShaderModule m_shaderGeom   = VK_NULL_HANDLE;
    VkShaderModule m_shaderFrag1D = VK_NULL_HANDLE;
    VkShaderModule m_shaderFrag2D = VK_NULL_HANDLE;
    VkShaderModule m_shaderFrag3D = VK_NULL_HANDLE;
    
    std::mutex m_mutex;
    
    std::unordered_map<
      VkFormat,
      VkRenderPass> m_renderPasses;
    
    std::unordered_map<
      DxvkMetaBlitPipelineKey,
      DxvkMetaBlitPipeline,
      DxvkHash, DxvkEq> m_pipelines;
    
    VkRenderPass getRenderPass(
            VkFormat        viewFormat);
    
    VkSampler createSampler(
            VkFilter                    filter) const;
    
    VkShaderModule createShaderModule(
      const SpirvCodeBuffer&            code) const;
    
    DxvkMetaBlitPipeline createPipeline(
      const DxvkMetaBlitPipelineKey&    key);
    
    VkRenderPass createRenderPass(
            VkFormat                    format) const;
    
    VkDescriptorSetLayout createDescriptorSetLayout(
            VkImageViewType             viewType) const;
    
    VkPipelineLayout createPipelineLayout(
            VkDescriptorSetLayout       descriptorSetLayout) const;
    
    VkPipeline createPipeline(
            VkImageViewType             imageViewType,
            VkPipelineLayout            pipelineLayout,
            VkRenderPass                renderPass) const;
    
  };
  
}
