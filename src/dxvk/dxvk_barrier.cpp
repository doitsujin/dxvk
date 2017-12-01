#include "dxvk_barrier.h"

namespace dxvk {
  
  DxvkBarrierSet:: DxvkBarrierSet() { }
  DxvkBarrierSet::~DxvkBarrierSet() { }
  
  void DxvkBarrierSet::accessBuffer(
    const Rc<DxvkBuffer>&           buffer,
          VkDeviceSize              offset,
          VkDeviceSize              size,
          VkPipelineStageFlags      stages,
          VkAccessFlags             access) {
    const DxvkResourceAccessTypes accessTypes
      = this->getAccessTypes(access);
    
    m_srcStages |= stages;
    m_dstStages |= buffer->info().stages;
    
    if (accessTypes.test(DxvkResourceAccessType::Write)) {
      VkBufferMemoryBarrier barrier;
      barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      barrier.pNext               = nullptr;
      barrier.srcAccessMask       = access;
      barrier.dstAccessMask       = buffer->info().access;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.buffer              = buffer->handle();
      barrier.offset              = offset;
      barrier.size                = size;
      m_bufBarriers.push_back(barrier);
    }
  }
  
  
  void DxvkBarrierSet::recordCommands(const Rc<DxvkCommandList>& commandList) {
    if ((m_srcStages | m_dstStages) != 0) {
      VkPipelineStageFlags srcFlags = m_srcStages;
      VkPipelineStageFlags dstFlags = m_dstStages;
      
      if (srcFlags == 0) srcFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      if (dstFlags == 0) dstFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
      
      commandList->cmdPipelineBarrier(
        srcFlags, dstFlags, 0,
        m_memBarriers.size(), m_memBarriers.data(),
        m_bufBarriers.size(), m_bufBarriers.data(),
        m_imgBarriers.size(), m_imgBarriers.data());
      
      this->reset();
    }
  }
  
  
  void DxvkBarrierSet::reset() {
    m_srcStages = 0;
    m_dstStages = 0;
    
    m_memBarriers.resize(0);
    m_bufBarriers.resize(0);
    m_imgBarriers.resize(0);
  }
  
  
  DxvkResourceAccessTypes DxvkBarrierSet::getAccessTypes(VkAccessFlags flags) const {
    const VkAccessFlags rflags
      = VK_ACCESS_INDIRECT_COMMAND_READ_BIT
      | VK_ACCESS_INDEX_READ_BIT
      | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
      | VK_ACCESS_UNIFORM_READ_BIT
      | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
      | VK_ACCESS_SHADER_READ_BIT
      | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
      | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
      | VK_ACCESS_TRANSFER_READ_BIT
      | VK_ACCESS_HOST_READ_BIT
      | VK_ACCESS_MEMORY_READ_BIT;
      
    const VkAccessFlags wflags
      = VK_ACCESS_SHADER_WRITE_BIT
      | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
      | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
      | VK_ACCESS_TRANSFER_WRITE_BIT
      | VK_ACCESS_HOST_WRITE_BIT
      | VK_ACCESS_MEMORY_WRITE_BIT;
    
    DxvkResourceAccessTypes result;
    if (flags & rflags) result.set(DxvkResourceAccessType::Read);
    if (flags & wflags) result.set(DxvkResourceAccessType::Write);
    return result;
  }
  
}