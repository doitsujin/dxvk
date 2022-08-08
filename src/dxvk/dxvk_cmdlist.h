#pragma once

#include <limits>

#include "dxvk_bind_mask.h"
#include "dxvk_buffer.h"
#include "dxvk_descriptor.h"
#include "dxvk_fence.h"
#include "dxvk_gpu_event.h"
#include "dxvk_gpu_query.h"
#include "dxvk_graphics.h"
#include "dxvk_lifetime.h"
#include "dxvk_limits.h"
#include "dxvk_pipelayout.h"
#include "dxvk_signal.h"
#include "dxvk_staging.h"
#include "dxvk_stats.h"

namespace dxvk {
  
  /**
   * \brief Command buffer flags
   * 
   * A set of flags used to specify which of
   * the command buffers need to be submitted.
   */
  enum class DxvkCmdBuffer : uint32_t {
    InitBuffer = 0,
    ExecBuffer = 1,
    SdmaBuffer = 2,
  };
  
  using DxvkCmdBufferFlags = Flags<DxvkCmdBuffer>;

  /**
   * \brief Queue submission info
   *
   * Convenience struct that holds data for
   * actual command submissions. Internal use
   * only, array sizes are based on need.
   */
  struct DxvkQueueSubmission {
    std::vector<VkSemaphoreSubmitInfo>     waitSync;
    std::vector<VkSemaphoreSubmitInfo>     wakeSync;
    std::vector<VkCommandBufferSubmitInfo> cmdBuffers;

    void reset() {
      waitSync.clear();
      wakeSync.clear();
      cmdBuffers.clear();
    }
  };

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
    
    DxvkCommandList(DxvkDevice* device);
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
    void addStatCtr(DxvkStatCounter ctr, uint64_t val) {
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
    template<DxvkAccess Access, typename T>
    void trackResource(const Rc<T>& rc) {
      m_resources.trackResource<Access>(rc.ptr());
    }
    
    /**
     * \brief Tracks a GPU event
     * 
     * The event will be returned to its event pool
     * after the command buffer has finished executing.
     * \param [in] handle Event handle
     */
    void trackGpuEvent(DxvkGpuEventHandle handle) {
      m_gpuEventTracker.trackEvent(handle);
    }
    
    /**
     * \brief Tracks a GPU query
     * 
     * The query handle will be returned to its allocator
     * after the command buffer has finished executing.
     * \param [in] handle Event handle
     */
    void trackGpuQuery(DxvkGpuQueryHandle handle) {
      m_gpuQueryTracker.trackQuery(handle);
    }
    
    /**
     * \brief Tracks a graphics pipeline
     * \param [in] pipeline Pipeline
     */
    void trackGraphicsPipeline(DxvkGraphicsPipeline* pipeline) {
      pipeline->acquirePipeline();
      m_pipelines.push_back(pipeline);
    }

    /**
     * \brief Queues signal
     * 
     * The signal will be notified once the command
     * buffer has finished executing on the GPU.
     * \param [in] signal The signal
     * \param [in] value Signal value
     */
    void queueSignal(const Rc<sync::Signal>& signal, uint64_t value) {
      m_signalTracker.add(signal, value);
    }

    /**
     * \brief Notifies resources and signals
     */
    void notifyObjects() {
      m_resources.notify();
      m_signalTracker.notify();
    }

    /**
     * \brief Waits for fence
     *
     * \param [in] fence Fence to wait on
     * \param [in] value Value to wait for
     */
    void waitFence(Rc<DxvkFence> fence, uint64_t value) {
      m_waitSemaphores.emplace_back(std::move(fence), value);
    }
    
    /**
     * \brief Signals fence
     *
     * \param [in] fence Fence to signal
     * \param [in] value Value to signal to
     */
    void signalFence(Rc<DxvkFence> fence, uint64_t value) {
      m_signalSemaphores.emplace_back(std::move(fence), value);
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
            VkDescriptorUpdateTemplate    descriptorTemplate,
      const void*                         data) {
      m_vkd->vkUpdateDescriptorSetWithTemplate(m_vkd->device(),
        descriptorSet, descriptorTemplate, data);
    }


    void cmdBeginConditionalRendering(
      const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin) {
      m_vkd->vkCmdBeginConditionalRenderingEXT(
        m_execBuffer, pConditionalRenderingBegin);
    }


    void cmdEndConditionalRendering() {
      m_vkd->vkCmdEndConditionalRenderingEXT(m_execBuffer);
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


    void cmdBeginRendering(
      const VkRenderingInfo*        pRenderingInfo) {
      m_vkd->vkCmdBeginRendering(m_execBuffer, pRenderingInfo);
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
    
    
    void cmdBindDescriptorSets(
            VkPipelineBindPoint       pipeline,
            VkPipelineLayout          pipelineLayout,
            uint32_t                  firstSet,
            uint32_t                  descriptorSetCount,
      const VkDescriptorSet*          descriptorSets,
            uint32_t                  dynamicOffsetCount,
      const uint32_t*                 pDynamicOffsets) {
      m_vkd->vkCmdBindDescriptorSets(m_execBuffer,
        pipeline, pipelineLayout, firstSet, descriptorSetCount,
        descriptorSets, dynamicOffsetCount, pDynamicOffsets);
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
      const VkDeviceSize*           pOffsets,
      const VkDeviceSize*           pSizes,
      const VkDeviceSize*           pStrides) {
      m_vkd->vkCmdBindVertexBuffers2(m_execBuffer,
        firstBinding, bindingCount, pBuffers, pOffsets,
        pSizes, pStrides);
    }
    
    void cmdLaunchCuKernel(VkCuLaunchInfoNVX launchInfo) {
      m_vkd->vkCmdCuLaunchKernelNVX(m_execBuffer, &launchInfo);
    }
    
    void cmdBlitImage(
        const VkBlitImageInfo2*     pBlitInfo) {
      m_vkd->vkCmdBlitImage2(m_execBuffer, pBlitInfo);
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
            DxvkCmdBuffer           cmdBuffer,
      const VkCopyBufferInfo2*      copyInfo) {
      m_cmdBuffersUsed.set(cmdBuffer);

      m_vkd->vkCmdCopyBuffer2(getCmdBuffer(cmdBuffer), copyInfo);
    }
    
    
    void cmdCopyBufferToImage(
            DxvkCmdBuffer           cmdBuffer,
      const VkCopyBufferToImageInfo2* copyInfo) {
      m_cmdBuffersUsed.set(cmdBuffer);

      m_vkd->vkCmdCopyBufferToImage2(getCmdBuffer(cmdBuffer), copyInfo);
    }
    
    
    void cmdCopyImage(
            DxvkCmdBuffer           cmdBuffer,
      const VkCopyImageInfo2*       copyInfo) {
      m_cmdBuffersUsed.set(cmdBuffer);

      m_vkd->vkCmdCopyImage2(getCmdBuffer(cmdBuffer), copyInfo);
    }
    
    
    void cmdCopyImageToBuffer(
            DxvkCmdBuffer           cmdBuffer,
      const VkCopyImageToBufferInfo2* copyInfo) {
      m_cmdBuffersUsed.set(cmdBuffer);

      m_vkd->vkCmdCopyImageToBuffer2(getCmdBuffer(cmdBuffer), copyInfo);
    }


    void cmdCopyQueryPoolResults(
            VkQueryPool             queryPool,
            uint32_t                firstQuery,
            uint32_t                queryCount,
            VkBuffer                dstBuffer,
            VkDeviceSize            dstOffset,
            VkDeviceSize            stride,
            VkQueryResultFlags      flags) {
      m_vkd->vkCmdCopyQueryPoolResults(m_execBuffer,
        queryPool, firstQuery, queryCount,
        dstBuffer, dstOffset, stride, flags);
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
    
    
    void cmdDrawIndirectCount(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            VkBuffer                countBuffer,
            VkDeviceSize            countOffset,
            uint32_t                maxDrawCount,
            uint32_t                stride) {
      m_vkd->vkCmdDrawIndirectCount(m_execBuffer,
        buffer, offset, countBuffer, countOffset, maxDrawCount, stride);
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


    void cmdDrawIndexedIndirectCount(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            VkBuffer                countBuffer,
            VkDeviceSize            countOffset,
            uint32_t                maxDrawCount,
            uint32_t                stride) {
      m_vkd->vkCmdDrawIndexedIndirectCount(m_execBuffer,
        buffer, offset, countBuffer, countOffset, maxDrawCount, stride);
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
    
    
    void cmdEndRendering() {
      m_vkd->vkCmdEndRendering(m_execBuffer);
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
            DxvkCmdBuffer           cmdBuffer,
            VkBuffer                dstBuffer,
            VkDeviceSize            dstOffset,
            VkDeviceSize            size,
            uint32_t                data) {
      m_cmdBuffersUsed.set(cmdBuffer);

      m_vkd->vkCmdFillBuffer(getCmdBuffer(cmdBuffer),
        dstBuffer, dstOffset, size, data);
    }
    
    
    void cmdPipelineBarrier(
            DxvkCmdBuffer           cmdBuffer,
      const VkDependencyInfo*       dependencyInfo) {
      m_cmdBuffersUsed.set(cmdBuffer);

      m_vkd->vkCmdPipelineBarrier2(getCmdBuffer(cmdBuffer), dependencyInfo);
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


    void cmdResolveImage(
      const VkResolveImageInfo2*    resolveInfo) {
      m_vkd->vkCmdResolveImage2(m_execBuffer, resolveInfo);
    }
    
    
    void cmdUpdateBuffer(
            DxvkCmdBuffer           cmdBuffer,
            VkBuffer                dstBuffer,
            VkDeviceSize            dstOffset,
            VkDeviceSize            dataSize,
      const void*                   pData) {
      m_cmdBuffersUsed.set(cmdBuffer);

      m_vkd->vkCmdUpdateBuffer(getCmdBuffer(cmdBuffer),
        dstBuffer, dstOffset, dataSize, pData);
    }
    
    
    void cmdSetBlendConstants(const float blendConstants[4]) {
      m_vkd->vkCmdSetBlendConstants(m_execBuffer, blendConstants);
    }
    

    void cmdSetDepthBiasState(
            VkBool32                depthBiasEnable) {
      m_vkd->vkCmdSetDepthBiasEnable(m_execBuffer, depthBiasEnable);
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


    void cmdSetDepthBounds(
            float                   minDepthBounds,
            float                   maxDepthBounds) {
      m_vkd->vkCmdSetDepthBounds(m_execBuffer,
        minDepthBounds,
        maxDepthBounds);
    }


    void cmdSetDepthBoundsState(
            VkBool32                depthBoundsTestEnable) {
      m_vkd->vkCmdSetDepthBoundsTestEnable(m_execBuffer, depthBoundsTestEnable);
    }


    void cmdSetDepthState(
            VkBool32                depthTestEnable,
            VkBool32                depthWriteEnable,
            VkCompareOp             depthCompareOp) {
      m_vkd->vkCmdSetDepthTestEnable(m_execBuffer, depthTestEnable);

      if (depthTestEnable) {
        m_vkd->vkCmdSetDepthWriteEnable(m_execBuffer, depthWriteEnable);
        m_vkd->vkCmdSetDepthCompareOp(m_execBuffer, depthCompareOp);
      } else {
        m_vkd->vkCmdSetDepthWriteEnable(m_execBuffer, VK_FALSE);
        m_vkd->vkCmdSetDepthCompareOp(m_execBuffer, VK_COMPARE_OP_ALWAYS);
      }
    }


    void cmdSetEvent(
            VkEvent                 event,
      const VkDependencyInfo*       dependencyInfo) {
      m_vkd->vkCmdSetEvent2(m_execBuffer, event, dependencyInfo);
    }


    void cmdSetRasterizerState(
            VkCullModeFlags         cullMode,
            VkFrontFace             frontFace) {
      m_vkd->vkCmdSetCullMode(m_execBuffer, cullMode);
      m_vkd->vkCmdSetFrontFace(m_execBuffer, frontFace);
    }

    
    void cmdSetScissor(
            uint32_t                scissorCount,
      const VkRect2D*               scissors) {
      m_vkd->vkCmdSetScissorWithCount(
        m_execBuffer, scissorCount, scissors);
    }


    void cmdSetStencilState(
            VkBool32                enableStencilTest,
      const VkStencilOpState&       front,
      const VkStencilOpState&       back) {
      m_vkd->vkCmdSetStencilTestEnable(
        m_execBuffer, enableStencilTest);

      if (enableStencilTest) {
        m_vkd->vkCmdSetStencilOp(m_execBuffer,
          VK_STENCIL_FACE_FRONT_BIT, front.failOp,
          front.passOp, front.depthFailOp, front.compareOp);
        m_vkd->vkCmdSetStencilCompareMask(m_execBuffer,
          VK_STENCIL_FACE_FRONT_BIT, front.compareMask);
        m_vkd->vkCmdSetStencilWriteMask(m_execBuffer,
          VK_STENCIL_FACE_FRONT_BIT, front.writeMask);

        m_vkd->vkCmdSetStencilOp(m_execBuffer,
          VK_STENCIL_FACE_BACK_BIT, back.failOp,
          back.passOp, back.depthFailOp, back.compareOp);
        m_vkd->vkCmdSetStencilCompareMask(m_execBuffer,
          VK_STENCIL_FACE_BACK_BIT, back.compareMask);
        m_vkd->vkCmdSetStencilWriteMask(m_execBuffer,
          VK_STENCIL_FACE_BACK_BIT, back.writeMask);
      } else {
        m_vkd->vkCmdSetStencilOp(m_execBuffer,
          VK_STENCIL_FACE_FRONT_AND_BACK,
          VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
          VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS);
        m_vkd->vkCmdSetStencilCompareMask(m_execBuffer,
          VK_STENCIL_FACE_FRONT_AND_BACK, 0x0);
        m_vkd->vkCmdSetStencilWriteMask(m_execBuffer,
          VK_STENCIL_FACE_FRONT_AND_BACK, 0x0);
      }
    }


    void cmdSetStencilReference(
            VkStencilFaceFlags      faceMask,
            uint32_t                reference) {
      m_vkd->vkCmdSetStencilReference(m_execBuffer,
        faceMask, reference);
    }
    
    
    void cmdSetViewport(
            uint32_t                viewportCount,
      const VkViewport*             viewports) {
      m_vkd->vkCmdSetViewportWithCount(
        m_execBuffer, viewportCount, viewports);
    }


    void cmdWriteTimestamp(
            VkPipelineStageFlagBits2 pipelineStage,
            VkQueryPool             queryPool,
            uint32_t                query) {
      m_vkd->vkCmdWriteTimestamp2(m_execBuffer,
        pipelineStage, queryPool, query);
    }
    
    void cmdBeginDebugUtilsLabel(VkDebugUtilsLabelEXT *pLabelInfo);

    void cmdEndDebugUtilsLabel();

    void cmdInsertDebugUtilsLabel(VkDebugUtilsLabelEXT *pLabelInfo);

    void resetQuery(
            VkQueryPool             queryPool,
            uint32_t                queryId) {
      m_vkd->vkResetQueryPool(
        m_vkd->device(), queryPool, queryId, 1);
    }

    void trackDescriptorPool(
      const Rc<DxvkDescriptorPool>&       pool,
      const Rc<DxvkDescriptorManager>&    manager) {
      pool->updateStats(m_statCounters);
      m_descriptorPools.push_back({ pool, manager });
    }

  private:
    
    DxvkDevice*         m_device;
    Rc<vk::DeviceFn>    m_vkd;
    Rc<vk::InstanceFn>  m_vki;
    
    VkFence             m_fence;
    
    VkCommandPool       m_graphicsPool = VK_NULL_HANDLE;
    VkCommandPool       m_transferPool = VK_NULL_HANDLE;
    
    VkCommandBuffer     m_execBuffer = VK_NULL_HANDLE;
    VkCommandBuffer     m_initBuffer = VK_NULL_HANDLE;
    VkCommandBuffer     m_sdmaBuffer = VK_NULL_HANDLE;

    VkSemaphore         m_sdmaSemaphore = VK_NULL_HANDLE;

    DxvkCmdBufferFlags  m_cmdBuffersUsed;
    DxvkLifetimeTracker m_resources;
    DxvkSignalTracker   m_signalTracker;
    DxvkGpuEventTracker m_gpuEventTracker;
    DxvkGpuQueryTracker m_gpuQueryTracker;
    DxvkBufferTracker   m_bufferTracker;
    DxvkStatCounters    m_statCounters;
    DxvkQueueSubmission m_submission;

    std::vector<DxvkFenceValuePair> m_waitSemaphores;
    std::vector<DxvkFenceValuePair> m_signalSemaphores;
    
    std::vector<std::pair<
      Rc<DxvkDescriptorPool>,
      Rc<DxvkDescriptorManager>>> m_descriptorPools;

    std::vector<DxvkGraphicsPipeline*> m_pipelines;

    VkCommandBuffer getCmdBuffer(DxvkCmdBuffer cmdBuffer) const {
      if (cmdBuffer == DxvkCmdBuffer::ExecBuffer) return m_execBuffer;
      if (cmdBuffer == DxvkCmdBuffer::InitBuffer) return m_initBuffer;
      if (cmdBuffer == DxvkCmdBuffer::SdmaBuffer) return m_sdmaBuffer;
      return VK_NULL_HANDLE;
    }

    VkResult submitToQueue(
            VkQueue               queue,
            VkFence               fence,
      const DxvkQueueSubmission&  info);
    
  };
  
}
