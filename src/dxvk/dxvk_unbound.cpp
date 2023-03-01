#include "dxvk_device.h"

namespace dxvk {
  
  DxvkUnboundResources::DxvkUnboundResources(DxvkDevice* dev)
  : m_device(dev) {

  }
  
  
  DxvkUnboundResources::~DxvkUnboundResources() {
    
  }


  VkBuffer DxvkUnboundResources::bufferHandle() {
    VkBuffer buffer = m_bufferHandle.load();

    if (likely(buffer != VK_NULL_HANDLE))
      return buffer;

    std::lock_guard lock(m_mutex);
    buffer = m_bufferHandle.load();

    if (buffer)
      return buffer;

    m_buffer = createBuffer();
    buffer = m_buffer->getSliceHandle().handle;

    m_bufferHandle.store(buffer, std::memory_order_release);
    return buffer;
  }


  VkSampler DxvkUnboundResources::samplerHandle() {
    VkSampler sampler = m_samplerHandle.load();

    if (likely(sampler != VK_NULL_HANDLE))
      return sampler;

    std::lock_guard lock(m_mutex);
    sampler = m_samplerHandle.load();

    if (sampler)
      return sampler;

    m_sampler = createSampler();
    sampler = m_sampler->handle();

    m_samplerHandle.store(sampler, std::memory_order_release);
    return sampler;
  }


  Rc<DxvkSampler> DxvkUnboundResources::createSampler() {
    DxvkSamplerCreateInfo info;
    info.minFilter      = VK_FILTER_LINEAR;
    info.magFilter      = VK_FILTER_LINEAR;
    info.mipmapMode     = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.mipmapLodBias  = 0.0f;
    info.mipmapLodMin   = -256.0f;
    info.mipmapLodMax   =  256.0f;
    info.useAnisotropy  = VK_FALSE;
    info.maxAnisotropy  = 1.0f;
    info.addressModeU   = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV   = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW   = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.compareToDepth = VK_FALSE;
    info.compareOp      = VK_COMPARE_OP_NEVER;
    info.reductionMode  = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;
    info.borderColor    = VkClearColorValue();
    info.usePixelCoord  = VK_FALSE;
    info.nonSeamless    = VK_FALSE;
    
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
    
    Rc<DxvkBuffer> buffer = m_device->createBuffer(info,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    std::memset(buffer->mapPtr(0), 0, info.size);
    return buffer;
  }
  
}