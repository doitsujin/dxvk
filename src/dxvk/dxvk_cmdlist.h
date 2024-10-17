#pragma once

#include <limits>

#include "dxvk_bind_mask.h"
#include "dxvk_buffer.h"
#include "dxvk_descriptor.h"
#include "dxvk_fence.h"
#include "dxvk_gpu_event.h"
#include "dxvk_gpu_query.h"
#include "dxvk_graphics.h"
#include "dxvk_limits.h"
#include "dxvk_pipelayout.h"
#include "dxvk_presenter.h"
#include "dxvk_signal.h"
#include "dxvk_sparse.h"
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
   * \brief Queue command submission
   *
   * Convenience class that holds data for a single
   * command submission, which then easily be executed.
   */
  class DxvkCommandSubmission {

  public:

    DxvkCommandSubmission();
    ~DxvkCommandSubmission();

    /**
     * \brief Adds a semaphore to wait on
     *
     * \param [in] semaphore The semaphore
     * \param [in] value Semaphore value
     * \param [in] stageMask Stages to block
     */
    void waitSemaphore(
            VkSemaphore           semaphore,
            uint64_t              value,
            VkPipelineStageFlags2 stageMask);

    /**
     * \brief Adds a semaphore to signal
     *
     * \param [in] semaphore The semaphore
     * \param [in] value Semaphore value
     * \param [in] stageMask Stages to signal on
     */
    void signalSemaphore(
            VkSemaphore           semaphore,
            uint64_t              value,
            VkPipelineStageFlags2 stageMask);

    /**
     * \brief Adds a fence to signal
     * \param [in] fence The fence
     */
    void signalFence(
            VkFence               fence);

    /**
     * \brief Adds a command buffer to execute
     * \param [in] commandBuffer The command buffer
     */
    void executeCommandBuffer(
            VkCommandBuffer       commandBuffer);

    /**
     * \brief Executes submission and resets object
     *
     * \param [in] device DXVK device
     * \param [in] queue Queue to submit to
     * \returns Submission return value
     */
    VkResult submit(
            DxvkDevice*           device,
            VkQueue               queue);

    /**
     * \brief Resets object
     */
    void reset();

    /**
     * \brief Checks whether the submission is empty
     *
     * \returns \c true if there are no command
     *    buffers or semaphores.
     */
    bool isEmpty() const;

  private:

    VkFence                                m_fence = VK_NULL_HANDLE;
    std::vector<VkSemaphoreSubmitInfo>     m_semaphoreWaits;
    std::vector<VkSemaphoreSubmitInfo>     m_semaphoreSignals;
    std::vector<VkCommandBufferSubmitInfo> m_commandBuffers;

  };


  /**
   * \brief Command submission info
   *
   * Stores a set of command buffers, as well as a
   * mask of command buffers that were actually used.
   */
  struct DxvkCommandSubmissionInfo {
    DxvkCmdBufferFlags  usedFlags   = 0;
    VkCommandBuffer     execBuffer  = VK_NULL_HANDLE;
    VkCommandBuffer     initBuffer  = VK_NULL_HANDLE;
    VkCommandBuffer     sdmaBuffer  = VK_NULL_HANDLE;
    VkBool32            sparseBind  = VK_FALSE;
    uint32_t            sparseCmd   = 0;
  };


  /**
   * \brief Command pool
   *
   * Simple command pool abstraction that allows
   * us to easily obtain command buffers.
   */
  class DxvkCommandPool : public RcObject {

  public:

    /**
     * \brief Creates command pool
     *
     * \param [in] device DXVK device
     * \param [in] queueFamily Target queue family
     */
    DxvkCommandPool(
            DxvkDevice*           device,
            uint32_t              queueFamily);

    ~DxvkCommandPool();

    /**
     * \brief Retrieves or allocates a command buffer
     * \returns New command buffer in begun state
     */
    VkCommandBuffer getCommandBuffer();

    /**
     * \brief Resets command pool and all command buffers
     */
    void reset();

  private:

    DxvkDevice*                   m_device;

    VkCommandPool                 m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer>  m_commandBuffers;
    size_t                        m_next        = 0;

  };


  /**
   * \brief Command list
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
     * \returns Submission status
     */
    VkResult submit();
    
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
     * \brief Initializes command buffers
     *
     * Prepares command list for command recording.
     */
    void init();
    
    /**
     * \brief Ends recording
     *
     * Ends command buffer recording, making
     * the command list ready for submission.
     * \param [in] stats Stat counters
     */
    void finalize();

    /**
     * \brief Interrupts recording
     *
     * Begins a new set of command buffers while adding the
     * current set to the submission list. This can be useful
     * to split the command list into multiple submissions.
     */
    void next();
    
    /**
     * \brief Tracks an object
     *
     * Keeps the object alive until the command list finishes
     * execution on the GPU.
     * \param [in] object Object to track
     */
    template<typename T>
    void track(Rc<T> object) {
      m_objectTracker.track<DxvkObjectRef<T>>(std::move(object));
    }

    /**
     * \brief Tracks a resource with access mode
     *
     * Keeps the object alive and tracks resource access for
     * the purpoe of CPU access synchronization. The different
     * overloads try to reduce atomic operations.
     * \param [in] object Object to track
     * \param [in] access Resource access mode
     */
    template<typename T>
    void track(Rc<T>&& object, DxvkAccess access) {
      m_objectTracker.track<DxvkResourceRef>(std::move(object), access);
    }

    template<typename T>
    void track(const Rc<T>& object, DxvkAccess access) {
      m_objectTracker.track<DxvkResourceRef>(object.ptr(), access);
    }

    template<typename T>
    void track(T* object, DxvkAccess access) {
      m_objectTracker.track<DxvkResourceRef>(object, access);
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
      m_objectTracker.clear();
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
     * \brief Sets WSI semaphores to synchronize with
     *
     * The given semaphores must be binary semaphores.
     * \param [in] wsiSemaphores Pair of WSI semaphores
     */
    void setWsiSemaphores(const PresenterSync& wsiSemaphores) {
      m_wsiSemaphores = wsiSemaphores;
    }

    /**
     * \brief Synchronizes with command list fence
     * \returns Return value of vkWaitForFences call
     */
    VkResult synchronizeFence();

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


    void cmdBeginQuery(
            VkQueryPool             queryPool,
            uint32_t                query,
            VkQueryControlFlags     flags) {
      m_cmd.usedFlags.set(DxvkCmdBuffer::ExecBuffer);

      m_vkd->vkCmdBeginQuery(m_cmd.execBuffer,
        queryPool, query, flags);
    }
    
    
    void cmdBeginQueryIndexed(
            VkQueryPool             queryPool,
            uint32_t                query,
            VkQueryControlFlags     flags,
            uint32_t                index) {
      m_cmd.usedFlags.set(DxvkCmdBuffer::ExecBuffer);

      m_vkd->vkCmdBeginQueryIndexedEXT(
        m_cmd.execBuffer, queryPool, query, flags, index);
    }


    void cmdBeginRendering(
      const VkRenderingInfo*        pRenderingInfo) {
      m_cmd.usedFlags.set(DxvkCmdBuffer::ExecBuffer);

      m_vkd->vkCmdBeginRendering(m_cmd.execBuffer, pRenderingInfo);
    }

    
    void cmdBeginTransformFeedback(
            uint32_t                  firstBuffer,
            uint32_t                  bufferCount,
      const VkBuffer*                 counterBuffers,
      const VkDeviceSize*             counterOffsets) {
      m_vkd->vkCmdBeginTransformFeedbackEXT(m_cmd.execBuffer,
        firstBuffer, bufferCount, counterBuffers, counterOffsets);
    }
    
    
    void cmdBindDescriptorSet(
            DxvkCmdBuffer             cmdBuffer,
            VkPipelineBindPoint       pipeline,
            VkPipelineLayout          pipelineLayout,
            VkDescriptorSet           descriptorSet,
            uint32_t                  dynamicOffsetCount,
      const uint32_t*                 pDynamicOffsets) {
      m_vkd->vkCmdBindDescriptorSets(getCmdBuffer(cmdBuffer),
        pipeline, pipelineLayout, 0, 1,
        &descriptorSet, dynamicOffsetCount, pDynamicOffsets);
    }
    
    
    void cmdBindDescriptorSets(
            DxvkCmdBuffer             cmdBuffer,
            VkPipelineBindPoint       pipeline,
            VkPipelineLayout          pipelineLayout,
            uint32_t                  firstSet,
            uint32_t                  descriptorSetCount,
      const VkDescriptorSet*          descriptorSets,
            uint32_t                  dynamicOffsetCount,
      const uint32_t*                 pDynamicOffsets) {
      m_vkd->vkCmdBindDescriptorSets(getCmdBuffer(cmdBuffer),
        pipeline, pipelineLayout, firstSet, descriptorSetCount,
        descriptorSets, dynamicOffsetCount, pDynamicOffsets);
    }


    void cmdBindIndexBuffer(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            VkIndexType             indexType) {
      m_vkd->vkCmdBindIndexBuffer(m_cmd.execBuffer,
        buffer, offset, indexType);
    }
    
    
    void cmdBindIndexBuffer2(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            VkDeviceSize            size,
            VkIndexType             indexType) {
      m_vkd->vkCmdBindIndexBuffer2KHR(m_cmd.execBuffer,
        buffer, offset, size, indexType);
    }


    void cmdBindPipeline(
            DxvkCmdBuffer           cmdBuffer,
            VkPipelineBindPoint     pipelineBindPoint,
            VkPipeline              pipeline) {
      m_vkd->vkCmdBindPipeline(getCmdBuffer(cmdBuffer),
        pipelineBindPoint, pipeline);
    }


    void cmdBindTransformFeedbackBuffers(
            uint32_t                firstBinding,
            uint32_t                bindingCount,
      const VkBuffer*               pBuffers,
      const VkDeviceSize*           pOffsets,
      const VkDeviceSize*           pSizes) {
      m_vkd->vkCmdBindTransformFeedbackBuffersEXT(m_cmd.execBuffer,
        firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
    }
    
    
    void cmdBindVertexBuffers(
            uint32_t                firstBinding,
            uint32_t                bindingCount,
      const VkBuffer*               pBuffers,
      const VkDeviceSize*           pOffsets,
      const VkDeviceSize*           pSizes,
      const VkDeviceSize*           pStrides) {
      m_vkd->vkCmdBindVertexBuffers2(m_cmd.execBuffer,
        firstBinding, bindingCount, pBuffers, pOffsets,
        pSizes, pStrides);
    }
    
    void cmdLaunchCuKernel(VkCuLaunchInfoNVX launchInfo) {
      m_cmd.usedFlags.set(DxvkCmdBuffer::ExecBuffer);

      m_vkd->vkCmdCuLaunchKernelNVX(m_cmd.execBuffer, &launchInfo);
    }
    

    void cmdBlitImage(
        const VkBlitImageInfo2*     pBlitInfo) {
      m_cmd.usedFlags.set(DxvkCmdBuffer::ExecBuffer);

      m_vkd->vkCmdBlitImage2(m_cmd.execBuffer, pBlitInfo);
    }
    
    
    void cmdClearAttachments(
            uint32_t                attachmentCount,
      const VkClearAttachment*      pAttachments,
            uint32_t                rectCount,
      const VkClearRect*            pRects) {
      m_vkd->vkCmdClearAttachments(m_cmd.execBuffer,
        attachmentCount, pAttachments,
        rectCount, pRects);
    }
    
    
    void cmdClearColorImage(
            DxvkCmdBuffer           cmdBuffer,
            VkImage                 image,
            VkImageLayout           imageLayout,
      const VkClearColorValue*      pColor,
            uint32_t                rangeCount,
      const VkImageSubresourceRange* pRanges) {
      m_cmd.usedFlags.set(cmdBuffer);

      m_vkd->vkCmdClearColorImage(getCmdBuffer(cmdBuffer),
        image, imageLayout, pColor,
        rangeCount, pRanges);
    }
    
    
    void cmdClearDepthStencilImage(
            DxvkCmdBuffer           cmdBuffer,
            VkImage                 image,
            VkImageLayout           imageLayout,
      const VkClearDepthStencilValue* pDepthStencil,
            uint32_t                rangeCount,
      const VkImageSubresourceRange* pRanges) {
      m_cmd.usedFlags.set(cmdBuffer);

      m_vkd->vkCmdClearDepthStencilImage(getCmdBuffer(cmdBuffer),
        image, imageLayout, pDepthStencil,
        rangeCount, pRanges);
    }
    
    
    void cmdCopyBuffer(
            DxvkCmdBuffer           cmdBuffer,
      const VkCopyBufferInfo2*      copyInfo) {
      m_cmd.usedFlags.set(cmdBuffer);

      m_vkd->vkCmdCopyBuffer2(getCmdBuffer(cmdBuffer), copyInfo);
    }
    
    
    void cmdCopyBufferToImage(
            DxvkCmdBuffer           cmdBuffer,
      const VkCopyBufferToImageInfo2* copyInfo) {
      m_cmd.usedFlags.set(cmdBuffer);

      m_vkd->vkCmdCopyBufferToImage2(getCmdBuffer(cmdBuffer), copyInfo);
    }
    
    
    void cmdCopyImage(
            DxvkCmdBuffer           cmdBuffer,
      const VkCopyImageInfo2*       copyInfo) {
      m_cmd.usedFlags.set(cmdBuffer);

      m_vkd->vkCmdCopyImage2(getCmdBuffer(cmdBuffer), copyInfo);
    }
    
    
    void cmdCopyImageToBuffer(
            DxvkCmdBuffer           cmdBuffer,
      const VkCopyImageToBufferInfo2* copyInfo) {
      m_cmd.usedFlags.set(cmdBuffer);

      m_vkd->vkCmdCopyImageToBuffer2(getCmdBuffer(cmdBuffer), copyInfo);
    }


    void cmdCopyQueryPoolResults(
            DxvkCmdBuffer           cmdBuffer,
            VkQueryPool             queryPool,
            uint32_t                firstQuery,
            uint32_t                queryCount,
            VkBuffer                dstBuffer,
            VkDeviceSize            dstOffset,
            VkDeviceSize            stride,
            VkQueryResultFlags      flags) {
      m_cmd.usedFlags.set(cmdBuffer);

      m_vkd->vkCmdCopyQueryPoolResults(getCmdBuffer(cmdBuffer),
        queryPool, firstQuery, queryCount,
        dstBuffer, dstOffset, stride, flags);
    }
    
    
    void cmdDispatch(
            DxvkCmdBuffer           cmdBuffer,
            uint32_t                x,
            uint32_t                y,
            uint32_t                z) {
      m_cmd.usedFlags.set(cmdBuffer);

      m_vkd->vkCmdDispatch(getCmdBuffer(cmdBuffer), x, y, z);
    }
    
    
    void cmdDispatchIndirect(
            DxvkCmdBuffer           cmdBuffer,
            VkBuffer                buffer,
            VkDeviceSize            offset) {
      m_cmd.usedFlags.set(cmdBuffer);

      m_vkd->vkCmdDispatchIndirect(getCmdBuffer(cmdBuffer), buffer, offset);
    }
    
    
    void cmdDraw(
            uint32_t                vertexCount,
            uint32_t                instanceCount,
            uint32_t                firstVertex,
            uint32_t                firstInstance) {
      m_vkd->vkCmdDraw(m_cmd.execBuffer,
        vertexCount, instanceCount,
        firstVertex, firstInstance);
    }
    
    
    void cmdDrawIndirect(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            uint32_t                drawCount,
            uint32_t                stride) {
      m_vkd->vkCmdDrawIndirect(m_cmd.execBuffer,
        buffer, offset, drawCount, stride);
    }
    
    
    void cmdDrawIndirectCount(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            VkBuffer                countBuffer,
            VkDeviceSize            countOffset,
            uint32_t                maxDrawCount,
            uint32_t                stride) {
      m_vkd->vkCmdDrawIndirectCount(m_cmd.execBuffer,
        buffer, offset, countBuffer, countOffset, maxDrawCount, stride);
    }
    
    
    void cmdDrawIndexed(
            uint32_t                indexCount,
            uint32_t                instanceCount,
            uint32_t                firstIndex,
            int32_t                 vertexOffset,
            uint32_t                firstInstance) {
      m_vkd->vkCmdDrawIndexed(m_cmd.execBuffer,
        indexCount, instanceCount,
        firstIndex, vertexOffset,
        firstInstance);
    }
    
    
    void cmdDrawIndexedIndirect(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            uint32_t                drawCount,
            uint32_t                stride) {
      m_vkd->vkCmdDrawIndexedIndirect(m_cmd.execBuffer,
        buffer, offset, drawCount, stride);
    }


    void cmdDrawIndexedIndirectCount(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            VkBuffer                countBuffer,
            VkDeviceSize            countOffset,
            uint32_t                maxDrawCount,
            uint32_t                stride) {
      m_vkd->vkCmdDrawIndexedIndirectCount(m_cmd.execBuffer,
        buffer, offset, countBuffer, countOffset, maxDrawCount, stride);
    }
    
    
    void cmdDrawIndirectVertexCount(
            uint32_t                instanceCount,
            uint32_t                firstInstance,
            VkBuffer                counterBuffer,
            VkDeviceSize            counterBufferOffset,
            uint32_t                counterOffset,
            uint32_t                vertexStride) {
      m_vkd->vkCmdDrawIndirectByteCountEXT(m_cmd.execBuffer,
        instanceCount, firstInstance, counterBuffer,
        counterBufferOffset, counterOffset, vertexStride);
    }
    
    
    void cmdEndQuery(
            VkQueryPool             queryPool,
            uint32_t                query) {
      m_vkd->vkCmdEndQuery(m_cmd.execBuffer, queryPool, query);
    }


    void cmdEndQueryIndexed(
            VkQueryPool             queryPool,
            uint32_t                query,
            uint32_t                index) {
      m_vkd->vkCmdEndQueryIndexedEXT(
        m_cmd.execBuffer, queryPool, query, index);
    }
    
    
    void cmdEndRendering() {
      m_vkd->vkCmdEndRendering(m_cmd.execBuffer);
    }

    
    void cmdEndTransformFeedback(
            uint32_t                  firstBuffer,
            uint32_t                  bufferCount,
      const VkBuffer*                 counterBuffers,
      const VkDeviceSize*             counterOffsets) {
      m_vkd->vkCmdEndTransformFeedbackEXT(m_cmd.execBuffer,
        firstBuffer, bufferCount, counterBuffers, counterOffsets);
    }


    void cmdFillBuffer(
            DxvkCmdBuffer           cmdBuffer,
            VkBuffer                dstBuffer,
            VkDeviceSize            dstOffset,
            VkDeviceSize            size,
            uint32_t                data) {
      m_cmd.usedFlags.set(cmdBuffer);

      m_vkd->vkCmdFillBuffer(getCmdBuffer(cmdBuffer),
        dstBuffer, dstOffset, size, data);
    }
    
    
    void cmdPipelineBarrier(
            DxvkCmdBuffer           cmdBuffer,
      const VkDependencyInfo*       dependencyInfo) {
      m_cmd.usedFlags.set(cmdBuffer);

      m_vkd->vkCmdPipelineBarrier2(getCmdBuffer(cmdBuffer), dependencyInfo);
    }
    
    
    void cmdPushConstants(
            DxvkCmdBuffer           cmdBuffer,
            VkPipelineLayout        layout,
            VkShaderStageFlags      stageFlags,
            uint32_t                offset,
            uint32_t                size,
      const void*                   pValues) {
      m_vkd->vkCmdPushConstants(getCmdBuffer(cmdBuffer),
        layout, stageFlags, offset, size, pValues);
    }


    void cmdResetQueryPool(
            DxvkCmdBuffer           cmdBuffer,
            VkQueryPool             queryPool,
            uint32_t                firstQuery,
            uint32_t                queryCount) {
      m_cmd.usedFlags.set(cmdBuffer);

      m_vkd->vkCmdResetQueryPool(getCmdBuffer(cmdBuffer),
        queryPool, firstQuery, queryCount);
    }


    void cmdResolveImage(
      const VkResolveImageInfo2*    resolveInfo) {
      m_cmd.usedFlags.set(DxvkCmdBuffer::ExecBuffer);

      m_vkd->vkCmdResolveImage2(m_cmd.execBuffer, resolveInfo);
    }
    
    
    void cmdUpdateBuffer(
            DxvkCmdBuffer           cmdBuffer,
            VkBuffer                dstBuffer,
            VkDeviceSize            dstOffset,
            VkDeviceSize            dataSize,
      const void*                   pData) {
      m_cmd.usedFlags.set(cmdBuffer);

      m_vkd->vkCmdUpdateBuffer(getCmdBuffer(cmdBuffer),
        dstBuffer, dstOffset, dataSize, pData);
    }


    void cmdSetAlphaToCoverageState(
            VkBool32                alphaToCoverageEnable) {
      m_vkd->vkCmdSetAlphaToCoverageEnableEXT(m_cmd.execBuffer, alphaToCoverageEnable);
    }

    
    void cmdSetBlendConstants(const float blendConstants[4]) {
      m_vkd->vkCmdSetBlendConstants(m_cmd.execBuffer, blendConstants);
    }
    

    void cmdSetDepthBiasState(
            VkBool32                depthBiasEnable) {
      m_vkd->vkCmdSetDepthBiasEnable(m_cmd.execBuffer, depthBiasEnable);
    }


    void cmdSetDepthClipState(
            VkBool32                depthClipEnable) {
      m_vkd->vkCmdSetDepthClipEnableEXT(m_cmd.execBuffer, depthClipEnable);
    }


    void cmdSetDepthBias(
            float                   depthBiasConstantFactor,
            float                   depthBiasClamp,
            float                   depthBiasSlopeFactor) {
      m_vkd->vkCmdSetDepthBias(m_cmd.execBuffer,
        depthBiasConstantFactor,
        depthBiasClamp,
        depthBiasSlopeFactor);
    }


    void cmdSetDepthBias2(
      const VkDepthBiasInfoEXT     *depthBiasInfo) {
      m_vkd->vkCmdSetDepthBias2EXT(m_cmd.execBuffer, depthBiasInfo);
    }


    void cmdSetDepthBounds(
            float                   minDepthBounds,
            float                   maxDepthBounds) {
      m_vkd->vkCmdSetDepthBounds(m_cmd.execBuffer,
        minDepthBounds,
        maxDepthBounds);
    }


    void cmdSetDepthBoundsState(
            VkBool32                depthBoundsTestEnable) {
      m_vkd->vkCmdSetDepthBoundsTestEnable(m_cmd.execBuffer, depthBoundsTestEnable);
    }


    void cmdSetDepthState(
            VkBool32                depthTestEnable,
            VkBool32                depthWriteEnable,
            VkCompareOp             depthCompareOp) {
      m_vkd->vkCmdSetDepthTestEnable(m_cmd.execBuffer, depthTestEnable);

      if (depthTestEnable) {
        m_vkd->vkCmdSetDepthWriteEnable(m_cmd.execBuffer, depthWriteEnable);
        m_vkd->vkCmdSetDepthCompareOp(m_cmd.execBuffer, depthCompareOp);
      } else {
        m_vkd->vkCmdSetDepthWriteEnable(m_cmd.execBuffer, VK_FALSE);
        m_vkd->vkCmdSetDepthCompareOp(m_cmd.execBuffer, VK_COMPARE_OP_ALWAYS);
      }
    }


    void cmdSetEvent(
            VkEvent                 event,
      const VkDependencyInfo*       dependencyInfo) {
      m_cmd.usedFlags.set(DxvkCmdBuffer::ExecBuffer);

      m_vkd->vkCmdSetEvent2(m_cmd.execBuffer, event, dependencyInfo);
    }


    void cmdSetMultisampleState(
            VkSampleCountFlagBits   sampleCount,
            VkSampleMask            sampleMask) {
      m_vkd->vkCmdSetRasterizationSamplesEXT(m_cmd.execBuffer, sampleCount);
      m_vkd->vkCmdSetSampleMaskEXT(m_cmd.execBuffer, sampleCount, &sampleMask);
    }


    void cmdSetRasterizerState(
            VkCullModeFlags         cullMode,
            VkFrontFace             frontFace) {
      m_vkd->vkCmdSetCullMode(m_cmd.execBuffer, cullMode);
      m_vkd->vkCmdSetFrontFace(m_cmd.execBuffer, frontFace);
    }

    
    void cmdSetScissor(
            uint32_t                scissorCount,
      const VkRect2D*               scissors) {
      m_vkd->vkCmdSetScissorWithCount(
        m_cmd.execBuffer, scissorCount, scissors);
    }


    void cmdSetStencilState(
            VkBool32                enableStencilTest,
      const VkStencilOpState&       front,
      const VkStencilOpState&       back) {
      m_vkd->vkCmdSetStencilTestEnable(
        m_cmd.execBuffer, enableStencilTest);

      if (enableStencilTest) {
        m_vkd->vkCmdSetStencilOp(m_cmd.execBuffer,
          VK_STENCIL_FACE_FRONT_BIT, front.failOp,
          front.passOp, front.depthFailOp, front.compareOp);
        m_vkd->vkCmdSetStencilCompareMask(m_cmd.execBuffer,
          VK_STENCIL_FACE_FRONT_BIT, front.compareMask);
        m_vkd->vkCmdSetStencilWriteMask(m_cmd.execBuffer,
          VK_STENCIL_FACE_FRONT_BIT, front.writeMask);

        m_vkd->vkCmdSetStencilOp(m_cmd.execBuffer,
          VK_STENCIL_FACE_BACK_BIT, back.failOp,
          back.passOp, back.depthFailOp, back.compareOp);
        m_vkd->vkCmdSetStencilCompareMask(m_cmd.execBuffer,
          VK_STENCIL_FACE_BACK_BIT, back.compareMask);
        m_vkd->vkCmdSetStencilWriteMask(m_cmd.execBuffer,
          VK_STENCIL_FACE_BACK_BIT, back.writeMask);
      } else {
        m_vkd->vkCmdSetStencilOp(m_cmd.execBuffer,
          VK_STENCIL_FACE_FRONT_AND_BACK,
          VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
          VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS);
        m_vkd->vkCmdSetStencilCompareMask(m_cmd.execBuffer,
          VK_STENCIL_FACE_FRONT_AND_BACK, 0x0);
        m_vkd->vkCmdSetStencilWriteMask(m_cmd.execBuffer,
          VK_STENCIL_FACE_FRONT_AND_BACK, 0x0);
      }
    }


    void cmdSetStencilReference(
            VkStencilFaceFlags      faceMask,
            uint32_t                reference) {
      m_vkd->vkCmdSetStencilReference(m_cmd.execBuffer,
        faceMask, reference);
    }


    void cmdSetStencilWriteMask(
            VkStencilFaceFlags      faceMask,
            uint32_t                writeMask) {
      m_vkd->vkCmdSetStencilWriteMask(m_cmd.execBuffer, faceMask, writeMask);
    }

    
    void cmdSetViewport(
            uint32_t                viewportCount,
      const VkViewport*             viewports) {
      m_vkd->vkCmdSetViewportWithCount(
        m_cmd.execBuffer, viewportCount, viewports);
    }


    void cmdWriteTimestamp(
            DxvkCmdBuffer           cmdBuffer,
            VkPipelineStageFlagBits2 pipelineStage,
            VkQueryPool             queryPool,
            uint32_t                query) {
      m_cmd.usedFlags.set(cmdBuffer);

      m_vkd->vkCmdWriteTimestamp2(getCmdBuffer(cmdBuffer),
        pipelineStage, queryPool, query);
    }
    

    void cmdBeginDebugUtilsLabel(
            VkDebugUtilsLabelEXT*   pLabelInfo) {
      m_cmd.usedFlags.set(DxvkCmdBuffer::ExecBuffer);

      m_vki->vkCmdBeginDebugUtilsLabelEXT(m_cmd.execBuffer, pLabelInfo);
    }


    void cmdEndDebugUtilsLabel() {
      m_cmd.usedFlags.set(DxvkCmdBuffer::ExecBuffer);

      m_vki->vkCmdEndDebugUtilsLabelEXT(m_cmd.execBuffer);
    }


    void cmdInsertDebugUtilsLabel(
            VkDebugUtilsLabelEXT*   pLabelInfo) {
      m_cmd.usedFlags.set(DxvkCmdBuffer::ExecBuffer);

      m_vki->vkCmdInsertDebugUtilsLabelEXT(m_cmd.execBuffer, pLabelInfo);
    }


    void resetQuery(
            VkQueryPool             queryPool,
            uint32_t                queryId) {
      m_vkd->vkResetQueryPool(
        m_vkd->device(), queryPool, queryId, 1);
    }


    void bindBufferMemory(
      const DxvkSparseBufferBindKey& key,
      const DxvkResourceMemoryInfo& memory) {
      getSparseBindSubmission().bindBufferMemory(key, memory);
    }


    void bindImageMemory(
      const DxvkSparseImageBindKey& key,
      const DxvkResourceMemoryInfo& memory) {
      getSparseBindSubmission().bindImageMemory(key, memory);
    }


    void bindImageOpaqueMemory(
      const DxvkSparseImageOpaqueBindKey& key,
      const DxvkResourceMemoryInfo& memory) {
      getSparseBindSubmission().bindImageOpaqueMemory(key, memory);
    }


    void trackDescriptorPool(
      const Rc<DxvkDescriptorPool>&       pool,
      const Rc<DxvkDescriptorManager>&    manager) {
      pool->updateStats(m_statCounters);
      m_descriptorPools.push_back({ pool, manager });
    }

  private:
    
    DxvkDevice*               m_device;
    Rc<vk::DeviceFn>          m_vkd;
    Rc<vk::InstanceFn>        m_vki;
    
    Rc<DxvkCommandPool>       m_graphicsPool;
    Rc<DxvkCommandPool>       m_transferPool;

    VkSemaphore               m_bindSemaphore = VK_NULL_HANDLE;
    VkSemaphore               m_postSemaphore = VK_NULL_HANDLE;
    VkSemaphore               m_sdmaSemaphore = VK_NULL_HANDLE;
    VkFence                   m_fence         = VK_NULL_HANDLE;

    DxvkCommandSubmissionInfo m_cmd;

    PresenterSync             m_wsiSemaphores = { };

    DxvkObjectTracker         m_objectTracker;
    DxvkSignalTracker         m_signalTracker;
    DxvkStatCounters          m_statCounters;

    DxvkCommandSubmission     m_commandSubmission;

    std::vector<DxvkFenceValuePair> m_waitSemaphores;
    std::vector<DxvkFenceValuePair> m_signalSemaphores;

    std::vector<DxvkCommandSubmissionInfo> m_cmdSubmissions;
    std::vector<DxvkSparseBindSubmission>  m_cmdSparseBinds;
    
    std::vector<std::pair<
      Rc<DxvkDescriptorPool>,
      Rc<DxvkDescriptorManager>>> m_descriptorPools;

    std::vector<DxvkGraphicsPipeline*> m_pipelines;

    VkCommandBuffer getCmdBuffer(DxvkCmdBuffer cmdBuffer) const {
      if (cmdBuffer == DxvkCmdBuffer::ExecBuffer) return m_cmd.execBuffer;
      if (cmdBuffer == DxvkCmdBuffer::InitBuffer) return m_cmd.initBuffer;
      if (cmdBuffer == DxvkCmdBuffer::SdmaBuffer) return m_cmd.sdmaBuffer;
      return VK_NULL_HANDLE;
    }

    DxvkSparseBindSubmission& getSparseBindSubmission() {
      if (likely(m_cmd.sparseBind))
        return m_cmdSparseBinds[m_cmd.sparseCmd];

      m_cmd.sparseBind = VK_TRUE;
      m_cmd.sparseCmd = uint32_t(m_cmdSparseBinds.size());

      return m_cmdSparseBinds.emplace_back();
    }

    void endCommandBuffer(VkCommandBuffer cmdBuffer);

  };
  
}
