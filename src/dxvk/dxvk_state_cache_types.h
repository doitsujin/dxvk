#pragma once

#include "dxvk_pipemanager.h"
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
    DxvkShaderKey cs;

    bool eq(const DxvkStateCacheKey& key) const;

    size_t hash() const;
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
    DxvkStateCacheKey             shaders;
    DxvkGraphicsPipelineStateInfo gpState;
    DxvkComputePipelineStateInfo  cpState;
    DxvkRenderPassFormat          format;
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
    uint32_t version    = 5;
    uint32_t entrySize  = sizeof(DxvkStateCacheEntry);
  };

  static_assert(sizeof(DxvkStateCacheHeader) == 12);


  /**
   * \brief Version 4 graphics pipeline state
   */
  struct DxvkGraphicsPipelineStateInfoV4 {
    DxvkBindingMask                     bsBindingMask;
    
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
    
    VkCompareOp                         xsAlphaCompareOp;
    
    VkBool32                            dsEnableDepthTest;
    VkBool32                            dsEnableDepthWrite;
    VkBool32                            dsEnableStencilTest;
    VkCompareOp                         dsDepthCompareOp;
    VkStencilOpState                    dsStencilOpFront;
    VkStencilOpState                    dsStencilOpBack;
    
    VkBool32                            omEnableLogicOp;
    VkLogicOp                           omLogicOp;
    VkPipelineColorBlendAttachmentState omBlendAttachments[MaxNumRenderTargets];
    VkComponentMapping                  omComponentMapping[MaxNumRenderTargets];
  };


  /**
   * \brief Version 4 state cache entry
   */
  struct DxvkStateCacheEntryV4 {
    DxvkStateCacheKey               shaders;
    DxvkGraphicsPipelineStateInfoV4 gpState;
    DxvkComputePipelineStateInfo    cpState;
    DxvkRenderPassFormat            format;
    Sha1Hash                        hash;
  };

}
