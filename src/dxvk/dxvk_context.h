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
     * This will not change any context state
     * other than the active command list.
     */
    void endRecording();
    
    /**
     * \brief Sets framebuffer
     * \param [in] fb Framebuffer
     */
    void bindFramebuffer(
      const Rc<DxvkFramebuffer>& fb);
    
    /**
     * \brief Binds index buffer
     * \param [in] buffer New index buffer
     */
    void bindIndexBuffer(
      const DxvkBufferBinding&    buffer);
    
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
     * \brief Binds vertex buffer
     * 
     * \param [in] binding Vertex buffer binding
     * \param [in] buffer New vertex buffer
     */
    void bindVertexBuffer(
            uint32_t              binding,
      const DxvkBufferBinding&    buffer);
    
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
     * \brief Copies data from one buffer to another
     * 
     * \param [in] dstBuffer Destination buffer
     * \param [in] dstOffset Destination data offset
     * \param [in] srcBuffer Source buffer
     * \param [in] srcOffset Source data offset
     * \param [in] numBytes Number of bytes to copy
     */
    void copyBuffer(
      const Rc<DxvkBuffer>&       dstBuffer,
            VkDeviceSize          dstOffset,
      const Rc<DxvkBuffer>&       srcBuffer,
            VkDeviceSize          srcOffset,
            VkDeviceSize          numBytes);
    
    /**
     * \brief Starts compute jobs
     * 
     * \param [in] x Number of threads in X direction
     * \param [in] y Number of threads in Y direction
     * \param [in] z Number of threads in Z direction
     */
    void dispatch(
            uint32_t x,
            uint32_t y,
            uint32_t z);
    
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
    
    /**
     * \brief Sets viewports
     * 
     * \param [in] viewportCount Number of viewports
     * \param [in] viewports The viewports
     * \param [in] scissorRects Schissor rectangles
     */
    void setViewports(
            uint32_t            viewportCount,
      const VkViewport*         viewports,
      const VkRect2D*           scissorRects);
    
    /**
     * \brief Sets input assembly state
     * \param [in] state New state object
     */
    void setInputAssemblyState(
      const Rc<DxvkInputAssemblyState>& state);
    
    /**
     * \brief Sets input layout state
     * \param [in] state New state object
     */
    void setInputLayout(
      const Rc<DxvkInputLayout>& state);
    
    /**
     * \brief Sets rasterizer state
     * \param [in] state New state object
     */
    void setRasterizerState(
      const Rc<DxvkRasterizerState>& state);
    
    /**
     * \brief Sets multisample state
     * \param [in] state New state object
     */
    void setMultisampleState(
      const Rc<DxvkMultisampleState>& state);
    
    /**
     * \brief Sets depth stencil state
     * \param [in] state New state object
     */
    void setDepthStencilState(
      const Rc<DxvkDepthStencilState>& state);
    
    /**
     * \brief Sets color blend state
     * \param [in] state New state object
     */
    void setBlendState(
      const Rc<DxvkBlendState>& state);
    
  private:
    
    const Rc<DxvkDevice>          m_device;
    const Rc<DxvkPipelineManager> m_pipeMgr;
    
    Rc<DxvkRecorder> m_cmd;
    DxvkContextState m_state;
    DxvkBarrierSet   m_barriers;
    
    void renderPassBegin();
    void renderPassEnd();
    
    void bindComputePipeline();
    void bindGraphicsPipeline();
    
    void updateDynamicState();
    void updateViewports();
    
    void updateIndexBufferBinding();
    void updateVertexBufferBindings();
    
    void commitComputeState();
    void commitGraphicsState();
    
    DxvkShaderStageState* getShaderStage(
            VkShaderStageFlagBits     stage);
    
    
    
  };
  
}