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
  struct DxvkMetaMipGenPushConstants {
    uint32_t layerCount;
  };
  
  /**
   * \brief Mip map generation pipeline key
   * 
   * We have to create pipelines for each
   * combination of source image view type
   * and image format.
   */
  struct DxvkMetaMipGenPipelineKey {
    VkImageViewType viewType;
    VkFormat        viewFormat;
    
    bool eq(const DxvkMetaMipGenPipelineKey& other) const {
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
   * \brief Mip map generation pipeline
   * 
   * Stores the objects for a single pipeline
   * that is used for mipmap generation.
   */
  struct DxvkMetaMipGenPipeline {
    VkDescriptorSetLayout dsetLayout;
    VkPipelineLayout      pipeLayout;
    VkPipeline            pipeHandle;
  };
  
  
  /**
   * \brief Mip map generation framebuffer
   * 
   * Stores the image views and framebuffer
   * handle used to generate one mip level.
   */
  struct DxvkMetaMipGenPass {
    VkImageView   srcView;
    VkImageView   dstView;
    VkFramebuffer framebuffer;
  };
  
  
  /**
   * \brief Mip map generation render pass
   * 
   * Stores image views, framebuffer objects and
   * a render pass object for mip map generation.
   * This must be created per image view.
   */
  class DxvkMetaMipGenRenderPass : public DxvkResource {
    
  public:
    
    DxvkMetaMipGenRenderPass(
      const Rc<vk::DeviceFn>&   vkd,
      const Rc<DxvkImageView>&  view);
    
    ~DxvkMetaMipGenRenderPass();
    
    /**
     * \brief Render pass handle
     * \returns Render pass handle
     */
    VkRenderPass renderPass() const {
      return m_renderPass;
    }
    
    /**
     * \brief Source image view type
     * 
     * Use this to figure out which type the
     * resource descriptor needs to have.
     * \returns Source image view type
     */
    VkImageViewType viewType() const {
      return m_srcViewType;
    }
    
    /**
     * \brief Render pass count
     * 
     * Number of mip levels to generate.
     * \returns Render pass count
     */
    uint32_t passCount() const {
      return m_passes.size();
    }
    
    /**
     * \brief Framebuffer handles
     * 
     * Returns image view and framebuffer handles
     * required to generate a single mip level.
     * \param [in] pass Render pass index
     * \returns Object handles for the given pass
     */
    DxvkMetaMipGenPass pass(uint32_t passId) const {
      return m_passes.at(passId);
    }
    
    /**
     * \brief Framebuffer size for a given pass
     * 
     * Stores the width, height, and layer count
     * of the framebuffer for the given pass ID.
     */
    VkExtent3D passExtent(uint32_t passId) const;
    
  private:
    
    Rc<vk::DeviceFn>  m_vkd;
    Rc<DxvkImageView> m_view;
    
    VkRenderPass m_renderPass;
    
    VkImageViewType m_srcViewType;
    VkImageViewType m_dstViewType;
    
    std::vector<DxvkMetaMipGenPass> m_passes;
    
    VkRenderPass createRenderPass() const;
    
    DxvkMetaMipGenPass createFramebuffer(uint32_t pass) const;
    
  };
  
  
  /**
   * \brief Mip map generation objects
   * 
   * Stores render pass objects and pipelines used
   * to generate mip maps. Due to Vulkan API design
   * decisions, we have to create one render pass
   * and pipeline object per image format used.
   */
  class DxvkMetaMipGenObjects : public RcObject {
    
  public:
    
    DxvkMetaMipGenObjects(const Rc<vk::DeviceFn>& vkd);
    ~DxvkMetaMipGenObjects();
    
    /**
     * \brief Creates a mip map generation pipeline
     * 
     * \param [in] viewType Source image view type
     * \param [in] viewFormat Image view format
     * \returns The mip map generation pipeline
     */
    DxvkMetaMipGenPipeline getPipeline(
            VkImageViewType viewType,
            VkFormat        viewFormat);
    
  private:
    
    Rc<vk::DeviceFn>  m_vkd;
    
    VkSampler m_sampler;
    
    VkShaderModule m_shaderVert;
    VkShaderModule m_shaderGeom;
    VkShaderModule m_shaderFrag1D;
    VkShaderModule m_shaderFrag2D;
    VkShaderModule m_shaderFrag3D;
    
    std::mutex m_mutex;
    
    std::unordered_map<
      VkFormat,
      VkRenderPass> m_renderPasses;
    
    std::unordered_map<
      DxvkMetaMipGenPipelineKey,
      DxvkMetaMipGenPipeline,
      DxvkHash, DxvkEq> m_pipelines;
    
    VkRenderPass getRenderPass(
            VkFormat        viewFormat);
    
    VkSampler createSampler() const;
    
    VkShaderModule createShaderModule(
      const SpirvCodeBuffer&            code) const;
    
    DxvkMetaMipGenPipeline createPipeline(
      const DxvkMetaMipGenPipelineKey&  key);
    
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
