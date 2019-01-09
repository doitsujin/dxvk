#pragma once

#include <limits>

#include "dxvk_bind_mask.h"
#include "dxvk_buffer.h"
#include "dxvk_descriptor.h"
#include "dxvk_event.h"
#include "dxvk_lifetime.h"
#include "dxvk_limits.h"
#include "dxvk_pipelayout.h"
#include "dxvk_query_tracker.h"
#include "dxvk_staging.h"
#include "dxvk_stats.h"

namespace dxvk {
  
  /**
   * \brief Command buffer flags
   * 
   * A set of flags used to specify which of
   * the command buffers need to be submitted.
   */
  enum class DxvkCmdBufferFlag : uint32_t {
    InitBuffer = 0,
    ExecBuffer = 1,
  };
  
  using DxvkCmdBufferFlags = Flags<DxvkCmdBufferFlag>;
  
  /**
   * \brief DXVK command list
   * 
   * Stores a command buffer that a context can use to record Vulkan
   * commands. The command list shall also reference the resources
   * used by the recorded commands for automatic lifetime tracking.
   * When the command list has completed execution, resources that
   * are no longer used may get destroyed.
   */
  class DxvkCommandList : public RcObject {
    
  public:
    
    DxvkCommandList(
            DxvkDevice*       device,
            uint32_t          queueFamily);
    ~DxvkCommandList();
    
    /**
     * \brief Submits command list
     * 
     * \param [in] queue Device queue
     * \param [in] waitSemaphore Semaphore to wait on
     * \param [in] wakeSemaphore Semaphore to signal
     * \returns Submission status
     */
    VkResult submit(
            VkQueue         queue,
            VkSemaphore     waitSemaphore,
            VkSemaphore     wakeSemaphore);
    
    /**
     * \brief Synchronizes command buffer execution
     * 
     * Waits for the fence associated with
     * this command buffer to get signaled.
     * \returns Synchronization status
     */
    VkResult synchronize();
    
    /**
     * \brief Stat counters
     * 
     * Retrieves some info about per-command list
     * statistics, such as the number of draw calls
     * or the number of pipelines compiled.
     * \returns Reference to stat counters
     */
    DxvkStatCounters& statCounters() {
      return m_statCounters;
    }
    
    /**
     * \brief Increments a stat counter value
     * 
     * \param [in] ctr The counter to increment
     * \param [in] val The value to add
     */
    void addStatCtr(DxvkStatCounter ctr, uint32_t val) {
      m_statCounters.addCtr(ctr, val);
    }
    
    /**
     * \brief Begins recording
     * 
     * Resets the command buffer and
     * begins command buffer recording.
     */
    void beginRecording();
    
    /**
     * \brief Ends recording
     * 
     * Ends command buffer recording, making
     * the command list ready for submission.
     * \param [in] stats Stat counters
     */
    void endRecording();
    
    /**
     * \brief Frees buffer slice
     * 
     * After the command buffer execution has finished,
     * the given buffer slice will be released to the
     * virtual buffer object so that it can be reused.
     * \param [in] buffer The virtual buffer object
     * \param [in] slice The buffer slice handle
     */
    void freeBufferSlice(
      const Rc<DxvkBuffer>&           buffer,
      const DxvkBufferSliceHandle&    slice) {
      m_bufferTracker.freeBufferSlice(buffer, slice);
    }
    
    /**
     * \brief Adds a resource to track
     * 
     * Adds a resource to the internal resource tracker.
     * Resources will be kept alive and "in use" until
     * the device can guarantee that the submission has
     * completed.
     */
    void trackResource(Rc<DxvkResource> rc) {
      m_resources.trackResource(std::move(rc));
    }
    
    /**
     * \brief Adds a query range to track
     * 
     * Query data will be retrieved and written back to
     * the query objects after the command buffer has
     * finished executing on the GPU.
     * \param [in] queries The query range
     */
    void trackQueryRange(DxvkQueryRange&& queries) {
      m_queryTracker.trackQueryRange(std::move(queries));
    }
    
    /**
     * \brief Adds an event revision to track
     * 
     * The event will be signaled after the command
     * buffer has finished executing on the GPU.
     */
    void trackEvent(const DxvkEventRevision& event) {
      m_eventTracker.trackEvent(event);
    }

    /**
     * \brief Tracks a descriptor pool
     * \param [in] pool The descriptor pool
     */
    void trackDescriptorPool(Rc<DxvkDescriptorPool> pool) {
      m_descriptorPoolTracker.trackDescriptorPool(pool);
    }
    
    /**
     * \brief Signals tracked events
     * 
     * Marks all tracked events as signaled. Call this after
     * synchronizing with a fence for this command list.
     */
    void signalEvents() {
      m_eventTracker.signalEvents();
    }
    
    /**
     * \brief Writes back query results
     * 
     * Writes back query data to all queries tracked by the
     * query range tracker. Call this after synchronizing
     * with a fence for this command list.
     */
    void writeQueryData() {
      m_queryTracker.writeQueryData();
    }
    
    /**
     * \brief Resets the command list
     * 
     * Resets the internal command buffer of the command list and
     * marks all tracked resources as unused. When submitting the
     * command list to the device, this method will be called once
     * the command list completes execution.
     */
    void reset();
    
    void updateDescriptorSets(
            uint32_t                      descriptorWriteCount,
      const VkWriteDescriptorSet*         pDescriptorWrites) {
      m_vkd->vkUpdateDescriptorSets(m_vkd->device(),
        descriptorWriteCount, pDescriptorWrites,
        0, nullptr);
    }
    
    
    void updateDescriptorSetWithTemplate(
            VkDescriptorSet               descriptorSet,
            VkDescriptorUpdateTemplateKHR descriptorTemplate,
      const void*                         data) {
      m_vkd->vkUpdateDescriptorSetWithTemplateKHR(m_vkd->device(),
        descriptorSet, descriptorTemplate, data);
    }
    
    
    void cmdBeginQuery(
            VkQueryPool             queryPool,
            uint32_t                query,
            VkQueryControlFlags     flags) {
      m_vkd->vkCmdBeginQuery(m_execBuffer,
        queryPool, query, flags);
    }
    
    
    void cmdBeginQueryIndexed(
            VkQueryPool             queryPool,
            uint32_t                query,
            VkQueryControlFlags     flags,
            uint32_t                index) {
      m_vkd->vkCmdBeginQueryIndexedEXT(
        m_execBuffer, queryPool, query, flags, index);
    }
    
    
    void cmdBeginRenderPass(
      const VkRenderPassBeginInfo*  pRenderPassBegin,
            VkSubpassContents       contents) {
      m_vkd->vkCmdBeginRenderPass(m_execBuffer,
        pRenderPassBegin, contents);
    }


    void cmdBeginTransformFeedback(
            uint32_t                  firstBuffer,
            uint32_t                  bufferCount,
      const VkBuffer*                 counterBuffers,
      const VkDeviceSize*             counterOffsets) {
      m_vkd->vkCmdBeginTransformFeedbackEXT(m_execBuffer,
        firstBuffer, bufferCount, counterBuffers, counterOffsets);
    }
    
    
    void cmdBindDescriptorSet(
            VkPipelineBindPoint       pipeline,
            VkPipelineLayout          pipelineLayout,
            VkDescriptorSet           descriptorSet,
            uint32_t                  dynamicOffsetCount,
      const uint32_t*                 pDynamicOffsets) {
      m_vkd->vkCmdBindDescriptorSets(m_execBuffer,
        pipeline, pipelineLayout, 0, 1,
        &descriptorSet, dynamicOffsetCount, pDynamicOffsets);
    }
    
    
    void cmdBindIndexBuffer(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            VkIndexType             indexType) {
      m_vkd->vkCmdBindIndexBuffer(m_execBuffer,
        buffer, offset, indexType);
    }
    
    
    void cmdBindPipeline(
            VkPipelineBindPoint     pipelineBindPoint,
            VkPipeline              pipeline) {
      m_vkd->vkCmdBindPipeline(m_execBuffer,
        pipelineBindPoint, pipeline);
    }


    void cmdBindTransformFeedbackBuffers(
            uint32_t                firstBinding,
            uint32_t                bindingCount,
      const VkBuffer*               pBuffers,
      const VkDeviceSize*           pOffsets,
      const VkDeviceSize*           pSizes) {
      m_vkd->vkCmdBindTransformFeedbackBuffersEXT(m_execBuffer,
        firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
    }
    
    
    void cmdBindVertexBuffers(
            uint32_t                firstBinding,
            uint32_t                bindingCount,
      const VkBuffer*               pBuffers,
      const VkDeviceSize*           pOffsets) {
      m_vkd->vkCmdBindVertexBuffers(m_execBuffer,
        firstBinding, bindingCount, pBuffers, pOffsets);
    }
    
    
    void cmdBlitImage(
            VkImage                 srcImage,
            VkImageLayout           srcImageLayout,
            VkImage                 dstImage,
            VkImageLayout           dstImageLayout,
            uint32_t                regionCount,
      const VkImageBlit*            pRegions,
            VkFilter                filter) {
      m_vkd->vkCmdBlitImage(m_execBuffer,
        srcImage, srcImageLayout,
        dstImage, dstImageLayout,
        regionCount, pRegions, filter);
    }
    
    
    void cmdClearAttachments(
            uint32_t                attachmentCount,
      const VkClearAttachment*      pAttachments,
            uint32_t                rectCount,
      const VkClearRect*            pRects) {
      m_vkd->vkCmdClearAttachments(m_execBuffer,
        attachmentCount, pAttachments,
        rectCount, pRects);
    }
    
    
    void cmdClearColorImage(
            VkImage                 image,
            VkImageLayout           imageLayout,
      const VkClearColorValue*      pColor,
            uint32_t                rangeCount,
      const VkImageSubresourceRange* pRanges) {
      m_vkd->vkCmdClearColorImage(m_execBuffer,
        image, imageLayout, pColor,
        rangeCount, pRanges);
    }
    
    
    void cmdClearDepthStencilImage(
            VkImage                 image,
            VkImageLayout           imageLayout,
      const VkClearDepthStencilValue* pDepthStencil,
            uint32_t                rangeCount,
      const VkImageSubresourceRange* pRanges) {
      m_vkd->vkCmdClearDepthStencilImage(m_execBuffer,
        image, imageLayout, pDepthStencil,
        rangeCount, pRanges);
    }
    
    
    void cmdCopyBuffer(
            VkBuffer                srcBuffer,
            VkBuffer                dstBuffer,
            uint32_t                regionCount,
      const VkBufferCopy*           pRegions) {
      m_vkd->vkCmdCopyBuffer(m_execBuffer,
        srcBuffer, dstBuffer,
        regionCount, pRegions);
    }
    
    
    void cmdCopyBufferToImage(
            VkBuffer                srcBuffer,
            VkImage                 dstImage,
            VkImageLayout           dstImageLayout,
            uint32_t                regionCount,
      const VkBufferImageCopy*      pRegions) {
      m_vkd->vkCmdCopyBufferToImage(m_execBuffer,
        srcBuffer, dstImage, dstImageLayout,
        regionCount, pRegions);
    }
    
    
    void cmdCopyImage(
            VkImage                 srcImage,
            VkImageLayout           srcImageLayout,
            VkImage                 dstImage,
            VkImageLayout           dstImageLayout,
            uint32_t                regionCount,
      const VkImageCopy*            pRegions) {
      m_vkd->vkCmdCopyImage(m_execBuffer,
        srcImage, srcImageLayout,
        dstImage, dstImageLayout,
        regionCount, pRegions);
    }
    
    
    void cmdCopyImageToBuffer(
            VkImage                 srcImage,
            VkImageLayout           srcImageLayout,
            VkBuffer                dstBuffer,
            uint32_t                regionCount,
      const VkBufferImageCopy*      pRegions) {
      m_vkd->vkCmdCopyImageToBuffer(m_execBuffer,
        srcImage, srcImageLayout, dstBuffer,
        regionCount, pRegions);
    }
    
    
    void cmdDispatch(
            uint32_t                x,
            uint32_t                y,
            uint32_t                z) {
      m_vkd->vkCmdDispatch(m_execBuffer, x, y, z);
    }
    
    
    void cmdDispatchIndirect(
            VkBuffer                buffer,
            VkDeviceSize            offset) {
      m_vkd->vkCmdDispatchIndirect(
        m_execBuffer, buffer, offset);
    }
    
    
    void cmdDraw(
            uint32_t                vertexCount,
            uint32_t                instanceCount,
            uint32_t                firstVertex,
            uint32_t                firstInstance) {
      m_vkd->vkCmdDraw(m_execBuffer,
        vertexCount, instanceCount,
        firstVertex, firstInstance);
    }
    
    
    void cmdDrawIndirect(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            uint32_t                drawCount,
            uint32_t                stride) {
      m_vkd->vkCmdDrawIndirect(m_execBuffer,
        buffer, offset, drawCount, stride);
    }
    
    
    void cmdDrawIndexed(
            uint32_t                indexCount,
            uint32_t                instanceCount,
            uint32_t                firstIndex,
            uint32_t                vertexOffset,
            uint32_t                firstInstance) {
      m_vkd->vkCmdDrawIndexed(m_execBuffer,
        indexCount, instanceCount,
        firstIndex, vertexOffset,
        firstInstance);
    }
    
    
    void cmdDrawIndexedIndirect(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            uint32_t                drawCount,
            uint32_t                stride) {
      m_vkd->vkCmdDrawIndexedIndirect(m_execBuffer,
        buffer, offset, drawCount, stride);
    }


    void cmdDrawIndirectVertexCount(
            uint32_t                instanceCount,
            uint32_t                firstInstance,
            VkBuffer                counterBuffer,
            VkDeviceSize            counterBufferOffset,
            uint32_t                counterOffset,
            uint32_t                vertexStride) {
      m_vkd->vkCmdDrawIndirectByteCountEXT(m_execBuffer,
        instanceCount, firstInstance, counterBuffer,
        counterBufferOffset, counterOffset, vertexStride);
    }
    
    
    void cmdEndQuery(
            VkQueryPool             queryPool,
            uint32_t                query) {
      m_vkd->vkCmdEndQuery(m_execBuffer, queryPool, query);
    }


    void cmdEndQueryIndexed(
            VkQueryPool             queryPool,
            uint32_t                query,
            uint32_t                index) {
      m_vkd->vkCmdEndQueryIndexedEXT(
        m_execBuffer, queryPool, query, index);
    }
    
    
    void cmdEndRenderPass() {
      m_vkd->vkCmdEndRenderPass(m_execBuffer);
    }
    
    
    void cmdEndTransformFeedback(
            uint32_t                  firstBuffer,
            uint32_t                  bufferCount,
      const VkBuffer*                 counterBuffers,
      const VkDeviceSize*             counterOffsets) {
      m_vkd->vkCmdEndTransformFeedbackEXT(m_execBuffer,
        firstBuffer, bufferCount, counterBuffers, counterOffsets);
    }


    void cmdFillBuffer(
            VkBuffer                dstBuffer,
            VkDeviceSize            dstOffset,
            VkDeviceSize            size,
            uint32_t                data) {
      m_vkd->vkCmdFillBuffer(m_execBuffer,
        dstBuffer, dstOffset, size, data);
    }
    
    
    void cmdPipelineBarrier(
            VkPipelineStageFlags    srcStageMask,
            VkPipelineStageFlags    dstStageMask,
            VkDependencyFlags       dependencyFlags,
            uint32_t                memoryBarrierCount,
      const VkMemoryBarrier*        pMemoryBarriers,
            uint32_t                bufferMemoryBarrierCount,
      const VkBufferMemoryBarrier*  pBufferMemoryBarriers,
            uint32_t                imageMemoryBarrierCount,
      const VkImageMemoryBarrier*   pImageMemoryBarriers) {
      m_vkd->vkCmdPipelineBarrier(m_execBuffer,
        srcStageMask, dstStageMask, dependencyFlags,
        memoryBarrierCount,       pMemoryBarriers,
        bufferMemoryBarrierCount, pBufferMemoryBarriers,
        imageMemoryBarrierCount,  pImageMemoryBarriers);
    }
    
    
    void cmdPushConstants(
            VkPipelineLayout        layout,
            VkShaderStageFlags      stageFlags,
            uint32_t                offset,
            uint32_t                size,
      const void*                   pValues) {
      m_vkd->vkCmdPushConstants(m_execBuffer,
        layout, stageFlags, offset, size, pValues);
    }
    
    
    void cmdResetQueryPool(
            VkQueryPool             queryPool,
            uint32_t                firstQuery,
            uint32_t                queryCount) {
      m_cmdBuffersUsed.set(DxvkCmdBufferFlag::InitBuffer);
      
      m_vkd->vkCmdResetQueryPool(m_initBuffer,
        queryPool, firstQuery, queryCount);
    }
    
    
    void cmdResolveImage(
            VkImage                 srcImage,
            VkImageLayout           srcImageLayout,
            VkImage                 dstImage,
            VkImageLayout           dstImageLayout,
            uint32_t                regionCount,
      const VkImageResolve*         pRegions) {
      m_vkd->vkCmdResolveImage(m_execBuffer,
        srcImage, srcImageLayout,
        dstImage, dstImageLayout,
        regionCount, pRegions);
    }
    
    
    void cmdUpdateBuffer(
            VkBuffer                dstBuffer,
            VkDeviceSize            dstOffset,
            VkDeviceSize            dataSize,
      const void*                   pData) {
      m_vkd->vkCmdUpdateBuffer(m_execBuffer,
        dstBuffer, dstOffset, dataSize, pData);
    }
    
    
    void cmdSetBlendConstants(const float blendConstants[4]) {
      m_vkd->vkCmdSetBlendConstants(m_execBuffer, blendConstants);
    }
    

    void cmdSetDepthBias(
            float                   depthBiasConstantFactor,
            float                   depthBiasClamp,
            float                   depthBiasSlopeFactor) {
      m_vkd->vkCmdSetDepthBias(m_execBuffer,
        depthBiasConstantFactor,
        depthBiasClamp,
        depthBiasSlopeFactor);
    }

    
    void cmdSetScissor(
            uint32_t                firstScissor,
            uint32_t                scissorCount,
      const VkRect2D*               scissors) {
      m_vkd->vkCmdSetScissor(m_execBuffer,
        firstScissor, scissorCount, scissors);
    }
    
    
    void cmdSetStencilReference(
            VkStencilFaceFlags      faceMask,
            uint32_t                reference) {
      m_vkd->vkCmdSetStencilReference(m_execBuffer,
        faceMask, reference);
    }
    
    
    void cmdSetViewport(
            uint32_t                firstViewport,
            uint32_t                viewportCount,
      const VkViewport*             viewports) {
      m_vkd->vkCmdSetViewport(m_execBuffer,
        firstViewport, viewportCount, viewports);
    }
    
    
    void cmdWriteTimestamp(
            VkPipelineStageFlagBits pipelineStage,
            VkQueryPool             queryPool,
            uint32_t                query) {
      m_vkd->vkCmdWriteTimestamp(m_execBuffer,
        pipelineStage, queryPool, query);
    }
    
    
    DxvkStagingBufferSlice stagedAlloc(
            VkDeviceSize            size);
    
    
    void stagedBufferCopy(
            VkBuffer                dstBuffer,
            VkDeviceSize            dstOffset,
            VkDeviceSize            dataSize,
      const DxvkStagingBufferSlice& dataSlice);
    
    
    void stagedBufferImageCopy(
            VkImage                 dstImage,
            VkImageLayout           dstImageLayout,
      const VkBufferImageCopy&      dstImageRegion,
      const DxvkStagingBufferSlice& dataSlice);
    
  private:
    
    Rc<vk::DeviceFn>    m_vkd;
    
    VkFence             m_fence;
    
    VkCommandPool       m_pool;
    VkCommandBuffer     m_execBuffer;
    VkCommandBuffer     m_initBuffer;
    
    DxvkCmdBufferFlags  m_cmdBuffersUsed;
    DxvkLifetimeTracker m_resources;
    DxvkDescriptorPoolTracker m_descriptorPoolTracker;
    DxvkStagingAlloc    m_stagingAlloc;
    DxvkQueryTracker    m_queryTracker;
    DxvkEventTracker    m_eventTracker;
    DxvkBufferTracker   m_bufferTracker;
    DxvkStatCounters    m_statCounters;
    
  };
  
}