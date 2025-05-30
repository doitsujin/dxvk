#include "dxvk_descriptor_heap.h"
#include "dxvk_device.h"

namespace dxvk {

  DxvkResourceDescriptorRange::DxvkResourceDescriptorRange(
          DxvkResourceDescriptorHeap*         heap,
          Rc<DxvkBuffer>                      gpuBuffer,
          VkDeviceSize                        rangeSize,
          VkDeviceSize                        rangeIndex,
          VkDeviceSize                        reservedSize)
  : m_heap        (heap),
    m_gpuBuffer   (std::move(gpuBuffer)),
    m_rangeOffset (rangeSize * rangeIndex + reservedSize),
    m_rangeSize   (rangeSize),
    m_reservedSize(reservedSize),
    m_bufferSize  (m_gpuBuffer->info().size),
    m_rangeInfo   (m_gpuBuffer->getSliceInfo(m_rangeOffset, m_rangeSize)) {

  }


  DxvkResourceDescriptorRange::~DxvkResourceDescriptorRange() {

  }




  DxvkResourceDescriptorHeap::DxvkResourceDescriptorHeap(DxvkDevice* device)
  : m_device(device) {
    if (m_device->canUseDescriptorHeap())
      m_reservedSize = device->properties().extDescriptorHeap.minResourceHeapReservedRange;
  }


  DxvkResourceDescriptorHeap::~DxvkResourceDescriptorHeap() {

  }


  Rc<DxvkResourceDescriptorRange> DxvkResourceDescriptorHeap::allocRange() {
    VkDeviceAddress baseAddress = 0u;

    if (likely(m_currentRange))
      baseAddress = m_currentRange->getHeapInfo().gpuAddress;

    // Check if there are any existing ranges not in use, and prioritize
    // a range with the same base address as the current one.
    DxvkResourceDescriptorRange* newRange = nullptr;

    for (auto& r : m_ranges) {
      if (!r.isInUse()) {
        newRange = &r;

        if (r.getHeapInfo().gpuAddress == baseAddress)
          break;
      }
    }

    // If there is no unused range, allocate a new one.
    if (!newRange)
      newRange = addRanges();

    newRange->reset();

    return (m_currentRange = newRange);
  }


  DxvkResourceDescriptorRange* DxvkResourceDescriptorHeap::addRanges() {
    // Use a fixed heap size regardless of descriptor size. This avoids
    // creating unnecessarily large buffers in simple apps on devices
    // that have pathologically large descriptors.
    constexpr VkDeviceSize MaxHeapSize = env::is32BitHostPlatform() ? (4ull << 20) : (8ull << 20);
    constexpr VkDeviceSize SliceCount = 8u;

    // Check selected heap size against device capabilities. If the device
    // gives us indices in place of real descriptors, we might only get a
    // smaller maximum supported size as well.
    VkDeviceSize deviceHeapSize = MaxHeapSize;
    VkDeviceSize deviceDescriptorAlignment = m_device->getDescriptorProperties().getDescriptorSetAlignment();

    if (m_device->canUseDescriptorHeap()) {
      const auto& properties = m_device->properties().extDescriptorHeap;
      deviceHeapSize = std::min(deviceHeapSize, properties.maxResourceHeapSize - properties.minResourceHeapReservedRange);
    }

    if (m_device->canUseDescriptorBuffer())
      deviceHeapSize = std::min(deviceHeapSize, m_device->properties().extDescriptorBuffer.maxResourceDescriptorBufferRange);

    // Ensure that the selected slice size meets all alignment requirements.
    VkDeviceSize sliceSize = std::min(MaxHeapSize, deviceHeapSize) / SliceCount;
    sliceSize &= ~(deviceDescriptorAlignment - 1u);

    // Create buffer and add ranges all using one slice of that new buffer
    Rc<DxvkBuffer> buffer = createBuffer(sliceSize * SliceCount);

    DxvkResourceDescriptorRange* first = nullptr;

    for (uint32_t i = 0u; i < SliceCount; i++) {
      auto& range = m_ranges.emplace_back(this, buffer, sliceSize, i, m_reservedSize);

      if (!first)
        first = &range;
    }

    return first;
  }


  Rc<DxvkBuffer> DxvkResourceDescriptorHeap::createBuffer(VkDeviceSize baseSize) {
    DxvkBufferCreateInfo info = { };
    info.size = baseSize + m_reservedSize;
    info.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    info.debugName = "Resource heap";

    if (m_device->canUseDescriptorHeap())
      info.usage |= VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT;

    if (m_device->canUseDescriptorBuffer())
      info.usage |= VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;

    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                      | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    Logger::info(str::format("Creating resource descriptor heap (", info.size >> 10u, " kB)"));

    m_device->addStatCtr(DxvkStatCounter::DescriptorHeapSize, info.size);
    m_device->addStatCtr(DxvkStatCounter::DescriptorHeapCount, 1u);
    return m_device->createBuffer(info, memoryFlags);
  }

}
