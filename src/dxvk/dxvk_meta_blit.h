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
    
  private:
    
    Rc<vk::DeviceFn>  m_vkd;
    
    VkShaderModule m_shaderVert   = VK_NULL_HANDLE;
    VkShaderModule m_shaderGeom   = VK_NULL_HANDLE;
    VkShaderModule m_shaderFrag1D = VK_NULL_HANDLE;
    VkShaderModule m_shaderFrag2D = VK_NULL_HANDLE;
    VkShaderModule m_shaderFrag3D = VK_NULL_HANDLE;
    
    dxvk::mutex m_mutex;
    
    std::unordered_map<
      DxvkMetaBlitPipelineKey,
      DxvkMetaBlitPipeline,
      DxvkHash, DxvkEq> m_pipelines;
    
    VkShaderModule createShaderModule(
      const SpirvCodeBuffer&            code) const;
    
    DxvkMetaBlitPipeline createPipeline(
      const DxvkMetaBlitPipelineKey&    key);
    
    VkDescriptorSetLayout createDescriptorSetLayout(
            VkImageViewType             viewType) const;
    
    VkPipelineLayout createPipelineLayout(
            VkDescriptorSetLayout       descriptorSetLayout) const;
    
    VkPipeline createPipeline(
            VkPipelineLayout            pipelineLayout,
            VkImageViewType             imageViewType,
            VkFormat                    format,
            VkSampleCountFlagBits       samples) const;
    
  };
  
}
