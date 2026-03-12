#pragma once

#include <mutex>
#include <unordered_map>

#include "../spirv/spirv_code_buffer.h"

#include "dxvk_hash.h"
#include "dxvk_image.h"

namespace dxvk {

  /**
   * \brief Resolve mode for multisampled blits
   */
  enum class DxvkMetaBlitResolveMode : uint32_t {
    FilterNearest     = 0u,
    FilterLinear      = 1u,
    ResolveAverage    = 2u,
  };

  /**
   * \brief Meta blit pipeline
   *
   * Used for all sorts of different 2D operations that cannot be
   * modeled as regular copies, resolves or Vulkan blits. A very
   * common use case includes blits involving mutlisampled images.
   */
  struct DxvkMetaBlit {
    /** Shader arguments for blit pipeline */
    struct Args {
      VkOffset3D srcCoord0 = { 0, 0, 0 };
      VkOffset3D srcCoord1 = { 0, 0, 0 };
      uint32_t sampler = 0u;
      uint32_t layerIndex = 0u;
      uint32_t layerCount = 0u;
    };

    /** Look-up info for blit pipeline */
    struct Key {
      VkImageViewType         srcViewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
      VkFormat                dstFormat   = VK_FORMAT_UNDEFINED;
      VkSampleCountFlagBits   dstSamples  = VK_SAMPLE_COUNT_1_BIT;
      VkSampleCountFlagBits   srcSamples  = VK_SAMPLE_COUNT_1_BIT;
      DxvkMetaBlitResolveMode resolveMode = DxvkMetaBlitResolveMode::FilterNearest;

      bool eq(const Key& other) const {
        return srcViewType  == other.srcViewType
            && dstFormat    == other.dstFormat
            && dstSamples   == other.dstSamples
            && srcSamples   == other.srcSamples
            && resolveMode  == other.resolveMode;
      }

      size_t hash() const {
        DxvkHashState hash;
        hash.add(uint32_t(srcViewType));
        hash.add(uint32_t(dstFormat));
        hash.add(uint32_t(dstSamples));
        hash.add(uint32_t(srcSamples));
        hash.add(uint32_t(resolveMode));
        return hash;
      }
    };

    const DxvkPipelineLayout* layout = nullptr;
    VkPipeline pipeline = VK_NULL_HANDLE;;
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
    
    DxvkMetaBlitObjects(DxvkDevice* device);

    ~DxvkMetaBlitObjects();
    
    /**
     * \brief Creates a blit pipeline
     * 
     * \param [in] key Pipeline properties
     * \returns The blit pipeline
     */
    DxvkMetaBlit getPipeline(const DxvkMetaBlit::Key& key);

  private:

    DxvkDevice* m_device = nullptr;

    dxvk::mutex m_mutex;

    std::unordered_map<DxvkMetaBlit::Key, DxvkMetaBlit, DxvkHash, DxvkEq> m_pipelines;

    struct SampleProperties {
      VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
      uint32_t scaleX = 1u;
      uint32_t scaleY = 1u;
      uint64_t mapping = 0u;
    };

    const DxvkPipelineLayout* createPipelineLayout() const;

    std::vector<uint32_t> createVs(
      const DxvkMetaBlit::Key&          key,
            VkImageAspectFlagBits       aspect,
      const DxvkPipelineLayout*         layout);

    std::vector<uint32_t> createPsSimple(
      const DxvkMetaBlit::Key&          key,
            VkImageAspectFlagBits       aspect,
      const DxvkPipelineLayout*         layout);

    std::vector<uint32_t> createPsResolve(
      const DxvkMetaBlit::Key&          key,
            VkImageAspectFlagBits       aspect,
      const DxvkPipelineLayout*         layout);

    std::vector<uint32_t> createPsSampleMs(
      const DxvkMetaBlit::Key&          key,
            VkImageAspectFlagBits       aspect,
      const DxvkPipelineLayout*         layout);

    DxvkMetaBlit createPipeline(const DxvkMetaBlit::Key& key);

    static std::string getName(const DxvkMetaBlit::Key& key, const char* type);

  };

}
