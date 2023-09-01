#pragma once

#include "dxvk_compute.h"
#include "dxvk_graphics.h"
#include "dxvk_renderpass.h"

namespace dxvk {

  /**
   * \brief State cache entry key
   * 
   * Stores the shader keys for all
   * graphics shader stages. Used to
   * look up cached state entries.
   */
  struct DxvkStateCacheKey {
    DxvkShaderKey vs;
    DxvkShaderKey tcs;
    DxvkShaderKey tes;
    DxvkShaderKey gs;
    DxvkShaderKey fs;

    bool eq(const DxvkStateCacheKey& key) const;

    size_t hash() const;
  };


  /**
   * \brief State entry type
   */
  enum class DxvkStateCacheEntryType : uint32_t {
    MonolithicPipeline  = 0,
    PipelineLibrary     = 1,
  };

  
  /**
   * \brief State entry
   * 
   * Stores the shaders used in a pipeline, as well
   * as the full state vector, including its render
   * pass format. This also includes a SHA-1 hash
   * that is used as a check sum to verify integrity.
   */
  struct DxvkStateCacheEntry {
    DxvkStateCacheEntryType       type;
    DxvkStateCacheKey             shaders;
    DxvkGraphicsPipelineStateInfo gpState;
    Sha1Hash                      hash;
  };


  /**
   * \brief State cache header
   * 
   * Stores the state cache format version. If an
   * existing cache file is incompatible to the
   * current version, it will be discarded.
   */
  struct DxvkStateCacheHeader {
    char     magic[4]   = { 'D', 'X', 'V', 'K' };
    uint32_t version    = 17;
    uint32_t entrySize  = 0; /* no longer meaningful */
  };

  static_assert(sizeof(DxvkStateCacheHeader) == 12);

  using DxvkBindingMaskV10 = DxvkBindingSet<384>;
  using DxvkBindingMaskV8 = DxvkBindingSet<128>;

  class DxvkIlBindingV9 {

  public:

    uint32_t m_binding                : 5;
    uint32_t m_stride                 : 12;
    uint32_t m_inputRate              : 1;
    uint32_t m_reserved               : 14;
    uint32_t m_divisor;

    DxvkIlBinding convert() const {
      return DxvkIlBinding(m_binding, m_stride,
        VkVertexInputRate(m_inputRate), m_divisor);
    }

  };

  /**
   * \brief Old attachment format struct
   */
  struct DxvkAttachmentFormatV11 {
    VkFormat      format = VK_FORMAT_UNDEFINED;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
  };
  
  
  /**
   * \brief Old render pass format struct
   */
  struct DxvkRenderPassFormatV11 {
    VkSampleCountFlagBits sampleCount;
    DxvkAttachmentFormatV11 depth;
    DxvkAttachmentFormatV11 color[MaxNumRenderTargets];

    DxvkRtInfo convert() const {
      VkImageAspectFlags readOnlyAspects = 0;
      auto depthFormatInfo = lookupFormatInfo(depth.format);

      if (depth.format && depthFormatInfo) {
        readOnlyAspects = depthFormatInfo->aspectMask
          & ~vk::getWritableAspectsForLayout(depth.layout);
      }

      std::array<VkFormat, MaxNumRenderTargets> colorFormats;
      for (uint32_t i = 0; i < MaxNumRenderTargets; i++)
        colorFormats[i] = color[i].format;

      return DxvkRtInfo(MaxNumRenderTargets, colorFormats.data(),
        depth.format, readOnlyAspects);
    }
  };

  class DxvkRsInfoV12 {

  public:

    uint32_t m_depthClipEnable        : 1;
    uint32_t m_depthBiasEnable        : 1;
    uint32_t m_polygonMode            : 2;
    uint32_t m_cullMode               : 2;
    uint32_t m_frontFace              : 1;
    uint32_t m_viewportCount          : 5;
    uint32_t m_sampleCount            : 5;
    uint32_t m_conservativeMode       : 2;
    uint32_t m_reserved               : 13;

    DxvkRsInfo convert() const {
      return DxvkRsInfo(
        VkBool32(m_depthClipEnable),
        VkBool32(m_depthBiasEnable),
        VkPolygonMode(m_polygonMode),
        VkSampleCountFlags(m_sampleCount),
        VkConservativeRasterizationModeEXT(m_conservativeMode),
        VK_FALSE, VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT);
    }

  };


  class DxvkRsInfoV13 {

  public:

    uint16_t m_depthClipEnable        : 1;
    uint16_t m_depthBiasEnable        : 1;
    uint16_t m_polygonMode            : 2;
    uint16_t m_cullMode               : 2;
    uint16_t m_frontFace              : 1;
    uint16_t m_sampleCount            : 5;
    uint16_t m_conservativeMode       : 2;
    uint16_t m_reserved               : 2;

    DxvkRsInfo convert() const {
      return DxvkRsInfo(
        VkBool32(m_depthClipEnable),
        VkBool32(m_depthBiasEnable),
        VkPolygonMode(m_polygonMode),
        VkSampleCountFlags(m_sampleCount),
        VkConservativeRasterizationModeEXT(m_conservativeMode),
        VK_FALSE, VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT);
    }

  };

}
