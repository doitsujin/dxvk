#pragma once

#include "dxvk_barrier.h"
#include "dxvk_bind_mask.h"
#include "dxvk_cmdlist.h"
#include "dxvk_context_state.h"
#include "dxvk_data.h"
#include "dxvk_objects.h"
#include "dxvk_queue.h"
#include "dxvk_resource.h"
#include "dxvk_util.h"
#include "dxvk_marker.h"

namespace dxvk {
  
  /**
   * \brief DXVk context
   * 
   * Tracks pipeline state and records command lists.
   * This is where the actual rendering commands are
   * recorded.
   */
  class DxvkContext : public RcObject {
    constexpr static VkDeviceSize StagingBufferSize = 4ull << 20;
  public:
    
    DxvkContext(const Rc<DxvkDevice>& device, DxvkContextType type);
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
     * \brief Ends frame
     *
     * Must be called once per frame before the
     * final call to \ref endRecording.
     */
    void endFrame();

    /**
     * \brief Flushes command buffer
     * 
     * Transparently submits the current command
     * buffer and allocates a new one.
     * \param [out] status Submission feedback
     */
    void flushCommandList(DxvkSubmitStatus* status);
    
    /**
     * \brief Begins generating query data
     * \param [in] query The query to end
     */
    void beginQuery(
      const Rc<DxvkGpuQuery>&   query);
    
    /**
     * \brief Ends generating query data
     * \param [in] query The query to end
     */
    void endQuery(
      const Rc<DxvkGpuQuery>&   query);
    
    /**
     * \brief Sets render targets
     * 
     * Creates a framebuffer on the fly if necessary
     * and binds it using \c bindFramebuffer.
     * \param [in] targets Render targets to bind
     */
    void bindRenderTargets(
            DxvkRenderTargets&&   targets,
            VkImageAspectFlags    feedbackLoop) {
      // Set up default render pass ops
      m_state.om.renderTargets = std::move(targets);

      if (unlikely(m_state.gp.state.om.feedbackLoop() != feedbackLoop)) {
        m_state.gp.state.om.setFeedbackLoop(feedbackLoop);
        m_flags.set(DxvkContextFlag::GpDirtyPipelineState);
      }

      this->resetRenderPassOps(
        m_state.om.renderTargets,
        m_state.om.renderPassOps);

      if (!m_state.om.framebufferInfo.hasTargets(m_state.om.renderTargets)) {
        // Create a new framebuffer object next
        // time we start rendering something
        m_flags.set(DxvkContextFlag::GpDirtyFramebuffer);
      } else {
        // Don't redundantly spill the render pass if
        // the same render targets are bound again
        m_flags.clr(DxvkContextFlag::GpDirtyFramebuffer);
      }
    }

    /**
     * \brief Binds indirect argument buffer
     * 
     * Sets the buffers that are going to be used
     * for indirect draw and dispatch operations.
     * \param [in] argBuffer New argument buffer
     * \param [in] cntBuffer New count buffer
     */
    void bindDrawBuffers(
            DxvkBufferSlice&&     argBuffer,
            DxvkBufferSlice&&     cntBuffer) {
      m_state.id.argBuffer = std::move(argBuffer);
      m_state.id.cntBuffer = std::move(cntBuffer);

      m_flags.set(DxvkContextFlag::DirtyDrawBuffer);
    }

    /**
     * \brief Binds index buffer
     * 
     * The index buffer will be used when
     * issuing \c drawIndexed commands.
     * \param [in] buffer New index buffer
     * \param [in] indexType Index type
     */
    void bindIndexBuffer(
            DxvkBufferSlice&&     buffer,
            VkIndexType           indexType) {
      if (!m_state.vi.indexBuffer.matchesBuffer(buffer))
        m_vbTracked.clr(MaxNumVertexBindings);

      m_state.vi.indexBuffer = std::move(buffer);
      m_state.vi.indexType   = indexType;

      m_flags.set(DxvkContextFlag::GpDirtyIndexBuffer);
    }

    /**
     * \brief Binds index buffer range
     * 
     * Canges the offset and size of the bound index buffer.
     * \param [in] offset Index buffer offset
     * \param [in] length Index buffer size
     * \param [in] indexType Index type
     */
    void bindIndexBufferRange(
            VkDeviceSize          offset,
            VkDeviceSize          length,
            VkIndexType           indexType) {
      m_state.vi.indexBuffer.setRange(offset, length);
      m_state.vi.indexType = indexType;

      m_flags.set(DxvkContextFlag::GpDirtyIndexBuffer);
    }

    /**
     * \brief Binds buffer to the UBO set
     * 
     * Can be used for uniform and storage buffers bound that
     * are used within the UBO descriptor set. Storage buffers
     * within the view set must be bound via a view.
     * \param [in] stages Shader stages that access the binding
     * \param [in] slot Resource binding slot
     * \param [in] buffer Buffer to bind
     */
    void bindUniformBuffer(
            VkShaderStageFlags    stages,
            uint32_t              slot,
            DxvkBufferSlice&&     buffer) {
      bool needsUpdate = !m_rc[slot].bufferSlice.matchesBuffer(buffer);

      if (likely(needsUpdate))
        m_rcTracked.clr(slot);

      m_rc[slot].bufferSlice = std::move(buffer);

      m_descriptorState.dirtyBuffers(stages);
    }

    /**
     * \brief Changes bound range of a uniform buffer
     * 
     * Can be used to quickly bind a new sub-range of
     * a buffer rather than re-binding the entire buffer.
     */
    void bindUniformBufferRange(
            VkShaderStageFlags    stages,
            uint32_t              slot,
            VkDeviceSize          offset,
            VkDeviceSize          length) {
      m_rc[slot].bufferSlice.setRange(offset, length);

      m_descriptorState.dirtyBuffers(stages);
    }
    
    /**
     * \brief Binds image view
     *
     * \param [in] stages Shader stages that access the binding
     * \param [in] slot Resource binding slot
     * \param [in] view Image view to bind
     */
    void bindResourceImageView(
            VkShaderStageFlags    stages,
            uint32_t              slot,
            Rc<DxvkImageView>&&   view) {
      if (m_rc[slot].bufferView != nullptr) {
        m_rc[slot].bufferSlice = DxvkBufferSlice();
        m_rc[slot].bufferView  = nullptr;
      }

      m_rc[slot].imageView = std::move(view);
      m_rcTracked.clr(slot);

      m_descriptorState.dirtyViews(stages);
    }

    /**
     * \brief Binds buffer view
     *
     * \param [in] stages Shader stages that access the binding
     * \param [in] slot Resource binding slot
     * \param [in] view Buffer view to bind
     */
    void bindResourceBufferView(
            VkShaderStageFlags    stages,
            uint32_t              slot,
            Rc<DxvkBufferView>&&  view) {
      if (m_rc[slot].imageView != nullptr)
        m_rc[slot].imageView = nullptr;

      if (view != nullptr) {
        m_rc[slot].bufferSlice = view->slice();
        m_rc[slot].bufferView = std::move(view);
      } else {
        m_rc[slot].bufferSlice = DxvkBufferSlice();
        m_rc[slot].bufferView = nullptr;
      }

      m_rcTracked.clr(slot);

      m_descriptorState.dirtyViews(stages);
    }

    /**
     * \brief Binds image sampler
     * 
     * Binds a sampler that can be used together with
     * an image in order to read from a texture.
     * \param [in] stages Shader stages that access the binding
     * \param [in] slot Resource binding slot
     * \param [in] sampler Sampler view to bind
     */
    void bindResourceSampler(
            VkShaderStageFlags    stages,
            uint32_t              slot,
            Rc<DxvkSampler>&&     sampler) {
      m_rc[slot].sampler = std::move(sampler);
      m_rcTracked.clr(slot);

      m_descriptorState.dirtyViews(stages);
    }

    /**
     * \brief Binds a shader to a given state
     * 
     * \param [in] stage Target shader stage
     * \param [in] shader The shader to bind
     */
    template<VkShaderStageFlagBits Stage>
    void bindShader(
            Rc<DxvkShader>&&      shader) {
      switch (Stage) {
        case VK_SHADER_STAGE_VERTEX_BIT:
          m_state.gp.shaders.vs = std::move(shader);
          break;

        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
          m_state.gp.shaders.tcs = std::move(shader);
          break;

        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
          m_state.gp.shaders.tes = std::move(shader);
          break;

        case VK_SHADER_STAGE_GEOMETRY_BIT:
          m_state.gp.shaders.gs = std::move(shader);
          break;

        case VK_SHADER_STAGE_FRAGMENT_BIT:
          m_state.gp.shaders.fs = std::move(shader);
          break;

        case VK_SHADER_STAGE_COMPUTE_BIT:
          m_state.cp.shaders.cs = std::move(shader);
          break;

        default:
          return;
      }

      if (Stage == VK_SHADER_STAGE_COMPUTE_BIT) {
        m_flags.set(
          DxvkContextFlag::CpDirtyPipelineState);
      } else {
        m_flags.set(
          DxvkContextFlag::GpDirtyPipeline,
          DxvkContextFlag::GpDirtyPipelineState);
      }
    }
    
    /**
     * \brief Binds vertex buffer
     * 
     * \param [in] binding Vertex buffer binding
     * \param [in] buffer New vertex buffer
     * \param [in] stride Stride between vertices
     */
    void bindVertexBuffer(
            uint32_t              binding,
            DxvkBufferSlice&&     buffer,
            uint32_t              stride) {
      if (!m_state.vi.vertexBuffers[binding].matchesBuffer(buffer))
        m_vbTracked.clr(binding);

      m_state.vi.vertexBuffers[binding] = std::move(buffer);
      m_state.vi.vertexStrides[binding] = stride;
      m_flags.set(DxvkContextFlag::GpDirtyVertexBuffers);
    }

    /**
     * \brief Binds vertex buffer range
     * 
     * Only changes offsets of a bound vertex buffer.
     * \param [in] binding Vertex buffer binding
     * \param [in] offset Vertex buffer offset
     * \param [in] length Vertex buffer size
     * \param [in] stride Stride between vertices
     */
    void bindVertexBufferRange(
            uint32_t              binding,
            VkDeviceSize          offset,
            VkDeviceSize          length,
            uint32_t              stride) {
      m_state.vi.vertexBuffers[binding].setRange(offset, length);
      m_state.vi.vertexStrides[binding] = stride;
      m_flags.set(DxvkContextFlag::GpDirtyVertexBuffers);
    }

    /**
     * \brief Binds transform feedback buffer
     * 
     * \param [in] binding Xfb buffer binding
     * \param [in] buffer The buffer to bind
     * \param [in] counter Xfb counter buffer
     */
    void bindXfbBuffer(
            uint32_t              binding,
            DxvkBufferSlice&&     buffer,
            DxvkBufferSlice&&     counter) {
      m_state.xfb.buffers [binding] = std::move(buffer);
      m_state.xfb.counters[binding] = std::move(counter);

      m_flags.set(DxvkContextFlag::GpDirtyXfbBuffers);
    }

    /**
     * \brief Blits an image
     * 
     * \param [in] dstImage Destination image
     * \param [in] dstMapping Destination swizzle
     * \param [in] srcImage Source image
     * \param [in] srcMapping Source swizzle
     * \param [in] region Blit region
     * \param [in] filter Texture filter
     */
    void blitImage(
      const Rc<DxvkImage>&        dstImage,
      const VkComponentMapping&   dstMapping,
      const Rc<DxvkImage>&        srcImage,
      const VkComponentMapping&   srcMapping,
      const VkImageBlit&          region,
            VkFilter              filter);
    
    /**
     * \brief Changes image layout
     * 
     * Permanently changes the layout for a given
     * image. Immediately performs the transition.
     * \param [in] image The image to transition
     * \param [in] layout New image layout
     */
    void changeImageLayout(
      const Rc<DxvkImage>&        image,
            VkImageLayout         layout);
    
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
     * \brief Clears a buffer view
     * 
     * Unlike \c clearBuffer, this method can be used
     * to clear a buffer view with format conversion. 
     * \param [in] bufferView The buffer view
     * \param [in] offset Offset of the region to clear
     * \param [in] length Extent of the region to clear
     * \param [in] value The clear value
     */
    void clearBufferView(
      const Rc<DxvkBufferView>&   bufferView,
            VkDeviceSize          offset,
            VkDeviceSize          length,
            VkClearColorValue     value);
    
    /**
     * \brief Clears an active render target
     * 
     * \param [in] imageView Render target view to clear
     * \param [in] clearAspects Image aspects to clear
     * \param [in] clearValue The clear value
     */
    void clearRenderTarget(
      const Rc<DxvkImageView>&    imageView,
            VkImageAspectFlags    clearAspects,
            VkClearValue          clearValue);
    
    /**
     * \brief Clears an image view
     * 
     * Can be used to clear sub-regions of storage images
     * that are not going to be used as render targets.
     * Implicit format conversion will be applied.
     * \param [in] imageView The image view
     * \param [in] offset Offset of the rect to clear
     * \param [in] extent Extent of the rect to clear
     * \param [in] aspect Aspect mask to clear
     * \param [in] value The clear value
     */
    void clearImageView(
      const Rc<DxvkImageView>&    imageView,
            VkOffset3D            offset,
            VkExtent3D            extent,
            VkImageAspectFlags    aspect,
            VkClearValue          value);
    
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
     * \brief Copies overlapping buffer region
     * 
     * Can be used to copy potentially overlapping
     * buffer regions within the same buffer. If
     * the source and destination regions do not
     * overlap, it will behave as \ref copyBuffer.
     * \param [in] dstBuffer The buffer
     * \param [in] dstOffset Offset of target region
     * \param [in] srcOffset Offset of source region
     * \param [in] numBytes Number of bytes to copy
     */
    void copyBufferRegion(
      const Rc<DxvkBuffer>&       dstBuffer,
            VkDeviceSize          dstOffset,
            VkDeviceSize          srcOffset,
            VkDeviceSize          numBytes);
    
    /**
     * \brief Copies data from a buffer to an image
     * 
     * Source data must be packed, except for the row alignment.
     * \param [in] dstImage Destination image
     * \param [in] dstSubresource Destination subresource
     * \param [in] dstOffset Destination area offset
     * \param [in] dstExtent Destination area size
     * \param [in] srcBuffer Source buffer
     * \param [in] srcOffset Source offset, in bytes
     * \param [in] rowAlignment Row alignment, in bytes
     * \param [in] sliceAlignment Slice alignment, in bytes
     */
    void copyBufferToImage(
      const Rc<DxvkImage>&        dstImage,
            VkImageSubresourceLayers dstSubresource,
            VkOffset3D            dstOffset,
            VkExtent3D            dstExtent,
      const Rc<DxvkBuffer>&       srcBuffer,
            VkDeviceSize          srcOffset,
            VkDeviceSize          rowAlignment,
            VkDeviceSize          sliceAlignment);
    
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
     * \brief Copies overlapping image region
     *
     * \param [in] dstImage The image
     * \param [in] dstSubresource The image subresource
     * \param [in] dstOffset Destination region offset
     * \param [in] srcOffset Source region offset
     * \param [in] extent Size of the copy region
     */
    void copyImageRegion(
      const Rc<DxvkImage>&        dstImage,
            VkImageSubresourceLayers dstSubresource,
            VkOffset3D            dstOffset,
            VkOffset3D            srcOffset,
            VkExtent3D            extent);
    
    /**
     * \brief Copies data from an image into a buffer
     * 
     * \param [in] dstBuffer Destination buffer
     * \param [in] dstOffset Destination offset, in bytes
     * \param [in] dstExtent Destination data extent
     * \param [in] rowAlignment Row alignment, in bytes
     * \param [in] sliceAlignment Slice alignment, in bytes
     * \param [in] srcImage Source image
     * \param [in] srcSubresource Source subresource
     * \param [in] srcOffset Source area offset
     * \param [in] srcExtent Source area size
     */
    void copyImageToBuffer(
      const Rc<DxvkBuffer>&       dstBuffer,
            VkDeviceSize          dstOffset,
            VkDeviceSize          rowAlignment,
            VkDeviceSize          sliceAlignment,
      const Rc<DxvkImage>&        srcImage,
            VkImageSubresourceLayers srcSubresource,
            VkOffset3D            srcOffset,
            VkExtent3D            srcExtent);
    
    /**
     * \brief Packs depth-stencil image data to a buffer
     * 
     * Packs data from both the depth and stencil aspects
     * of an image into a buffer. The supported formats are:
     * - \c VK_FORMAT_D24_UNORM_S8_UINT: 0xssdddddd
     * - \c VK_FORMAT_D32_SFLOAT_S8_UINT: 0xdddddddd 0x000000ss
     * \param [in] dstBuffer Destination buffer
     * \param [in] dstBufferOffset Destination offset, in bytes
     * \param [in] dstOffset Destination image offset
     * \param [in] dstSize Destination image size
     * \param [in] srcImage Source image
     * \param [in] srcSubresource Source subresource
     * \param [in] srcOffset Source area offset
     * \param [in] srcExtent Source area size
     * \param [in] format Packed data format
     */
    void copyDepthStencilImageToPackedBuffer(
      const Rc<DxvkBuffer>&       dstBuffer,
            VkDeviceSize          dstBufferOffset,
            VkOffset2D            dstOffset,
            VkExtent2D            dstExtent,
      const Rc<DxvkImage>&        srcImage,
            VkImageSubresourceLayers srcSubresource,
            VkOffset2D            srcOffset,
            VkExtent2D            srcExtent,
            VkFormat              format);
    
    /**
     * \brief Copies image data stored in a linear buffer to another
     *
     * The source and destination regions may overlap, in which case
     * a temporary copy of the source buffer will be created.
     * \param [in] dstBuffer Destination buffer
     * \param [in] dstBufferOffset Destination subresource offset
     * \param [in] dstOffset Destination image offset
     * \param [in] dstSize Total size of the destination image
     * \param [in] srcBuffer Source buffer
     * \param [in] srcBufferOffset Source subresource offset
     * \param [in] srcOffset Source image offset
     * \param [in] srcSize Total size of the source image
     * \param [in] extent Number of pixels to copy
     * \param [in] elementSize Pixel size, in bytes
     */
    void copyPackedBufferImage(
      const Rc<DxvkBuffer>&       dstBuffer,
            VkDeviceSize          dstBufferOffset,
            VkOffset3D            dstOffset,
            VkExtent3D            dstSize,
      const Rc<DxvkBuffer>&       srcBuffer,
            VkDeviceSize          srcBufferOffset,
            VkOffset3D            srcOffset,
            VkExtent3D            srcSize,
            VkExtent3D            extent,
            VkDeviceSize          elementSize);

    /**
     * \brief Unpacks buffer data to a depth-stencil image
     * 
     * Writes the packed depth-stencil data to an image.
     * See \ref copyDepthStencilImageToPackedBuffer for
     * which formats are supported and how they are packed.
     * \param [in] dstImage Destination image
     * \param [in] dstSubresource Destination subresource
     * \param [in] dstOffset Image area offset
     * \param [in] dstExtent Image area size
     * \param [in] srcBuffer Packed data buffer
     * \param [in] srcBufferOffset Buffer offset of source image
     * \param [in] srcOffset Offset into the source image
     * \param [in] srcExtent Total size of the source image
     * \param [in] format Packed data format
     */
    void copyPackedBufferToDepthStencilImage(
      const Rc<DxvkImage>&        dstImage,
            VkImageSubresourceLayers dstSubresource,
            VkOffset2D            dstOffset,
            VkExtent2D            dstExtent,
      const Rc<DxvkBuffer>&       srcBuffer,
            VkDeviceSize          srcBufferOffset,
            VkOffset2D            srcOffset,
            VkExtent2D            srcExtent,
            VkFormat              format);

    /**
     * \brief Copies pages from a sparse resource to a buffer
     *
     * \param [in] dstBuffer Buffer to write to
     * \param [in] dstOffset Buffer offset
     * \param [in] srcResource Source resource
     * \param [in] pageCount Number of pages to copy
     * \param [in] pages Page indices to copy
     */
    void copySparsePagesToBuffer(
      const Rc<DxvkBuffer>&       dstBuffer,
            VkDeviceSize          dstOffset,
      const Rc<DxvkPagedResource>& srcResource,
            uint32_t              pageCount,
      const uint32_t*             pages);

    /**
     * \brief Copies pages from a buffer to a sparse resource
     *
     * \param [in] dstResource Resource to write to
     * \param [in] pageCount Number of pages to copy
     * \param [in] pages Page indices to copy
     * \param [in] srcBuffer Source buffer
     * \param [in] srcOffset Buffer offset
     */
    void copySparsePagesFromBuffer(
      const Rc<DxvkPagedResource>& dstResource,
            uint32_t              pageCount,
      const uint32_t*             pages,
      const Rc<DxvkBuffer>&       srcBuffer,
            VkDeviceSize          srcOffset);

    /**
     * \brief Discards a buffer
     * 
     * Renames the buffer in case it is currently
     * used by the GPU in order to avoid having to
     * insert barriers before future commands using
     * the buffer.
     * \param [in] buffer The buffer to discard
     */
    void discardBuffer(
      const Rc<DxvkBuffer>&       buffer);
    
    /**
     * \brief Discards contents of an image view
     * 
     * Discards the current contents of the image
     * and performs a fast layout transition. This
     * may improve performance in some cases.
     * \param [in] imageView View to discard
     * \param [in] discardAspects Image aspects to discard
     */
    void discardImageView(
      const Rc<DxvkImageView>&      imageView,
            VkImageAspectFlags      discardAspects);

    /**
     * \brief Starts compute jobs
     * 
     * \param [in] x Number of threads in X direction
     * \param [in] y Number of threads in Y direction
     * \param [in] z Number of threads in Z direction
     */
    void dispatch(
            uint32_t          x,
            uint32_t          y,
            uint32_t          z);
    
    /**
     * \brief Indirect dispatch call
     * 
     * Takes arguments from a buffer. The buffer must contain
     * a structure of the type \c VkDispatchIndirectCommand.
     * \param [in] offset Draw buffer offset
     */
    void dispatchIndirect(
            VkDeviceSize      offset);
    
    /**
     * \brief Draws primitive without using an index buffer
     * 
     * \param [in] vertexCount Number of vertices to draw
     * \param [in] instanceCount Number of instances to render
     * \param [in] firstVertex First vertex in vertex buffer
     * \param [in] firstInstance First instance ID
     */
    void draw(
            uint32_t          vertexCount,
            uint32_t          instanceCount,
            uint32_t          firstVertex,
            uint32_t          firstInstance);
    
    /**
     * \brief Indirect draw call
     * 
     * Takes arguments from a buffer. The structure stored
     * in the buffer must be of type \c VkDrawIndirectCommand.
     * \param [in] offset Draw buffer offset
     * \param [in] count Number of draws
     * \param [in] stride Stride between dispatch calls
     */
    void drawIndirect(
            VkDeviceSize      offset,
            uint32_t          count,
            uint32_t          stride);
    
    /**
     * \brief Indirect draw call
     * 
     * Takes arguments from a buffer. The structure stored
     * in the buffer must be of type \c VkDrawIndirectCommand.
     * \param [in] offset Draw buffer offset
     * \param [in] countOffset Draw count offset
     * \param [in] maxCount Maximum number of draws
     * \param [in] stride Stride between dispatch calls
     */
    void drawIndirectCount(
            VkDeviceSize      offset,
            VkDeviceSize      countOffset,
            uint32_t          maxCount,
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
            int32_t  vertexOffset,
            uint32_t firstInstance);
    
    /**
     * \brief Indirect indexed draw call
     * 
     * Takes arguments from a buffer. The structure type for
     * the draw buffer is \c VkDrawIndexedIndirectCommand.
     * \param [in] offset Draw buffer offset
     * \param [in] count Number of draws
     * \param [in] stride Stride between dispatch calls
     */
    void drawIndexedIndirect(
            VkDeviceSize      offset,
            uint32_t          count,
            uint32_t          stride);
    
    /**
     * \brief Indirect indexed draw call
     * 
     * Takes arguments from a buffer. The structure type for
     * the draw buffer is \c VkDrawIndexedIndirectCommand.
     * \param [in] offset Draw buffer offset
     * \param [in] countOffset Draw count offset
     * \param [in] maxCount Maximum number of draws
     * \param [in] stride Stride between dispatch calls
     */
    void drawIndexedIndirectCount(
            VkDeviceSize      offset,
            VkDeviceSize      countOffset,
            uint32_t          maxCount,
            uint32_t          stride);
    
    /**
     * \brief Transform feddback draw call

     * \param [in] counterBuffer Xfb counter buffer
     * \param [in] counterDivisor Vertex stride
     * \param [in] counterBias Counter bias
     */
    void drawIndirectXfb(
      const DxvkBufferSlice&  counterBuffer,
            uint32_t          counterDivisor,
            uint32_t          counterBias);
    
    /**
     * \brief Emits graphics barrier
     *
     * Needs to be used when the fragment shader reads a bound
     * render target, or when subsequent draw calls access any
     * given resource for writing. It is assumed that no hazards
     * can happen between storage descriptors and other resources.
     * \param [in] srcStages Source pipeline stages
     * \param [in] srcAccess Source access
     * \param [in] dstStages Destination pipeline stages
     * \param [in] dstAccess Destination access
     */
    void emitGraphicsBarrier(
            VkPipelineStageFlags      srcStages,
            VkAccessFlags             srcAccess,
            VkPipelineStageFlags      dstStages,
            VkAccessFlags             dstAccess);

    /**
     * \brief Emits buffer barrier
     *
     * Can be used to transition foreign resources
     * into a state that DXVK can work with.
     * \param [in] resource Buffer resource
     * \param [in] srcStages Source pipeline stages
     * \param [in] srcAccess Source access
     * \param [in] dstStages Destination pipeline stages
     * \param [in] dstAccess Destination access
     */
    void emitBufferBarrier(
      const Rc<DxvkBuffer>&           resource,
            VkPipelineStageFlags      srcStages,
            VkAccessFlags             srcAccess,
            VkPipelineStageFlags      dstStages,
            VkAccessFlags             dstAccess);

    /**
     * \brief Emits image barrier
     *
     * Can be used to transition foreign resources
     * into a state that DXVK can work with.
     * \param [in] resource Image resource
     * \param [in] srcLayout Current image layout
     * \param [in] srcStages Source pipeline stages
     * \param [in] srcAccess Source access
     * \param [in] dstLayout New image layout
     * \param [in] dstStages Destination pipeline stages
     * \param [in] dstAccess Destination access
     */
    void emitImageBarrier(
      const Rc<DxvkImage>&            resource,
            VkImageLayout             srcLayout,
            VkPipelineStageFlags      srcStages,
            VkAccessFlags             srcAccess,
            VkImageLayout             dstLayout,
            VkPipelineStageFlags      dstStages,
            VkAccessFlags             dstAccess);

    /**
     * \brief Generates mip maps
     * 
     * Uses blitting to generate lower mip levels from
     * the top-most mip level passed to this method.
     * \param [in] imageView The image to generate mips for
     * \param [in] filter The filter to use for generation
     */
    void generateMipmaps(
      const Rc<DxvkImageView>&        imageView,
            VkFilter                  filter);

    /**
     * \brief Initializes a buffer
     *
     * Clears the given buffer to zero. Only safe to call
     * if the buffer is not currently in use by the GPU.
     * \param [in] buffer Buffer to clear
     */
    void initBuffer(
      const Rc<DxvkBuffer>&           buffer);

    /**
     * \brief Initializes an image
     * 
     * Transitions the image into its default layout, and clears
     * it to black unless the initial layout is preinitialized.
     * Only safe to call if the image is not in use by the GPU.
     * \param [in] image The image to initialize
     * \param [in] subresources Image subresources
     * \param [in] initialLayout Initial image layout
     */
    void initImage(
      const Rc<DxvkImage>&            image,
      const VkImageSubresourceRange&  subresources,
            VkImageLayout             initialLayout);

    /**
     * \brief Initializes sparse image
     *
     * Binds any metadata aspects that the image might
     * have, and performs the initial layout transition.
     * \param [in] image Image to initialize
     */
    void initSparseImage(
      const Rc<DxvkImage>&            image);

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
     * \param [in] slice New buffer slice handle
     */
    void invalidateBuffer(
      const Rc<DxvkBuffer>&           buffer,
      const DxvkBufferSliceHandle&    slice);
    
    /**
     * \brief Updates push constants
     * 
     * Updates the given push constant range.
     * \param [in] offset Byte offset of data to update
     * \param [in] size Number of bytes to update
     * \param [in] data Pointer to raw data
     */
    void pushConstants(
            uint32_t                  offset,
            uint32_t                  size,
      const void*                     data) {
      std::memcpy(&m_state.pc.data[offset], data, size);

      m_flags.set(DxvkContextFlag::DirtyPushConstants);
    }

    /**
     * \brief Resolves a multisampled image resource
     * 
     * Resolves a multisampled image into a non-multisampled
     * image. The subresources of both images must have the
     * same size and compatible formats.
     * A format can be specified for the resolve operation.
     * If it is \c VK_FORMAT_UNDEFINED, the resolve operation
     * will use the source image format.
     * \param [in] dstImage Destination image
     * \param [in] srcImage Source image
     * \param [in] region Region to resolve
     * \param [in] format Format for the resolve operation
     */
    void resolveImage(
      const Rc<DxvkImage>&            dstImage,
      const Rc<DxvkImage>&            srcImage,
      const VkImageResolve&           region,
            VkFormat                  format);
    
    /**
     * \brief Resolves a multisampled depth-stencil resource
     * 
     * \param [in] dstImage Destination image
     * \param [in] srcImage Source image
     * \param [in] region Region to resolve
     * \param [in] depthMode Resolve mode for depth aspect
     * \param [in] stencilMode Resolve mode for stencil aspect
     */
    void resolveDepthStencilImage(
      const Rc<DxvkImage>&            dstImage,
      const Rc<DxvkImage>&            srcImage,
      const VkImageResolve&           region,
            VkResolveModeFlagBits     depthMode,
            VkResolveModeFlagBits     stencilMode);

    /**
     * \brief Transforms image subresource layouts
     * 
     * \param [in] dstImage Image to transform
     * \param [in] dstSubresources Subresources
     * \param [in] srcLayout Current layout
     * \param [in] dstLayout Desired layout
     */
    void transformImage(
      const Rc<DxvkImage>&            dstImage,
      const VkImageSubresourceRange&  dstSubresources,
            VkImageLayout             srcLayout,
            VkImageLayout             dstLayout);
    
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
     * \brief Updates an depth-stencil image
     * 
     * \param [in] image Destination image
     * \param [in] subsresources Image subresources to update
     * \param [in] imageOffset Offset of the image area to update
     * \param [in] imageExtent Size of the image area to update
     * \param [in] data Source data
     * \param [in] pitchPerRow Row pitch of the source data
     * \param [in] pitchPerLayer Layer pitch of the source data
     * \param [in] format Packed depth-stencil format
     */
    void updateDepthStencilImage(
      const Rc<DxvkImage>&            image,
      const VkImageSubresourceLayers& subresources,
            VkOffset2D                imageOffset,
            VkExtent2D                imageExtent,
      const void*                     data,
            VkDeviceSize              pitchPerRow,
            VkDeviceSize              pitchPerLayer,
            VkFormat                  format);
    
    /**
     * \brief Uses transfer queue to initialize buffer
     * 
     * Only safe to use if the buffer is not in use by the GPU.
     * \param [in] buffer The buffer to initialize
     * \param [in] data The data to copy to the buffer
     */
    void uploadBuffer(
      const Rc<DxvkBuffer>&           buffer,
      const void*                     data);
    
    /**
     * \brief Uses transfer queue to initialize image
     * 
     * Only safe to use if the image is not in use by the GPU.
     * \param [in] image The image to initialize
     * \param [in] subresources Subresources to initialize
     * \param [in] data Source data
     * \param [in] pitchPerRow Row pitch of the source data
     * \param [in] pitchPerLayer Layer pitch of the source data
     */
    void uploadImage(
      const Rc<DxvkImage>&            image,
      const VkImageSubresourceLayers& subresources,
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
            DxvkBlendConstants  blendConstants);
    
    /**
     * \brief Sets depth bias
     * 
     * Depth bias has to be enabled explicitly in
     * the rasterizer state to have any effect.
     * \param [in] depthBias Depth bias values
     */
    void setDepthBias(
            DxvkDepthBias       depthBias);

    /**
     * \brief Sets depth bias representation
     *
     * \param [in] depthBiasRepresentation Depth bias representation
     */
    void setDepthBiasRepresentation(
            DxvkDepthBiasRepresentation  depthBiasRepresentation);
    
    /**
     * \brief Sets depth bounds
     *
     * Enables or disables the depth bounds test,
     * and updates the values if necessary.
     * \param [in] depthBounds Depth bounds
     */
    void setDepthBounds(
            DxvkDepthBounds     depthBounds);
    
    /**
     * \brief Sets stencil reference
     * 
     * Sets the reference value for stencil compare operations.
     * \param [in] reference Reference value
     */
    void setStencilReference(
            uint32_t            reference);
    
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
     * \brief Sets specialization constants
     * 
     * Replaces current specialization constants
     * with the given list of constant entries.
     * \param [in] pipeline Graphics or Compute pipeline
     * \param [in] index Constant index
     * \param [in] value Constant value
     */
    void setSpecConstant(
            VkPipelineBindPoint pipeline,
            uint32_t            index,
            uint32_t            value) {
      auto& scState = pipeline == VK_PIPELINE_BIND_POINT_GRAPHICS
        ? m_state.gp.constants : m_state.cp.constants;
      
      if (scState.data[index] != value) {
        scState.data[index] = value;

        if (scState.mask & (1u << index)) {
          m_flags.set(pipeline == VK_PIPELINE_BIND_POINT_GRAPHICS
            ? DxvkContextFlag::GpDirtySpecConstants
            : DxvkContextFlag::CpDirtySpecConstants);
        }
      }
    }
    
    /**
     * \brief Sets barrier control flags
     *
     * Barrier control flags can be used to control
     * implicit synchronization of compute shaders.
     * \param [in] control New barrier control flags
     */
    void setBarrierControl(
            DxvkBarrierControlFlags control);

    /**
     * \brief Updates page table for a given sparse resource
     *
     * Note that this is a very high overhead operation.
     * \param [in] bindInfo Sparse bind info
     * \param [in] flags Sparse bind flags
     */
    void updatePageTable(
      const DxvkSparseBindInfo&   bindInfo,
            DxvkSparseBindFlags   flags);

    /**
     * \brief Launches a Cuda kernel
     *
     * Since the kernel is launched with an opaque set of
     * kernel-specific parameters which may reference
     * resources bindlessly, such resources must be listed by
     * the caller in the 'buffers' and 'images' parameters so
     * that their access may be tracked appropriately.
     * \param [in] nvxLaunchInfo Kernel launch parameter struct
     * \param [in] buffers List of {buffer,read,write} used by kernel
     * \param [in] images List of {image,read,write} used by kernel
     */
    void launchCuKernelNVX(
      const VkCuLaunchInfoNVX& nvxLaunchInfo,
      const std::vector<std::pair<Rc<DxvkBuffer>, DxvkAccessFlags>>& buffers,
      const std::vector<std::pair<Rc<DxvkImage>, DxvkAccessFlags>>& images);
    
    /**
     * \brief Signals a GPU event
     * \param [in] event The event
     */
    void signalGpuEvent(
      const Rc<DxvkGpuEvent>&   event);
    
    /**
     * \brief Writes to a timestamp query
     * \param [in] query The timestamp query
     */
    void writeTimestamp(
      const Rc<DxvkGpuQuery>&   query);
    
    /**
     * \brief Queues a signal
     * 
     * The signal will be notified after all
     * previously submitted commands have
     * finished execution on the GPU.
     * \param [in] signal The signal
     * \param [in] value Signal value
     */
    void signal(
      const Rc<sync::Signal>&   signal,
            uint64_t            value);

    /**
     * \brief Waits for fence
     *
     * Stalls current command list execution until
     * the fence reaches the given value or higher.
     * \param [in] fence Fence to wait on
     * \param [in] value Value to wait on
     */
    void waitFence(const Rc<DxvkFence>& fence, uint64_t value);

    /**
     * \brief Signals fence
     *
     * Signals fence to the given value once the current
     * command list execution completes on the GPU.
     * \param [in] fence Fence to signal
     * \param [in] value Value to signal
     */
    void signalFence(const Rc<DxvkFence>& fence, uint64_t value);

    /**
     * \brief Begins a debug label region
     * \param [in] label The debug label
     *
     * Marks the start of a debug label region. Used by debugging/profiling
     * tools to mark different workloads within a frame.
     */
    void beginDebugLabel(VkDebugUtilsLabelEXT *label);

    /**
     * \brief Ends a debug label region
     *
     * Marks the close of a debug label region. Used by debugging/profiling
     * tools to mark different workloads within a frame.
     */
    void endDebugLabel();

    /**
     * \brief Inserts a debug label
     * \param [in] label The debug label
     *
     * Inserts an instantaneous debug label. Used by debugging/profiling
     * tools to mark different workloads within a frame.
     */
    void insertDebugLabel(VkDebugUtilsLabelEXT *label);

    /**
     * \brief Inserts a marker object
     * \param [in] marker The marker
     */
    template<typename T>
    void insertMarker(const Rc<DxvkMarker<T>>& marker) {
      m_cmd->trackResource<DxvkAccess::Write>(marker);
    }

    /**
     * \brief Increments a given stat counter
     *
     * The stat counters will be merged into the global
     * stat counters upon execution of the command list.
     * \param [in] counter Stat counter to increment
     * \param [in] value Increment value
     */
    void addStatCtr(DxvkStatCounter counter, uint64_t value) {
      if (m_cmd != nullptr)
        m_cmd->addStatCtr(counter, value);
    }

  private:
    
    Rc<DxvkDevice>          m_device;
    DxvkContextType         m_type;
    DxvkObjects*            m_common;
    
    Rc<DxvkCommandList>     m_cmd;
    Rc<DxvkBuffer>          m_zeroBuffer;

    DxvkContextFlags        m_flags;
    DxvkContextState        m_state;
    DxvkContextFeatures     m_features;
    DxvkDescriptorState     m_descriptorState;

    Rc<DxvkDescriptorPool>  m_descriptorPool;
    Rc<DxvkDescriptorManager> m_descriptorManager;

    DxvkBarrierSet          m_sdmaAcquires;
    DxvkBarrierSet          m_sdmaBarriers;
    DxvkBarrierSet          m_initBarriers;
    DxvkBarrierSet          m_execAcquires;
    DxvkBarrierSet          m_execBarriers;
    DxvkBarrierControlFlags m_barrierControl;

    DxvkGpuQueryManager     m_queryManager;
    DxvkStagingBuffer       m_staging;
    
    DxvkGlobalPipelineBarrier m_globalRoGraphicsBarrier;
    DxvkGlobalPipelineBarrier m_globalRwGraphicsBarrier;

    DxvkRenderTargetLayouts m_rtLayouts = { };

    DxvkBindingSet<MaxNumVertexBindings + 1>  m_vbTracked;
    DxvkBindingSet<MaxNumResourceSlots>       m_rcTracked;

    std::vector<DxvkDeferredClear> m_deferredClears;

    std::vector<VkWriteDescriptorSet> m_descriptorWrites;
    std::vector<DxvkDescriptorInfo>   m_descriptors;

    std::array<DxvkShaderResourceSlot, MaxNumResourceSlots>  m_rc;
    std::array<DxvkGraphicsPipeline*, 4096> m_gpLookupCache = { };
    std::array<DxvkComputePipeline*,   256> m_cpLookupCache = { };

    void blitImageFb(
      const Rc<DxvkImage>&        dstImage,
      const Rc<DxvkImage>&        srcImage,
      const VkImageBlit&          region,
      const VkComponentMapping&   mapping,
            VkFilter              filter);

    void blitImageHw(
      const Rc<DxvkImage>&        dstImage,
      const Rc<DxvkImage>&        srcImage,
      const VkImageBlit&          region,
            VkFilter              filter);

    template<bool ToImage>
    void copyImageBufferData(
            DxvkCmdBuffer         cmd,
      const Rc<DxvkImage>&        image,
      const VkImageSubresourceLayers& imageSubresource,
            VkOffset3D            imageOffset,
            VkExtent3D            imageExtent,
            VkImageLayout         imageLayout,
      const DxvkBufferSliceHandle& bufferSlice,
            VkDeviceSize          bufferRowAlignment,
            VkDeviceSize          bufferSliceAlignment);

    void copyImageHostData(
            DxvkCmdBuffer         cmd,
      const Rc<DxvkImage>&        image,
      const VkImageSubresourceLayers& imageSubresource,
            VkOffset3D            imageOffset,
            VkExtent3D            imageExtent,
      const void*                 hostData,
            VkDeviceSize          rowPitch,
            VkDeviceSize          slicePitch);

    void clearImageViewFb(
      const Rc<DxvkImageView>&    imageView,
            VkOffset3D            offset,
            VkExtent3D            extent,
            VkImageAspectFlags    aspect,
            VkClearValue          value);
    
    void clearImageViewCs(
      const Rc<DxvkImageView>&    imageView,
            VkOffset3D            offset,
            VkExtent3D            extent,
            VkClearValue          value);
    
    void copyImageHw(
      const Rc<DxvkImage>&        dstImage,
            VkImageSubresourceLayers dstSubresource,
            VkOffset3D            dstOffset,
      const Rc<DxvkImage>&        srcImage,
            VkImageSubresourceLayers srcSubresource,
            VkOffset3D            srcOffset,
            VkExtent3D            extent);
    
    void copyImageFb(
      const Rc<DxvkImage>&        dstImage,
            VkImageSubresourceLayers dstSubresource,
            VkOffset3D            dstOffset,
      const Rc<DxvkImage>&        srcImage,
            VkImageSubresourceLayers srcSubresource,
            VkOffset3D            srcOffset,
            VkExtent3D            extent);
    
    void copyImageFbDirect(
      const Rc<DxvkImage>&        dstImage,
            VkImageSubresourceLayers dstSubresource,
            VkOffset3D            dstOffset,
            VkFormat              dstFormat,
      const Rc<DxvkImage>&        srcImage,
            VkImageSubresourceLayers srcSubresource,
            VkOffset3D            srcOffset,
            VkFormat              srcFormat,
            VkExtent3D            extent);

    bool copyImageClear(
      const Rc<DxvkImage>&        dstImage,
            VkImageSubresourceLayers dstSubresource,
            VkOffset3D            dstOffset,
            VkExtent3D            dstExtent,
      const Rc<DxvkImage>&        srcImage,
            VkImageSubresourceLayers srcSubresource);

    template<bool ToBuffer>
    void copySparsePages(
      const Rc<DxvkPagedResource>& sparse,
            uint32_t              pageCount,
      const uint32_t*             pages,
      const Rc<DxvkBuffer>&       buffer,
            VkDeviceSize          offset);

    template<bool ToBuffer>
    void copySparseBufferPages(
      const Rc<DxvkBuffer>&       sparse,
            uint32_t              pageCount,
      const uint32_t*             pages,
      const Rc<DxvkBuffer>&       buffer,
            VkDeviceSize          offset);

    template<bool ToBuffer>
    void copySparseImagePages(
      const Rc<DxvkImage>&        sparse,
            uint32_t              pageCount,
      const uint32_t*             pages,
      const Rc<DxvkBuffer>&       buffer,
            VkDeviceSize          offset);

    void resolveImageHw(
      const Rc<DxvkImage>&            dstImage,
      const Rc<DxvkImage>&            srcImage,
      const VkImageResolve&           region);
    
    void resolveImageDs(
      const Rc<DxvkImage>&            dstImage,
      const Rc<DxvkImage>&            srcImage,
      const VkImageResolve&           region,
            VkResolveModeFlagBits     depthMode,
            VkResolveModeFlagBits     stencilMode);
    
    void resolveImageFb(
      const Rc<DxvkImage>&            dstImage,
      const Rc<DxvkImage>&            srcImage,
      const VkImageResolve&           region,
            VkFormat                  format,
            VkResolveModeFlagBits     depthMode,
            VkResolveModeFlagBits     stencilMode);
    
    void resolveImageFbDirect(
      const Rc<DxvkImage>&            dstImage,
      const Rc<DxvkImage>&            srcImage,
      const VkImageResolve&           region,
            VkFormat                  format,
            VkResolveModeFlagBits     depthMode,
            VkResolveModeFlagBits     stencilMode);
    
    void performClear(
      const Rc<DxvkImageView>&        imageView,
            int32_t                   attachmentIndex,
            VkImageAspectFlags        discardAspects,
            VkImageAspectFlags        clearAspects,
            VkClearValue              clearValue);

    void deferClear(
      const Rc<DxvkImageView>&        imageView,
            VkImageAspectFlags        clearAspects,
            VkClearValue              clearValue);

    void deferDiscard(
      const Rc<DxvkImageView>&        imageView,
            VkImageAspectFlags        discardAspects);

    void flushClears(
            bool                      useRenderPass);

    void flushSharedImages();

    void startRenderPass();
    void spillRenderPass(bool suspend);
    
    void renderPassEmitInitBarriers(
      const DxvkFramebufferInfo&  framebufferInfo,
      const DxvkRenderPassOps&    ops);

    void renderPassEmitPostBarriers(
      const DxvkFramebufferInfo&  framebufferInfo,
      const DxvkRenderPassOps&    ops);

    void renderPassBindFramebuffer(
      const DxvkFramebufferInfo&  framebufferInfo,
      const DxvkRenderPassOps&    ops);
    
    void renderPassUnbindFramebuffer();
    
    void resetRenderPassOps(
      const DxvkRenderTargets&    renderTargets,
            DxvkRenderPassOps&    renderPassOps);

    void startTransformFeedback();
    void pauseTransformFeedback();
    
    void unbindComputePipeline();
    bool updateComputePipelineState();
    
    void unbindGraphicsPipeline();
    bool updateGraphicsPipeline();
    bool updateGraphicsPipelineState(DxvkGlobalPipelineBarrier srcBarrier);

    template<VkPipelineBindPoint BindPoint>
    void resetSpecConstants(
            uint32_t                newMask);

    template<VkPipelineBindPoint BindPoint>
    void updateSpecConstants();

    void invalidateState();

    template<VkPipelineBindPoint BindPoint>
    void updateResourceBindings(const DxvkBindingLayoutObjects* layout);

    void updateComputeShaderResources();
    void updateGraphicsShaderResources();

    DxvkFramebufferInfo makeFramebufferInfo(
      const DxvkRenderTargets&      renderTargets);

    void updateFramebuffer();
    
    void applyRenderTargetLoadLayouts();

    void applyRenderTargetStoreLayouts();

    void transitionRenderTargetLayouts(
            bool                    sharedOnly);

    void transitionColorAttachment(
      const DxvkAttachment&         attachment,
            VkImageLayout           oldLayout);

    void transitionDepthAttachment(
      const DxvkAttachment&         attachment,
            VkImageLayout           oldLayout);

    void updateRenderTargetLayouts(
      const DxvkFramebufferInfo&    newFb,
      const DxvkFramebufferInfo&    oldFb);

    void prepareImage(
      const Rc<DxvkImage>&          image,
      const VkImageSubresourceRange& subresources,
            bool                    flushClears = true);

    bool updateIndexBufferBinding();
    void updateVertexBufferBindings();

    void updateTransformFeedbackBuffers();
    void updateTransformFeedbackState();

    void updateDynamicState();

    template<VkPipelineBindPoint BindPoint>
    void updatePushConstants();
    
    bool commitComputeState();
    
    template<bool Indexed, bool Indirect>
    bool commitGraphicsState();
    
    template<bool DoEmit>
    void commitComputeBarriers();

    void commitComputePostBarriers();
    
    template<bool Indexed, bool Indirect, bool DoEmit>
    void commitGraphicsBarriers();

    template<bool DoEmit>
    bool checkBufferBarrier(
      const DxvkBufferSlice&          bufferSlice,
            VkPipelineStageFlags      stages,
            VkAccessFlags             access);

    template<bool DoEmit>
    bool checkBufferViewBarrier(
      const Rc<DxvkBufferView>&       bufferView,
            VkPipelineStageFlags      stages,
            VkAccessFlags             access);

    template<bool DoEmit>
    bool checkImageViewBarrier(
      const Rc<DxvkImageView>&        imageView,
            VkPipelineStageFlags      stages,
            VkAccessFlags             access);

    bool canIgnoreWawHazards(
            VkPipelineStageFlags      stages);

    void emitMemoryBarrier(
            VkPipelineStageFlags      srcStages,
            VkAccessFlags             srcAccess,
            VkPipelineStageFlags      dstStages,
            VkAccessFlags             dstAccess);
    
    void trackDrawBuffer();

    bool tryInvalidateDeviceLocalBuffer(
      const Rc<DxvkBuffer>&           buffer,
            VkDeviceSize              copySize);

    DxvkGraphicsPipeline* lookupGraphicsPipeline(
      const DxvkGraphicsPipelineShaders&  shaders);

    DxvkComputePipeline* lookupComputePipeline(
      const DxvkComputePipelineShaders&   shaders);
    
    Rc<DxvkBuffer> createZeroBuffer(
            VkDeviceSize              size);

    void resizeDescriptorArrays(
            uint32_t                  bindingCount);

    void beginCurrentCommands();

    void endCurrentCommands();

    void splitCommands();

  };
  
}
