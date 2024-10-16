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
    m_sharingMode   (device->getSharingMode()),
    m_info          (createInfo) {
    // Create and assign actual buffer resource
    assignStorage(allocateStorage());
  }


  DxvkBuffer::DxvkBuffer(
          DxvkDevice*           device,
    const DxvkBufferCreateInfo& createInfo,
    const DxvkBufferImportInfo& importInfo,
          DxvkMemoryAllocator&  allocator,
          VkMemoryPropertyFlags memFlags)
  : m_vkd           (device->vkd()),
    m_allocator     (&allocator),
    m_properties    (memFlags),
    m_shaderStages  (util::shaderStages(createInfo.stages)),
    m_sharingMode   (device->getSharingMode()),
    m_info          (createInfo) {
    VkBufferCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    info.flags = m_info.flags;
    info.usage = m_info.usage;
    info.size = m_info.size;
    m_sharingMode.fill(info);

    assignStorage(allocator.importBufferResource(info, importInfo));
  }


  DxvkBuffer::~DxvkBuffer() {

  }


  bool DxvkBuffer::canRelocate() const {
    return !m_bufferInfo.mapPtr && !m_stableAddress
        && !m_storage->flags().test(DxvkAllocationFlag::Imported)
        && !(m_info.flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT);
  }


  Rc<DxvkBufferView> DxvkBuffer::createView(
    const DxvkBufferViewKey& info) {
    std::unique_lock lock(m_viewMutex);

    auto entry = m_views.emplace(std::piecewise_construct,
      std::make_tuple(info), std::make_tuple(this, info));

    return &entry.first->second;
  }


  DxvkSparsePageTable* DxvkBuffer::getSparsePageTable() {
    return m_storage->getSparsePageTable();
  }
  
}
