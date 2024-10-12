#include "dxvk_gpu_event.h"
#include "dxvk_device.h"

namespace dxvk {

  DxvkGpuEvent::DxvkGpuEvent(
          DxvkGpuEventPool*           parent)
  : m_pool(parent) {
    auto vk = m_pool->m_vkd;

    VkEventCreateInfo info = { VK_STRUCTURE_TYPE_EVENT_CREATE_INFO };
    VkResult vr = vk->vkCreateEvent(vk->device(), &info, nullptr, &m_event);

    if (vr != VK_SUCCESS)
      throw DxvkError(str::format("Failed to create event: ", vr));
  }


  DxvkGpuEvent::~DxvkGpuEvent() {
    auto vk = m_pool->m_vkd;
    vk->vkDestroyEvent(vk->device(), m_event, nullptr);
  }


  void DxvkGpuEvent::free() {
    m_pool->freeEvent(this);
  }



  DxvkEvent::DxvkEvent(const Rc<DxvkDevice>& device)
  : m_device(device) { }


  DxvkEvent::~DxvkEvent() {

  }


  DxvkGpuEventStatus DxvkEvent::test() {
    std::lock_guard lock(m_mutex);

    if (m_status == VK_EVENT_SET)
      return DxvkGpuEventStatus::Signaled;

    if (!m_gpuEvent)
      return DxvkGpuEventStatus::Invalid;

    // Query current event status and recycle
    // it as soon as a signal is observed.
    auto vk = m_device->vkd();

    m_status = vk->vkGetEventStatus(
      vk->device(), m_gpuEvent->handle());

    switch (m_status) {
      case VK_EVENT_SET:
        m_gpuEvent = nullptr;
        return DxvkGpuEventStatus::Signaled;

      case VK_EVENT_RESET:
        return DxvkGpuEventStatus::Pending;

      default:
        return DxvkGpuEventStatus::Invalid;
    }
  }


  void DxvkEvent::assignGpuEvent(
          Rc<DxvkGpuEvent>             event) {
    std::lock_guard lock(m_mutex);

    m_gpuEvent = std::move(event);
    m_status = VK_NOT_READY;
  }




  DxvkGpuEventPool::DxvkGpuEventPool(const DxvkDevice* device)
  : m_vkd(device->vkd()) { }


  DxvkGpuEventPool::~DxvkGpuEventPool() {
    for (auto e : m_freeEvents)
      delete e;
  }

  
  Rc<DxvkGpuEvent> DxvkGpuEventPool::allocEvent() {
    std::lock_guard lock(m_mutex);

    Rc<DxvkGpuEvent> event;

    if (m_freeEvents.empty()) {
      event = new DxvkGpuEvent(this);
    } else {
      event = m_freeEvents.back();
      m_freeEvents.pop_back();
    }

    m_vkd->vkResetEvent(m_vkd->device(), event->handle());
    return event;
  }


  void DxvkGpuEventPool::freeEvent(DxvkGpuEvent* event) {
    std::lock_guard lock(m_mutex);
    m_freeEvents.push_back(event);
  }

}