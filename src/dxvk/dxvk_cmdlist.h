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
   * \brief Timeline semaphore pair
   *
   * One semaphore for each queue.
   */
  struct DxvkTimelineSemaphores {
    VkSemaphore graphics = VK_NULL_HANDLE;
    VkSemaphore transfer = VK_NULL_HANDLE;
  };


  /**
   * \brief Timeline semaphore values
   */
  struct DxvkTimelineSemaphoreValues {
    uint64_t graphics = 0u;
    uint64_t transfer = 0u;
  };


  /**
   * \brief Command buffer flags
   * 
   * A set of flags used to specify which of
   * the command buffers need to be submitted.
   */
  enum class DxvkCmdBuffer : uint32_t {
    ExecBuffer,
    InitBuffer,
    InitBarriers,
    SdmaBuffer,
    SdmaBarriers,

    Count
  };
  
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

    small_vector<VkSemaphoreSubmitInfo, 4>      m_semaphoreWaits;
    small_vector<VkSemaphoreSubmitInfo, 4>      m_semaphoreSignals;
    small_vector<VkCommandBufferSubmitInfo, 4>  m_commandBuffers;

  };


  /**
   * \brief Command submission info
   *
   * Stores a set of command buffers, as well as a
   * mask of command buffers that were actually used.
   */
  struct DxvkCommandSubmissionInfo {
    bool                execCommands = false;
    bool                syncSdma    = false;
    bool                sparseBind  = false;
    bool                reserved    = false;
    uint32_t            sparseCmd   = 0;

    std::array<VkCommandBuffer, uint32_t(DxvkCmdBuffer::Count)> cmdBuffers = { };
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
     *
     * \param [in] type Command buffer type
     * \returns New command buffer in begun state
     */
    VkCommandBuffer getCommandBuffer(DxvkCmdBuffer type);

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
     *
     * \param [in] semaphores Timeline semaphore pair
     * \param [in] timelines Timeline semaphore values
     * \returns Submission status
     */
    VkResult submit(
      const DxvkTimelineSemaphores&       semaphores,
            DxvkTimelineSemaphoreValues&  timelines);
    
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
      static_assert(!std::is_same_v<T, DxvkSampler>);
      m_objectTracker.track<DxvkObjectRef<T>>(std::move(object));
    }

    /**
     * \brief Tracks a sampler object
     *
     * Special code path that uses the tracking ID to ensure samplers
     * only get tracked once per submission. This is useful since
     * sampler objects are processed much the same way as resources.
     * \param [in] sampler Sampler object
     */
    void track(const Rc<DxvkSampler>& sampler) {
      if (sampler->trackId(m_trackingId))
        m_objectTracker.track<DxvkObjectRef<DxvkSampler>>(sampler.ptr());
    }

    void track(Rc<DxvkSampler>&& sampler) {
      if (sampler->trackId(m_trackingId))
        m_objectTracker.track<DxvkObjectRef<DxvkSampler>>(std::move(sampler));
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
      if (object->trackId(m_trackingId, access))
        m_objectTracker.track<DxvkResourceRef>(std::move(object), access);
    }

    template<typename T>
    void track(const Rc<T>& object, DxvkAccess access) {
      if (object->trackId(m_trackingId, access))
        m_objectTracker.track<DxvkResourceRef>(object.ptr(), access);
    }

    template<typename T>
    void track(T* object, DxvkAccess access) {
      if (object->trackId(m_trackingId, access))
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
     * \brief Sets flag to stall transfer queue
     *
     * If set, the current submission will submit a semaphore
     * wait to the transfer queue in order to stall subsequent
     * submissions. Necessary in case of resource relocations.
     */
    void setSubmissionBarrier() {
      m_cmd.syncSdma = VK_TRUE;
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


    void cmdBeginQuery(
            VkQueryPool             queryPool,
            uint32_t                query,
            VkQueryControlFlags     flags) {
      m_cmd.execCommands = true;

      m_vkd->vkCmdBeginQuery(getCmdBuffer(), queryPool, query, flags);
    }
    
    
    void cmdBeginQueryIndexed(
            VkQueryPool             queryPool,
            uint32_t                query,
            VkQueryControlFlags     flags,
            uint32_t                index) {
      m_cmd.execCommands = true;

      m_vkd->vkCmdBeginQueryIndexedEXT(getCmdBuffer(),
        queryPool, query, flags, index);
    }


    void cmdBeginRendering(
      const VkRenderingInfo*        pRenderingInfo) {
      m_cmd.execCommands = true;
      m_statCounters.addCtr(DxvkStatCounter::CmdRenderPassCount, 1);

      m_vkd->vkCmdBeginRendering(getCmdBuffer(), pRenderingInfo);
    }

    
    void cmdBeginTransformFeedback(
            uint32_t                  firstBuffer,
            uint32_t                  bufferCount,
      const VkBuffer*                 counterBuffers,
      const VkDeviceSize*             counterOffsets) {
      m_vkd->vkCmdBeginTransformFeedbackEXT(getCmdBuffer(),
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
      m_vkd->vkCmdBindIndexBuffer(getCmdBuffer(),
        buffer, offset, indexType);
    }
    
    
    void cmdBindIndexBuffer2(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            VkDeviceSize            size,
            VkIndexType             indexType) {
      m_vkd->vkCmdBindIndexBuffer2KHR(getCmdBuffer(),
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
      m_vkd->vkCmdBindTransformFeedbackBuffersEXT(getCmdBuffer(),
        firstBinding, bindingCount, pBuffers, pOffsets, pSizes);
    }
    
    
    void cmdBindVertexBuffers(
            uint32_t                firstBinding,
            uint32_t                bindingCount,
      const VkBuffer*               pBuffers,
      const VkDeviceSize*           pOffsets,
      const VkDeviceSize*           pSizes,
      const VkDeviceSize*           pStrides) {
      m_vkd->vkCmdBindVertexBuffers2(getCmdBuffer(),
        firstBinding, bindingCount, pBuffers, pOffsets,
        pSizes, pStrides);
    }
    
    void cmdLaunchCuKernel(VkCuLaunchInfoNVX launchInfo) {
      m_cmd.execCommands = true;

      m_vkd->vkCmdCuLaunchKernelNVX(getCmdBuffer(), &launchInfo);
    }
    

    void cmdBlitImage(
        const VkBlitImageInfo2*     pBlitInfo) {
      m_cmd.execCommands = true;

      m_vkd->vkCmdBlitImage2(getCmdBuffer(), pBlitInfo);
    }
    
    
    void cmdClearAttachments(
            uint32_t                attachmentCount,
      const VkClearAttachment*      pAttachments,
            uint32_t                rectCount,
      const VkClearRect*            pRects) {
      m_vkd->vkCmdClearAttachments(getCmdBuffer(),
        attachmentCount, pAttachments, rectCount, pRects);
    }
    
    
    void cmdClearColorImage(
            DxvkCmdBuffer           cmdBuffer,
            VkImage                 image,
            VkImageLayout           imageLayout,
      const VkClearColorValue*      pColor,
            uint32_t                rangeCount,
      const VkImageSubresourceRange* pRanges) {
      m_cmd.execCommands |= cmdBuffer == DxvkCmdBuffer::ExecBuffer;

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
      m_cmd.execCommands |= cmdBuffer == DxvkCmdBuffer::ExecBuffer;

      m_vkd->vkCmdClearDepthStencilImage(getCmdBuffer(cmdBuffer),
        image, imageLayout, pDepthStencil,
        rangeCount, pRanges);
    }
    
    
    void cmdCopyBuffer(
            DxvkCmdBuffer           cmdBuffer,
      const VkCopyBufferInfo2*      copyInfo) {
      m_cmd.execCommands |= cmdBuffer == DxvkCmdBuffer::ExecBuffer;

      m_vkd->vkCmdCopyBuffer2(getCmdBuffer(cmdBuffer), copyInfo);
    }
    
    
    void cmdCopyBufferToImage(
            DxvkCmdBuffer           cmdBuffer,
      const VkCopyBufferToImageInfo2* copyInfo) {
      m_cmd.execCommands |= cmdBuffer == DxvkCmdBuffer::ExecBuffer;

      m_vkd->vkCmdCopyBufferToImage2(getCmdBuffer(cmdBuffer), copyInfo);
    }
    
    
    void cmdCopyImage(
            DxvkCmdBuffer           cmdBuffer,
      const VkCopyImageInfo2*       copyInfo) {
      m_cmd.execCommands |= cmdBuffer == DxvkCmdBuffer::ExecBuffer;

      m_vkd->vkCmdCopyImage2(getCmdBuffer(cmdBuffer), copyInfo);
    }
    
    
    void cmdCopyImageToBuffer(
            DxvkCmdBuffer           cmdBuffer,
      const VkCopyImageToBufferInfo2* copyInfo) {
      m_cmd.execCommands |= cmdBuffer == DxvkCmdBuffer::ExecBuffer;

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
      m_cmd.execCommands |= cmdBuffer == DxvkCmdBuffer::ExecBuffer;

      m_vkd->vkCmdCopyQueryPoolResults(getCmdBuffer(cmdBuffer),
        queryPool, firstQuery, queryCount,
        dstBuffer, dstOffset, stride, flags);
    }
    
    
    void cmdDispatch(
            DxvkCmdBuffer           cmdBuffer,
            uint32_t                x,
            uint32_t                y,
            uint32_t                z) {
      m_cmd.execCommands |= cmdBuffer == DxvkCmdBuffer::ExecBuffer;
      m_statCounters.addCtr(DxvkStatCounter::CmdDispatchCalls, 1);

      m_vkd->vkCmdDispatch(getCmdBuffer(cmdBuffer), x, y, z);
    }
    
    
    void cmdDispatchIndirect(
            DxvkCmdBuffer           cmdBuffer,
            VkBuffer                buffer,
            VkDeviceSize            offset) {
      m_cmd.execCommands |= cmdBuffer == DxvkCmdBuffer::ExecBuffer;
      m_statCounters.addCtr(DxvkStatCounter::CmdDispatchCalls, 1);

      m_vkd->vkCmdDispatchIndirect(getCmdBuffer(cmdBuffer), buffer, offset);
    }
    
    
    void cmdDraw(
            uint32_t                vertexCount,
            uint32_t                instanceCount,
            uint32_t                firstVertex,
            uint32_t                firstInstance) {
      m_statCounters.addCtr(DxvkStatCounter::CmdDrawCalls, 1);

      m_vkd->vkCmdDraw(getCmdBuffer(),
        vertexCount, instanceCount,
        firstVertex, firstInstance);
    }
    
    
    void cmdDrawIndirect(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            uint32_t                drawCount,
            uint32_t                stride) {
      m_statCounters.addCtr(DxvkStatCounter::CmdDrawCalls, 1);

      m_vkd->vkCmdDrawIndirect(getCmdBuffer(),
        buffer, offset, drawCount, stride);
    }
    
    
    void cmdDrawIndirectCount(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            VkBuffer                countBuffer,
            VkDeviceSize            countOffset,
            uint32_t                maxDrawCount,
            uint32_t                stride) {
      m_statCounters.addCtr(DxvkStatCounter::CmdDrawCalls, 1);

      m_vkd->vkCmdDrawIndirectCount(getCmdBuffer(),
        buffer, offset, countBuffer, countOffset, maxDrawCount, stride);
    }
    
    
    void cmdDrawIndexed(
            uint32_t                indexCount,
            uint32_t                instanceCount,
            uint32_t                firstIndex,
            int32_t                 vertexOffset,
            uint32_t                firstInstance) {
      m_statCounters.addCtr(DxvkStatCounter::CmdDrawCalls, 1);

      m_vkd->vkCmdDrawIndexed(getCmdBuffer(),
        indexCount, instanceCount,
        firstIndex, vertexOffset,
        firstInstance);
    }
    
    
    void cmdDrawIndexedIndirect(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            uint32_t                drawCount,
            uint32_t                stride) {
      m_statCounters.addCtr(DxvkStatCounter::CmdDrawCalls, 1);

      m_vkd->vkCmdDrawIndexedIndirect(getCmdBuffer(),
        buffer, offset, drawCount, stride);
    }


    void cmdDrawIndexedIndirectCount(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            VkBuffer                countBuffer,
            VkDeviceSize            countOffset,
            uint32_t                maxDrawCount,
            uint32_t                stride) {
      m_statCounters.addCtr(DxvkStatCounter::CmdDrawCalls, 1);

      m_vkd->vkCmdDrawIndexedIndirectCount(getCmdBuffer(),
        buffer, offset, countBuffer, countOffset, maxDrawCount, stride);
    }
    
    
    void cmdDrawIndirectVertexCount(
            uint32_t                instanceCount,
            uint32_t                firstInstance,
            VkBuffer                counterBuffer,
            VkDeviceSize            counterBufferOffset,
            uint32_t                counterOffset,
            uint32_t                vertexStride) {
      m_statCounters.addCtr(DxvkStatCounter::CmdDrawCalls, 1);

      m_vkd->vkCmdDrawIndirectByteCountEXT(getCmdBuffer(),
        instanceCount, firstInstance, counterBuffer,
        counterBufferOffset, counterOffset, vertexStride);
    }
    
    
    void cmdEndQuery(
            VkQueryPool             queryPool,
            uint32_t                query) {
      m_vkd->vkCmdEndQuery(getCmdBuffer(), queryPool, query);
    }


    void cmdEndQueryIndexed(
            VkQueryPool             queryPool,
            uint32_t                query,
            uint32_t                index) {
      m_vkd->vkCmdEndQueryIndexedEXT(getCmdBuffer(),
        queryPool, query, index);
    }
    
    
    void cmdEndRendering() {
      m_vkd->vkCmdEndRendering(getCmdBuffer());
    }

    
    void cmdEndTransformFeedback(
            uint32_t                  firstBuffer,
            uint32_t                  bufferCount,
      const VkBuffer*                 counterBuffers,
      const VkDeviceSize*             counterOffsets) {
      m_vkd->vkCmdEndTransformFeedbackEXT(getCmdBuffer(),
        firstBuffer, bufferCount, counterBuffers, counterOffsets);
    }


    void cmdFillBuffer(
            DxvkCmdBuffer           cmdBuffer,
            VkBuffer                dstBuffer,
            VkDeviceSize            dstOffset,
            VkDeviceSize            size,
            uint32_t                data) {
      m_cmd.execCommands |= cmdBuffer == DxvkCmdBuffer::ExecBuffer;

      m_vkd->vkCmdFillBuffer(getCmdBuffer(cmdBuffer),
        dstBuffer, dstOffset, size, data);
    }
    
    
    void cmdPipelineBarrier(
            DxvkCmdBuffer           cmdBuffer,
      const VkDependencyInfo*       dependencyInfo) {
      m_cmd.execCommands |= cmdBuffer == DxvkCmdBuffer::ExecBuffer;
      m_statCounters.addCtr(DxvkStatCounter::CmdBarrierCount, 1);

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
      m_cmd.execCommands |= cmdBuffer == DxvkCmdBuffer::ExecBuffer;

      m_vkd->vkCmdResetQueryPool(getCmdBuffer(cmdBuffer),
        queryPool, firstQuery, queryCount);
    }


    void cmdResolveImage(
      const VkResolveImageInfo2*    resolveInfo) {
      m_cmd.execCommands = true;

      m_vkd->vkCmdResolveImage2(getCmdBuffer(), resolveInfo);
    }
    
    
    void cmdUpdateBuffer(
            DxvkCmdBuffer           cmdBuffer,
            VkBuffer                dstBuffer,
            VkDeviceSize            dstOffset,
            VkDeviceSize            dataSize,
      const void*                   pData) {
      m_cmd.execCommands |= cmdBuffer == DxvkCmdBuffer::ExecBuffer;

      m_vkd->vkCmdUpdateBuffer(getCmdBuffer(cmdBuffer),
        dstBuffer, dstOffset, dataSize, pData);
    }


    void cmdSetAlphaToCoverageState(
            VkBool32                alphaToCoverageEnable) {
      m_vkd->vkCmdSetAlphaToCoverageEnableEXT(getCmdBuffer(), alphaToCoverageEnable);
    }

    
    void cmdSetBlendConstants(const float blendConstants[4]) {
      m_vkd->vkCmdSetBlendConstants(getCmdBuffer(), blendConstants);
    }
    

    void cmdSetDepthBiasState(
            VkBool32                depthBiasEnable) {
      m_vkd->vkCmdSetDepthBiasEnable(getCmdBuffer(), depthBiasEnable);
    }


    void cmdSetDepthClipState(
            VkBool32                depthClipEnable) {
      m_vkd->vkCmdSetDepthClipEnableEXT(getCmdBuffer(), depthClipEnable);
    }


    void cmdSetDepthBias(
            float                   depthBiasConstantFactor,
            float                   depthBiasClamp,
            float                   depthBiasSlopeFactor) {
      m_vkd->vkCmdSetDepthBias(getCmdBuffer(),
        depthBiasConstantFactor,
        depthBiasClamp,
        depthBiasSlopeFactor);
    }


    void cmdSetDepthBias2(
      const VkDepthBiasInfoEXT     *depthBiasInfo) {
      m_vkd->vkCmdSetDepthBias2EXT(getCmdBuffer(), depthBiasInfo);
    }


    void cmdSetDepthBounds(
            float                   minDepthBounds,
            float                   maxDepthBounds) {
      m_vkd->vkCmdSetDepthBounds(getCmdBuffer(),
        minDepthBounds,
        maxDepthBounds);
    }


    void cmdSetDepthBoundsState(
            VkBool32                depthBoundsTestEnable) {
      m_vkd->vkCmdSetDepthBoundsTestEnable(getCmdBuffer(), depthBoundsTestEnable);
    }


    void cmdSetDepthState(
            VkBool32                depthTestEnable,
            VkBool32                depthWriteEnable,
            VkCompareOp             depthCompareOp) {
      VkCommandBuffer cmdBuffer = getCmdBuffer();
      m_vkd->vkCmdSetDepthTestEnable(cmdBuffer, depthTestEnable);

      if (depthTestEnable) {
        m_vkd->vkCmdSetDepthWriteEnable(cmdBuffer, depthWriteEnable);
        m_vkd->vkCmdSetDepthCompareOp(cmdBuffer, depthCompareOp);
      } else {
        m_vkd->vkCmdSetDepthWriteEnable(cmdBuffer, VK_FALSE);
        m_vkd->vkCmdSetDepthCompareOp(cmdBuffer, VK_COMPARE_OP_ALWAYS);
      }
    }


    void cmdSetEvent(
            VkEvent                 event,
      const VkDependencyInfo*       dependencyInfo) {
      m_cmd.execCommands = true;

      m_vkd->vkCmdSetEvent2(getCmdBuffer(), event, dependencyInfo);
    }


    void cmdSetMultisampleState(
            VkSampleCountFlagBits   sampleCount,
            VkSampleMask            sampleMask) {
      VkCommandBuffer cmdBuffer = getCmdBuffer();

      m_vkd->vkCmdSetRasterizationSamplesEXT(cmdBuffer, sampleCount);
      m_vkd->vkCmdSetSampleMaskEXT(cmdBuffer, sampleCount, &sampleMask);
    }


    void cmdSetRasterizerState(
            VkCullModeFlags         cullMode,
            VkFrontFace             frontFace) {
      VkCommandBuffer cmdBuffer = getCmdBuffer();

      m_vkd->vkCmdSetCullMode(cmdBuffer, cullMode);
      m_vkd->vkCmdSetFrontFace(cmdBuffer, frontFace);
    }

    
    void cmdSetScissor(
            uint32_t                scissorCount,
      const VkRect2D*               scissors) {
      m_vkd->vkCmdSetScissorWithCount(getCmdBuffer(), scissorCount, scissors);
    }


    void cmdSetStencilState(
            VkBool32                enableStencilTest,
      const VkStencilOpState&       front,
      const VkStencilOpState&       back) {
      VkCommandBuffer cmdBuffer = getCmdBuffer();

      m_vkd->vkCmdSetStencilTestEnable(
        cmdBuffer, enableStencilTest);

      if (enableStencilTest) {
        m_vkd->vkCmdSetStencilOp(cmdBuffer,
          VK_STENCIL_FACE_FRONT_BIT, front.failOp,
          front.passOp, front.depthFailOp, front.compareOp);
        m_vkd->vkCmdSetStencilCompareMask(cmdBuffer,
          VK_STENCIL_FACE_FRONT_BIT, front.compareMask);
        m_vkd->vkCmdSetStencilWriteMask(cmdBuffer,
          VK_STENCIL_FACE_FRONT_BIT, front.writeMask);

        m_vkd->vkCmdSetStencilOp(cmdBuffer,
          VK_STENCIL_FACE_BACK_BIT, back.failOp,
          back.passOp, back.depthFailOp, back.compareOp);
        m_vkd->vkCmdSetStencilCompareMask(cmdBuffer,
          VK_STENCIL_FACE_BACK_BIT, back.compareMask);
        m_vkd->vkCmdSetStencilWriteMask(cmdBuffer,
          VK_STENCIL_FACE_BACK_BIT, back.writeMask);
      } else {
        m_vkd->vkCmdSetStencilOp(cmdBuffer,
          VK_STENCIL_FACE_FRONT_AND_BACK,
          VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
          VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS);
        m_vkd->vkCmdSetStencilCompareMask(cmdBuffer,
          VK_STENCIL_FACE_FRONT_AND_BACK, 0x0);
        m_vkd->vkCmdSetStencilWriteMask(cmdBuffer,
          VK_STENCIL_FACE_FRONT_AND_BACK, 0x0);
      }
    }


    void cmdSetStencilReference(
            VkStencilFaceFlags      faceMask,
            uint32_t                reference) {
      m_vkd->vkCmdSetStencilReference(getCmdBuffer(),
        faceMask, reference);
    }


    void cmdSetStencilWriteMask(
            VkStencilFaceFlags      faceMask,
            uint32_t                writeMask) {
      m_vkd->vkCmdSetStencilWriteMask(getCmdBuffer(), faceMask, writeMask);
    }

    
    void cmdSetViewport(
            uint32_t                viewportCount,
      const VkViewport*             viewports) {
      m_vkd->vkCmdSetViewportWithCount(getCmdBuffer(), viewportCount, viewports);
    }


    void cmdWriteTimestamp(
            DxvkCmdBuffer           cmdBuffer,
            VkPipelineStageFlagBits2 pipelineStage,
            VkQueryPool             queryPool,
            uint32_t                query) {
      m_cmd.execCommands |= cmdBuffer == DxvkCmdBuffer::ExecBuffer;

      m_vkd->vkCmdWriteTimestamp2(getCmdBuffer(cmdBuffer),
        pipelineStage, queryPool, query);
    }
    

    void cmdBeginDebugUtilsLabel(
            DxvkCmdBuffer           cmdBuffer,
      const VkDebugUtilsLabelEXT&   labelInfo) {
      m_cmd.execCommands = true;

      m_vki->vkCmdBeginDebugUtilsLabelEXT(getCmdBuffer(cmdBuffer), &labelInfo);
    }


    void cmdEndDebugUtilsLabel(
            DxvkCmdBuffer           cmdBuffer) {
      m_cmd.execCommands = true;

      m_vki->vkCmdEndDebugUtilsLabelEXT(getCmdBuffer(cmdBuffer));
    }


    void cmdInsertDebugUtilsLabel(
            DxvkCmdBuffer           cmdBuffer,
      const VkDebugUtilsLabelEXT&   labelInfo) {
      m_cmd.execCommands = true;

      m_vki->vkCmdInsertDebugUtilsLabelEXT(getCmdBuffer(cmdBuffer), &labelInfo);
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


    void setTrackingId(uint64_t id) {
      m_trackingId = id;
    }

  private:
    
    DxvkDevice*               m_device;
    Rc<vk::DeviceFn>          m_vkd;
    Rc<vk::InstanceFn>        m_vki;
    
    Rc<DxvkCommandPool>       m_graphicsPool;
    Rc<DxvkCommandPool>       m_transferPool;

    DxvkCommandSubmissionInfo m_cmd;

    PresenterSync             m_wsiSemaphores = { };
    uint64_t                  m_trackingId = 0u;

    DxvkObjectTracker         m_objectTracker;
    DxvkSignalTracker         m_signalTracker;
    DxvkStatCounters          m_statCounters;

    DxvkCommandSubmission     m_commandSubmission;

    small_vector<DxvkFenceValuePair, 4> m_waitSemaphores;
    small_vector<DxvkFenceValuePair, 4> m_signalSemaphores;

    small_vector<DxvkCommandSubmissionInfo, 4> m_cmdSubmissions;
    small_vector<DxvkSparseBindSubmission, 4>  m_cmdSparseBinds;
    
    std::vector<std::pair<
      Rc<DxvkDescriptorPool>,
      Rc<DxvkDescriptorManager>>> m_descriptorPools;

    std::vector<DxvkGraphicsPipeline*> m_pipelines;

    force_inline VkCommandBuffer getCmdBuffer() const {
      // Allocation logic will always provide an execution buffer
      return m_cmd.cmdBuffers[uint32_t(DxvkCmdBuffer::ExecBuffer)];
    }

    force_inline VkCommandBuffer getCmdBuffer(DxvkCmdBuffer cmdBuffer) {
      VkCommandBuffer buffer = m_cmd.cmdBuffers[uint32_t(cmdBuffer)];

      if (likely(cmdBuffer == DxvkCmdBuffer::ExecBuffer || buffer))
        return buffer;

      // Allocate a new command buffer if necessary
      buffer = allocateCommandBuffer(cmdBuffer);
      m_cmd.cmdBuffers[uint32_t(cmdBuffer)] = buffer;
      return buffer;
    }

    DxvkSparseBindSubmission& getSparseBindSubmission() {
      if (likely(m_cmd.sparseBind))
        return m_cmdSparseBinds[m_cmd.sparseCmd];

      m_cmd.sparseBind = VK_TRUE;
      m_cmd.sparseCmd = uint32_t(m_cmdSparseBinds.size());

      return m_cmdSparseBinds.emplace_back();
    }

    void endCommandBuffer(VkCommandBuffer cmdBuffer);

    VkCommandBuffer allocateCommandBuffer(DxvkCmdBuffer type);

  };
  
}
