#include "dxvk_gpu_event.h"
#include "dxvk_device.h"

namespace dxvk {

  DxvkGpuEvent::DxvkGpuEvent(const Rc<vk::DeviceFn>& vkd)
  : m_vkd(vkd) { }


  DxvkGpuEvent::~DxvkGpuEvent() {
    if (m_handle.pool && m_handle.event)
      m_handle.pool->freeEvent(m_handle.event);
  }


  DxvkGpuEventStatus DxvkGpuEvent::test() const {
    if (!m_handle.event)
      return DxvkGpuEventStatus::Invalid;
    
    VkResult status = m_vkd->vkGetEventStatus(
      m_vkd->device(), m_handle.event);
    
    switch (status) {
      case VK_EVENT_SET:    return DxvkGpuEventStatus::Signaled;
      case VK_EVENT_RESET:  return DxvkGpuEventStatus::Pending;
      default:              return DxvkGpuEventStatus::Invalid;
    }
  }


  DxvkGpuEventHandle DxvkGpuEvent::reset(DxvkGpuEventHandle handle) {
    m_vkd->vkResetEvent(m_vkd->device(), handle.event);
    return std::exchange(m_handle, handle);
  }




  DxvkGpuEventPool::DxvkGpuEventPool(const DxvkDevice* device)
  : m_vkd(device->vkd()) { }


  DxvkGpuEventPool::~DxvkGpuEventPool() {
    for (VkEvent ev : m_events)
      m_vkd->vkDestroyEvent(m_vkd->device(), ev, nullptr);
  }

  
  DxvkGpuEventHandle DxvkGpuEventPool::allocEvent() {
    VkEvent event = VK_NULL_HANDLE;

    { std::lock_guard<sync::Spinlock> lock(m_mutex);
      
      if (m_events.size() > 0) {
        event = m_events.back();
        m_events.pop_back();
      }
    }

    if (!event) {
      VkEventCreateInfo info;
      info.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
      info.pNext = nullptr;
      info.flags = 0;

      VkResult status = m_vkd->vkCreateEvent(
        m_vkd->device(), &info, nullptr, &event);
      
      if (status != VK_SUCCESS) {
        Logger::err("DXVK: Failed to create GPU event");
        return DxvkGpuEventHandle();
      }
    }

    return { this, event };
  }


  void DxvkGpuEventPool::freeEvent(VkEvent event) {
    std::lock_guard<sync::Spinlock> lock(m_mutex);
    m_events.push_back(event);
  }




  DxvkGpuEventTracker::DxvkGpuEventTracker() { }
  DxvkGpuEventTracker::~DxvkGpuEventTracker() { }


  void DxvkGpuEventTracker::trackEvent(DxvkGpuEventHandle handle) {
    if (handle.pool && handle.event)
      m_handles.push_back(handle);
  }


  void DxvkGpuEventTracker::reset() {
    for (const auto& h : m_handles)
      h.pool->freeEvent(h.event);
    
    m_handles.clear();
  }

}