#pragma once

#include "dxvk_format.h"
#include "dxvk_limits.h"

#include <atomic>
#include <cstring>
#include <optional>
#include <utility>

namespace dxvk {

  /**
   * \brief Packed input assembly state
   *
   * Stores the primitive topology
   * and primitive restart info.
   */
  class DxvkIaInfo {

  public:

    DxvkIaInfo() = default;

    DxvkIaInfo(
            VkPrimitiveTopology primitiveTopology,
            VkBool32            primitiveRestart,
            uint32_t            patchVertexCount)
    : m_primitiveTopology (uint16_t(primitiveTopology)),
      m_primitiveRestart  (uint16_t(primitiveRestart)),
      m_patchVertexCount  (uint16_t(patchVertexCount)),
      m_reserved          (0) { }

    VkPrimitiveTopology primitiveTopology() const {
      return m_primitiveTopology <= VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
        ? VkPrimitiveTopology(m_primitiveTopology)
        : VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    }

    VkBool32 primitiveRestart() const {
      return VkBool32(m_primitiveRestart);
    }

    uint32_t patchVertexCount() const {
      return m_patchVertexCount;
    }

  private:

    uint16_t m_primitiveTopology      : 4;
    uint16_t m_primitiveRestart       : 1;
    uint16_t m_patchVertexCount       : 6;
    uint16_t m_reserved               : 5;

  };


  /**
   * \brief Packed input layout metadata
   * 
   * Stores the number of vertex attributes
   * and bindings in one byte each.
   */
  class DxvkIlInfo {

  public:

    DxvkIlInfo() = default;

    DxvkIlInfo(
            uint32_t        attributeCount,
            uint32_t        bindingCount)
    : m_attributeCount(uint8_t(attributeCount)),
      m_bindingCount  (uint8_t(bindingCount)) { }

    uint32_t attributeCount() const {
      return m_attributeCount;
    }

    uint32_t bindingCount() const {
      return m_bindingCount;
    }

  private:

    uint8_t m_attributeCount;
    uint8_t m_bindingCount;

  };


  /**
   * \brief Packed vertex attribute
   *
   * Stores a vertex attribute description. Assumes
   * that all vertex formats have numerical values
   * of 127 or less (i.e. fit into 7 bits).
   */
  class DxvkIlAttribute {

  public:

    DxvkIlAttribute() = default;

    DxvkIlAttribute(
            uint32_t                        location,
            uint32_t                        binding,
            VkFormat                        format,
            uint32_t                        offset)
    : m_location(uint32_t(location)),
      m_binding (uint32_t(binding)),
      m_format  (uint32_t(format)),
      m_offset  (uint32_t(offset)),
      m_reserved(0) { }
    
    uint32_t location() const {
      return m_location;
    }
    
    uint32_t binding() const {
      return m_binding;
    }

    VkFormat format() const {
      return VkFormat(m_format);
    }

    uint32_t offset() const {
      return m_offset;
    }
    
    VkVertexInputAttributeDescription description() const {
      VkVertexInputAttributeDescription result;
      result.location = m_location;
      result.binding  = m_binding;
      result.format   = VkFormat(m_format);
      result.offset   = m_offset;
      return result;
    }

  private:

    uint32_t m_location               : 5;
    uint32_t m_binding                : 5;
    uint32_t m_format                 : 7;
    uint32_t m_offset                 : 11;
    uint32_t m_reserved               : 4;
  
  };


  /**
   * \brief Packed vertex binding
   *
   * Stores a vertex binding description,
   * including the 32-bit divisor.
   */
  class DxvkIlBinding {

  public:

    DxvkIlBinding() = default;

    DxvkIlBinding(
            uint32_t                        binding,
            uint32_t                        stride,
            VkVertexInputRate               inputRate,
            uint32_t                        divisor)
    : m_binding   (uint32_t(binding)),
      m_stride    (uint32_t(stride)),
      m_inputRate (uint32_t(inputRate)),
      m_divisor   (uint32_t(divisor < (1u << 14) ? divisor : 0u)) { }
    
    uint32_t binding() const {
      return m_binding;
    }
    
    uint32_t stride() const {
      return m_stride;
    }

    VkVertexInputRate inputRate() const {
      return VkVertexInputRate(m_inputRate);
    }
    
    uint32_t divisor() const {
      return m_divisor;
    }

    VkVertexInputBindingDescription description() const {
      VkVertexInputBindingDescription result;
      result.binding = m_binding;
      result.stride  = m_stride;
      result.inputRate = VkVertexInputRate(m_inputRate);
      return result;
    }

    void setStride(uint32_t stride) {
      m_stride = stride;
    }

  private:

    uint32_t m_binding                : 5;
    uint32_t m_stride                 : 12;
    uint32_t m_inputRate              : 1;
    uint32_t m_divisor                : 14;

  };


  /**
   * \brief Packed rasterizer state
   *
   * Stores a bunch of flags and parameters
   * related to rasterization in four bytes.
   */
  class DxvkRsInfo {

  public:

    DxvkRsInfo() = default;

    DxvkRsInfo(
            VkBool32              depthClipEnable,
            VkPolygonMode         polygonMode,
            VkSampleCountFlags    sampleCount,
            VkConservativeRasterizationModeEXT conservativeMode,
            VkBool32              flatShading,
            VkLineRasterizationModeEXT lineMode)
    : m_depthClipEnable (uint16_t(depthClipEnable)),
      m_polygonMode     (uint16_t(polygonMode)),
      m_sampleCount     (uint16_t(sampleCount)),
      m_conservativeMode(uint16_t(conservativeMode)),
      m_flatShading     (uint16_t(flatShading)),
      m_lineMode        (uint16_t(lineMode)),
      m_reserved        (0) { }
    
    VkBool32 depthClipEnable() const {
      return VkBool32(m_depthClipEnable);
    }

    VkPolygonMode polygonMode() const {
      return VkPolygonMode(m_polygonMode);
    }

    VkSampleCountFlags sampleCount() const {
      return VkSampleCountFlags(m_sampleCount);
    }

    VkConservativeRasterizationModeEXT conservativeMode() const {
      return VkConservativeRasterizationModeEXT(m_conservativeMode);
    }

    VkBool32 flatShading() const {
      return VkBool32(m_flatShading);
    }

    VkLineRasterizationModeEXT lineMode() const {
      return VkLineRasterizationModeEXT(m_lineMode);
    }

    bool eq(const DxvkRsInfo& other) const {
      return !std::memcmp(this, &other, sizeof(*this));
    }

  private:

    uint16_t m_depthClipEnable        : 1;
    uint16_t m_polygonMode            : 2;
    uint16_t m_sampleCount            : 5;
    uint16_t m_conservativeMode       : 2;
    uint16_t m_flatShading            : 1;
    uint16_t m_lineMode               : 2;
    uint16_t m_reserved               : 3;
  
  };


  /**
   * \brief Packed multisample info
   *
   * Stores the sample mask, sample count override
   * and alpha-to-coverage state in four bytes.
   */
  class DxvkMsInfo {

  public:

    DxvkMsInfo() = default;

    DxvkMsInfo(
            VkSampleCountFlags      sampleCount,
            uint32_t                sampleMask,
            VkBool32                enableAlphaToCoverage)
    : m_sampleCount           (uint16_t(sampleCount)),
      m_enableAlphaToCoverage (uint16_t(enableAlphaToCoverage)),
      m_reserved              (0),
      m_sampleMask            (uint16_t(sampleMask)) { }
    
    VkSampleCountFlags sampleCount() const {
      return VkSampleCountFlags(m_sampleCount);
    }

    uint32_t sampleMask() const {
      return m_sampleMask;
    }

    VkBool32 enableAlphaToCoverage() const {
      return VkBool32(m_enableAlphaToCoverage);
    }

    void setSampleCount(VkSampleCountFlags sampleCount) {
      m_sampleCount = uint16_t(sampleCount);
    }

  private:

    uint16_t m_sampleCount            : 5;
    uint16_t m_enableAlphaToCoverage  : 1;
    uint16_t m_reserved               : 10;
    uint16_t m_sampleMask;

  };


  /**
   * \brief Packed output merger metadata
   *
   * Stores the logic op state in two bytes.
   * Blend modes are stored separately.
   */
  class DxvkOmInfo {

  public:

    DxvkOmInfo() = default;

    DxvkOmInfo(
            VkBool32           enableLogicOp,
            VkLogicOp          logicOp,
            VkImageAspectFlags feedbackLoop)
    : m_enableLogicOp (uint16_t(enableLogicOp)),
      m_logicOp       (uint16_t(logicOp)),
      m_feedbackLoop  (uint16_t(feedbackLoop)),
      m_reserved      (0) { }
    
    VkBool32 enableLogicOp() const {
      return VkBool32(m_enableLogicOp);
    }

    VkLogicOp logicOp() const {
      return VkLogicOp(m_logicOp);
    }

    VkImageAspectFlags feedbackLoop() const {
      return VkImageAspectFlags(m_feedbackLoop);
    }

    void setFeedbackLoop(VkImageAspectFlags feedbackLoop) {
      m_feedbackLoop = uint16_t(feedbackLoop);
    }

  private:

    uint16_t m_enableLogicOp          : 1;
    uint16_t m_logicOp                : 4;
    uint16_t m_feedbackLoop           : 2;
    uint16_t m_reserved               : 9;

  };


  /**
   * \brief Packed render target formats
   *
   * Compact representation of depth-stencil and color attachments,
   * as well as the read-only mask for the depth-stencil attachment,
   * which needs to be known at pipeline compile time.
   */
  class DxvkRtInfo {

  public:

    DxvkRtInfo() = default;

    DxvkRtInfo(
            uint32_t            colorFormatCount,
      const VkFormat*           colorFormats,
            VkFormat            depthStencilFormat,
            VkImageAspectFlags  depthStencilReadOnlyAspects)
    : m_packedData(0ull) {
      m_packedData |= encodeDepthStencilFormat(depthStencilFormat);
      m_packedData |= encodeDepthStencilAspects(depthStencilReadOnlyAspects);

      for (uint32_t i = 0; i < colorFormatCount; i++)
        m_packedData |= encodeColorFormat(colorFormats[i], i);
    }

    VkFormat getColorFormat(uint32_t index) const {
      return decodeColorFormat(m_packedData, index);
    }

    VkFormat getDepthStencilFormat() const {
      return decodeDepthStencilFormat(m_packedData);
    }

    VkImageAspectFlags getDepthStencilReadOnlyAspects() const {
      return decodeDepthStencilAspects(m_packedData);
    }

  private:

    uint64_t m_packedData;

    static uint64_t encodeDepthStencilAspects(VkImageAspectFlags aspects) {
      return uint64_t(aspects) << 61;
    }

    static uint64_t encodeDepthStencilFormat(VkFormat format) {
      return format
        ? (uint64_t(format) - uint64_t(VK_FORMAT_E5B9G9R9_UFLOAT_PACK32)) << 56
        : (uint64_t(0));
    }

    static uint64_t encodeColorFormat(VkFormat format, uint32_t index) {
      uint64_t value = 0u;

      for (const auto& p : s_colorFormatRanges) {
        if (format >= p.first && format <= p.second) {
          value += uint32_t(format) - uint32_t(p.first);
          break;
        }

        value += uint32_t(p.second) - uint32_t(p.first) + 1u;
      }

      return value << (7 * index);
    }

    static VkImageAspectFlags decodeDepthStencilAspects(uint64_t value) {
      return VkImageAspectFlags((value >> 61) & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT));
    }

    static VkFormat decodeDepthStencilFormat(uint64_t value) {
      value = (value >> 56) & 0x1F;

      return value
        ? VkFormat(value + uint64_t(VK_FORMAT_E5B9G9R9_UFLOAT_PACK32))
        : VkFormat(VK_FORMAT_UNDEFINED);
    }

    static VkFormat decodeColorFormat(uint64_t value, uint32_t index) {
      value = (value >> (7 * index)) & 0x7F;

      for (const auto& p : s_colorFormatRanges) {
        uint32_t rangeSize = uint32_t(p.second) - uint32_t(p.first) + 1u;

        if (value < rangeSize)
          return VkFormat(uint32_t(p.first) + uint32_t(value));

        value -= rangeSize;
      }

      return VK_FORMAT_UNDEFINED;
    }

    static constexpr std::array<std::pair<VkFormat, VkFormat>, 3> s_colorFormatRanges = {{
      { VK_FORMAT_UNDEFINED,                  VK_FORMAT_E5B9G9R9_UFLOAT_PACK32  },  /*   0 - 123 */
      { VK_FORMAT_A4R4G4B4_UNORM_PACK16,      VK_FORMAT_A4B4G4R4_UNORM_PACK16   },  /* 124 - 125 */
      { VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR,  VK_FORMAT_A8_UNORM_KHR            },  /* 126 - 127 */
    }};

  };


  /**
   * \brief Packed attachment blend mode
   *
   * Stores blendig parameters for a single
   * color attachment in four bytes.
   */
  class DxvkOmAttachmentBlend {

  public:

    DxvkOmAttachmentBlend() = default;

    DxvkOmAttachmentBlend(
            VkBool32                    blendEnable,
            VkBlendFactor               srcColorBlendFactor,
            VkBlendFactor               dstColorBlendFactor,
            VkBlendOp                   colorBlendOp,
            VkBlendFactor               srcAlphaBlendFactor,
            VkBlendFactor               dstAlphaBlendFactor,
            VkBlendOp                   alphaBlendOp,
            VkColorComponentFlags       colorWriteMask)
    : m_blendEnable         (uint32_t(blendEnable)),
      m_srcColorBlendFactor (uint32_t(srcColorBlendFactor)),
      m_dstColorBlendFactor (uint32_t(dstColorBlendFactor)),
      m_colorBlendOp        (uint32_t(colorBlendOp)),
      m_srcAlphaBlendFactor (uint32_t(srcAlphaBlendFactor)),
      m_dstAlphaBlendFactor (uint32_t(dstAlphaBlendFactor)),
      m_alphaBlendOp        (uint32_t(alphaBlendOp)),
      m_colorWriteMask      (uint32_t(colorWriteMask)),
      m_reserved            (0) { }
    
    VkBool32 blendEnable() const {
      return m_blendEnable;
    }

    VkBlendFactor srcColorBlendFactor() const {
      return VkBlendFactor(m_srcColorBlendFactor);
    }

    VkBlendFactor dstColorBlendFactor() const {
      return VkBlendFactor(m_dstColorBlendFactor);
    }

    VkBlendOp colorBlendOp() const {
      return VkBlendOp(m_colorBlendOp);
    }

    VkBlendFactor srcAlphaBlendFactor() const {
      return VkBlendFactor(m_srcAlphaBlendFactor);
    }

    VkBlendFactor dstAlphaBlendFactor() const {
      return VkBlendFactor(m_dstAlphaBlendFactor);
    }

    VkBlendOp alphaBlendOp() const {
      return VkBlendOp(m_alphaBlendOp);
    }

    VkColorComponentFlags colorWriteMask() const {
      return VkColorComponentFlags(m_colorWriteMask);
    }

    VkPipelineColorBlendAttachmentState state() const {
      VkPipelineColorBlendAttachmentState result;
      result.blendEnable         = VkBool32(m_blendEnable);
      result.srcColorBlendFactor = VkBlendFactor(m_srcColorBlendFactor);
      result.dstColorBlendFactor = VkBlendFactor(m_dstColorBlendFactor);
      result.colorBlendOp        = VkBlendOp(m_colorBlendOp);
      result.srcAlphaBlendFactor = VkBlendFactor(m_srcAlphaBlendFactor);
      result.dstAlphaBlendFactor = VkBlendFactor(m_dstAlphaBlendFactor);
      result.alphaBlendOp        = VkBlendOp(m_alphaBlendOp);
      result.colorWriteMask      = VkColorComponentFlags(m_colorWriteMask);
      return result;
    }

  private:

    uint32_t m_blendEnable            : 1;
    uint32_t m_srcColorBlendFactor    : 5;
    uint32_t m_dstColorBlendFactor    : 5;
    uint32_t m_colorBlendOp           : 3;
    uint32_t m_srcAlphaBlendFactor    : 5;
    uint32_t m_dstAlphaBlendFactor    : 5;
    uint32_t m_alphaBlendOp           : 3;
    uint32_t m_colorWriteMask         : 4;
    uint32_t m_reserved               : 1;

  };


  /**
   * \brief Packed attachment swizzle
   *
   * Stores the component mapping for one
   * single color attachment in one byte.
   */
  class DxvkOmAttachmentSwizzle {

  public:

    DxvkOmAttachmentSwizzle() = default;

    DxvkOmAttachmentSwizzle(VkComponentMapping mapping)
    : m_r(util::getComponentIndex(mapping.r, 0)),
      m_g(util::getComponentIndex(mapping.g, 1)),
      m_b(util::getComponentIndex(mapping.b, 2)),
      m_a(util::getComponentIndex(mapping.a, 3)) { }
    
    uint32_t rIndex() const { return m_r; }
    uint32_t gIndex() const { return m_g; }
    uint32_t bIndex() const { return m_b; }
    uint32_t aIndex() const { return m_a; }
    
    VkComponentMapping mapping() const {
      VkComponentMapping result;
      result.r = decodeSwizzle(m_r);
      result.g = decodeSwizzle(m_g);
      result.b = decodeSwizzle(m_b);
      result.a = decodeSwizzle(m_a);
      return result;
    }

  private:

    uint8_t m_r : 2;
    uint8_t m_g : 2;
    uint8_t m_b : 2;
    uint8_t m_a : 2;

    static VkComponentSwizzle decodeSwizzle(uint8_t swizzle) {
      return VkComponentSwizzle(uint32_t(swizzle) + uint32_t(VK_COMPONENT_SWIZZLE_R));
    }

  };


  /**
   * \brief Specialization constant state
   *
   * Stores the raw 32-bit spec constant values.
   */
  struct DxvkScInfo {
    uint32_t specConstants[DxvkLimits::MaxNumSpecConstants];
  };


  /**
   * \brief Packed graphics pipeline state
   *
   * Stores a compressed representation of the full
   * graphics pipeline state which is optimized for
   * lookup performance.
   */
  struct alignas(32) DxvkGraphicsPipelineStateInfo {
    DxvkGraphicsPipelineStateInfo() {
      std::memset(this, 0, sizeof(*this));
    }

    DxvkGraphicsPipelineStateInfo(const DxvkGraphicsPipelineStateInfo& other) {
      std::memcpy(this, &other, sizeof(*this));
    }
    
    DxvkGraphicsPipelineStateInfo& operator = (const DxvkGraphicsPipelineStateInfo& other) {
      std::memcpy(this, &other, sizeof(*this));
      return *this;
    }
    
    bool eq(const DxvkGraphicsPipelineStateInfo& other) const {
      return bit::bcmpeq(this, &other);
    }

    size_t hash() const {
      auto src = reinterpret_cast<const unsigned char*>(this);
      return size_t(bit::fnv1a_hash(src, sizeof(*this)));
    }

    bool useDynamicDepthTest() const {
      return rt.getDepthStencilFormat();
    }

    bool useDynamicDepthBounds() const {
      return rt.getDepthStencilFormat();
    }

    bool useDynamicStencilTest() const {
      auto format = rt.getDepthStencilFormat();
      return format && (lookupFormatInfo(format)->aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT);
    }

    bool useDynamicVertexStrides() const {
      if (!il.bindingCount())
        return false;

      bool result = true;

      for (uint32_t i = 0; i < il.bindingCount() && result; i++)
        result = !ilBindings[i].stride();

      return result;
    }

    bool useDynamicBlendConstants() const {
      bool result = false;
      
      for (uint32_t i = 0; i < MaxNumRenderTargets && !result; i++) {
        result |= rt.getColorFormat(i) && omBlend[i].blendEnable()
         && (util::isBlendConstantBlendFactor(omBlend[i].srcColorBlendFactor())
          || util::isBlendConstantBlendFactor(omBlend[i].dstColorBlendFactor())
          || util::isBlendConstantBlendFactor(omBlend[i].srcAlphaBlendFactor())
          || util::isBlendConstantBlendFactor(omBlend[i].dstAlphaBlendFactor()));
      }

      return result;
    }

    bool useDualSourceBlending() const {
      return omBlend[0].blendEnable() && (
        util::isDualSourceBlendFactor(omBlend[0].srcColorBlendFactor()) ||
        util::isDualSourceBlendFactor(omBlend[0].dstColorBlendFactor()) ||
        util::isDualSourceBlendFactor(omBlend[0].srcAlphaBlendFactor()) ||
        util::isDualSourceBlendFactor(omBlend[0].dstAlphaBlendFactor()));
    }

    bool writesRenderTarget(
            uint32_t                        target) const {
      if (!omBlend[target].colorWriteMask())
        return false;

      VkFormat rtFormat = rt.getColorFormat(target);
      return rtFormat != VK_FORMAT_UNDEFINED;
    }


    DxvkIaInfo              ia;
    DxvkIlInfo              il;
    DxvkRsInfo              rs;
    DxvkMsInfo              ms;
    DxvkOmInfo              om;
    DxvkRtInfo              rt;
    DxvkScInfo              sc;
    DxvkOmAttachmentSwizzle omSwizzle         [DxvkLimits::MaxNumRenderTargets];
    DxvkOmAttachmentBlend   omBlend           [DxvkLimits::MaxNumRenderTargets];
    DxvkIlAttribute         ilAttributes      [DxvkLimits::MaxNumVertexAttributes];
    DxvkIlBinding           ilBindings        [DxvkLimits::MaxNumVertexBindings];
  };


  /**
   * \brief Compute pipeline state info
   */
  struct alignas(32) DxvkComputePipelineStateInfo {
    DxvkComputePipelineStateInfo() {
      std::memset(this, 0, sizeof(*this));
    }

    DxvkComputePipelineStateInfo(const DxvkComputePipelineStateInfo& other) {
      std::memcpy(this, &other, sizeof(*this));
    }
    
    DxvkComputePipelineStateInfo& operator = (const DxvkComputePipelineStateInfo& other) {
      std::memcpy(this, &other, sizeof(*this));
      return *this;
    }
    
    bool eq(const DxvkComputePipelineStateInfo& other) const {
      return bit::bcmpeq(this, &other);
    }
    
    size_t hash() const {
      auto src = reinterpret_cast<const unsigned char*>(this);
      return size_t(bit::fnv1a_hash(src, sizeof(*this)));
    }

    DxvkScInfo              sc;
  };


  /**
   * \brief Pipeline state look-up table
   *
   * Provides a thread-safe, adaptive data structure for pipeline variants.
   * Look-up and insertion are expected to be O(log n).
   */
  template<typename K, typename V>
  class DxvkPipelineVariantTable {
    static constexpr size_t LayerBits = 5u;
    static constexpr size_t LayerSize = 1u << LayerBits;

    static constexpr uint32_t HashThreshold = 4u;
  public:

    ~DxvkPipelineVariantTable() {
      iter(m_table, [] (Entry* e) { delete e; });
    }

    V* find(const K& k) const {
      // If the number of variants is small, avoid computing the
      // state hash since that is somewhat expensive to do
      uint32_t mask = m_table.mask.load(std::memory_order::memory_order_acquire);

      bool useSimple = !(mask & (mask - 1u));

      if (!useSimple)
        useSimple = bit::popcnt(mask) < HashThreshold;

      if (likely(useSimple)) {
        for (auto index : bit::BitMask(mask)) {
          // If more that one level is present, we need to consider
          // those as well, but we can only do that on the hash path.
          auto e = m_table.entries[index].load(std::memory_order_acquire);
          useSimple = useSimple && !e->table.mask.load(std::memory_order_relaxed);

          // Scan entries with the same hash
          while (e) {
            if (e->key.eq(k))
              return &e->value;

            e = e->next.load(std::memory_order_acquire);
          }
        }

        if (likely(useSimple))
          return nullptr;
      }

      // Compute hash and traverse entries
      size_t hash = k.hash();
      size_t shift = 0u;

      const Table* table = &m_table;

      while (true) {
        size_t index = computeListIndex(hash, shift);
        shift += LayerBits;

        auto e = table->entries[index].load(std::memory_order_acquire);

        if (!e)
          break;

        // Fetch next table from list head
        // and ensure that the hash matches
        table = &e->table;

        if (e->hash != hash)
          continue;

        // Scan entries with the same hash
        while (e) {
          if (e->key.eq(k))
            return &e->value;

          e = e->next.load(std::memory_order_acquire);
        }
      }

      // No pipeline found
      return nullptr;
    }

    template<typename... Args>
    V* add(const K& k, Args&&... args) {
      size_t hash = k.hash();

      // Try to insert the new entry into the top-level look-up table.
      // If the given entry is already set, try the next level.
      Entry* entry = new Entry(k, hash, std::forward<Args>(args)...);
      Table* table = &m_table;
      Entry* target = nullptr;

      size_t index = -1;
      size_t shift = 0u;

      while (!target) {
        index = computeListIndex(hash, shift);

        // If this succeeds, this is the first entry at the given index
        if (table->entries[index].compare_exchange_strong(target, entry,
            std::memory_order_release, std::memory_order_acquire))
          break;

        // Check if there is a hash collision
        if (target->hash == hash)
          break;

        table = &target->table;
        target = nullptr;

        shift += LayerBits;
      }

      if (target) {
        // The new entry has the same hash as the target entry, so
        // just append it to the linked list. This should be rare.
        while (true) {
          Entry* next = nullptr;

          if (target->next.compare_exchange_strong(next, entry,
              std::memory_order_release, std::memory_order_acquire))
            break;

          target = next;
        }
      } else {
        // Update mask now that the corresponding entry is non-null
        table->mask.fetch_or(1u << index, std::memory_order_release);
      }

      return &entry->value;
    }

    template<typename Fn>
    void forEach(const Fn& fn) const {
      iter(m_table, [&] (Entry* e) { fn(e->value); });
    }

  private:

    struct Entry;

    struct Table {
      std::array<std::atomic<Entry*>, LayerSize> entries = { };
      std::atomic<uint32_t> mask = { 0u };
    };

    struct Entry {
      template<typename... Args>
      Entry(const K& k, size_t h, Args&&... args)
      : key(k), hash(h), value(std::forward<Args>(args)...) { }

      K       key   = { };
      size_t  hash  = 0u;
      V       value = { };
      Table   table = { };

      std::atomic<Entry*> next = { nullptr };
    };

    Table m_table;

    template<typename Fn>
    static void iter(const Table& table, const Fn& fn) {
      uint32_t mask = table.mask.load(std::memory_order_acquire);

      for (auto index : bit::BitMask(mask)) {
        Entry* e = table.entries[index].load(std::memory_order_relaxed);

        // Recurse first so that the function can be used for destruction.
        // Only the first entry in each list can have a sub-table.
        if (e->table.mask.load(std::memory_order_relaxed))
          iter(e->table, fn);

        while (e) {
          Entry* next = e->next.load(std::memory_order_acquire);
          fn(e);
          e = next;
        }
      }
    }

    static size_t computeListIndex(size_t hash, size_t shift) {
      // Swap bytes to ensure that high bits of the hash contribute to the index.
      // This is useful since hashes often only differ in the high 32 bits.
      if constexpr (sizeof(size_t) == sizeof(uint64_t)) {
        uint64_t index = hash;

        index = ((index >> 56u) & 0x00000000000000ffull)
              | ((index >> 40u) & 0x000000000000ff00ull)
              | ((index >> 24u) & 0x0000000000ff0000ull)
              | ((index >>  8u) & 0x00000000ff000000ull)
              | ((index <<  8u) & 0x000000ff00000000ull)
              | ((index << 24u) & 0x0000ff0000000000ull)
              | ((index << 40u) & 0x00ff000000000000ull)
              | ((index << 56u) & 0xff00000000000000ull);

        return size_t((index + hash) >> shift) % LayerSize;
      } else {
        uint32_t index = hash;

        index = ((index >> 24u) & 0x000000ffu)
              | ((index >>  8u) & 0x0000ff00u)
              | ((index <<  8u) & 0x00ff0000u)
              | ((index << 24u) & 0xff000000u);

        return size_t((index + hash) >> shift) % LayerSize;
      }
    }

  };

}
