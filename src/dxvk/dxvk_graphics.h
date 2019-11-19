#pragma once

#include <mutex>

#include "dxvk_bind_mask.h"
#include "dxvk_constant_state.h"
#include "dxvk_graphics_state.h"
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
   * \brief Flags that describe pipeline properties
   */
  enum class DxvkGraphicsPipelineFlag {
    HasTransformFeedback,
    HasStorageDescriptors,
  };

  using DxvkGraphicsPipelineFlags = Flags<DxvkGraphicsPipelineFlag>;


  /**
   * \brief Shaders used in graphics pipelines
   */
  struct DxvkGraphicsPipelineShaders {
    Rc<DxvkShader> vs;
    Rc<DxvkShader> tcs;
    Rc<DxvkShader> tes;
    Rc<DxvkShader> gs;
    Rc<DxvkShader> fs;

    bool eq(const DxvkGraphicsPipelineShaders& other) const {
      return vs == other.vs && tcs == other.tcs
          && tes == other.tes && gs == other.gs
          && fs == other.fs;
    }

    size_t hash() const {
      DxvkHashState state;
      state.add(DxvkShader::getHash(vs));
      state.add(DxvkShader::getHash(tcs));
      state.add(DxvkShader::getHash(tes));
      state.add(DxvkShader::getHash(gs));
      state.add(DxvkShader::getHash(fs));
      return state;
    }
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

    DxvkGraphicsPipelineInstance()
    : m_stateVector (),
      m_renderPass  (VK_NULL_HANDLE),
      m_pipeline    (VK_NULL_HANDLE) { }

    DxvkGraphicsPipelineInstance(
      const DxvkGraphicsPipelineStateInfo&  state,
      const DxvkRenderPass*                 rp,
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
      const DxvkRenderPass*                 rp) {
      return m_renderPass  == rp
          && m_stateVector == state;
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
    const DxvkRenderPass*         m_renderPass;
    VkPipeline                    m_pipeline;

  };

  
  /**
   * \brief Graphics pipeline
   * 
   * Stores the pipeline layout as well as methods to
   * recompile the graphics pipeline against a given
   * pipeline state vector.
   */
  class DxvkGraphicsPipeline {
    
  public:
    
    DxvkGraphicsPipeline(
            DxvkPipelineManager*        pipeMgr,
            DxvkGraphicsPipelineShaders shaders);

    ~DxvkGraphicsPipeline();

    /**
     * \brief Shaders used by the pipeline
     * \returns Shaders used by the pipeline
     */
    const DxvkGraphicsPipelineShaders& shaders() const {
      return m_shaders;
    }
    
    /**
     * \brief Returns graphics pipeline flags
     * \returns Graphics pipeline property flags
     */
    DxvkGraphicsPipelineFlags flags() const {
      return m_flags;
    }
    
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
     * \brief Queries shader for a given stage
     * 
     * In case no shader is specified for the
     * given stage, \c nullptr will be returned.
     * \param [in] stage The shader stage
     * \returns Shader of the given stage
     */
    Rc<DxvkShader> getShader(
            VkShaderStageFlagBits             stage) const;
    
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
      const DxvkRenderPass*                   renderPass);
    
    /**
     * \brief Compiles a pipeline
     * 
     * Asynchronously compiles the given pipeline
     * and stores the result for future use.
     * \param [in] state Pipeline state vector
     * \param [in] renderPass The render pass
     */
    void compilePipeline(
      const DxvkGraphicsPipelineStateInfo&    state,
      const DxvkRenderPass*                   renderPass);
    
  private:
    
    Rc<vk::DeviceFn>            m_vkd;
    DxvkPipelineManager*        m_pipeMgr;

    DxvkGraphicsPipelineShaders m_shaders;
    DxvkDescriptorSlotMapping   m_slotMapping;

    Rc<DxvkPipelineLayout>      m_layout;
    
    uint32_t m_vsIn  = 0;
    uint32_t m_fsOut = 0;
    
    DxvkGraphicsPipelineFlags           m_flags;
    DxvkGraphicsCommonPipelineStateInfo m_common;
    
    // List of pipeline instances, shared between threads
    alignas(CACHE_LINE_SIZE) sync::Spinlock   m_mutex;
    std::vector<DxvkGraphicsPipelineInstance> m_pipelines;
    
    DxvkGraphicsPipelineInstance* createInstance(
      const DxvkGraphicsPipelineStateInfo& state,
      const DxvkRenderPass*                renderPass);
    
    DxvkGraphicsPipelineInstance* findInstance(
      const DxvkGraphicsPipelineStateInfo& state,
      const DxvkRenderPass*                renderPass);
    
    VkPipeline createPipeline(
      const DxvkGraphicsPipelineStateInfo& state,
      const DxvkRenderPass*                renderPass) const;
    
    void destroyPipeline(
            VkPipeline                     pipeline) const;
    
    DxvkShaderModule createShaderModule(
      const Rc<DxvkShader>&                shader,
      const DxvkGraphicsPipelineStateInfo& state) const;
    
    Rc<DxvkShader> getPrevStageShader(
            VkShaderStageFlagBits          stage) const;

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