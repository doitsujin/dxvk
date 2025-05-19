#include "dxvk_device.h"

namespace dxvk {
  
  DxvkUnboundResources::DxvkUnboundResources(DxvkDevice* dev)
  : m_device(dev) {

  }
  
  
  DxvkUnboundResources::~DxvkUnboundResources() {
    
  }


  DxvkResourceBufferInfo DxvkUnboundResources::bufferInfo() {
    if (unlikely(!m_bufferCreated.load(std::memory_order_acquire))) {
      std::lock_guard lock(m_mutex);

      if (!m_bufferCreated.load(std::memory_order_acquire)) {
        m_buffer = createBuffer();
        m_bufferCreated.store(true, std::memory_order_release);
      }
    }

    return m_buffer->getSliceInfo();;
  }


  DxvkSamplerDescriptor DxvkUnboundResources::samplerInfo() {
    if (unlikely(!m_samplerCreated.load(std::memory_order_acquire))) {
      std::lock_guard lock(m_mutex);

      if (!m_samplerCreated.load(std::memory_order_acquire)) {
        m_sampler = createSampler();
        m_samplerCreated.store(true, std::memory_order_release);
      }
    }

    return m_sampler->getDescriptor();
  }


  Rc<DxvkSampler> DxvkUnboundResources::createSampler() {
    DxvkSamplerKey info;
    info.setFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR);
    info.setLodRange(-256.0f, 256.0f, 0.0f);
    info.setAddressModes(
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    info.setReduction(VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE);

    return m_device->createSampler(info);
  }
  
  
  Rc<DxvkBuffer> DxvkUnboundResources::createBuffer() {
    DxvkBufferCreateInfo info;
    info.size       = MaxUniformBufferSize;
    info.usage      = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                    | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                    | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                    | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                    | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                    | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
                    | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
                    | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;
    info.stages     = VK_PIPELINE_STAGE_TRANSFER_BIT
                    | m_device->getShaderPipelineStages();
    info.access     = VK_ACCESS_UNIFORM_READ_BIT
                    | VK_ACCESS_SHADER_READ_BIT
                    | VK_ACCESS_SHADER_WRITE_BIT;
    info.debugName  = "Null buffer";
    
    Rc<DxvkBuffer> buffer = m_device->createBuffer(info,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    std::memset(buffer->mapPtr(0), 0, info.size);
    return buffer;
  }
  
}
