#pragma once

#include "dxvk_buffer.h"
#include "dxvk_framebuffer.h"
#include "dxvk_limits.h"
#include "dxvk_shader.h"

namespace dxvk {
  
  /**
   * \brief Blend constants
   * 
   * Stores a blend factor
   * as an RGBA color value.
   */
  struct DxvkBlendConstants {
    float r, g, b, a;

    bool operator == (const DxvkBlendConstants& other) const {
      return this->r == other.r && this->g == other.g
          && this->b == other.b && this->a == other.a;
    }

    bool operator != (const DxvkBlendConstants& other) const {
      return this->r != other.r || this->g != other.g
          || this->b != other.b || this->a != other.a;
    }
  };


  /**
   * \brief Depth bias representation
   * 
   * Stores depth bias representation info.
   */
  struct DxvkDepthBiasRepresentation {
    VkDepthBiasRepresentationEXT depthBiasRepresentation;
    VkBool32                     depthBiasExact;

    bool operator == (const DxvkDepthBiasRepresentation& other) const {
      return depthBiasRepresentation == other.depthBiasRepresentation
          && depthBiasExact          == other.depthBiasExact;
    }

    bool operator != (const DxvkDepthBiasRepresentation& other) const {
      return depthBiasRepresentation != other.depthBiasRepresentation
          || depthBiasExact          != other.depthBiasExact;
    }
  };


  /**
   * \brief Depth bias
   * 
   * Stores depth bias values.
   */
  struct DxvkDepthBias {
    float                        depthBiasConstant;
    float                        depthBiasSlope;
    float                        depthBiasClamp;

    bool operator == (const DxvkDepthBias& other) const {
      return depthBiasConstant       == other.depthBiasConstant
          && depthBiasSlope          == other.depthBiasSlope
          && depthBiasClamp          == other.depthBiasClamp;
    }

    bool operator != (const DxvkDepthBias& other) const {
      return depthBiasConstant       != other.depthBiasConstant
          || depthBiasSlope          != other.depthBiasSlope
          || depthBiasClamp          != other.depthBiasClamp;
    }
  };


  /**
   * \brief Depth bounds
   * 
   * Stores depth bounds values.
   */
  struct DxvkDepthBounds {
    VkBool32            enableDepthBounds;
    float               minDepthBounds;
    float               maxDepthBounds;

    bool operator == (const DxvkDepthBounds& other) const {
      return enableDepthBounds == other.enableDepthBounds
          && minDepthBounds == other.minDepthBounds
          && maxDepthBounds == other.maxDepthBounds;
    }

    bool operator != (const DxvkDepthBounds& other) const {
      return enableDepthBounds != other.enableDepthBounds
          || minDepthBounds != other.minDepthBounds
          || maxDepthBounds != other.maxDepthBounds;
    }
  };
  
  
  /**
   * \brief Input assembly state
   * 
   * Stores the primitive topology and
   * whether or not primitive restart
   * is enabled.
   */
  struct DxvkInputAssemblyState {
    VkPrimitiveTopology primitiveTopology;
    VkBool32            primitiveRestart;
    uint32_t            patchVertexCount;
  };
  
  
  /**
   * \brief Rasterizer state
   * 
   * Stores the operating mode of the
   * rasterizer, including the depth bias.
   */
  struct DxvkRasterizerState {
    VkPolygonMode       polygonMode;
    VkCullModeFlags     cullMode;
    VkFrontFace         frontFace;
    VkBool32            depthClipEnable;
    VkBool32            depthBiasEnable;
    VkConservativeRasterizationModeEXT conservativeMode;
    VkSampleCountFlags  sampleCount;
    VkBool32            flatShading;
    VkLineRasterizationModeEXT lineMode;
  };
  
  
  /**
   * \brief Multisample state
   * 
   * Defines how to handle certain
   * aspects of multisampling.
   */
  struct DxvkMultisampleState {
    uint32_t            sampleMask;
    VkBool32            enableAlphaToCoverage;
  };
  

  /**
   * \brief Stencil operation
   */
  class DxvkStencilOp {

  public:

    VkStencilOp failOp() const {
      return VkStencilOp(m_failOp);
    }

    VkStencilOp passOp() const {
      return VkStencilOp(m_passOp);
    }

    VkStencilOp depthFailOp() const {
      return VkStencilOp(m_depthFailOp);
    }

    VkCompareOp compareOp() const {
      return VkCompareOp(m_compareOp);
    }

    uint8_t compareMask() const {
      return m_compareMask;
    }

    uint8_t writeMask() const {
      return m_writeMask;
    }

    void setFailOp(VkStencilOp op) {
      m_failOp = uint16_t(op);
    }

    void setPassOp(VkStencilOp op) {
      m_passOp = uint16_t(op);
    }

    void setDepthFailOp(VkStencilOp op) {
      m_depthFailOp = uint16_t(op);
    }

    void setCompareOp(VkCompareOp op) {
      m_compareOp = uint16_t(op);
    }

    void setCompareMask(uint8_t mask) {
      m_compareMask = mask;
    }

    void setWriteMask(uint8_t mask) {
      m_writeMask = mask;
    }

  private:

    uint16_t m_failOp            : 3;
    uint16_t m_passOp            : 3;
    uint16_t m_depthFailOp       : 3;
    uint16_t m_compareOp         : 3;
    uint16_t m_reserved          : 4;
    uint8_t  m_compareMask;
    uint8_t  m_writeMask;

  };


  /**
   * \brief Depth-stencil state
   * 
   * Defines the depth test and stencil
   * operations for the graphics pipeline.
   */
  class DxvkDepthStencilState {

  public:

    bool depthTest() const {
      return m_enableDepthTest;
    }

    bool depthWrite() const {
      return m_enableDepthWrite;
    }

    bool stencilTest() const {
      return m_enableStencilTest;
    }

    VkCompareOp depthCompareOp() const {
      return VkCompareOp(m_depthCompareOp);
    }

    DxvkStencilOp stencilOpFront() const {
      return m_stencilOpFront;
    }

    DxvkStencilOp stencilOpBack() const {
      return m_stencilOpBack;
    }

    void setDepthTest(bool depthTest) {
      m_enableDepthTest = depthTest;
    }

    void setDepthWrite(bool depthWrite) {
      m_enableDepthWrite = depthWrite;
    }

    void setStencilTest(bool stencilTest) {
      m_enableStencilTest = stencilTest;
    }

    void setDepthCompareOp(VkCompareOp compareOp) {
      m_depthCompareOp = uint16_t(compareOp);
    }

    void setStencilOpFront(DxvkStencilOp op) {
      m_stencilOpFront = op;
    }

    void setStencilOpBack(DxvkStencilOp op) {
      m_stencilOpBack = op;
    }

  private:

    uint16_t m_enableDepthTest   : 1;
    uint16_t m_enableDepthWrite  : 1;
    uint16_t m_enableStencilTest : 1;
    uint16_t m_depthCompareOp    : 3;
    uint16_t m_reserved          : 10;
    DxvkStencilOp m_stencilOpFront;
    DxvkStencilOp m_stencilOpBack;

  };
  
  
  /**
   * \brief Logic op state
   * Defines a logic op.
   */
  struct DxvkLogicOpState {
    VkBool32  enableLogicOp;
    VkLogicOp logicOp;
  };
  
  
  /**
   * \brief Blend mode for a single attachment
   * 
   * Stores the blend state for a single color attachment.
   * Blend modes can be set separately for each attachment.
   */
  struct DxvkBlendMode {
    VkBool32              enableBlending;
    VkBlendFactor         colorSrcFactor;
    VkBlendFactor         colorDstFactor;
    VkBlendOp             colorBlendOp;
    VkBlendFactor         alphaSrcFactor;
    VkBlendFactor         alphaDstFactor;
    VkBlendOp             alphaBlendOp;
    VkColorComponentFlags writeMask;
  };
  
  
  /**
   * \brief Vertex attribute description
   * 
   * Stores information about a
   * single vertex attribute.
   */
  struct DxvkVertexAttribute {
    uint32_t location;
    uint32_t binding;
    VkFormat format;
    uint32_t offset;
  };
  
  
  /**
   * \brief Vertex binding description
   * 
   * Stores information about a
   * single vertex binding slot.
   */
  struct DxvkVertexBinding {
    uint32_t          binding;
    uint32_t          fetchRate;
    VkVertexInputRate inputRate;
    uint32_t          extent;
  };
  
  
  /**
   * \brief Input layout
   * 
   * Stores the description of all active
   * vertex attributes and vertex bindings.
   */
  struct DxvkInputLayout {
    uint32_t numAttributes;
    uint32_t numBindings;
    
    std::array<DxvkVertexAttribute, DxvkLimits::MaxNumVertexAttributes> attributes;
    std::array<DxvkVertexBinding,   DxvkLimits::MaxNumVertexBindings>   bindings;
  };

}
