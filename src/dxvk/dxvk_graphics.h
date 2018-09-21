#pragma once

#include <mutex>

#include "dxvk_bind_mask.h"
#include "dxvk_constant_state.h"
#include "dxvk_pipecache.h"
#include "dxvk_pipelayout.h"
#include "dxvk_renderpass.h"
#include "dxvk_resource.h"
#include "dxvk_shader.h"
#include "dxvk_stats.h"

namespace dxvk {
  
  class DxvkDevice;
  class DxvkPipelineManager;
  
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
    
    DxvkBindingMask                     bsBindingMask;
    
    VkPrimitiveTopology                 iaPrimitiveTopology;
    VkBool32                            iaPrimitiveRestart;
    uint32_t                            iaPatchVertexCount;
    
    uint32_t                            ilAttributeCount;
    uint32_t                            ilBindingCount;
    VkVertexInputAttributeDescription   ilAttributes[DxvkLimits::MaxNumVertexAttributes];
    VkVertexInputBindingDescription     ilBindings[DxvkLimits::MaxNumVertexBindings];
    uint32_t                            ilDivisors[DxvkLimits::MaxNumVertexBindings];
    
    VkBool32                            rsDepthClampEnable;
    VkBool32                            rsDepthBiasEnable;
    VkPolygonMode                       rsPolygonMode;
    VkCullModeFlags                     rsCullMode;
    VkFrontFace                         rsFrontFace;
    uint32_t                            rsViewportCount;
    VkSampleCountFlags                  rsSampleCount;
    
    VkSampleCountFlags                  msSampleCount;
    uint32_t                            msSampleMask;
    VkBool32                            msEnableAlphaToCoverage;
    VkBool32                            msEnableAlphaToOne;
    
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
   * \brief Graphics pipeline instance
   * 
   * Stores a state vector and the
   * corresponding pipeline handle.
   */
  class DxvkGraphicsPipelineInstance {

  public:

    DxvkGraphicsPipelineInstance() { }
    DxvkGraphicsPipelineInstance(
      const DxvkGraphicsPipelineStateInfo&  state,
            VkRenderPass                    rp,
            VkPipeline                      pipe)
    : m_stateVector (state),
      m_renderPass  (rp),
      m_pipeline    (pipe) { }

    /**
     * \brief Checks for matching pipeline state
     * 
     * \param [in] stateVector Graphics pipeline state
     * \param [in] renderPass Render pass handle
     * \returns \c true if the specialization is compatible
     */
    bool isCompatible(
      const DxvkGraphicsPipelineStateInfo&  state,
            VkRenderPass                    rp) const {
      return m_stateVector == state
          && m_renderPass  == rp;
    }

    /**
     * \brief Retrieves pipeline
     * \returns The pipeline handle
     */
    VkPipeline pipeline() const {
      return m_pipeline;
    }

  private:

    DxvkGraphicsPipelineStateInfo m_stateVector;
    VkRenderPass                  m_renderPass;
    VkPipeline                    m_pipeline;

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
            DxvkPipelineManager*      pipeMgr,
      const Rc<DxvkShader>&           vs,
      const Rc<DxvkShader>&           tcs,
      const Rc<DxvkShader>&           tes,
      const Rc<DxvkShader>&           gs,
      const Rc<DxvkShader>&           fs);
    ~DxvkGraphicsPipeline();
    
    /**
     * \brief Pipeline layout
     * 
     * Stores the pipeline layout and the descriptor set
     * layout, as well as information on the resource
     * slots used by the pipeline.
     * \returns Pipeline layout
     */
    DxvkPipelineLayout* layout() const {
      return m_layout.ptr();
    }
    
    /**
     * \brief Pipeline handle
     * 
     * Retrieves a pipeline handle for the given pipeline
     * state. If necessary, a new pipeline will be created.
     * \param [in] state Pipeline state vector
     * \param [in] renderPass The render pass
     * \returns Pipeline handle
     */
    VkPipeline getPipelineHandle(
      const DxvkGraphicsPipelineStateInfo&    state,
      const DxvkRenderPass&                   renderPass);
    
  private:
    
    struct PipelineStruct {
      DxvkGraphicsPipelineStateInfo stateVector;
      VkRenderPass                  renderPass;
      VkPipeline                    pipeline;
    };
    
    Rc<vk::DeviceFn>        m_vkd;
    DxvkPipelineManager*    m_pipeMgr;

    Rc<DxvkPipelineLayout>  m_layout;
    Rc<DxvkShaderModule>    m_vs;
    Rc<DxvkShaderModule>    m_tcs;
    Rc<DxvkShaderModule>    m_tes;
    Rc<DxvkShaderModule>    m_gs;
    Rc<DxvkShaderModule>    m_fs;
    
    uint32_t m_vsIn  = 0;
    uint32_t m_fsOut = 0;
    
    DxvkGraphicsCommonPipelineStateInfo m_common;
    
    // List of pipeline instances, shared between threads
    alignas(CACHE_LINE_SIZE) sync::Spinlock   m_mutex;
    std::vector<DxvkGraphicsPipelineInstance> m_pipelines;
    
    // Pipeline handles used for derivative pipelines
    std::atomic<VkPipeline> m_basePipeline = { VK_NULL_HANDLE };
    
    const DxvkGraphicsPipelineInstance* findInstance(
      const DxvkGraphicsPipelineStateInfo& state,
            VkRenderPass                   renderPass) const;
    
    VkPipeline compilePipeline(
      const DxvkGraphicsPipelineStateInfo& state,
            VkRenderPass                   renderPass,
            VkPipeline                     baseHandle) const;
    
    void destroyPipeline(
            VkPipeline                     pipeline) const;
    
    bool validatePipelineState(
      const DxvkGraphicsPipelineStateInfo& state) const;
    
    void writePipelineStateToCache(
      const DxvkGraphicsPipelineStateInfo& state,
      const DxvkRenderPassFormat&          format) const;
    
    void logPipelineState(
            LogLevel                       level,
      const DxvkGraphicsPipelineStateInfo& state) const;
    
  };
  
}