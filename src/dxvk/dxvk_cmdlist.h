#pragma once

#include <unordered_set>

#include "dxvk_descriptor.h"
#include "dxvk_lifetime.h"
#include "dxvk_pipelayout.h"
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
    void trackResource(
      const Rc<DxvkResource>& rc);
    
    /**
     * \brief Resets the command list
     * 
     * Resets the internal command buffer of the command list and
     * marks all tracked resources as unused. When submitting the
     * command list to the device, this method will be called once
     * the command list completes execution.
     */
    void reset();
    
    void bindResourceDescriptors(
            VkPipelineBindPoint     pipeline,
            VkPipelineLayout        pipelineLayout,
            VkDescriptorSetLayout   descriptorLayout,
            uint32_t                descriptorCount,
      const DxvkDescriptorSlot*     descriptorSlots,
      const DxvkDescriptorInfo*     descriptorInfos);
    
    void cmdBeginRenderPass(
      const VkRenderPassBeginInfo*  pRenderPassBegin,
            VkSubpassContents       contents);
    
    void cmdBindIndexBuffer(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            VkIndexType             indexType);
    
    void cmdBindPipeline(
            VkPipelineBindPoint     pipelineBindPoint,
            VkPipeline              pipeline);
    
    void cmdBindVertexBuffers(
            uint32_t                firstBinding,
            uint32_t                bindingCount,
      const VkBuffer*               pBuffers,
      const VkDeviceSize*           pOffsets);
    
    void cmdClearAttachments(
            uint32_t                attachmentCount,
      const VkClearAttachment*      pAttachments,
            uint32_t                rectCount,
      const VkClearRect*            pRects);
    
    void cmdClearColorImage(
            VkImage                 image,
            VkImageLayout           imageLayout,
      const VkClearColorValue*      pColor,
            uint32_t                rangeCount,
      const VkImageSubresourceRange* pRanges);
    
    void cmdClearDepthStencilImage(
            VkImage                 image,
            VkImageLayout           imageLayout,
      const VkClearDepthStencilValue* pDepthStencil,
            uint32_t                rangeCount,
      const VkImageSubresourceRange* pRanges);
    
    void cmdCopyBuffer(
            VkBuffer                srcBuffer,
            VkBuffer                dstBuffer,
            uint32_t                regionCount,
      const VkBufferCopy*           pRegions);
    
    void cmdDispatch(
            uint32_t                x,
            uint32_t                y,
            uint32_t                z);
    
    void cmdDraw(
            uint32_t                vertexCount,
            uint32_t                instanceCount,
            uint32_t                firstVertex,
            uint32_t                firstInstance);
    
    void cmdDrawIndexed(
            uint32_t                indexCount,
            uint32_t                instanceCount,
            uint32_t                firstIndex,
            uint32_t                vertexOffset,
            uint32_t                firstInstance);
    
    void cmdEndRenderPass();
    
    void cmdPipelineBarrier(
            VkPipelineStageFlags    srcStageMask,
            VkPipelineStageFlags    dstStageMask,
            VkDependencyFlags       dependencyFlags,
            uint32_t                memoryBarrierCount,
      const VkMemoryBarrier*        pMemoryBarriers,
            uint32_t                bufferMemoryBarrierCount,
      const VkBufferMemoryBarrier*  pBufferMemoryBarriers,
            uint32_t                imageMemoryBarrierCount,
      const VkImageMemoryBarrier*   pImageMemoryBarriers);
    
    void cmdResolveImage(
            VkImage                 srcImage,
            VkImageLayout           srcImageLayout,
            VkImage                 dstImage,
            VkImageLayout           dstImageLayout,
            uint32_t                regionCount,
      const VkImageResolve*         pRegions);
    
    void cmdUpdateBuffer(
            VkBuffer                dstBuffer,
            VkDeviceSize            dstOffset,
            VkDeviceSize            dataSize,
      const void*                   pData);
    
    void cmdSetBlendConstants(
            float                   blendConstants[4]);
    
    void cmdSetScissor(
            uint32_t                firstScissor,
            uint32_t                scissorCount,
      const VkRect2D*               scissors);
    
    void cmdSetStencilReference(
            VkStencilFaceFlags      faceMask,
            uint32_t                reference);
    
    void cmdSetViewport(
            uint32_t                firstViewport,
            uint32_t                viewportCount,
      const VkViewport*             viewports);
    
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
    
  };
  
}