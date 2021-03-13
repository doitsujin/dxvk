#pragma once

#include "dxvk_limits.h"

#include <cstring>

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
            VkBool32              depthBiasEnable,
            VkPolygonMode         polygonMode,
            VkCullModeFlags       cullMode,
            VkFrontFace           frontFace,
            uint32_t              viewportCount,
            VkSampleCountFlags    sampleCount,
            VkConservativeRasterizationModeEXT conservativeMode)
    : m_depthClipEnable (uint32_t(depthClipEnable)),
      m_depthBiasEnable (uint32_t(depthBiasEnable)),
      m_polygonMode     (uint32_t(polygonMode)),
      m_cullMode        (uint32_t(cullMode)),
      m_frontFace       (uint32_t(frontFace)),
      m_viewportCount   (uint32_t(viewportCount)),
      m_sampleCount     (uint32_t(sampleCount)),
      m_conservativeMode(uint32_t(conservativeMode)),
      m_reserved        (0) { }
    
    VkBool32 depthClipEnable() const {
      return VkBool32(m_depthClipEnable);
    }

    VkBool32 depthBiasEnable() const {
      return VkBool32(m_depthBiasEnable);
    }

    VkPolygonMode polygonMode() const {
      return VkPolygonMode(m_polygonMode);
    }

    VkCullModeFlags cullMode() const {
      return VkCullModeFlags(m_cullMode);
    }

    VkFrontFace frontFace() const {
      return VkFrontFace(m_frontFace);
    }

    uint32_t viewportCount() const {
      return m_viewportCount;
    }

    VkSampleCountFlags sampleCount() const {
      return VkSampleCountFlags(m_sampleCount);
    }

    VkConservativeRasterizationModeEXT conservativeMode() const {
      return VkConservativeRasterizationModeEXT(m_conservativeMode);
    }

    void setViewportCount(uint32_t viewportCount) {
      m_viewportCount = viewportCount;
    }

  private:

    uint32_t m_depthClipEnable        : 1;
    uint32_t m_depthBiasEnable        : 1;
    uint32_t m_polygonMode            : 2;
    uint32_t m_cullMode               : 2;
    uint32_t m_frontFace              : 1;
    uint32_t m_viewportCount          : 5;
    uint32_t m_sampleCount            : 5;
    uint32_t m_conservativeMode       : 2;
    uint32_t m_reserved               : 13;
  
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
   * \brief Packed depth-stencil metadata
   *
   * Stores some flags and the depth-compare op in
   * two bytes. Stencil ops are stored separately.
   */
  class DxvkDsInfo {

  public:

    DxvkDsInfo() = default;

    DxvkDsInfo(
            VkBool32 enableDepthTest,
            VkBool32 enableDepthWrite,
            VkBool32 enableDepthBoundsTest,
            VkBool32 enableStencilTest,
            VkCompareOp depthCompareOp)
    : m_enableDepthTest       (uint16_t(enableDepthTest)),
      m_enableDepthWrite      (uint16_t(enableDepthWrite)),
      m_enableDepthBoundsTest (uint16_t(enableDepthBoundsTest)),
      m_enableStencilTest     (uint16_t(enableStencilTest)),
      m_depthCompareOp        (uint16_t(depthCompareOp)),
      m_reserved              (0) { }
    
    VkBool32 enableDepthTest() const {
      return VkBool32(m_enableDepthTest);
    }

    VkBool32 enableDepthWrite() const {
      return VkBool32(m_enableDepthWrite);
    }

    VkBool32 enableDepthBoundsTest() const {
      return VkBool32(m_enableDepthBoundsTest);
    }

    VkBool32 enableStencilTest() const {
      return VkBool32(m_enableStencilTest);
    }

    VkCompareOp depthCompareOp() const {
      return VkCompareOp(m_depthCompareOp);
    }

    void setEnableDepthBoundsTest(VkBool32 enableDepthBoundsTest) {
      m_enableDepthBoundsTest = VkBool32(enableDepthBoundsTest);
    }

  private:

    uint16_t m_enableDepthTest        : 1;
    uint16_t m_enableDepthWrite       : 1;
    uint16_t m_enableDepthBoundsTest  : 1;
    uint16_t m_enableStencilTest      : 1;
    uint16_t m_depthCompareOp         : 3;
    uint16_t m_reserved               : 9;

  };


  /**
   * \brief Packed stencil op
   *
   * Stores various stencil op parameters
   * for one single face in four bytes.
   */
  class DxvkDsStencilOp {

  public:

    DxvkDsStencilOp() = default;

    DxvkDsStencilOp(VkStencilOpState state)
    : m_failOp      (uint32_t(state.failOp)),
      m_passOp      (uint32_t(state.passOp)),
      m_depthFailOp (uint32_t(state.depthFailOp)),
      m_compareOp   (uint32_t(state.compareOp)),
      m_reserved    (0),
      m_compareMask (uint32_t(state.compareMask)),
      m_writeMask   (uint32_t(state.writeMask)) { }
    
    VkStencilOpState state() const {
      VkStencilOpState result;
      result.failOp      = VkStencilOp(m_failOp);
      result.passOp      = VkStencilOp(m_passOp);
      result.depthFailOp = VkStencilOp(m_depthFailOp);
      result.compareOp   = VkCompareOp(m_compareOp);
      result.compareMask = m_compareMask;
      result.writeMask   = m_writeMask;
      result.reference   = 0;
      return result;
    }

  private:

    uint32_t m_failOp                 : 3;
    uint32_t m_passOp                 : 3;
    uint32_t m_depthFailOp            : 3;
    uint32_t m_compareOp              : 3;
    uint32_t m_reserved               : 4;
    uint32_t m_compareMask            : 8;
    uint32_t m_writeMask              : 8;

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
            VkBool32          enableLogicOp,
            VkLogicOp         logicOp)
    : m_enableLogicOp (uint16_t(enableLogicOp)),
      m_logicOp       (uint16_t(logicOp)),
      m_reserved      (0) { }
    
    VkBool32 enableLogicOp() const {
      return VkBool32(m_enableLogicOp);
    }

    VkLogicOp logicOp() const {
      return VkLogicOp(m_logicOp);
    }

  private:

    uint16_t m_enableLogicOp          : 1;
    uint16_t m_logicOp                : 4;
    uint16_t m_reserved               : 11;

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
    
    bool operator == (const DxvkGraphicsPipelineStateInfo& other) const {
      return bit::bcmpeq(this, &other);
    }

    bool operator != (const DxvkGraphicsPipelineStateInfo& other) const {
      return !bit::bcmpeq(this, &other);
    }

    bool useDynamicStencilRef() const {
      return ds.enableStencilTest();
    }

    bool useDynamicDepthBias() const {
      return rs.depthBiasEnable();
    }

    bool useDynamicDepthBounds() const {
      return ds.enableDepthBoundsTest();
    }

    bool useDynamicBlendConstants() const {
      bool result = false;
      
      for (uint32_t i = 0; i < MaxNumRenderTargets && !result; i++) {
        result |= omBlend[i].blendEnable()
         && (util::isBlendConstantBlendFactor(omBlend[i].srcColorBlendFactor())
          || util::isBlendConstantBlendFactor(omBlend[i].dstColorBlendFactor())
          || util::isBlendConstantBlendFactor(omBlend[i].srcAlphaBlendFactor())
          || util::isBlendConstantBlendFactor(omBlend[i].dstAlphaBlendFactor()));
      }

      return result;
    }
    
    DxvkBindingMask         bsBindingMask;
    DxvkIaInfo              ia;
    DxvkIlInfo              il;
    DxvkRsInfo              rs;
    DxvkMsInfo              ms;
    DxvkDsInfo              ds;
    DxvkOmInfo              om;
    DxvkScInfo              sc;
    DxvkDsStencilOp         dsFront;
    DxvkDsStencilOp         dsBack;
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
    
    bool operator == (const DxvkComputePipelineStateInfo& other) const {
      return bit::bcmpeq(this, &other);
    }

    bool operator != (const DxvkComputePipelineStateInfo& other) const {
      return !bit::bcmpeq(this, &other);
    }
    
    DxvkBindingMask         bsBindingMask;
    DxvkScInfo              sc;
  };

}
