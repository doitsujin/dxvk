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
      m_reserved  (0),
      m_divisor   (divisor) { }
    
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
    uint32_t m_reserved               : 14;
    uint32_t m_divisor;

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
      return !std::memcmp(this, &other, sizeof(*this));
    }

    bool operator != (const DxvkGraphicsPipelineStateInfo& other) const {
      return std::memcmp(this, &other, sizeof(*this));
    }

    bool useDynamicStencilRef() const {
      return dsEnableStencilTest;
    }

    bool useDynamicDepthBias() const {
      return rsDepthBiasEnable;
    }

    bool useDynamicDepthBounds() const {
      return dsEnableDepthBoundsTest;
    }

    bool useDynamicBlendConstants() const {
      bool result = false;
      
      for (uint32_t i = 0; i < MaxNumRenderTargets && !result; i++) {
        result |= omBlendAttachments[i].blendEnable
         && (util::isBlendConstantBlendFactor(omBlendAttachments[i].srcColorBlendFactor)
          || util::isBlendConstantBlendFactor(omBlendAttachments[i].dstColorBlendFactor)
          || util::isBlendConstantBlendFactor(omBlendAttachments[i].srcAlphaBlendFactor)
          || util::isBlendConstantBlendFactor(omBlendAttachments[i].dstAlphaBlendFactor));
      }

      return result;
    }
    
    DxvkBindingMask         bsBindingMask;
    DxvkIaInfo              ia;
    DxvkIlInfo              il;
    
    VkBool32                            rsDepthClipEnable;
    VkBool32                            rsDepthBiasEnable;
    VkPolygonMode                       rsPolygonMode;
    VkCullModeFlags                     rsCullMode;
    VkFrontFace                         rsFrontFace;
    uint32_t                            rsViewportCount;
    VkSampleCountFlags                  rsSampleCount;
    
    VkSampleCountFlags                  msSampleCount;
    uint32_t                            msSampleMask;
    VkBool32                            msEnableAlphaToCoverage;
    
    VkBool32                            dsEnableDepthTest;
    VkBool32                            dsEnableDepthWrite;
    VkBool32                            dsEnableDepthBoundsTest;
    VkBool32                            dsEnableStencilTest;
    VkCompareOp                         dsDepthCompareOp;
    VkStencilOpState                    dsStencilOpFront;
    VkStencilOpState                    dsStencilOpBack;
    
    VkBool32                            omEnableLogicOp;
    VkLogicOp                           omLogicOp;
    VkPipelineColorBlendAttachmentState omBlendAttachments[MaxNumRenderTargets];
    VkComponentMapping                  omComponentMapping[MaxNumRenderTargets];

    uint32_t                            scSpecConstants[MaxNumSpecConstants];

    DxvkIlAttribute         ilAttributes      [DxvkLimits::MaxNumVertexAttributes];
    DxvkIlBinding           ilBindings        [DxvkLimits::MaxNumVertexBindings];
  };

}
