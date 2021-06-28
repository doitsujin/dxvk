#pragma once

#include <mutex>
#include <unordered_map>

#include "../spirv/spirv_code_buffer.h"

#include "dxvk_hash.h"
#include "dxvk_image.h"

namespace dxvk {

  /**
   * \brief Texture coordinates
   */
  struct DxvkMetaBlitOffset {
    float x, y, z;
  };
  
  /**
   * \brief Push constant data
   */
  struct DxvkMetaBlitPushConstants {
    DxvkMetaBlitOffset srcCoord0;
    uint32_t           pad1;
    DxvkMetaBlitOffset srcCoord1;
    uint32_t           layerCount;
  };
  
  /**
   * \brief Blit pipeline key
   * 
   * We have to create pipelines for each
   * combination of source image view type
   * and image format.
   */
  struct DxvkMetaBlitPipelineKey {
    VkImageViewType       viewType;
    VkFormat              viewFormat;
    VkSampleCountFlagBits samples;
    
    bool eq(const DxvkMetaBlitPipelineKey& other) const {
      return this->viewType   == other.viewType
          && this->viewFormat == other.viewFormat
          && this->samples    == other.samples;
    }
    
    size_t hash() const {
      DxvkHashState result;
      result.add(uint32_t(this->viewType));
      result.add(uint32_t(this->viewFormat));
      result.add(uint32_t(this->samples));
      return result;
    }
  };
  
  /**
   * \brief Blit render pass key
   */
  struct DxvkMetaBlitRenderPassKey {
    VkFormat              viewFormat;
    VkSampleCountFlagBits samples;
    
    bool eq(const DxvkMetaBlitRenderPassKey& other) const {
      return this->viewFormat == other.viewFormat
          && this->samples    == other.samples;
    }
    
    size_t hash() const {
      DxvkHashState result;
      result.add(uint32_t(this->viewFormat));
      result.add(uint32_t(this->samples));
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
    VkRenderPass  renderPass;
    VkFramebuffer framebuffer;
  };
  

  /**
   * \brief Blit render pass
   *
   * Stores image view, render pass and framebuffer
   * objects for a blit operation, as well as some
   * metadata.
   */
  class DxvkMetaBlitRenderPass : public DxvkResource {

  public:

    DxvkMetaBlitRenderPass(
      const Rc<DxvkDevice>&       device,
      const Rc<DxvkImage>&        dstImage,
      const Rc<DxvkImage>&        srcImage,
      const VkImageBlit&          region,
      const VkComponentMapping&   mapping);

    ~DxvkMetaBlitRenderPass();

    VkImageViewType viewType() const;

    uint32_t framebufferLayerIndex() const;
    uint32_t framebufferLayerCount() const;

    DxvkMetaBlitPass pass() const;

  private:

    Rc<vk::DeviceFn>  m_vkd;
    Rc<DxvkImage>     m_dstImage;
    Rc<DxvkImage>     m_srcImage;

    VkImageBlit       m_region;
    VkImageView       m_dstView;
    VkImageView       m_srcView;
    VkRenderPass      m_renderPass;
    VkFramebuffer     m_framebuffer;

    VkImageView createDstView();
    VkImageView createSrcView(const VkComponentMapping& mapping);

    VkRenderPass createRenderPass();
    VkFramebuffer createFramebuffer();

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
     * \param [in] samples Target sample count
     * \returns The blit pipeline
     */
    DxvkMetaBlitPipeline getPipeline(
            VkImageViewType       viewType,
            VkFormat              viewFormat,
            VkSampleCountFlagBits samples);
    
    /**
     * \brief Retrieves sampler with a given filter
     *
     * \param [in] filter The desired filter
     * \returns Sampler object with the given filter
     */
    VkSampler getSampler(
            VkFilter              filter);
    
  private:
    
    Rc<vk::DeviceFn>  m_vkd;
    
    VkSampler m_samplerCopy;
    VkSampler m_samplerBlit;
    
    VkShaderModule m_shaderVert   = VK_NULL_HANDLE;
    VkShaderModule m_shaderGeom   = VK_NULL_HANDLE;
    VkShaderModule m_shaderFrag1D = VK_NULL_HANDLE;
    VkShaderModule m_shaderFrag2D = VK_NULL_HANDLE;
    VkShaderModule m_shaderFrag3D = VK_NULL_HANDLE;
    
    dxvk::mutex m_mutex;
    
    std::unordered_map<
      DxvkMetaBlitRenderPassKey,
      VkRenderPass,
      DxvkHash, DxvkEq> m_renderPasses;
    
    std::unordered_map<
      DxvkMetaBlitPipelineKey,
      DxvkMetaBlitPipeline,
      DxvkHash, DxvkEq> m_pipelines;
    
    VkRenderPass getRenderPass(
            VkFormat                    viewFormat,
            VkSampleCountFlagBits       samples);
    
    VkSampler createSampler(
            VkFilter                    filter) const;
    
    VkShaderModule createShaderModule(
      const SpirvCodeBuffer&            code) const;
    
    DxvkMetaBlitPipeline createPipeline(
      const DxvkMetaBlitPipelineKey&    key);
    
    VkRenderPass createRenderPass(
            VkFormat                    format,
            VkSampleCountFlagBits       samples) const;
    
    VkDescriptorSetLayout createDescriptorSetLayout(
            VkImageViewType             viewType) const;
    
    VkPipelineLayout createPipelineLayout(
            VkDescriptorSetLayout       descriptorSetLayout) const;
    
    VkPipeline createPipeline(
            VkImageViewType             imageViewType,
            VkPipelineLayout            pipelineLayout,
            VkRenderPass                renderPass,
            VkSampleCountFlagBits       samples) const;
    
  };
  
}
