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

  public:

    DxvkInputAssemblyState() = default;

    DxvkInputAssemblyState(VkPrimitiveTopology topology, bool restart)
    : m_primitiveTopology (uint16_t(topology)),
      m_primitiveRestart  (uint16_t(restart)),
      m_patchVertexCount  (0u),
      m_reserved          (0u) { }

    VkPrimitiveTopology primitiveTopology() const {
      return VkPrimitiveTopology(m_primitiveTopology) <= VK_PRIMITIVE_TOPOLOGY_PATCH_LIST
        ? VkPrimitiveTopology(m_primitiveTopology)
        : VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    }

    bool primitiveRestart() const {
      return m_primitiveRestart;
    }

    uint32_t patchVertexCount() const {
      return m_patchVertexCount;
    }

    void setPrimitiveTopology(VkPrimitiveTopology topology) {
      m_primitiveTopology = uint16_t(topology);
    }

    void setPrimitiveRestart(bool enable) {
      m_primitiveRestart = enable;
    }

    void setPatchVertexCount(uint32_t count) {
      m_patchVertexCount = count;
    }

  private:

    uint16_t m_primitiveTopology  : 4;
    uint16_t m_primitiveRestart   : 1;
    uint16_t m_patchVertexCount   : 6;
    uint16_t m_reserved           : 5;

  };


  /**
   * \brief Rasterizer state
   * 
   * Stores the operating mode of the
   * rasterizer, including the depth bias.
   */
  struct DxvkRasterizerState {

  public:

    VkPolygonMode polygonMode() const {
      return VkPolygonMode(m_polygonMode);
    }

    VkCullModeFlags cullMode() const {
      return VkCullModeFlags(m_cullMode);
    }

    VkFrontFace frontFace() const {
      return VkFrontFace(m_frontFace);
    }

    bool depthClip() const {
      return m_depthClipEnable;
    }

    bool depthBias() const {
      return m_depthBiasEnable;
    }

    VkConservativeRasterizationModeEXT conservativeMode() const {
      return VkConservativeRasterizationModeEXT(m_conservativeMode);
    }

    VkSampleCountFlags sampleCount() const {
      return VkSampleCountFlags(m_sampleCount);
    }

    bool flatShading() const {
      return m_flatShading;
    }

    VkLineRasterizationModeEXT lineMode() const {
      return VkLineRasterizationModeEXT(m_lineMode);
    }

    void setPolygonMode(VkPolygonMode mode) {
      m_polygonMode = uint32_t(mode);
    }

    void setCullMode(VkCullModeFlags mode) {
      m_cullMode = uint32_t(mode);
    }

    void setFrontFace(VkFrontFace face) {
      m_frontFace = uint32_t(face);
    }

    void setDepthClip(bool enable) {
      m_depthClipEnable = enable;
    }

    void setDepthBias(bool enable) {
      m_depthBiasEnable = enable;
    }

    void setConservativeMode(VkConservativeRasterizationModeEXT mode) {
      m_conservativeMode = uint32_t(mode);
    }

    void setSampleCount(VkSampleCountFlags count) {
      m_sampleCount = uint32_t(count);
    }

    void setFlatShading(bool enable) {
      m_flatShading = enable;
    }

    void setLineMode(VkLineRasterizationModeEXT mode) {
      m_lineMode = uint32_t(mode);
    }

  private:

    uint32_t m_polygonMode       : 2;
    uint32_t m_cullMode          : 2;
    uint32_t m_frontFace         : 1;
    uint32_t m_depthClipEnable   : 1;
    uint32_t m_depthBiasEnable   : 1;
    uint32_t m_conservativeMode  : 2;
    uint32_t m_sampleCount       : 5;
    uint32_t m_flatShading       : 1;
    uint32_t m_lineMode          : 2;
    uint32_t m_reserved          : 15;

  };
  
  
  /**
   * \brief Multisample state
   * 
   * Defines how to handle certain
   * aspects of multisampling.
   */
  class DxvkMultisampleState {

  public:

    uint16_t sampleMask() const {
      return m_sampleMask;
    }

    bool alphaToCoverage() const {
      return m_enableAlphaToCoverage;
    }

    void setSampleMask(uint16_t mask) {
      m_sampleMask = mask;
    }

    void setAlphaToCoverage(bool alphaToCoverage) {
      m_enableAlphaToCoverage = alphaToCoverage;
    }

  private:

    uint16_t m_sampleMask;
    uint16_t m_enableAlphaToCoverage : 1;
    uint16_t m_reserved              : 15;

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

    bool normalize(VkCompareOp depthOp);

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

    void normalize();

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
  class DxvkLogicOpState {

  public:

    bool logicOpEnable() const {
      return m_logicOpEnable;
    }

    VkLogicOp logicOp() const {
      return VkLogicOp(m_logicOp);
    }

    void setLogicOp(bool enable, VkLogicOp op) {
      m_logicOpEnable = enable;
      m_logicOp = uint8_t(op);
    }

  private:

    uint8_t m_logicOpEnable : 1;
    uint8_t m_logicOp       : 4;
    uint8_t m_reserved      : 3;

  };
  
  
  /**
   * \brief Blend mode for a single attachment
   * 
   * Stores the blend state for a single color attachment.
   * Blend modes can be set separately for each attachment.
   */
  class DxvkBlendMode {

  public:

    bool blendEnable() const {
      return m_enableBlending;
    }

    VkBlendFactor colorSrcFactor() const {
      return VkBlendFactor(m_colorSrcFactor);
    }

    VkBlendFactor colorDstFactor() const {
      return VkBlendFactor(m_colorDstFactor);
    }

    VkBlendOp colorBlendOp() const {
      return VkBlendOp(m_colorBlendOp);
    }

    VkBlendFactor alphaSrcFactor() const {
      return VkBlendFactor(m_alphaSrcFactor);
    }

    VkBlendFactor alphaDstFactor() const {
      return VkBlendFactor(m_alphaDstFactor);
    }

    VkBlendOp alphaBlendOp() const {
      return VkBlendOp(m_alphaBlendOp);
    }

    VkColorComponentFlags writeMask() const {
      return VkColorComponentFlags(m_writeMask);
    }

    void setBlendEnable(bool enable) {
      m_enableBlending = enable;
    }

    void setColorOp(VkBlendFactor srcFactor, VkBlendFactor dstFactor, VkBlendOp op) {
      m_colorSrcFactor = uint32_t(srcFactor);
      m_colorDstFactor = uint32_t(dstFactor);
      m_colorBlendOp = uint32_t(op);
    }

    void setAlphaOp(VkBlendFactor srcFactor, VkBlendFactor dstFactor, VkBlendOp op) {
      m_alphaSrcFactor = uint32_t(srcFactor);
      m_alphaDstFactor = uint32_t(dstFactor);
      m_alphaBlendOp = uint32_t(op);
    }

    void setWriteMask(VkColorComponentFlags writeMask) {
      m_writeMask = writeMask;
    }

    void normalize();

  private:

    uint32_t m_enableBlending : 1;
    uint32_t m_colorSrcFactor : 5;
    uint32_t m_colorDstFactor : 5;
    uint32_t m_colorBlendOp   : 3;
    uint32_t m_alphaSrcFactor : 5;
    uint32_t m_alphaDstFactor : 5;
    uint32_t m_alphaBlendOp   : 3;
    uint32_t m_writeMask      : 4;
    uint32_t m_reserved       : 1;

  };
  
  
  /**
   * \brief Vertex attribute description
   */
  struct DxvkVertexAttribute {
    uint32_t location;
    uint32_t binding;
    VkFormat format;
    uint32_t offset;
  };


  /**
   * \brief Packed vertex attribute
   *
   * Compact representation of a vertex attribute.
   */
  struct DxvkPackedVertexAttribute {
    DxvkPackedVertexAttribute() = default;
    DxvkPackedVertexAttribute(const DxvkVertexAttribute& a)
    : location  (a.location),
      binding   (a.binding),
      format    (uint32_t(a.format)),
      offset    (a.offset),
      reserved  (0u) { }

    uint32_t location   : 5;
    uint32_t binding    : 5;
    uint32_t format     : 7;
    uint32_t offset     : 11;
    uint32_t reserved   : 4;

    DxvkVertexAttribute unpack() const {
      DxvkVertexAttribute result = { };
      result.location = location;
      result.binding = binding;
      result.format = VkFormat(format);
      result.offset = offset;
      return result;
    }
  };


  /**
   * \brief Vertex binding description
   */
  struct DxvkVertexBinding {
    uint32_t binding;
    uint32_t extent;
    VkVertexInputRate inputRate;
    uint32_t divisor;
  };


  /**
   * \brief Packed vertex attribute
   *
   * Compact representation of a vertex binding.
   */
  struct DxvkPackedVertexBinding {
    DxvkPackedVertexBinding() = default;
    DxvkPackedVertexBinding(const DxvkVertexBinding& b)
    : binding   (b.binding),
      extent    (b.extent),
      inputRate (uint32_t(b.inputRate)),
      divisor   (b.divisor < (1u << 14) ? b.divisor : 0u) { }

    uint32_t binding    : 5;
    uint32_t extent     : 12;
    uint32_t inputRate  : 1;
    uint32_t divisor    : 14;

    DxvkVertexBinding unpack() const {
      DxvkVertexBinding result = { };
      result.binding = binding;
      result.extent = extent;
      result.inputRate = VkVertexInputRate(inputRate);
      result.divisor = divisor;
      return result;
    }
  };

  /**
   * \brief Packed attribute binding
   *
   * Relies on attriute and bining
   * structures to have the same size.
   */
  class DxvkVertexInput {

  public:

    DxvkVertexInput() = default;

    DxvkVertexInput(const DxvkVertexAttribute& attribute)
    : m_attribute(attribute) { }

    DxvkVertexInput(const DxvkVertexBinding& binding)
    : m_binding(binding) { }

    DxvkVertexAttribute attribute() const {
      return m_attribute.unpack();
    }

    DxvkVertexBinding binding() const {
      return m_binding.unpack();
    }

  private:

    union {
      DxvkPackedVertexAttribute m_attribute;
      DxvkPackedVertexBinding   m_binding;
    };

  };

  static_assert(sizeof(DxvkVertexInput) == sizeof(uint32_t));

}
