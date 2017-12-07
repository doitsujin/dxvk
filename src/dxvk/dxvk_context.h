#pragma once

#include "dxvk_barrier.h"
#include "dxvk_binding.h"
#include "dxvk_cmdlist.h"
#include "dxvk_context_state.h"
#include "dxvk_data.h"
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
    
    DxvkContext(const Rc<DxvkDevice>& device);
    ~DxvkContext();
    
    /**
     * \brief Begins command buffer recording
     * 
     * Begins recording a command list. This does
     * not alter any context state other than the
     * active command list.
     * \param [in] cmdList Target command list
     */
    void beginRecording(
      const Rc<DxvkCommandList>& cmdList);
    
    /**
     * \brief Ends command buffer recording
     * 
     * Finishes recording the active command list.
     * The command list can then be submitted to
     * the device.
     * 
     * This will not change any context state
     * other than the active command list.
     * \returns Active command list
     */
    Rc<DxvkCommandList> endRecording();
    
    /**
     * \brief Sets framebuffer
     * \param [in] fb Framebuffer
     */
    void bindFramebuffer(
      const Rc<DxvkFramebuffer>& fb);
    
    /**
     * \brief Binds index buffer
     * 
     * The index buffer will be used when
     * issuing \c drawIndexed commands.
     * \param [in] buffer New index buffer
     * \param [in] indexType Index type
     */
    void bindIndexBuffer(
      const DxvkBufferBinding&    buffer,
            VkIndexType           indexType);
    
    /**
     * \brief Binds buffer as a shader resource
     * 
     * Can be used for uniform and storage buffers.
     * \param [in] pipe Target pipeline
     * \param [in] slot Resource binding slot
     * \param [in] buffer Buffer to bind
     */
    void bindResourceBuffer(
            VkPipelineBindPoint   pipe,
            uint32_t              slot,
      const DxvkBufferBinding&    buffer);
    
    /**
     * \brief Binds texel buffer view
     * 
     * Can be used for both uniform texel
     * buffers and storage texel buffers.
     * \param [in] pipe Target pipeline
     * \param [in] slot Resource binding slot
     * \param [in] bufferView Buffer view to bind
     */
    void bindResourceTexelBuffer(
            VkPipelineBindPoint   pipe,
            uint32_t              slot,
      const Rc<DxvkBufferView>&   bufferView);
    
    /**
     * \brief Binds image view
     * 
     * Can be used for sampled images with a
     * dedicated sampler and storage images.
     * \param [in] pipe Target pipeline
     * \param [in] slot Resource binding slot
     * \param [in] imageView Image view to bind
     */
    void bindResourceImage(
            VkPipelineBindPoint   pipe,
            uint32_t              slot,
      const Rc<DxvkImageView>&    image);
    
    /**
     * \brief Binds image sampler
     * 
     * Binds a sampler that can be used together with
     * an image in order to read from a texture.
     * \param [in] pipe Target pipeline
     * \param [in] slot Resource binding slot
     * \param [in] sampler Sampler view to bind
     */
    void bindResourceSampler(
            VkPipelineBindPoint   pipe,
            uint32_t              slot,
      const Rc<DxvkSampler>&      sampler);
    
    /**
     * \brief Binds a shader to a given state
     * 
     * \param [in] stage Target shader stage
     * \param [in] shader The shader to bind
     */
    void bindShader(
            VkShaderStageFlagBits stage,
      const Rc<DxvkShader>&       shader);
    
    /**
     * \brief Binds vertex buffer
     * 
     * \param [in] binding Vertex buffer binding
     * \param [in] buffer New vertex buffer
     * \param [in] stride Stride between vertices
     */
    void bindVertexBuffer(
            uint32_t              binding,
      const DxvkBufferBinding&    buffer);
    
    /**
     * \brief Clears subresources of a color image
     * 
     * \param [in] image The image to clear
     * \param [in] value Clear value
     * \param [in] subresources Subresources to clear
     */
    void clearColorImage(
      const Rc<DxvkImage>&            image,
      const VkClearColorValue&        value,
      const VkImageSubresourceRange&  subresources);
    
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
     * \brief Initializes or invalidates an image
     * 
     * Sets up the image layout for future operations
     * while discarding any previous contents.
     * \param [in] image The image to initialize
     * \param [in] subresources Image subresources
     */
    void initImage(
      const Rc<DxvkImage>&           image,
      const VkImageSubresourceRange& subresources);
    
    /**
     * \brief Updates a buffer
     * 
     * Copies data from the host into a buffer.
     * \param [in] buffer Destination buffer
     * \param [in] offset Offset of sub range to update
     * \param [in] size Length of sub range to update
     * \param [in] data Data to upload
     */
    void updateBuffer(
      const Rc<DxvkBuffer>&           buffer,
            VkDeviceSize              offset,
            VkDeviceSize              size,
      const void*                     data);
    
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
    
    const Rc<DxvkDevice> m_device;
    
    Rc<DxvkCommandList> m_cmd;
    DxvkContextFlags    m_flags;
    DxvkContextState    m_state;
    DxvkBarrierSet      m_barriers;
    
    DxvkShaderResourceSlots m_cResources = { 1024 };
    DxvkShaderResourceSlots m_gResources = { 4096 };
    
    void renderPassBegin();
    void renderPassEnd();
    
    void updateComputePipeline();
    void updateGraphicsPipeline();
    
    void updateComputeShaderResources();
    void updateGraphicsShaderResources();
    
    void updateDynamicState();
    void updateViewports();
    
    void updateIndexBufferBinding();
    void updateVertexBufferBindings();
    
    void commitComputeState();
    void commitGraphicsState();
    
    void commitComputeBarriers();
    
    void transformLayoutsRenderPassBegin(
      const DxvkRenderTargets& renderTargets);
    
    void transformLayoutsRenderPassEnd(
      const DxvkRenderTargets& renderTargets);
    
    DxvkShaderResourceSlots* getShaderResourceSlots(
            VkPipelineBindPoint pipe);
    
    DxvkContextFlag getResourceDirtyFlag(
            VkPipelineBindPoint pipe) const;
    
  };
  
}