#pragma once

#include "dxvk_limits.h"

#include <cstring>

namespace dxvk {

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
    
    VkPrimitiveTopology                 iaPrimitiveTopology;
    VkBool32                            iaPrimitiveRestart;
    uint32_t                            iaPatchVertexCount;
    
    uint32_t                            ilAttributeCount;
    uint32_t                            ilBindingCount;
    VkVertexInputAttributeDescription   ilAttributes[DxvkLimits::MaxNumVertexAttributes];
    VkVertexInputBindingDescription     ilBindings[DxvkLimits::MaxNumVertexBindings];
    uint32_t                            ilDivisors[DxvkLimits::MaxNumVertexBindings];
    
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
  };

}
