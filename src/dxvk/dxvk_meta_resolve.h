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
    VkResolveModeFlagBits     modeD;
    VkResolveModeFlagBits     modeS;

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
   * \brief Meta resolve views for attachment-based resolves
   */
  class DxvkMetaResolveViews : public DxvkResource {

  public:

    DxvkMetaResolveViews(
      const Rc<vk::DeviceFn>&         vkd,
      const Rc<DxvkImage>&            dstImage,
      const VkImageSubresourceLayers& dstSubresources,
      const Rc<DxvkImage>&            srcImage,
      const VkImageSubresourceLayers& srcSubresources,
            VkFormat                  format);

    ~DxvkMetaResolveViews();

    VkImageView getDstView() const { return m_dstImageView; }
    VkImageView getSrcView() const { return m_srcImageView; }

  private:

    Rc<vk::DeviceFn> m_vkd;

    VkImageView m_dstImageView = VK_NULL_HANDLE;
    VkImageView m_srcImageView = VK_NULL_HANDLE;

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
            VkResolveModeFlagBits     depthResolveMode,
            VkResolveModeFlagBits     stencilResolveMode);

  private:

    Rc<vk::DeviceFn> m_vkd;

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
    
    VkShaderModule createShaderModule(
      const SpirvCodeBuffer&          code) const;
    
    DxvkMetaResolvePipeline createPipeline(
      const DxvkMetaResolvePipelineKey& key);

    VkDescriptorSetLayout createDescriptorSetLayout(
      const DxvkMetaResolvePipelineKey& key);
    
    VkPipelineLayout createPipelineLayout(
            VkDescriptorSetLayout  descriptorSetLayout);
    
    VkPipeline createPipelineObject(
      const DxvkMetaResolvePipelineKey& key,
            VkPipelineLayout       pipelineLayout);
    
  };

}