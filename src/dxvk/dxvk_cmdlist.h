#pragma once

#include <unordered_set>

#include "dxvk_binding.h"
#include "dxvk_descriptor.h"
#include "dxvk_lifetime.h"
#include "dxvk_limits.h"
#include "dxvk_pipelayout.h"
#include "dxvk_query_tracker.h"
#include "dxvk_staging.h"

namespace dxvk {
  
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
      const Rc<vk::DeviceFn>& vkd,
            DxvkDevice*       device,
            uint32_t          queueFamily);
    ~DxvkCommandList();
    
    /**
     * \brief Submits command list
     * 
     * \param [in] queue Device queue
     * \param [in] waitSemaphore Semaphore to wait on
     * \param [in] wakeSemaphore Semaphore to signal
     * \param [in] fence Fence to signal
     */
    void submit(
            VkQueue         queue,
            VkSemaphore     waitSemaphore,
            VkSemaphore     wakeSemaphore,
            VkFence         fence);
    
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
     */
    void endRecording();
    
    /**
     * \brief Adds a resource to track
     * 
     * Adds a resource to the internal resource tracker.
     * Resources will be kept alive and "in use" until
     * the device can guarantee that the submission has
     * completed.
     */
    void trackResource(const Rc<DxvkResource>& rc) {
      m_resources.trackResource(rc);
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
     * \brief Writes back query results
     * 
     * Uses the query range to write back query data
     * after the command list has finished executing
     * on the GPU.
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
    
    
    VkDescriptorSet allocateDescriptorSet(
            VkDescriptorSetLayout   descriptorLayout) {
      return m_descAlloc.alloc(descriptorLayout);
    }
    
    
    void updateDescriptorSet(
            uint32_t                descriptorCount,
      const VkWriteDescriptorSet*   descriptorWrites) {
      m_vkd->vkUpdateDescriptorSets(m_vkd->device(),
        descriptorCount, descriptorWrites, 0, nullptr);
    }
    
    
    void cmdBeginQuery(
            VkQueryPool             queryPool,
            uint32_t                query,
            VkQueryControlFlags     flags) {
      m_vkd->vkCmdBeginQuery(m_buffer,
        queryPool, query, flags);
    }
    
    
    void cmdBeginRenderPass(
      const VkRenderPassBeginInfo*  pRenderPassBegin,
            VkSubpassContents       contents) {
      m_vkd->vkCmdBeginRenderPass(m_buffer,
        pRenderPassBegin, contents);
    }
    
    
    void cmdBindDescriptorSet(
          VkPipelineBindPoint       pipeline,
          VkPipelineLayout          pipelineLayout,
          VkDescriptorSet           descriptorSet) {
      m_vkd->vkCmdBindDescriptorSets(m_buffer,
        pipeline, pipelineLayout, 0, 1,
        &descriptorSet, 0, nullptr);
    }
    
    
    void cmdBindIndexBuffer(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            VkIndexType             indexType) {
      m_vkd->vkCmdBindIndexBuffer(m_buffer,
        buffer, offset, indexType);
    }
    
    
    void cmdBindPipeline(
            VkPipelineBindPoint     pipelineBindPoint,
            VkPipeline              pipeline) {
      m_vkd->vkCmdBindPipeline(m_buffer,
        pipelineBindPoint, pipeline);
    }
    
    
    void cmdBindVertexBuffers(
            uint32_t                firstBinding,
            uint32_t                bindingCount,
      const VkBuffer*               pBuffers,
      const VkDeviceSize*           pOffsets) {
      m_vkd->vkCmdBindVertexBuffers(m_buffer,
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
      m_vkd->vkCmdBlitImage(m_buffer,
        srcImage, srcImageLayout,
        dstImage, dstImageLayout,
        regionCount, pRegions, filter);
    }
    
    
    void cmdClearAttachments(
            uint32_t                attachmentCount,
      const VkClearAttachment*      pAttachments,
            uint32_t                rectCount,
      const VkClearRect*            pRects) {
      m_vkd->vkCmdClearAttachments(m_buffer,
        attachmentCount, pAttachments,
        rectCount, pRects);
    }
    
    
    void cmdClearColorImage(
            VkImage                 image,
            VkImageLayout           imageLayout,
      const VkClearColorValue*      pColor,
            uint32_t                rangeCount,
      const VkImageSubresourceRange* pRanges) {
      m_vkd->vkCmdClearColorImage(m_buffer,
        image, imageLayout, pColor,
        rangeCount, pRanges);
    }
    
    
    void cmdClearDepthStencilImage(
            VkImage                 image,
            VkImageLayout           imageLayout,
      const VkClearDepthStencilValue* pDepthStencil,
            uint32_t                rangeCount,
      const VkImageSubresourceRange* pRanges) {
      m_vkd->vkCmdClearDepthStencilImage(m_buffer,
        image, imageLayout, pDepthStencil,
        rangeCount, pRanges);
    }
    
    
    void cmdCopyBuffer(
            VkBuffer                srcBuffer,
            VkBuffer                dstBuffer,
            uint32_t                regionCount,
      const VkBufferCopy*           pRegions) {
      m_vkd->vkCmdCopyBuffer(m_buffer,
        srcBuffer, dstBuffer,
        regionCount, pRegions);
    }
    
    
    void cmdCopyBufferToImage(
            VkBuffer                srcBuffer,
            VkImage                 dstImage,
            VkImageLayout           dstImageLayout,
            uint32_t                regionCount,
      const VkBufferImageCopy*      pRegions) {
      m_vkd->vkCmdCopyBufferToImage(m_buffer,
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
      m_vkd->vkCmdCopyImage(m_buffer,
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
      m_vkd->vkCmdCopyImageToBuffer(m_buffer,
        srcImage, srcImageLayout, dstBuffer,
        regionCount, pRegions);
    }
    
    
    void cmdDispatch(
            uint32_t                x,
            uint32_t                y,
            uint32_t                z) {
      m_vkd->vkCmdDispatch(m_buffer, x, y, z);
    }
    
    
    void cmdDispatchIndirect(
            VkBuffer                buffer,
            VkDeviceSize            offset) {
      m_vkd->vkCmdDispatchIndirect(
        m_buffer, buffer, offset);
    }
    
    
    void cmdDraw(
            uint32_t                vertexCount,
            uint32_t                instanceCount,
            uint32_t                firstVertex,
            uint32_t                firstInstance) {
      m_vkd->vkCmdDraw(m_buffer,
        vertexCount, instanceCount,
        firstVertex, firstInstance);
    }
    
    
    void cmdDrawIndirect(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            uint32_t                drawCount,
            uint32_t                stride) {
      m_vkd->vkCmdDrawIndirect(m_buffer,
        buffer, offset, drawCount, stride);
    }
    
    
    void cmdDrawIndexed(
            uint32_t                indexCount,
            uint32_t                instanceCount,
            uint32_t                firstIndex,
            uint32_t                vertexOffset,
            uint32_t                firstInstance) {
      m_vkd->vkCmdDrawIndexed(m_buffer,
        indexCount, instanceCount,
        firstIndex, vertexOffset,
        firstInstance);
    }
    
    
    void cmdDrawIndexedIndirect(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            uint32_t                drawCount,
            uint32_t                stride) {
      m_vkd->vkCmdDrawIndexedIndirect(m_buffer,
        buffer, offset, drawCount, stride);
    }
    
    
    void cmdEndQuery(
            VkQueryPool             queryPool,
            uint32_t                query) {
      m_vkd->vkCmdEndQuery(m_buffer, queryPool, query);
    }
    
    
    void cmdEndRenderPass() {
      m_vkd->vkCmdEndRenderPass(m_buffer);
    }
    
    
    void cmdFillBuffer(
            VkBuffer                dstBuffer,
            VkDeviceSize            dstOffset,
            VkDeviceSize            size,
            uint32_t                data) {
      m_vkd->vkCmdFillBuffer(m_buffer,
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
      m_vkd->vkCmdPipelineBarrier(m_buffer,
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
      m_vkd->vkCmdPushConstants(m_buffer,
        layout, stageFlags, offset, size, pValues);
    }
    
    
    void cmdResetQueryPool(
            VkQueryPool             queryPool,
            uint32_t                firstQuery,
            uint32_t                queryCount) {
      m_vkd->vkCmdResetQueryPool(m_buffer,
        queryPool, firstQuery, queryCount);
    }
    
    
    void cmdResolveImage(
            VkImage                 srcImage,
            VkImageLayout           srcImageLayout,
            VkImage                 dstImage,
            VkImageLayout           dstImageLayout,
            uint32_t                regionCount,
      const VkImageResolve*         pRegions) {
      m_vkd->vkCmdResolveImage(m_buffer,
        srcImage, srcImageLayout,
        dstImage, dstImageLayout,
        regionCount, pRegions);
    }
    
    
    void cmdUpdateBuffer(
            VkBuffer                dstBuffer,
            VkDeviceSize            dstOffset,
            VkDeviceSize            dataSize,
      const void*                   pData) {
      m_vkd->vkCmdUpdateBuffer(m_buffer,
        dstBuffer, dstOffset, dataSize, pData);
    }
    
    
    void cmdSetBlendConstants(const float blendConstants[4]) {
      m_vkd->vkCmdSetBlendConstants(m_buffer, blendConstants);
    }
    
    
    void cmdSetScissor(
            uint32_t                firstScissor,
            uint32_t                scissorCount,
      const VkRect2D*               scissors) {
      m_vkd->vkCmdSetScissor(m_buffer,
        firstScissor, scissorCount, scissors);
    }
    
    
    void cmdSetStencilReference(
            VkStencilFaceFlags      faceMask,
            uint32_t                reference) {
      m_vkd->vkCmdSetStencilReference(m_buffer,
        faceMask, reference);
    }
    
    
    void cmdSetViewport(
            uint32_t                firstViewport,
            uint32_t                viewportCount,
      const VkViewport*             viewports) {
      m_vkd->vkCmdSetViewport(m_buffer,
        firstViewport, viewportCount, viewports);
    }
    
    
    void cmdWriteTimestamp(
            VkPipelineStageFlagBits pipelineStage,
            VkQueryPool             queryPool,
            uint32_t                query) {
      m_vkd->vkCmdWriteTimestamp(m_buffer,
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
    
    VkCommandPool       m_pool;
    VkCommandBuffer     m_buffer;
    
    DxvkLifetimeTracker m_resources;
    DxvkDescriptorAlloc m_descAlloc;
    DxvkStagingAlloc    m_stagingAlloc;
    DxvkQueryTracker    m_queryTracker;
    
  };
  
}