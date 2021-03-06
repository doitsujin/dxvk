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
    uint32_t version    = 10;
    uint32_t entrySize  = 0; /* no longer meaningful */
  };

  static_assert(sizeof(DxvkStateCacheHeader) == 12);


  class DxvkBindingMaskV8 : DxvkBindingSet<128> {

  public:

    DxvkBindingMask convert() const {
      DxvkBindingMask result = { };
      for (uint32_t i = 0; i < 128; i++)
        result.set(i, test(i));
      return result;
    }

  };

  class DxvkIlBindingV9 {

  public:

    uint32_t m_binding                : 5;
    uint32_t m_stride                 : 12;
    uint32_t m_inputRate              : 1;
    uint32_t m_reserved               : 14;
    uint32_t m_divisor;

    DxvkIlBinding convert() const {
      return DxvkIlBinding(m_binding, m_stride,
        VkVertexInputRate(m_inputRate), m_divisor);
    }

  };

  /**
   * \brief Version 4 graphics pipeline state
   */
  struct DxvkGraphicsPipelineStateInfoV4 {
    DxvkBindingMaskV8                   bsBindingMask;
    
    VkPrimitiveTopology                 iaPrimitiveTopology;
    VkBool32                            iaPrimitiveRestart;
    uint32_t                            iaPatchVertexCount;
    
    uint32_t                            ilAttributeCount;
    uint32_t                            ilBindingCount;
    VkVertexInputAttributeDescription   ilAttributes[32];
    VkVertexInputBindingDescription     ilBindings[32];
    uint32_t                            ilDivisors[32];
    
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
    VkPipelineColorBlendAttachmentState omBlendAttachments[8];
    VkComponentMapping                  omComponentMapping[8];
  };


  /**
   * \brief Version 6 graphics pipeline state
   */
  struct DxvkGraphicsPipelineStateInfoV6 {
    DxvkBindingMaskV8                   bsBindingMask;
    
    VkPrimitiveTopology                 iaPrimitiveTopology;
    VkBool32                            iaPrimitiveRestart;
    uint32_t                            iaPatchVertexCount;
    
    uint32_t                            ilAttributeCount;
    uint32_t                            ilBindingCount;
    VkVertexInputAttributeDescription   ilAttributes[32];
    VkVertexInputBindingDescription     ilBindings[32];
    uint32_t                            ilDivisors[32];
    
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
    VkPipelineColorBlendAttachmentState omBlendAttachments[8];
    VkComponentMapping                  omComponentMapping[8];

    uint32_t                            scSpecConstants[8];
  };


  /**
   * \brief Version 5 compute pipeline state
   */
  struct DxvkComputePipelineStateInfoV5 {
    DxvkBindingMaskV8                   bsBindingMask;
  };


  /**
   * \brief Version 6 compute pipeline state
   */
  struct DxvkComputePipelineStateInfoV6 {
    DxvkBindingMaskV8                   bsBindingMask;
    uint32_t                            scSpecConstants[8];
  };


  /**
   * \brief Version 4 state cache entry
   */
  struct DxvkStateCacheEntryV4 {
    DxvkStateCacheKey               shaders;
    DxvkGraphicsPipelineStateInfoV4 gpState;
    DxvkComputePipelineStateInfoV5  cpState;
    DxvkRenderPassFormat            format;
    Sha1Hash                        hash;
  };


  /**
   * \brief Version 5 state cache entry
   */
  struct DxvkStateCacheEntryV5 {
    DxvkStateCacheKey               shaders;
    DxvkGraphicsPipelineStateInfoV6 gpState;
    DxvkComputePipelineStateInfoV5  cpState;
    DxvkRenderPassFormat            format;
    Sha1Hash                        hash;
  };


  /**
   * \brief Version 6 state cache entry
   */
  struct DxvkStateCacheEntryV6 {
    DxvkStateCacheKey               shaders;
    DxvkGraphicsPipelineStateInfoV6 gpState;
    DxvkComputePipelineStateInfoV6  cpState;
    DxvkRenderPassFormat            format;
    Sha1Hash                        hash;
  };

}
