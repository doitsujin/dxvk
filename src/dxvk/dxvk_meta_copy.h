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
      VkExtent3D srcExtent  = { };
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
   * \brief Image to buffer copy pipeline
   *
   * Used primarily to write out interleaved depth-stencil
   * data to buffers.
   */
  struct DxvkMetaImageToBufferCopy {
    /** Shader args for buffer to image copies */
    struct Args {
      VkOffset3D dstOffset = { };
      VkExtent2D dstExtent = { };
      VkOffset3D srcOffset = { };
      VkExtent3D srcExtent = { };
    };

    /** Look-up key for copy pipelines */
    struct Key {
      VkImageViewType srcViewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
      VkFormat srcFormat = VK_FORMAT_UNDEFINED;
      VkFormat dstFormat = VK_FORMAT_UNDEFINED;

      bool eq(const Key& other) const {
        return srcViewType    == other.srcViewType
            && srcFormat      == other.srcFormat
            && dstFormat      == other.dstFormat;
      }

      size_t hash() const {
        DxvkHashState hash;
        hash.add(uint32_t(srcViewType));
        hash.add(uint32_t(srcFormat));
        hash.add(uint32_t(dstFormat));
        return hash;
      }
    };

    const DxvkPipelineLayout* layout = nullptr;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkExtent3D workgroupSize = { };
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
     * \brief Gets or creates pipeline for image to buffer copies
     *
     * \param [in] key Pipeline properties
     * \returns Pipeline object
     */
    DxvkMetaImageToBufferCopy getPipeline(const DxvkMetaImageToBufferCopy::Key& key);

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
     * \brief Creates pipeline for buffer image copy
     * \returns Compute pipeline for buffer image copies
     */
    DxvkMetaCopyPipeline getCopyFormattedBufferPipeline();

  private:

    DxvkDevice* m_device = nullptr;

    dxvk::mutex m_mutex;

    std::unordered_map<DxvkMetaImageCopy::Key, DxvkMetaImageCopy, DxvkHash, DxvkEq> m_imageCopyPipelines;
    std::unordered_map<DxvkMetaBufferToImageCopy::Key, DxvkMetaBufferToImageCopy, DxvkHash, DxvkEq> m_bufferImageCopyPipelines;
    std::unordered_map<DxvkMetaImageToBufferCopy::Key, DxvkMetaImageToBufferCopy, DxvkHash, DxvkEq> m_imageBufferCopyPipelines;

    DxvkMetaCopyPipeline m_copyBufferImagePipeline = { };

    DxvkMetaCopyPipeline createCopyFormattedBufferPipeline();

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

    std::vector<uint32_t> createCsCopyImageToBuffer(
      const DxvkPipelineLayout*           layout,
      const DxvkMetaImageToBufferCopy::Key& key);

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

    DxvkMetaImageToBufferCopy createImageToBufferCopyPipeline(
      const DxvkMetaImageToBufferCopy::Key& key);

    static std::string getName(const DxvkMetaImageCopy::Key& key, const char* type);

    static std::string getName(const DxvkMetaBufferToImageCopy::Key& key, const char* type);

    static std::string getName(const DxvkMetaImageToBufferCopy::Key& key, const char* type);

    static VkExtent3D determineWorkgroupSize(const DxvkMetaImageToBufferCopy::Key& key);

  };
  
}
