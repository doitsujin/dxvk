#pragma once

#include <mutex>
#include <unordered_map>

#include "../spirv/spirv_code_buffer.h"

#include "dxvk_barrier.h"
#include "dxvk_cmdlist.h"
#include "dxvk_image.h"

namespace dxvk {

  /**
   * \brief Resolve pipeline
   * 
   * Stores the objects for a single pipeline
   * that is used for fragment shader resolve.
   */
  struct DxvkMetaResolvePipeline {
    const DxvkPipelineLayout* layout   = nullptr;
    VkPipeline                pipeline = VK_NULL_HANDLE;
  };

  /**
   * \brief Copy pipeline key
   * 
   * Used to look up copy pipelines based
   * on the copy operation they support.
   */
  struct DxvkMetaResolvePipelineKey {
    VkFormat                  format  = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits     samples = VK_SAMPLE_COUNT_1_BIT;
    VkResolveModeFlagBits     modeD   = VK_RESOLVE_MODE_NONE;
    VkResolveModeFlagBits     modeS   = VK_RESOLVE_MODE_NONE;

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
  class DxvkMetaResolveViews {

  public:

    DxvkMetaResolveViews(
      const Rc<DxvkImage>&            dstImage,
      const VkImageSubresourceLayers& dstSubresources,
      const Rc<DxvkImage>&            srcImage,
      const VkImageSubresourceLayers& srcSubresources,
            VkFormat                  format);

    ~DxvkMetaResolveViews();

    Rc<DxvkImageView> dstView;
    Rc<DxvkImageView> srcView;

  };


  /**
   * \brief Meta resolve objects
   * 
   * Implements resolve operations in fragment
   * shaders when using different formats.
   */
  class DxvkMetaResolveObjects {

  public:

    DxvkMetaResolveObjects(DxvkDevice* device);
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

    DxvkDevice* m_device = nullptr;

    dxvk::mutex m_mutex;

    std::unordered_map<
      DxvkMetaResolvePipelineKey,
      DxvkMetaResolvePipeline,
      DxvkHash, DxvkEq> m_pipelines;

    DxvkMetaResolvePipeline createPipeline(
      const DxvkMetaResolvePipelineKey& key);

  };

}
