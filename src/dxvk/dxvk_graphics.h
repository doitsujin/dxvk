#pragma once

#include <vector>

#include "dxvk_binding.h"
#include "dxvk_constant_state.h"
#include "dxvk_pipecache.h"
#include "dxvk_pipelayout.h"
#include "dxvk_resource.h"
#include "dxvk_shader.h"
#include "dxvk_stats.h"

namespace dxvk {
  
  class DxvkDevice;
  
  /**
   * \brief Graphics pipeline state info
   * 
   * Stores all information that is required to create
   * a graphics pipeline, except the shader objects
   * themselves. Also used to identify pipelines using
   * the current pipeline state vector.
   */
  struct DxvkGraphicsPipelineStateInfo {
    DxvkGraphicsPipelineStateInfo();
    DxvkGraphicsPipelineStateInfo(
      const DxvkGraphicsPipelineStateInfo& other);
    
    DxvkGraphicsPipelineStateInfo& operator = (
      const DxvkGraphicsPipelineStateInfo& other);
    
    bool operator == (const DxvkGraphicsPipelineStateInfo& other) const;
    bool operator != (const DxvkGraphicsPipelineStateInfo& other) const;
    
    DxvkBindingState                    bsBindingState;
    
    VkPrimitiveTopology                 iaPrimitiveTopology;
    VkBool32                            iaPrimitiveRestart;
    uint32_t                            iaPatchVertexCount;
    
    uint32_t                            ilAttributeCount;
    uint32_t                            ilBindingCount;
    VkVertexInputAttributeDescription   ilAttributes[DxvkLimits::MaxNumVertexAttributes];
    VkVertexInputBindingDescription     ilBindings[DxvkLimits::MaxNumVertexBindings];
    
    VkBool32                            rsEnableDepthClamp;
    VkBool32                            rsEnableDiscard;
    VkPolygonMode                       rsPolygonMode;
    VkCullModeFlags                     rsCullMode;
    VkFrontFace                         rsFrontFace;
    VkBool32                            rsDepthBiasEnable;
    float                               rsDepthBiasConstant;
    float                               rsDepthBiasClamp;
    float                               rsDepthBiasSlope;
    uint32_t                            rsViewportCount;
    
    VkSampleCountFlagBits               msSampleCount;
    uint32_t                            msSampleMask;
    VkBool32                            msEnableAlphaToCoverage;
    VkBool32                            msEnableAlphaToOne;
    
    VkBool32                            dsEnableDepthTest;
    VkBool32                            dsEnableDepthWrite;
    VkBool32                            dsEnableDepthBounds;
    VkBool32                            dsEnableStencilTest;
    VkCompareOp                         dsDepthCompareOp;
    VkStencilOpState                    dsStencilOpFront;
    VkStencilOpState                    dsStencilOpBack;
    float                               dsDepthBoundsMin;
    float                               dsDepthBoundsMax;
    
    VkBool32                            omEnableLogicOp;
    VkLogicOp                           omLogicOp;
    VkRenderPass                        omRenderPass;
    VkPipelineColorBlendAttachmentState omBlendAttachments[MaxNumRenderTargets];
  };
  
  
  /**
   * \brief Common graphics pipeline state
   * 
   * Non-dynamic pipeline state that cannot
   * be changed dynamically.
   */
  struct DxvkGraphicsCommonPipelineStateInfo {
    bool                                msSampleShadingEnable;
    float                               msSampleShadingFactor;
  };
  
  
  /**
   * \brief Graphics pipeline
   * 
   * Stores the pipeline layout as well as methods to
   * recompile the graphics pipeline against a given
   * pipeline state vector.
   */
  class DxvkGraphicsPipeline : public DxvkResource {
    
  public:
    
    DxvkGraphicsPipeline(
      const DxvkDevice*             device,
      const Rc<DxvkPipelineCache>&  cache,
      const Rc<DxvkShader>&         vs,
      const Rc<DxvkShader>&         tcs,
      const Rc<DxvkShader>&         tes,
      const Rc<DxvkShader>&         gs,
      const Rc<DxvkShader>&         fs);
    ~DxvkGraphicsPipeline();
    
    /**
     * \brief Pipeline layout
     * 
     * Stores the pipeline layout and the descriptor set
     * layout, as well as information on the resource
     * slots used by the pipeline.
     * \returns Pipeline layout
     */
    Rc<DxvkPipelineLayout> layout() const {
      return m_layout;
    }
    
    /**
     * \brief Pipeline handle
     * 
     * Retrieves a pipeline handle for the given pipeline
     * state. If necessary, a new pipeline will be created.
     * \param [in] state Pipeline state vector
     * \param [in,out] stats Stat counter
     * \returns Pipeline handle
     */
    VkPipeline getPipelineHandle(
      const DxvkGraphicsPipelineStateInfo& state,
            DxvkStatCounters&              stats);
    
  private:
    
    struct PipelineStruct {
      DxvkGraphicsPipelineStateInfo stateVector;
      VkPipeline                    pipeline;
    };
    
    const DxvkDevice* const m_device;
    const Rc<vk::DeviceFn>  m_vkd;
    
    Rc<DxvkPipelineCache>   m_cache;
    Rc<DxvkPipelineLayout>  m_layout;
    
    Rc<DxvkShaderModule>  m_vs;
    Rc<DxvkShaderModule>  m_tcs;
    Rc<DxvkShaderModule>  m_tes;
    Rc<DxvkShaderModule>  m_gs;
    Rc<DxvkShaderModule>  m_fs;
    
    uint32_t m_vsIn  = 0;
    uint32_t m_fsOut = 0;
    
    DxvkGraphicsCommonPipelineStateInfo m_common;
    
    std::vector<PipelineStruct> m_pipelines;
    
    VkPipeline m_basePipeline = VK_NULL_HANDLE;
    
    VkPipeline compilePipeline(
      const DxvkGraphicsPipelineStateInfo& state,
            VkPipeline                     baseHandle) const;
    
    void destroyPipelines();
    
    bool validatePipelineState(
      const DxvkGraphicsPipelineStateInfo& state) const;
    
    void logPipelineState(
            LogLevel                       level,
      const DxvkGraphicsPipelineStateInfo& state) const;
    
  };
  
}