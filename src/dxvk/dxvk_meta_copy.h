#pragma once

#include <mutex>
#include <unordered_map>

#include "../spirv/spirv_code_buffer.h"

#include "dxvk_barrier.h"
#include "dxvk_cmdlist.h"
#include "dxvk_hash.h"

namespace dxvk {

  class DxvkDevice;

  /**
   * \brief Push constants for formatted buffer copies
   */
  struct DxvkFormattedBufferCopyArgs {
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
    VkFormat dstFormat = VK_FORMAT_UNDEFINED;
    VkFormat srcFormat = VK_FORMAT_UNDEFINED;
  };

  /**
   * \brief Copy pipeline
   * 
   * Stores the objects for a single pipeline
   * that is used for fragment shader copies.
   */
  struct DxvkMetaCopyPipeline {
    VkDescriptorSetLayout dsetLayout = VK_NULL_HANDLE;
    VkPipelineLayout      pipeLayout = VK_NULL_HANDLE;
    VkPipeline            pipeHandle = VK_NULL_HANDLE;
  };


  /**
   * \brief Push constants for buffer <-> image copies
   */
  struct DxvkBufferImageCopyArgs {
    VkOffset3D imageOffset;
    uint32_t bufferOffset;
    VkExtent3D imageExtent;
    uint32_t bufferImageWidth;
    uint32_t bufferImageHeight;
    uint32_t stencilBitIndex;
  };

  /**
   * \brief Copy pipeline key
   * 
   * Used to look up copy pipelines based
   * on the copy operation they support.
   */
  struct DxvkMetaCopyPipelineKey {
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;

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
   * \brief Buffer to image copy pipeline key
   */
  struct DxvkMetaBufferImageCopyPipelineKey {
    VkImageViewType imageViewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    VkFormat bufferFormat = VK_FORMAT_UNDEFINED;
    VkImageAspectFlags imageAspects = 0u;

    bool eq(const DxvkMetaBufferImageCopyPipelineKey& other) const {
      return this->imageViewType == other.imageViewType
          && this->imageFormat   == other.imageFormat
          && this->imageAspects  == other.imageAspects
          && this->bufferFormat  == other.bufferFormat;
    }

    size_t hash() const {
      return (uint32_t(imageViewType))
           ^ (uint32_t(imageAspects) << 4)
           ^ (uint32_t(imageFormat) << 8)
           ^ (uint32_t(bufferFormat) << 16);
    }
  };


  /**
   * \brief Copy view objects
   * 
   * Creates and manages views used in a
   * framebuffer-based copy operations.
   */
  class DxvkMetaCopyViews {

  public:

    DxvkMetaCopyViews(
      const Rc<DxvkImage>&            dstImage,
      const VkImageSubresourceLayers& dstSubresources,
            VkFormat                  dstFormat,
      const Rc<DxvkImage>&            srcImage,
      const VkImageSubresourceLayers& srcSubresources,
            VkFormat                  srcFormat);
    
    ~DxvkMetaCopyViews();

    Rc<DxvkImageView> dstImageView;
    Rc<DxvkImageView> srcImageView;
    Rc<DxvkImageView> srcStencilView;

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
    DxvkMetaCopyFormats getCopyImageFormats(
            VkFormat              dstFormat,
            VkImageAspectFlags    dstAspect,
            VkFormat              srcFormat,
            VkImageAspectFlags    srcAspect) const;

    /**
     * \brief Creates pipeline for buffer to image copy
     *
     * Note that setting both depth and stencil aspects
     * requires device support for depth-stencil export.
     * \param [in] dstFormat Destionation image format
     * \param [in] srcFormat Source buffer data format
     * \param [in] aspects Aspect mask to copy
     */
    DxvkMetaCopyPipeline getCopyBufferToImagePipeline(
            VkFormat              dstFormat,
            VkFormat              srcFormat,
            VkImageAspectFlags    aspects);

    /**
     * \brief Creates pipeline for image to buffer copy
     *
     * This method always returns a compute pipeline.
     * \param [in] viewType Image view type
     * \param [in] dstFormat Destionation buffer format
     */
    DxvkMetaCopyPipeline getCopyImageToBufferPipeline(
            VkImageViewType       viewType,
            VkFormat              dstFormat);

    /**
     * \brief Creates pipeline for meta copy operation
     * 
     * \param [in] viewType Image view type
     * \param [in] dstFormat Destination image format
     * \param [in] dstSamples Destination sample count
     * \returns Compatible pipeline for the operation
     */
    DxvkMetaCopyPipeline getCopyImagePipeline(
            VkImageViewType       viewType,
            VkFormat              dstFormat,
            VkSampleCountFlagBits dstSamples);

    /**
     * \brief Creates pipeline for buffer image copy
     * \returns Compute pipeline for buffer image copies
     */
    DxvkMetaCopyPipeline getCopyFormattedBufferPipeline();

  private:

    struct FragShaders {
      VkShaderModule frag1D = VK_NULL_HANDLE;
      VkShaderModule frag2D = VK_NULL_HANDLE;
      VkShaderModule fragMs = VK_NULL_HANDLE;
    };

    Rc<vk::DeviceFn> m_vkd;

    VkShaderModule m_shaderVert = VK_NULL_HANDLE;
    VkShaderModule m_shaderGeom = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_bufferToImageCopySetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_bufferToImageCopyPipelineLayout = VK_NULL_HANDLE;

    VkShaderModule m_shaderBufferToImageD = VK_NULL_HANDLE;
    VkShaderModule m_shaderBufferToImageS = VK_NULL_HANDLE;
    VkShaderModule m_shaderBufferToImageDSExport = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_imageToBufferCopySetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_imageToBufferCopyPipelineLayout = VK_NULL_HANDLE;

    VkShaderModule m_shaderImageToBufferF = VK_NULL_HANDLE;
    VkShaderModule m_shaderImageToBufferDS = VK_NULL_HANDLE;

    FragShaders m_color;
    FragShaders m_depth;
    FragShaders m_depthStencil;

    dxvk::mutex m_mutex;

    std::unordered_map<
      DxvkMetaCopyPipelineKey,
      DxvkMetaCopyPipeline,
      DxvkHash, DxvkEq> m_pipelines;

    std::unordered_map<DxvkMetaBufferImageCopyPipelineKey,
      VkPipeline, DxvkHash, DxvkEq> m_bufferToImagePipelines;

    std::unordered_map<DxvkMetaBufferImageCopyPipelineKey,
      VkPipeline, DxvkHash, DxvkEq> m_imageToBufferPipelines;

    DxvkMetaCopyPipeline m_copyBufferImagePipeline = { };

    VkShaderModule createShaderModule(
      const SpirvCodeBuffer&          code) const;

    DxvkMetaCopyPipeline createCopyFormattedBufferPipeline();

    DxvkMetaCopyPipeline createPipeline(
      const DxvkMetaCopyPipelineKey&  key);

    VkPipeline createCopyBufferToImagePipeline(
      const DxvkMetaBufferImageCopyPipelineKey& key);

    VkPipeline createCopyImageToBufferPipeline(
      const DxvkMetaBufferImageCopyPipelineKey& key);

    VkDescriptorSetLayout createDescriptorSetLayout(
      const DxvkMetaCopyPipelineKey&  key) const;
    
    VkPipelineLayout createPipelineLayout(
            VkDescriptorSetLayout     descriptorSetLayout) const;
    
    VkPipeline createPipelineObject(
      const DxvkMetaCopyPipelineKey&  key,
            VkPipelineLayout          pipelineLayout);
    
  };
  
}