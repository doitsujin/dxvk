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
   * \brief Image-to-image copy pipeline
   *
   * The use case for these pipelines is to implement copies between color
   * and depth images on devices that do not support maintenance8, as well
   * as depth-stencil copies on older AMD drivers where the native copy is
   * inefficient.
   */
  struct DxvkMetaImageCopy {
    /** Shader args for image to image copies */
    struct Args {
      VkOffset3D srcOffset  = { };
      VkExtent3D extent     = { };
      uint32_t   layerIndex = 0u;
      uint32_t   stencilBit = 0u;
    };

    /** Look-up key for image copy pipelines */
    struct Key {
      VkImageViewType srcViewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
      VkFormat dstFormat = VK_FORMAT_UNDEFINED;
      VkImageAspectFlags dstAspects = 0u;
      VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
      VkBool32 bitwiseStencil = VK_FALSE;

      bool eq(const Key& other) const {
        return srcViewType    == other.srcViewType
            && dstFormat      == other.dstFormat
            && dstAspects     == other.dstAspects
            && samples        == other.samples
            && bitwiseStencil == other.bitwiseStencil;
      }

      size_t hash() const {
        DxvkHashState hash;
        hash.add(uint32_t(srcViewType));
        hash.add(uint32_t(dstFormat));
        hash.add(uint32_t(dstAspects));
        hash.add(uint32_t(samples));
        hash.add(uint32_t(bitwiseStencil));
        return hash;
      }
    };

    const DxvkPipelineLayout* layout = nullptr;
    VkPipeline pipeline = VK_NULL_HANDLE;
  };


  /**
   * \brief Buffer-to-image copy pipeline
   *
   * The primary use case for these pipelines is to support interleaved
   * depth-stencil uploads, as well as moving data to multisampled images
   * for legacy APIs. Uses a single texel buffer to copy from.
   */
  struct DxvkMetaBufferToImageCopy {
    /** Shader args for buffer to image copies */
    struct Args {
      VkOffset3D srcOffset = { };
      VkExtent2D srcExtent = { };
      VkExtent3D dstExtent = { };
      uint32_t   layerIndex = 0u;
      uint32_t   stencilBit = 0u;
    };

    /** Look-up key for copy pipelines */
    struct Key {
      VkImageViewType dstViewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
      VkFormat srcFormat = VK_FORMAT_UNDEFINED;
      VkFormat dstFormat = VK_FORMAT_UNDEFINED;
      VkFormat bufferFormat = VK_FORMAT_UNDEFINED;
      VkImageAspectFlags dstAspects = 0u;
      VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
      VkBool32 bitwiseStencil = VK_FALSE;

      bool eq(const Key& other) const {
        return dstViewType    == other.dstViewType
            && srcFormat      == other.srcFormat
            && dstFormat      == other.dstFormat
            && bufferFormat   == other.bufferFormat
            && dstAspects     == other.dstAspects
            && samples        == other.samples
            && bitwiseStencil == other.bitwiseStencil;
      }

      size_t hash() const {
        DxvkHashState hash;
        hash.add(uint32_t(dstViewType));
        hash.add(uint32_t(dstFormat));
        hash.add(uint32_t(dstAspects));
        hash.add(uint32_t(srcFormat));
        hash.add(uint32_t(bufferFormat));
        hash.add(uint32_t(samples));
        hash.add(uint32_t(bitwiseStencil));
        return hash;
      }
    };

    const DxvkPipelineLayout* layout = nullptr;
    VkPipeline pipeline = VK_NULL_HANDLE;
  };


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
    const DxvkPipelineLayout* layout   = nullptr;
    VkPipeline                pipeline = VK_NULL_HANDLE;
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
  struct DxvkMetaImageCopyPipelineKey {
    VkImageViewType       viewType  = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    VkFormat              format    = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples   = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;

    bool eq(const DxvkMetaImageCopyPipelineKey& other) const {
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
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

    bool eq(const DxvkMetaBufferImageCopyPipelineKey& other) const {
      return this->imageViewType == other.imageViewType
          && this->imageFormat   == other.imageFormat
          && this->imageAspects  == other.imageAspects
          && this->bufferFormat  == other.bufferFormat
          && this->sampleCount   == other.sampleCount;
    }

    size_t hash() const {
      return (uint32_t(imageViewType))
           ^ (uint32_t(imageAspects) << 4)
           ^ (uint32_t(imageFormat) << 8)
           ^ (uint32_t(bufferFormat) << 16)
           ^ (uint32_t(sampleCount) << 28);
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

    DxvkMetaCopyObjects(DxvkDevice* device);
    ~DxvkMetaCopyObjects();

    /**
     * \brief Gets or creates pipeline for image copies
     *
     * \param [in] key Pipeline properties
     * \returns Pipeline object
     */
    DxvkMetaImageCopy getPipeline(const DxvkMetaImageCopy::Key& key);

    /**
     * \brief Gets or creates pipeline for buffer to image copies
     *
     * \param [in] key Pipeline properties
     * \returns Pipeline object
     */
    DxvkMetaBufferToImageCopy getPipeline(const DxvkMetaBufferToImageCopy::Key& key);

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
     * \brief Creates pipeline for buffer image copy
     * \returns Compute pipeline for buffer image copies
     */
    DxvkMetaCopyPipeline getCopyFormattedBufferPipeline();

  private:

    DxvkDevice* m_device = nullptr;

    dxvk::mutex m_mutex;

    std::unordered_map<DxvkMetaImageCopy::Key, DxvkMetaImageCopy, DxvkHash, DxvkEq> m_imageCopyPipelines;
    std::unordered_map<DxvkMetaBufferToImageCopy::Key, DxvkMetaBufferToImageCopy, DxvkHash, DxvkEq> m_bufferImageCopyPipelines;

    std::unordered_map<DxvkMetaBufferImageCopyPipelineKey,
      DxvkMetaCopyPipeline, DxvkHash, DxvkEq> m_imageToBufferPipelines;

    DxvkMetaCopyPipeline m_copyBufferImagePipeline = { };

    DxvkMetaCopyPipeline createCopyFormattedBufferPipeline();

    DxvkMetaCopyPipeline createCopyImageToBufferPipeline(
      const DxvkMetaBufferImageCopyPipelineKey& key);

    std::vector<uint32_t> createVsCopyImage(
      const DxvkPipelineLayout*           layout,
      const DxvkMetaImageCopy::Key&       key);

    std::vector<uint32_t> createVsCopyBufferToImage(
      const DxvkPipelineLayout*           layout,
      const DxvkMetaBufferToImageCopy::Key& key);

    std::vector<uint32_t> createPsCopyImage(
      const DxvkPipelineLayout*           layout,
      const DxvkMetaImageCopy::Key&       key);

    std::vector<uint32_t> createPsCopyBufferToImage(
      const DxvkPipelineLayout*           layout,
      const DxvkMetaBufferToImageCopy::Key& key);

    VkPipeline createCopyToImagePipeline(
      const DxvkPipelineLayout*           layout,
      const util::DxvkBuiltInShaderStage& vs,
      const util::DxvkBuiltInShaderStage& ps,
            VkFormat                      dstFormat,
            VkImageAspectFlags            dstAspects,
            VkSampleCountFlagBits         samples,
            bool                          bitwiseStencil);

    DxvkMetaImageCopy createImageCopyPipeline(
      const DxvkMetaImageCopy::Key&       key);

    DxvkMetaBufferToImageCopy createBufferToImageCopyPipeline(
      const DxvkMetaBufferToImageCopy::Key& key);

    static std::string getName(const DxvkMetaImageCopy::Key& key, const char* type);

    static std::string getName(const DxvkMetaBufferToImageCopy::Key& key, const char* type);

  };
  
}
