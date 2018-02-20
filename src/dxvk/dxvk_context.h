#pragma once

#include "dxvk_barrier.h"
#include "dxvk_binding.h"
#include "dxvk_cmdlist.h"
#include "dxvk_context_state.h"
#include "dxvk_data.h"
#include "dxvk_event.h"
#include "dxvk_query.h"
#include "dxvk_query_pool.h"
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
     * \brief Begins generating query data
     * \param [in] query The query to end
     */
    void beginQuery(
      const DxvkQueryRevision&  query);
    
    /**
     * \brief Ends generating query data
     * \param [in] query The query to end
     */
    void endQuery(
      const DxvkQueryRevision&  query);
    
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
      const DxvkBufferSlice&      buffer,
            VkIndexType           indexType);
    
    /**
     * \brief Binds buffer as a shader resource
     * 
     * Can be used for uniform and storage buffers.
     * \param [in] slot Resource binding slot
     * \param [in] buffer Buffer to bind
     */
    void bindResourceBuffer(
            uint32_t              slot,
      const DxvkBufferSlice&      buffer);
    
    /**
     * \brief Binds texel buffer view
     * 
     * Can be used for both uniform texel
     * buffers and storage texel buffers.
     * \param [in] slot Resource binding slot
     * \param [in] bufferView Buffer view to bind
     */
    void bindResourceTexelBuffer(
            uint32_t              slot,
      const Rc<DxvkBufferView>&   bufferView);
    
    /**
     * \brief Binds image view
     * 
     * Can be used for sampled images with a
     * dedicated sampler and storage images.
     * \param [in] slot Resource binding slot
     * \param [in] imageView Image view to bind
     */
    void bindResourceImage(
            uint32_t              slot,
      const Rc<DxvkImageView>&    image);
    
    /**
     * \brief Binds image sampler
     * 
     * Binds a sampler that can be used together with
     * an image in order to read from a texture.
     * \param [in] slot Resource binding slot
     * \param [in] sampler Sampler view to bind
     */
    void bindResourceSampler(
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
      const DxvkBufferSlice&      buffer,
            uint32_t              stride);
    
    /**
     * \brief Clears a buffer with a fixed value
     * 
     * Note that both \c offset and \c length must
     * be multiples of four, and that \c value is
     * consumed as a four-byte word.
     * \param [in] buffer The buffer to clear
     * \param [in] offset Offset of the range to clear
     * \param [in] length Bumber of bytes to clear
     * \param [in] value Clear value
     */
    void clearBuffer(
      const Rc<DxvkBuffer>&       buffer,
            VkDeviceSize          offset,
            VkDeviceSize          length,
            uint32_t              value);
    
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
     * \brief Clears subresources of a depth-stencil image
     * 
     * \param [in] image The image to clear
     * \param [in] value Clear value
     * \param [in] subresources Subresources to clear
     */
    void clearDepthStencilImage(
      const Rc<DxvkImage>&            image,
      const VkClearDepthStencilValue& value,
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
     * \brief Copies data from a buffer to an image
     * 
     * \param [in] dstImage Destination image
     * \param [in] dstSubresource Destination subresource
     * \param [in] dstOffset Destination area offset
     * \param [in] dstExtent Destination area size
     * \param [in] srcBuffer Source buffer
     * \param [in] srcOffset Source offset, in bytes
     * \param [in] srcExtent Source data extent
     */
    void copyBufferToImage(
      const Rc<DxvkImage>&        dstImage,
            VkImageSubresourceLayers dstSubresource,
            VkOffset3D            dstOffset,
            VkExtent3D            dstExtent,
      const Rc<DxvkBuffer>&       srcBuffer,
            VkDeviceSize          srcOffset,
            VkExtent2D            srcExtent);
    
    /**
     * \brief Copies data from one image to another
     * 
     * \param [in] dstImage Destination image
     * \param [in] dstSubresource Destination subresource
     * \param [in] dstOffset Destination area offset
     * \param [in] srcImage Source image
     * \param [in] srcSubresource Source subresource
     * \param [in] srcOffset Source area offset
     * \param [in] extent Size of the area to copy
     */
    void copyImage(
      const Rc<DxvkImage>&        dstImage,
            VkImageSubresourceLayers dstSubresource,
            VkOffset3D            dstOffset,
      const Rc<DxvkImage>&        srcImage,
            VkImageSubresourceLayers srcSubresource,
            VkOffset3D            srcOffset,
            VkExtent3D            extent);
    
    /**
     * \brief Copies data from an image into a buffer
     * 
     * \param [in] dstBuffer Destination buffer
     * \param [in] dstOffset Destination offset, in bytes
     * \param [in] dstExtent Destination data extent
     * \param [in] srcImage Source image
     * \param [in] srcSubresource Source subresource
     * \param [in] srcOffset Source area offset
     * \param [in] srcExtent Source area size
     */
    void copyImageToBuffer(
      const Rc<DxvkBuffer>&       dstBuffer,
            VkDeviceSize          dstOffset,
            VkExtent2D            dstExtent,
      const Rc<DxvkImage>&        srcImage,
            VkImageSubresourceLayers srcSubresource,
            VkOffset3D            srcOffset,
            VkExtent3D            srcExtent);
    
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
     * \brief Indirect dispatch call
     * 
     * Takes arguments from a buffer. The buffer must contain
     * a structure of the type \c VkDispatchIndirectCommand.
     * \param [in] buffer The buffer slice
     */
    void dispatchIndirect(
      const DxvkBufferSlice&  buffer);
    
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
     * \brief Indirect indexed draw call
     * 
     * Takes arguments from a buffer. The structure stored
     * in the buffer must be of type \c VkDrawIndirectCommand.
     * \param [in] buffer The buffer slice
     * \param [in] count Number of dispatch calls
     * \param [in] stride Stride between dispatch calls
     */
    void drawIndirect(
      const DxvkBufferSlice&  buffer,
            uint32_t          count,
            uint32_t          stride);
    
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
     * \brief Indirect indexed draw call
     * 
     * Takes arguments from a buffer. The structure type for
     * the draw buffer is \c VkDrawIndexedIndirectCommand.
     * \param [in] buffer The buffer slice
     * \param [in] count Number of dispatch calls
     * \param [in] stride Stride between dispatch calls
     */
    void drawIndexedIndirect(
      const DxvkBufferSlice&  buffer,
            uint32_t          count,
            uint32_t          stride);
    
    /**
     * \brief Generates mip maps
     * 
     * Uses blitting to generate lower mip levels from
     * the top-most mip level passed to this method.
     * \param [in] image The image to generate mips for
     * \param [in] subresource The subresource range
     */
    void generateMipmaps(
      const Rc<DxvkImage>&            image,
      const VkImageSubresourceRange&  subresources);
    
    /**
     * \brief Initializes or invalidates an image
     * 
     * Sets up the image layout for future operations
     * while discarding any previous contents.
     * \param [in] image The image to initialize
     * \param [in] subresources Image subresources
     */
    void initImage(
      const Rc<DxvkImage>&            image,
      const VkImageSubresourceRange&  subresources);
    
    /**
     * \brief Invalidates a buffer's contents
     * 
     * Discards a buffer's contents by replacing the
     * backing resource. This allows the host to access
     * the buffer while the GPU is still accessing the
     * original backing resource.
     * 
     * \warning If the buffer is used by another context,
     * invalidating it will result in undefined behaviour.
     * \param [in] buffer The buffer to invalidate
     * \param [in] slice New physical buffer slice
     */
    void invalidateBuffer(
      const Rc<DxvkBuffer>&           buffer,
      const DxvkPhysicalBufferSlice&  slice);
    
    /**
     * \brief Resolves a multisampled image resource
     * 
     * Resolves a multisampled image into a non-multisampled
     * image. The subresources of both images must have the
     * same size and compatible formats
     * \param [in] dstImage Destination image
     * \param [in] dstSubresources Subresources to write to
     * \param [in] srcImage Source image
     * \param [in] srcSubresources Subresources to read from
     */
    void resolveImage(
      const Rc<DxvkImage>&            dstImage,
      const VkImageSubresourceLayers& dstSubresources,
      const Rc<DxvkImage>&            srcImage,
      const VkImageSubresourceLayers& srcSubresources);
    
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
     * \brief Updates an image
     * 
     * Copies data from the host into an image.
     * \param [in] image Destination image
     * \param [in] subsresources Image subresources to update
     * \param [in] imageOffset Offset of the image area to update
     * \param [in] imageExtent Size of the image area to update
     * \param [in] data Source data
     * \param [in] pitchPerRow Row pitch of the source data
     * \param [in] pitchPerLayer Layer pitch of the source data
     */
    void updateImage(
      const Rc<DxvkImage>&            image,
      const VkImageSubresourceLayers& subresources,
            VkOffset3D                imageOffset,
            VkExtent3D                imageExtent,
      const void*                     data,
            VkDeviceSize              pitchPerRow,
            VkDeviceSize              pitchPerLayer);
    
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
     * \brief Sets blend constants
     * 
     * Blend constants are a set of four floating
     * point numbers that may be used as an input
     * for blending operations.
     * \param [in] blendConstants Blend constants
     */
    void setBlendConstants(
      const DxvkBlendConstants& blendConstants);
    
    /**
     * \brief Sets stencil reference
     * 
     * Sets the reference value for stencil compare operations.
     * \param [in] reference Reference value
     */
    void setStencilReference(
      const uint32_t            reference);
    
    /**
     * \brief Sets input assembly state
     * \param [in] ia New state object
     */
    void setInputAssemblyState(
      const DxvkInputAssemblyState& ia);
    
    /**
     * \brief Sets input layout
     * 
     * \param [in] attributeCount Number of vertex attributes
     * \param [in] attributes The vertex attributes
     * \param [in] bindingCount Number of buffer bindings
     * \param [in] bindings Vertex buffer bindigs
     */
    void setInputLayout(
            uint32_t             attributeCount,
      const DxvkVertexAttribute* attributes,
            uint32_t             bindingCount,
      const DxvkVertexBinding*   bindings);
    
    /**
     * \brief Sets rasterizer state
     * \param [in] rs New state object
     */
    void setRasterizerState(
      const DxvkRasterizerState& rs);
    
    /**
     * \brief Sets multisample state
     * \param [in] ms New state object
     */
    void setMultisampleState(
      const DxvkMultisampleState& ms);
    
    /**
     * \brief Sets depth stencil state
     * \param [in] ds New state object
     */
    void setDepthStencilState(
      const DxvkDepthStencilState& ds);
    
    /**
     * \brief Sets logic op state
     * \param [in] lo New state object
     */
    void setLogicOpState(
      const DxvkLogicOpState&   lo);
    
    /**
     * \brief Sets blend mode for an attachment
     * 
     * \param [in] attachment The attachment index
     * \param [in] blendMode The blend mode
     */
    void setBlendMode(
            uint32_t            attachment,
      const DxvkBlendMode&      blendMode);
    
    /**
     * \brief Signals an event
     * \param [in] event The event
     */
    void signalEvent(
      const DxvkEventRevision&  event);
    
    /**
     * \brief Writes to a timestamp query
     * \param [in] query The timestamp query
     */
    void writeTimestamp(
      const DxvkQueryRevision&  query);
    
  private:
    
    const Rc<DxvkDevice> m_device;
    
    Rc<DxvkCommandList> m_cmd;
    DxvkContextFlags    m_flags;
    DxvkContextState    m_state;
    DxvkBarrierSet      m_barriers;
    
    // TODO implement this properly...
    Rc<DxvkQueryPool>   m_queryPools[3] = { nullptr, nullptr, nullptr };
    
    VkPipeline m_gpActivePipeline = VK_NULL_HANDLE;
    VkPipeline m_cpActivePipeline = VK_NULL_HANDLE;
    
    std::vector<DxvkQueryRevision> m_activeQueries;
    
    std::array<DxvkShaderResourceSlot, MaxNumResourceSlots>  m_rc;
    std::array<DxvkDescriptorInfo,     MaxNumActiveBindings> m_descInfos;
    std::array<VkWriteDescriptorSet,   MaxNumActiveBindings> m_descWrites;
    
    void renderPassBegin();
    void renderPassEnd();
    
    void updateComputePipeline();
    void updateComputePipelineState();
    
    void updateGraphicsPipeline();
    void updateGraphicsPipelineState();
    
    void updateComputeShaderResources();
    void updateComputeShaderDescriptors();
    
    void updateGraphicsShaderResources();
    void updateGraphicsShaderDescriptors();
    
    void updateShaderResources(
            VkPipelineBindPoint     bindPoint,
      const Rc<DxvkPipelineLayout>& layout);
    
    void updateShaderDescriptors(
            VkPipelineBindPoint     bindPoint,
      const DxvkBindingState&       bindingState,
      const Rc<DxvkPipelineLayout>& layout);
    
    void updateViewports();
    void updateBlendConstants();
    void updateStencilReference();
    
    void updateIndexBufferBinding();
    void updateVertexBufferBindings();
    
    void commitComputeState();
    void commitGraphicsState();
    
    void commitComputeBarriers();
    
    DxvkQueryHandle allocQuery(
      const DxvkQueryRevision& query);
    
    void resetQueryPool(
      const Rc<DxvkQueryPool>& pool);
    
    void trackQueryPool(
      const Rc<DxvkQueryPool>& pool);
    
    void beginActiveQueries();
    
    void endActiveQueries();
    
    void insertActiveQuery(
      const DxvkQueryRevision& query);
    
    void eraseActiveQuery(
      const DxvkQueryRevision& query);
    
    Rc<DxvkBuffer> getTransferBuffer(VkDeviceSize size);
    
  };
  
}