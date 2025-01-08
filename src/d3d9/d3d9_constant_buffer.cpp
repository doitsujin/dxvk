#include "d3d9_constant_buffer.h"
#include "d3d9_device.h"

namespace dxvk {

  D3D9ConstantBuffer::D3D9ConstantBuffer() {

  }


  D3D9ConstantBuffer::D3D9ConstantBuffer(
          D3D9DeviceEx*         pDevice,
          DxsoProgramType       ShaderStage,
          DxsoConstantBuffers   BufferType,
          VkDeviceSize          Size)
  : D3D9ConstantBuffer(pDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, GetShaderStage(ShaderStage),
      computeResourceSlotId(ShaderStage, DxsoBindingType::ConstantBuffer, BufferType),
      Size) {

  }


  D3D9ConstantBuffer::D3D9ConstantBuffer(
          D3D9DeviceEx*         pDevice,
          VkBufferUsageFlags    Usage,
          VkShaderStageFlags    Stages,
          uint32_t              ResourceSlot,
          VkDeviceSize          Size)
  : m_device    (pDevice)
  , m_binding   (ResourceSlot)
  , m_usage     (Usage)
  , m_stages    (Stages)
  , m_size      (Size)
  , m_align     (getAlignment(pDevice->GetDXVKDevice())) {

  }


  D3D9ConstantBuffer::~D3D9ConstantBuffer() {

  }


  void* D3D9ConstantBuffer::Alloc(VkDeviceSize size) {
    if (unlikely(m_buffer == nullptr))
      m_slice = this->createBuffer();

    size = align(size, m_align);

    if (unlikely(m_offset + size > m_size)) {
      m_slice = m_buffer->allocateStorage();
      m_offset = 0;

      m_device->EmitCs([
        cBuffer = m_buffer,
        cSlice  = m_slice
      ] (DxvkContext* ctx) mutable {
        ctx->invalidateBuffer(cBuffer, std::move(cSlice));
      });
    }

    m_device->EmitCs([
      cStages   = m_stages,
      cBinding  = m_binding,
      cOffset   = m_offset,
      cLength   = size
    ] (DxvkContext* ctx) {
      ctx->bindUniformBufferRange(cStages, cBinding, cOffset, cLength);
    });

    void* mapPtr = reinterpret_cast<char*>(m_slice->mapPtr()) + m_offset;
    m_offset += size;
    return mapPtr;
  }


  void* D3D9ConstantBuffer::AllocSlice() {
    if (unlikely(m_buffer == nullptr))
      m_slice = this->createBuffer();
    else
      m_slice = m_buffer->allocateStorage();

    m_device->EmitCs([
      cBuffer = m_buffer,
      cSlice  = m_slice
    ] (DxvkContext* ctx) mutable {
      ctx->invalidateBuffer(cBuffer, std::move(cSlice));
    });

    return m_slice->mapPtr();
  }


  Rc<DxvkResourceAllocation> D3D9ConstantBuffer::createBuffer() {
    auto options = m_device->GetOptions();

    // Buffer usage and access flags don't make much of a difference
    // in the backend, so set both STORAGE and UNIFORM usage/access.
    DxvkBufferCreateInfo bufferInfo;
    bufferInfo.size   = align(m_size, m_align);
    bufferInfo.usage  = m_usage;
    bufferInfo.access = 0;
    bufferInfo.stages = util::pipelineStages(m_stages);
    bufferInfo.debugName = "Constant buffer";

    if (m_usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
      bufferInfo.access |= VK_ACCESS_UNIFORM_READ_BIT;
    if (m_usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
      bufferInfo.access |= VK_ACCESS_SHADER_READ_BIT;

    VkMemoryPropertyFlags memoryFlags
      = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    if (options->deviceLocalConstantBuffers)
      memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    m_buffer = m_device->GetDXVKDevice()->createBuffer(bufferInfo, memoryFlags);

    m_device->EmitCs([
      cStages   = m_stages,
      cBinding  = m_binding,
      cSlice    = DxvkBufferSlice(m_buffer)
    ] (DxvkContext* ctx) mutable {
      ctx->bindUniformBuffer(cStages, cBinding, std::move(cSlice));
    });

    return m_buffer->storage();
  }


  VkDeviceSize D3D9ConstantBuffer::getAlignment(const Rc<DxvkDevice>& device) const {
    return std::max(std::max(
      device->properties().core.properties.limits.minUniformBufferOffsetAlignment,
      device->properties().core.properties.limits.minStorageBufferOffsetAlignment),
      device->properties().extRobustness2.robustUniformBufferAccessSizeAlignment);
  }

}
