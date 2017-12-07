#pragma once

#include "dxvk_buffer.h"
#include "dxvk_framebuffer.h"
#include "dxvk_limits.h"
#include "dxvk_resource.h"
#include "dxvk_shader.h"

namespace dxvk {
  
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
  };
  
  
  /**
   * \brief Rasterizer state
   * 
   * Stores the operating mode of the
   * rasterizer, including the depth bias.
   */
  struct DxvkRasterizerState {
    VkBool32            enableDepthClamp;
    VkBool32            enableDiscard;
    VkPolygonMode       polygonMode;
    VkCullModeFlags     cullMode;
    VkFrontFace         frontFace;
    VkBool32            depthBiasEnable;
    float               depthBiasConstant;
    float               depthBiasClamp;
    float               depthBiasSlope;
  };
  
  
  /**
   * \brief Multisample state
   * 
   * Defines how to handle certain
   * aspects of multisampling.
   */
  struct DxvkMultisampleState {
    VkBool32                enableAlphaToCoverage;
    VkBool32                enableAlphaToOne;
    VkBool32                enableSampleShading;
    float                   minSampleShading;
  };
  
  
  /**
   * \brief Depth-stencil state
   * 
   * Defines the depth test and stencil
   * operations for the graphics pipeline.
   */
  struct DxvkDepthStencilState {
    VkBool32            enableDepthTest;
    VkBool32            enableDepthWrite;
    VkBool32            enableDepthBounds;
    VkBool32            enableStencilTest;
    VkCompareOp         depthCompareOp;
    VkStencilOpState    stencilOpFront;
    VkStencilOpState    stencilOpBack;
    float               depthBoundsMin;
    float               depthBoundsMax;
  };
  
  
  /**
   * \brief Blend state
   * 
   * Stores the color blend state for each
   * available framebuffer attachment.
   */
  class DxvkBlendState : public RcObject {
    
  public:
    
    DxvkBlendState(
            VkBool32                             enableLogicOp,
            VkLogicOp                            logicOp,
            uint32_t                             attachmentCount,
      const VkPipelineColorBlendAttachmentState* attachmentState);
    
    const VkPipelineColorBlendStateCreateInfo& info() const {
      return m_info;
    }
    
  private:
    
    std::array<VkPipelineColorBlendAttachmentState,
      DxvkLimits::MaxNumRenderTargets> m_attachments;
    
    VkPipelineColorBlendStateCreateInfo m_info;
    
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
    VkVertexInputRate inputRate;
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
  
  
  struct DxvkConstantStateObjects {
    Rc<DxvkBlendState>          blendState;
  };
  
}