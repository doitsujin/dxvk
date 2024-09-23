#include "dxvk_barrier.h"
#include "dxvk_buffer.h"
#include "dxvk_device.h"

#include <algorithm>

namespace dxvk {
  
  DxvkBuffer::DxvkBuffer(
          DxvkDevice*           device,
    const DxvkBufferCreateInfo& createInfo,
          DxvkMemoryAllocator&  allocator,
          VkMemoryPropertyFlags memFlags)
  : m_vkd           (device->vkd()),
    m_allocator     (&allocator),
    m_properties    (memFlags),
    m_shaderStages  (util::shaderStages(createInfo.stages)),
    m_info          (createInfo) {
    // Create and assign actual buffer resource
    assignSlice(allocateSlice());
  }


  DxvkBuffer::DxvkBuffer(
          DxvkDevice*           device,
    const DxvkBufferCreateInfo& createInfo,
    const DxvkBufferImportInfo& importInfo,
          VkMemoryPropertyFlags memFlags)
  : m_vkd           (device->vkd()),
    m_properties    (memFlags),
    m_shaderStages  (util::shaderStages(createInfo.stages)),
    m_info          (createInfo),
    m_import        (importInfo) {

  }


  DxvkBuffer::~DxvkBuffer() {

  }



  
  DxvkBufferView::DxvkBufferView(
          DxvkDevice*               device,
    const Rc<DxvkBuffer>&           buffer,
    const DxvkBufferViewCreateInfo& info)
  : m_vkd(device->vkd()), m_info(info), m_buffer(buffer),
    m_usage       (device->features().khrMaintenance5.maintenance5 ? info.usage : 0u),
    m_bufferSlice (getSliceHandle()),
    m_bufferView  (VK_NULL_HANDLE) {
    if (m_info.format != VK_FORMAT_UNDEFINED)
      m_bufferView = createBufferView(m_bufferSlice);
  }
  
  
  DxvkBufferView::~DxvkBufferView() {
    if (m_views.empty()) {
      m_vkd->vkDestroyBufferView(
        m_vkd->device(), m_bufferView, nullptr);
    } else {
      for (const auto& pair : m_views) {
        m_vkd->vkDestroyBufferView(
          m_vkd->device(), pair.second, nullptr);
      }
    }
  }
  
  
  VkBufferView DxvkBufferView::createBufferView(
    const DxvkBufferSliceHandle& slice) {
    VkBufferUsageFlags2CreateInfoKHR viewFlags = { VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO_KHR };
    viewFlags.usage = m_usage;

    VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
    viewInfo.buffer = slice.handle;
    viewInfo.format = m_info.format;
    viewInfo.offset = slice.offset;
    viewInfo.range  = slice.length;

    if (m_usage)
      viewInfo.pNext = &viewFlags;

    VkBufferView result = VK_NULL_HANDLE;

    if (m_vkd->vkCreateBufferView(m_vkd->device(),
          &viewInfo, nullptr, &result) != VK_SUCCESS) {
      throw DxvkError(str::format(
        "DxvkBufferView: Failed to create buffer view:",
        "\n  Offset: ", viewInfo.offset,
        "\n  Range:  ", viewInfo.range,
        "\n  Format: ", viewInfo.format));
    }

    return result;
  }


  void DxvkBufferView::updateBufferView(
    const DxvkBufferSliceHandle& slice) {
    if (m_info.format != VK_FORMAT_UNDEFINED) {
      if (m_views.empty())
        m_views.insert({ m_bufferSlice, m_bufferView });

      m_bufferSlice = slice;

      auto entry = m_views.find(slice);
      if (entry != m_views.end()) {
        m_bufferView = entry->second;
      } else {
        m_bufferView = createBufferView(m_bufferSlice);
        m_views.insert({ m_bufferSlice, m_bufferView });
      }
    } else {
      m_bufferSlice = slice;
    }
  }
  
}