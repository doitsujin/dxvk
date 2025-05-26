#include "dxvk_descriptor_heap.h"
#include "dxvk_device.h"

namespace dxvk {

  DxvkResourceDescriptorRange::DxvkResourceDescriptorRange(
          DxvkResourceDescriptorHeap*         heap,
          Rc<DxvkBuffer>                      gpuBuffer,
          Rc<DxvkBuffer>                      cpuBuffer,
          VkDeviceSize                        rangeSize,
          VkDeviceSize                        rangeIndex,
          VkDeviceSize                        rangeCount)
  : m_heap        (heap),
    m_gpuBuffer   (std::move(gpuBuffer)),
    m_cpuBuffer   (std::move(cpuBuffer)),
    m_rangeOffset (rangeSize * rangeIndex),
    m_rangeSize   (rangeSize),
    m_heapSize    (rangeSize * rangeCount),
    m_bufferSize  (m_gpuBuffer->info().size),
    m_rangeInfo   (m_gpuBuffer->getSliceInfo(m_rangeOffset, m_rangeSize)) {
    m_rangeInfo.mapPtr = m_cpuBuffer->getSliceInfo(m_rangeOffset, m_rangeSize).mapPtr;
  }


  DxvkResourceDescriptorRange::~DxvkResourceDescriptorRange() {

  }




  DxvkResourceDescriptorHeap::DxvkResourceDescriptorHeap(DxvkDevice* device)
  : m_device(device) {

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
    constexpr VkDeviceSize HeapSize = env::is32BitHostPlatform() ? (4ull << 20) : (8ull << 20);

    constexpr VkDeviceSize SliceCount = 8u;
    constexpr VkDeviceSize SliceSize = HeapSize / SliceCount;

    // On integrated graphics, map the descriptor heap directly. Otherwise, it
    // is likely beneficial to write to system memory instead and upload the
    // data to VRAM on the transfer queue.
    bool isDiscreteGpu = m_device->properties().core.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

    Rc<DxvkBuffer> gpuBuffer = createGpuBuffer(HeapSize, !isDiscreteGpu);
    Rc<DxvkBuffer> cpuBuffer = isDiscreteGpu ? createCpuBuffer(HeapSize) : gpuBuffer;

    DxvkResourceDescriptorRange* first = nullptr;

    for (uint32_t i = 0u; i < SliceCount; i++) {
      auto& range = m_ranges.emplace_back(this, gpuBuffer, cpuBuffer, SliceSize, i, SliceCount);

      if (!first)
        first = &range;
    }

    return first;
  }


  Rc<DxvkBuffer> DxvkResourceDescriptorHeap::createGpuBuffer(VkDeviceSize baseSize, bool mapped) {
    DxvkBufferCreateInfo info = { };
    info.size = baseSize;
    info.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
               | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
    info.debugName = "Resource heap";

    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (mapped) {
      memoryFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                  |  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    } else {
      info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      info.stages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
      info.access |= VK_ACCESS_TRANSFER_WRITE_BIT;
    }

    m_device->addStatCtr(DxvkStatCounter::DescriptorHeapSize, info.size);
    m_device->addStatCtr(DxvkStatCounter::DescriptorHeapCount, 1u);
    return m_device->createBuffer(info, memoryFlags);
  }


  Rc<DxvkBuffer> DxvkResourceDescriptorHeap::createCpuBuffer(VkDeviceSize baseSize) {
    DxvkBufferCreateInfo info = { };
    info.size = baseSize;
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    info.access = VK_ACCESS_TRANSFER_READ_BIT;
    info.debugName = "Descriptor upload buffer";

    return m_device->createBuffer(info,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }

}
