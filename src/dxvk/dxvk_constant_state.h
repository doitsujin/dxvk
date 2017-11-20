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
  class DxvkInputAssemblyState : public RcObject {
    
  public:
    
    DxvkInputAssemblyState(
      VkPrimitiveTopology primitiveTopology,
      VkBool32            primitiveRestart);
    
    const VkPipelineInputAssemblyStateCreateInfo& info() const {
      return m_info;
    }
    
  private:
    
    VkPipelineInputAssemblyStateCreateInfo m_info;
    
  };
  
  
  /**
   * \brief Rasterizer state
   * 
   * Stores the operating mode of the
   * rasterizer, including the depth bias.
   */
  class DxvkRasterizerState : public RcObject {
    
  public:
    
    DxvkRasterizerState(
      VkBool32          enableDepthClamp,
      VkBool32          enableDiscard,
      VkPolygonMode     polygonMode,
      VkCullModeFlags   cullMode,
      VkFrontFace       frontFace,
      VkBool32          depthBiasEnable,
      float             depthBiasConstant,
      float             depthBiasClamp,
      float             depthBiasSlope,
      float             lineWidth);
    
    const VkPipelineRasterizationStateCreateInfo& info() const {
      return m_info;
    }
    
  private:
    
    VkPipelineRasterizationStateCreateInfo m_info;
    
  };
  
  
  /**
   * \brief Multisample state
   * 
   * Defines details on how to handle
   * multisampling, including the alpha
   * coverage mode.
   */
  class DxvkMultisampleState : public RcObject {
    
  public:
    
    DxvkMultisampleState(
      VkSampleCountFlagBits   sampleCount,
      uint32_t                sampleMask,
      VkBool32                enableAlphaToCoverage,
      VkBool32                enableAlphaToOne,
      VkBool32                enableSampleShading,
      float                   minSampleShading);
    
    const VkPipelineMultisampleStateCreateInfo& info() const {
      return m_info;
    }
    
  private:
    
    VkPipelineMultisampleStateCreateInfo m_info;
    uint32_t                             m_mask;
    
  };
  
  
  /**
   * \brief Depth-stencil state
   * 
   * Defines the depth test and stencil
   * operations for the graphics pipeline.
   */
  class DxvkDepthStencilState : public RcObject {
    
  public:
    
    DxvkDepthStencilState(
      VkBool32            enableDepthTest,
      VkBool32            enableDepthWrite,
      VkBool32            enableDepthBounds,
      VkBool32            enableStencilTest,
      VkCompareOp         depthCompareOp,
      VkStencilOpState    stencilOpFront,
      VkStencilOpState    stencilOpBack,
      float               depthBoundsMin,
      float               depthBoundsMax);
    
    const VkPipelineDepthStencilStateCreateInfo& info() const {
      return m_info;
    }
    
  private:
    
    VkPipelineDepthStencilStateCreateInfo m_info;
    
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
    
    std::vector<VkPipelineColorBlendAttachmentState> m_attachments;
    
    VkPipelineColorBlendStateCreateInfo m_info;
    
  };
  
  
  /**
   * \brief Input layout
   * 
   * Stores the attributes and vertex buffer binding
   * descriptions that the vertex shader will take
   * its input values from.
   */
  class DxvkInputLayout : public RcObject {
    
  public:
    
    DxvkInputLayout(
            uint32_t                           attributeCount,
      const VkVertexInputAttributeDescription* attributeInfo,
            uint32_t                           bindingCount,
      const VkVertexInputBindingDescription*   bindingInfo);
    
    const VkPipelineVertexInputStateCreateInfo& info() const {
      return m_info;
    }
    
  private:
    
    std::vector<VkVertexInputAttributeDescription> m_attributes;
    std::vector<VkVertexInputBindingDescription>   m_bindings;
    
    VkPipelineVertexInputStateCreateInfo m_info;
    
  };
  
  
  struct DxvkConstantStateObjects {
    Rc<DxvkInputAssemblyState>  inputAssembly;
    Rc<DxvkInputLayout>         inputLayout;
    Rc<DxvkRasterizerState>     rasterizerState;
    Rc<DxvkMultisampleState>    multisampleState;
    Rc<DxvkDepthStencilState>   depthStencilState;
    Rc<DxvkBlendState>          blendState;
  };
  
}