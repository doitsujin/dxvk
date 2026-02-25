#pragma once

#include <mutex>
#include <unordered_map>

#include "../spirv/spirv_code_buffer.h"

#include "dxvk_barrier.h"
#include "dxvk_cmdlist.h"
#include "dxvk_image.h"

namespace dxvk {

  /**
   * \brief Resolve pipeline properties
   *
   * This is used in a few situations where render pass resolves won't
   * work, e.g. if the source and destination offsets differ, or when
   * unsupported resolve modes are used, such as SAMPLE_ZERO on color
   * formats for implicit resolves.
   */
  struct DxvkMetaResolve {
    /** Shader arguments for resolve pipeline */
    struct Args {
      VkOffset2D srcOffset    = { };
      VkExtent2D extent       = { };
      uint32_t   layer        = 0u;
      uint32_t   stencilBit   = 0u;
    };

    /** Look-up key for resolve pipelines */
    struct Key {
      VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
      VkFormat format = VK_FORMAT_UNDEFINED;
      VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
      VkResolveModeFlagBits mode = VK_RESOLVE_MODE_NONE;
      VkResolveModeFlagBits modeStencil = VK_RESOLVE_MODE_NONE;
      VkBool32 bitwiseStencil = VK_FALSE;

      bool eq(const Key& other) const {
        return viewType       == other.viewType
            && format         == other.format
            && samples        == other.samples
            && mode           == other.mode
            && modeStencil    == other.modeStencil
            && bitwiseStencil == other.bitwiseStencil;
      }

      size_t hash() const {
        DxvkHashState hash;
        hash.add(uint32_t(viewType));
        hash.add(uint32_t(format));
        hash.add(uint32_t(samples));
        hash.add(uint32_t(mode));
        hash.add(uint32_t(modeStencil));
        hash.add(uint32_t(bitwiseStencil));
        return hash;
      }
    };

    const DxvkPipelineLayout* layout = nullptr;
    VkPipeline pipeline = VK_NULL_HANDLE;
  };

  /**
   * \brief Meta resolve views
   *
   * Creates views for attachment resolves if shader stage is 0,
   * or for pipeline-based resolves using the given shader stage.
   * TODO unify with copy path.
   */
  class DxvkMetaResolveViews {

  public:

    DxvkMetaResolveViews(
      const Rc<DxvkImage>&            dstImage,
      const VkImageSubresourceLayers& dstSubresources,
            VkFormat                  dstFormat,
      const Rc<DxvkImage>&            srcImage,
      const VkImageSubresourceLayers& srcSubresources,
            VkFormat                  srcFormat,
            VkShaderStageFlags        shaderStage);

    ~DxvkMetaResolveViews();

    Rc<DxvkImageView> dstView;
    Rc<DxvkImageView> srcView;
    Rc<DxvkImageView> srcStencilView;

    static VkImageViewType viewType(
      const DxvkImage&                image,
      const VkImageSubresourceLayers& subresources,
            VkImageUsageFlags         usage);
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
     * \param [in] key Requested resolve pipeline properties
     * \returns Compatible pipeline for the operation
     */
    DxvkMetaResolve getPipeline(const DxvkMetaResolve::Key& key);

  private:

    DxvkDevice* m_device = nullptr;

    dxvk::mutex m_mutex;

    std::unordered_map<DxvkMetaResolve::Key, DxvkMetaResolve, DxvkHash, DxvkEq> m_pipelines;

    std::vector<uint32_t> createVs(
      const DxvkMetaResolve::Key&       key,
      const DxvkPipelineLayout*         layout);

    std::vector<uint32_t> createPs(
      const DxvkMetaResolve::Key&       key,
      const DxvkPipelineLayout*         layout);

    DxvkMetaResolve createPipeline(const DxvkMetaResolve::Key& key);

    static std::string getName(const DxvkMetaResolve::Key& key, const char* type);

    static const char* getModeName(VkResolveModeFlagBits mode);

  };

}
