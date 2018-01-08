#include "d3d11_dummy_resource.h"

namespace dxvk {
  
  D3D11DummyResources::D3D11DummyResources(
    const Rc<DxvkDevice>&       device,
          VkPipelineStageFlags  enabledShaderStages) {
    // Create a sampler to use with dummy textures. Parameters
    // are the same as the default D3D11 sampling parameters.
    DxvkSamplerCreateInfo samplerInfo;
    samplerInfo.magFilter       = VK_FILTER_LINEAR;
    samplerInfo.minFilter       = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode      = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipmapLodBias   = 0.0f;
    samplerInfo.mipmapLodMin    = 0.0f;
    samplerInfo.mipmapLodMax    = 256.0f;
    samplerInfo.useAnisotropy   = VK_FALSE;
    samplerInfo.maxAnisotropy   = 1.0f;
    samplerInfo.addressModeU    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW    = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.compareToDepth  = VK_FALSE;
    samplerInfo.compareOp       = VK_COMPARE_OP_NEVER;
    samplerInfo.borderColor     = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.usePixelCoord   = VK_FALSE;
    
    this->sampler = device->createSampler(samplerInfo);
    
    // Create a dummy buffer. We'll use this for both texel buffers
    // and uniform buffers. The contents will be initialized to zero.
    DxvkBufferCreateInfo bufferInfo;
    bufferInfo.size             = 0x10000;  // Max constant buffer size
    bufferInfo.usage            = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                                | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
                                | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.stages           = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
                                | VK_PIPELINE_STAGE_TRANSFER_BIT
                                | enabledShaderStages;
    bufferInfo.access           = VK_ACCESS_TRANSFER_WRITE_BIT
                                | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
                                | VK_ACCESS_UNIFORM_READ_BIT;
    
    this->buffer = device->createBuffer(bufferInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    // Create buffer view to use for texel buffer bindings.
    DxvkBufferViewCreateInfo bufferViewInfo;
    bufferViewInfo.format       = VK_FORMAT_R8G8B8A8_UNORM;
    bufferViewInfo.rangeOffset  = 0;
    bufferViewInfo.rangeLength  = bufferInfo.size;
    
    this->bufferView = device->createBufferView(this->buffer, bufferViewInfo);
    
    // TODO images and image views
    // TODO initialize resources
  }
  
  
  D3D11DummyResources::~D3D11DummyResources() {
    
  }
  
}