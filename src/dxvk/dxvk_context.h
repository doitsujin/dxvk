#pragma once

#include "dxvk_barrier.h"
#include "dxvk_cmdlist.h"
#include "dxvk_context_state.h"
#include "dxvk_deferred.h"
#include "dxvk_pipemgr.h"
#include "dxvk_util.h"

namespace dxvk {
  
  /**
   * \brief DXVk context
   * 
   * Tracks pipeline state and records command lists.
   * This is where the actual rendering commands are
   * recorded.
   */
  class DxvkContext : public RcObject {
    
  public:
    
    DxvkContext(
      const Rc<DxvkDevice>&           device,
      const Rc<DxvkPipelineManager>&  pipeMgr);
    ~DxvkContext();
    
    /**
     * \brief Begins command buffer recording
     * 
     * Begins recording a command list. This does
     * not alter any context state other than the
     * active command list.
     * \param [in] recorder Target recorder
     */
    void beginRecording(
      const Rc<DxvkRecorder>& recorder);
    
    /**
     * \brief Ends command buffer recording
     * 
     * Finishes recording the active command list.
     * The command list can then be submitted to
     * the device.
     * 
     * The return value of this method can be used to
     * determine whether the command list needs to be
     * submitted. In case the command list is empty,
     * \c false will be returned and it shall not be
     * submitted to the device.
     * 
     * This will not change any context state
     * other than the active command list.
     * \returns \c true if any commands were recorded
     */
    bool endRecording();
    
    /**
     * \brief Sets framebuffer
     * \param [in] fb Framebuffer
     */
    void bindFramebuffer(
      const Rc<DxvkFramebuffer>& fb);
    
    /**
     * \brief Sets shader for a given shader stage
     * 
     * Binds a shader to a given stage, while unbinding the
     * existing one. If \c nullptr is passed as the shader
     * to bind, the given shader stage will be disabled.
     * When drawing, at least a vertex shader must be bound.
     * \param [in] stage The shader stage
     * \param [in] shader The shader to set
     */
    void bindShader(
            VkShaderStageFlagBits stage,
      const Rc<DxvkShader>&       shader);
    
    /**
     * \brief Binds a storage buffer
     * 
     * \param [in] stage Shader stage for this binding
     * \param [in] slot Binding slot index
     * \param [in] buffer Buffer binding info
     */
    void bindStorageBuffer(
            VkShaderStageFlagBits stage,
            uint32_t              slot,
      const Rc<DxvkBuffer>&       buffer,
            VkDeviceSize          offset,
            VkDeviceSize          length);
    
    /**
     * \brief Clears an active render target
     * 
     * \param [in] attachment Attachment to clear
     * \param [in] clearArea Rectangular area to clear
     */
    void clearRenderTarget(
      const VkClearAttachment&  attachment,
      const VkClearRect&        clearArea);
    
    /**
     * \brief Dispatches compute operations
     * 
     * \param [in] wgCountX Number of X work groups
     * \param [in] wgCountY Number of Y work groups
     * \param [in] wgCountZ Number of Z work groups
     */
    void dispatch(
            uint32_t wgCountX,
            uint32_t wgCountY,
            uint32_t wgCountZ);
    
    /**
     * \brief Draws primitive without using an index buffer
     * 
     * \param [in] vertexCount Number of vertices to draw
     * \param [in] instanceCount Number of instances to render
     * \param [in] firstVertex First vertex in vertex buffer
     * \param [in] firstInstance First instance ID
     */
    void draw(
            uint32_t vertexCount,
            uint32_t instanceCount,
            uint32_t firstVertex,
            uint32_t firstInstance);
    
    /**
     * \brief Draws primitives using an index buffer
     * 
     * \param [in] indexCount Number of indices to draw
     * \param [in] instanceCount Number of instances to render
     * \param [in] firstIndex First index within the index buffer
     * \param [in] vertexOffset Vertex ID that corresponds to index 0
     * \param [in] firstInstance First instance ID
     */
    void drawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount,
            uint32_t firstIndex,
            uint32_t vertexOffset,
            uint32_t firstInstance);
    
  private:
    
    const Rc<DxvkDevice>          m_device;
    const Rc<DxvkPipelineManager> m_pipeMgr;
    
    Rc<DxvkRecorder> m_cmd;
    DxvkContextState m_state;
    
    void flushComputeState();
    void flushGraphicsState();
    
    void beginRenderPass();
    void endRenderPass();
    
    void setPipelineDirty(VkShaderStageFlagBits stage);
    void setResourcesDirty(VkShaderStageFlagBits stage);
    
    void shaderResourceBarriers(
      DxvkBarrierSet&       barriers,
      VkShaderStageFlagBits stage);
    
    DxvkShaderState* getShaderState(
      VkShaderStageFlagBits stage);
      
    uint32_t addResourceBindingInfo(
            std::vector<DxvkResourceBinding>& bindings,
      const DxvkShaderState&                  stageInfo) const;
    
  };
  
}