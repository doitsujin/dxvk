#include "d3d9_constant_buffer.h"
#include "d3d9_device.h"

namespace dxvk {

  D3D9ConstantBuffer::D3D9ConstantBuffer() {

  }


  D3D9ConstantBuffer::D3D9ConstantBuffer(
          D3D9DeviceEx*         pDevice,
          Kind                  CbvType)
  : m_device    (pDevice)
  , m_kind      (CbvType)
  , m_size      (DetermineSize(CbvType))
  , m_usage     (DetermineUsage(pDevice, CbvType, m_size))
  , m_stages    (DetermineStages(CbvType))
  , m_align     (DetermineAlignment(pDevice, m_usage))
  , m_memType   (DetermineMemoryType(CbvType)) {

  }


  D3D9ConstantBuffer::~D3D9ConstantBuffer() {

  }


  void* D3D9ConstantBuffer::Alloc(VkDeviceSize size) {
    if (unlikely(!m_cpuBuffer))
      m_cpuSlice = createBuffer();

    size = align(size, m_align);

    if (m_offset + size > m_size) {
      m_cpuSlice = m_cpuBuffer->allocateStorage();

      m_device->EmitCs([
        cBinding  = uint32_t(m_kind),
        cStages   = m_stages,
        cBuffer   = m_cpuBuffer,
        cSlice    = m_cpuSlice,
        cSize     = size
      ] (DxvkContext* ctx) mutable {
        ctx->invalidateBuffer(cBuffer, std::move(cSlice));
        ctx->bindUniformBufferRange(cStages, cBinding, 0u, cSize);
      });

      m_offset = size;

      return m_cpuSlice->mapPtr();
    } else {
      m_device->EmitCs([
        cBinding  = uint32_t(m_kind),
        cStages   = m_stages,
        cOffset   = m_offset,
        cSize     = size
      ] (DxvkContext* ctx) {
        ctx->bindUniformBufferRange(cStages, cBinding, cOffset, cSize);
      });

      void* mapPtr = reinterpret_cast<char*>(m_cpuSlice->mapPtr()) + m_offset;
      m_offset += size;
      return mapPtr;
    }
  }


  Rc<DxvkResourceAllocation> D3D9ConstantBuffer::createBuffer() {
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

    m_cpuBuffer = m_device->GetDXVKDevice()->createBuffer(bufferInfo, m_memType);

    m_device->EmitCs([
      cBinding  = uint32_t(m_kind),
      cStages   = m_stages,
      cSlice    = DxvkBufferSlice(m_cpuBuffer)
    ] (DxvkContext* ctx) mutable {
      ctx->bindUniformBuffer(cStages, cBinding, std::move(cSlice));
    });

    return m_cpuBuffer->storage();
  }


  VkDeviceSize D3D9ConstantBuffer::DetermineSize(
          Kind                CbvType) {
    switch (CbvType) {
      case Kind::VSStaticConstants:
      case Kind::VSDynamicConstants:
      case Kind::PSStaticConstants:
        return ShaderConstSize;

      default:
        return MiscSize;
    }
  }


  VkBufferUsageFlags D3D9ConstantBuffer::DetermineUsage(
          D3D9DeviceEx*       pDevice,
          Kind                CbvType,
          VkDeviceSize        Size) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    // SWVP vertex constant buffers can get bigger than the minimum required
    // size for uniform buffers on some devices. Don't bother figuring out
    // the layout, just always allow storage buffer fallback in that case.
    if ((CbvType == Kind::VSStaticConstants || CbvType == Kind::VSDynamicConstants) && pDevice->CanSWVP())
      usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    // Vertex blend is a bit special w.r.t. nonuniform indexing, so enable
    // storage buffer usage there as well.
    if (CbvType == Kind::VSVertexBlendData)
      usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    return usage;
  }


  VkShaderStageFlags D3D9ConstantBuffer::DetermineStages(
          Kind                CbvType) {
    switch (CbvType) {
      case Kind::VSClipPlanes:
      case Kind::VSFixedFunction:
      case Kind::VSVertexBlendData:
      case Kind::VSStaticConstants:
      case Kind::VSDynamicConstants:
        return VK_SHADER_STAGE_VERTEX_BIT;

      case Kind::PSFixedFunction:
      case Kind::PSShared:
      case Kind::PSStaticConstants:
        return VK_SHADER_STAGE_FRAGMENT_BIT;

      case Kind::SpecData:
        return VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    return 0u;
  }


  VkDeviceSize D3D9ConstantBuffer::DetermineAlignment(
          D3D9DeviceEx*       pDevice,
          VkBufferUsageFlags  Usage) {
    auto dxvkDevice = pDevice->GetDXVKDevice();

    // At least a full cache line for CPU write perf
    VkDeviceSize result = CACHE_LINE_SIZE;

    // Check required offset alignment and robustness properties by usage.
    // Some buffers do not require robustness, but don't bother with that.
    // We will likely get no more than 64 bytes out of this anyway.
    if (Usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
      result = std::max(result, dxvkDevice->properties().core.properties.limits.minUniformBufferOffsetAlignment);
      result = std::max(result, dxvkDevice->properties().extRobustness2.robustUniformBufferAccessSizeAlignment);
    }

    if (Usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
      result = std::max(result, dxvkDevice->properties().core.properties.limits.minStorageBufferOffsetAlignment);
      result = std::max(result, dxvkDevice->properties().extRobustness2.robustStorageBufferAccessSizeAlignment);
    }

    return result;
  }


  VkMemoryPropertyFlags D3D9ConstantBuffer::DetermineMemoryType(
          Kind                CbvType) {
    VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    // Enable cached memory type for the miscellaneous buffers
    // since we don't really optimize write patterns there.
    // TODO fix that at some point.
    bool useCached = CbvType == Kind::VSClipPlanes
                  || CbvType == Kind::VSFixedFunction
                  || CbvType == Kind::VSVertexBlendData
                  || CbvType == Kind::PSFixedFunction
                  || CbvType == Kind::PSShared
                  || CbvType == Kind::SpecData;

    if (useCached)
      flags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

    return flags;
  }

}
