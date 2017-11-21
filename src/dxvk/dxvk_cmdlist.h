#pragma once

#include <unordered_set>

#include "dxvk_descriptor.h"
#include "dxvk_lifetime.h"
#include "dxvk_recorder.h"

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
  class DxvkCommandList : public DxvkRecorder {
    
  public:
    
    DxvkCommandList(
      const Rc<vk::DeviceFn>& vkd,
            uint32_t          queueFamily);
    ~DxvkCommandList();
    
    /**
     * \brief Command buffer handle
     * \returns Command buffer handle
     */
    VkCommandBuffer handle() const {
      return m_buffer;
    }
    
    /**
     * \brief Begins recording
     * 
     * Resets the command buffer and
     * begins command buffer recording.
     */
    void beginRecording() final;
    
    /**
     * \brief Ends recording
     * 
     * Ends command buffer recording, making
     * the command list ready for submission.
     */
    void endRecording() final;
    
    /**
     * \brief Adds a resource to track
     * 
     * Adds a resource to the internal resource tracker.
     * Resources will be kept alive and "in use" until
     * the device can guarantee that the submission has
     * completed.
     */
    void trackResource(
      const Rc<DxvkResource>& rc) final;
    
    /**
     * \brief Resets the command list
     * 
     * Resets the internal command buffer of the command list and
     * marks all tracked resources as unused. When submitting the
     * command list to the device, this method will be called once
     * the command list completes execution.
     */
    void reset() final;
    
    void bindShaderResources(
            VkPipelineBindPoint     pipeline,
            VkPipelineLayout        pipelineLayout,
            VkDescriptorSetLayout   descriptorLayout,
            uint32_t                bindingCount,
      const DxvkResourceBinding*    bindings) final;
    
    void cmdBeginRenderPass(
      const VkRenderPassBeginInfo*  pRenderPassBegin,
            VkSubpassContents       contents) final;
    
    void cmdBindIndexBuffer(
            VkBuffer                buffer,
            VkDeviceSize            offset,
            VkIndexType             indexType) final;
    
    void cmdBindPipeline(
            VkPipelineBindPoint     pipelineBindPoint,
            VkPipeline              pipeline) final;
    
    void cmdBindVertexBuffers(
            uint32_t                firstBinding,
            uint32_t                bindingCount,
      const VkBuffer*               pBuffers,
      const VkDeviceSize*           pOffsets) final;
    
    void cmdClearAttachments(
            uint32_t                attachmentCount,
      const VkClearAttachment*      pAttachments,
            uint32_t                rectCount,
      const VkClearRect*            pRects) final;
    
    void cmdDispatch(
            uint32_t                x,
            uint32_t                y,
            uint32_t                z) final;
    
    void cmdDraw(
            uint32_t                vertexCount,
            uint32_t                instanceCount,
            uint32_t                firstVertex,
            uint32_t                firstInstance) final;
    
    void cmdDrawIndexed(
            uint32_t                indexCount,
            uint32_t                instanceCount,
            uint32_t                firstIndex,
            uint32_t                vertexOffset,
            uint32_t                firstInstance) final;
    
    void cmdEndRenderPass() final;
    
    void cmdPipelineBarrier(
            VkPipelineStageFlags    srcStageMask,
            VkPipelineStageFlags    dstStageMask,
            VkDependencyFlags       dependencyFlags,
            uint32_t                memoryBarrierCount,
      const VkMemoryBarrier*        pMemoryBarriers,
            uint32_t                bufferMemoryBarrierCount,
      const VkBufferMemoryBarrier*  pBufferMemoryBarriers,
            uint32_t                imageMemoryBarrierCount,
      const VkImageMemoryBarrier*   pImageMemoryBarriers) final;
    
    void cmdSetScissor(
            uint32_t                firstScissor,
            uint32_t                scissorCount,
      const VkRect2D*               scissors) final;
    
    void cmdSetViewport(
            uint32_t                firstViewport,
            uint32_t                viewportCount,
      const VkViewport*             viewports) final;
    
  private:
    
    Rc<vk::DeviceFn>    m_vkd;
    
    VkCommandPool       m_pool;
    VkCommandBuffer     m_buffer;
    
    DxvkLifetimeTracker m_resources;
    DxvkDescriptorAlloc m_descAlloc;
    
    std::vector<VkWriteDescriptorSet> m_descriptorSetWrites;
    
  };
  
}