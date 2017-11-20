#include "dxvk_constant_state.h"

namespace dxvk {
  
  DxvkInputAssemblyState::DxvkInputAssemblyState(
          VkPrimitiveTopology primitiveTopology,
          VkBool32            primitiveRestart) {
    m_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    m_info.pNext                  = nullptr;
    m_info.flags                  = 0;
    m_info.topology               = primitiveTopology;
    m_info.primitiveRestartEnable = primitiveRestart;
  }
  
  
  DxvkRasterizerState::DxvkRasterizerState(
          VkBool32          enableDepthClamp,
          VkBool32          enableDiscard,
          VkPolygonMode     polygonMode,
          VkCullModeFlags   cullMode,
          VkFrontFace       frontFace,
          VkBool32          depthBiasEnable,
          float             depthBiasConstant,
          float             depthBiasClamp,
          float             depthBiasSlope,
          float             lineWidth) {
    m_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    m_info.pNext                  = nullptr;
    m_info.flags                  = 0;
    m_info.depthClampEnable       = enableDepthClamp;
    m_info.rasterizerDiscardEnable= enableDiscard;
    m_info.polygonMode            = polygonMode;
    m_info.cullMode               = cullMode;
    m_info.frontFace              = frontFace;
    m_info.depthBiasEnable        = depthBiasEnable;
    m_info.depthBiasConstantFactor= depthBiasConstant;
    m_info.depthBiasClamp         = depthBiasClamp;
    m_info.depthBiasSlopeFactor   = depthBiasSlope;
    m_info.lineWidth              = lineWidth;
  }
  
  
  DxvkMultisampleState::DxvkMultisampleState(
          VkSampleCountFlagBits   sampleCount,
          uint32_t                sampleMask,
          VkBool32                enableAlphaToCoverage,
          VkBool32                enableAlphaToOne,
          VkBool32                enableSampleShading,
          float                   minSampleShading) {
    m_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    m_info.pNext                  = nullptr;
    m_info.flags                  = 0;
    m_info.rasterizationSamples   = sampleCount;
    m_info.sampleShadingEnable    = enableSampleShading;
    m_info.minSampleShading       = minSampleShading;
    m_info.pSampleMask            = &m_mask;
    m_info.alphaToCoverageEnable  = enableAlphaToCoverage;
    m_info.alphaToOneEnable       = enableAlphaToOne;
    
    m_mask = sampleMask;
  }
  
  
  DxvkDepthStencilState::DxvkDepthStencilState(
        VkBool32            enableDepthTest,
        VkBool32            enableDepthWrite,
        VkBool32            enableDepthBounds,
        VkBool32            enableStencilTest,
        VkCompareOp         depthCompareOp,
        VkStencilOpState    stencilOpFront,
        VkStencilOpState    stencilOpBack,
        float               depthBoundsMin,
        float               depthBoundsMax) {
    m_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    m_info.pNext                  = nullptr;
    m_info.flags                  = 0;
    m_info.depthTestEnable        = enableDepthTest;
    m_info.depthWriteEnable       = enableDepthWrite;
    m_info.depthCompareOp         = depthCompareOp;
    m_info.depthBoundsTestEnable  = enableDepthBounds;
    m_info.stencilTestEnable      = enableStencilTest;
    m_info.front                  = stencilOpFront;
    m_info.back                   = stencilOpBack;
    m_info.minDepthBounds         = depthBoundsMin;
    m_info.maxDepthBounds         = depthBoundsMax;
  }
  
  
  DxvkBlendState::DxvkBlendState(
          VkBool32                             enableLogicOp,
          VkLogicOp                            logicOp,
          uint32_t                             attachmentCount,
    const VkPipelineColorBlendAttachmentState* attachmentState) {
    // Copy the provided blend states into the array
    for (uint32_t i = 0; i < attachmentCount; i++)
      m_attachments.at(i) = attachmentState[i];
    
    // Use default values for the remaining attachments
    for (uint32_t i = attachmentCount; i < m_attachments.size(); i++) {
      m_attachments.at(i).blendEnable         = VK_FALSE;
      m_attachments.at(i).srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
      m_attachments.at(i).dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
      m_attachments.at(i).colorBlendOp        = VK_BLEND_OP_ADD;
      m_attachments.at(i).srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      m_attachments.at(i).dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      m_attachments.at(i).alphaBlendOp        = VK_BLEND_OP_ADD;
      m_attachments.at(i).colorWriteMask      =
          VK_COLOR_COMPONENT_R_BIT
        | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT
        | VK_COLOR_COMPONENT_A_BIT;
    }
    
    m_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    m_info.pNext                  = nullptr;
    m_info.flags                  = 0;
    m_info.logicOpEnable          = enableLogicOp;
    m_info.logicOp                = logicOp;
    m_info.attachmentCount        = m_attachments.size();
    m_info.pAttachments           = m_attachments.data();
    
    for (uint32_t i = 0; i < 4; i++)
      m_info.blendConstants[i]    = 0.0f;
  }
  
  
  DxvkInputLayout::DxvkInputLayout(
          uint32_t                           attributeCount,
    const VkVertexInputAttributeDescription* attributeInfo,
          uint32_t                           bindingCount,
    const VkVertexInputBindingDescription*   bindingInfo) {
    // Copy vertex attribute info to a persistent array
    m_attributes.resize(attributeCount);
    for (uint32_t i = 0; i < attributeCount; i++)
      m_attributes.at(i) = attributeInfo[i];
    
    // Copy vertex binding info to a persistent array
    m_bindings.resize(bindingCount);
    for (uint32_t i = 0; i < bindingCount; i++)
      m_bindings.at(i) = bindingInfo[i];
    
    // Create info structure referencing those arrays
    m_info.sType                            = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    m_info.pNext                            = nullptr;
    m_info.flags                            = 0;
    m_info.vertexBindingDescriptionCount    = m_bindings.size();
    m_info.pVertexBindingDescriptions       = m_bindings.data();
    m_info.vertexAttributeDescriptionCount  = m_attributes.size();
    m_info.pVertexAttributeDescriptions     = m_attributes.data();
  }
  
}