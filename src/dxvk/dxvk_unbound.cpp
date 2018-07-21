#include "dxvk_device.h"

namespace dxvk {
  
  DxvkUnboundResources::DxvkUnboundResources(DxvkDevice* dev)
  : m_sampler       (createSampler(dev)),
    m_buffer        (createBuffer(dev)),
    m_bufferView    (createBufferView(dev, m_buffer)),
    m_image1D       (createImage(dev, VK_IMAGE_TYPE_1D, 1)),
    m_image2D       (createImage(dev, VK_IMAGE_TYPE_2D, 6)),
    m_image3D       (createImage(dev, VK_IMAGE_TYPE_3D, 1)),
    m_view1D        (createImageView(dev, m_image1D, VK_IMAGE_VIEW_TYPE_1D,         1)),
    m_view1DArr     (createImageView(dev, m_image1D, VK_IMAGE_VIEW_TYPE_1D_ARRAY,   1)),
    m_view2D        (createImageView(dev, m_image2D, VK_IMAGE_VIEW_TYPE_2D,         1)),
    m_view2DArr     (createImageView(dev, m_image2D, VK_IMAGE_VIEW_TYPE_2D_ARRAY,   1)),
    m_viewCube      (createImageView(dev, m_image2D, VK_IMAGE_VIEW_TYPE_CUBE,       6)),
    m_viewCubeArr   (createImageView(dev, m_image2D, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY, 6)),
    m_view3D        (createImageView(dev, m_image3D, VK_IMAGE_VIEW_TYPE_3D,         1)) {
    
  }
  
  
  DxvkUnboundResources::~DxvkUnboundResources() {
    
  }
  
  
  void DxvkUnboundResources::clearResources(DxvkDevice* dev) {
    const Rc<DxvkContext> ctx = dev->createContext();
    ctx->beginRecording(dev->createCommandList());
    
    this->clearBuffer(ctx, m_buffer);
    this->clearImage(ctx, m_image1D);
    this->clearImage(ctx, m_image2D);
    this->clearImage(ctx, m_image3D);
    
    dev->submitCommandList(
      ctx->endRecording(),
      nullptr, nullptr);
  }
  
  
  Rc<DxvkSampler> DxvkUnboundResources::createSampler(DxvkDevice* dev) {
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
    info.borderColor    = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    info.usePixelCoord  = VK_FALSE;
    
    return dev->createSampler(info);
  }
  
  
  Rc<DxvkBuffer> DxvkUnboundResources::createBuffer(DxvkDevice* dev) {
    DxvkBufferCreateInfo info;
    info.size       = MaxUniformBufferSize;
    info.usage      = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                    | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                    | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                    | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                    | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                    | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
                    | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    info.stages     = VK_PIPELINE_STAGE_TRANSFER_BIT
                    | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                    | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
                    | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT
                    | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
                    | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                    | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    info.access     = VK_ACCESS_UNIFORM_READ_BIT
                    | VK_ACCESS_SHADER_READ_BIT
                    | VK_ACCESS_SHADER_WRITE_BIT;
    
    return dev->createBuffer(info,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }
  
  
  Rc<DxvkBufferView> DxvkUnboundResources::createBufferView(
          DxvkDevice*     dev,
    const Rc<DxvkBuffer>& buffer) {
    DxvkBufferViewCreateInfo info;
    info.format      = VK_FORMAT_R32_UINT;
    info.rangeOffset = 0;
    info.rangeLength = buffer->info().size;
    
    return dev->createBufferView(buffer, info);
  }
  
  
  Rc<DxvkImage> DxvkUnboundResources::createImage(
          DxvkDevice*     dev,
          VkImageType     type,
          uint32_t        layers) {
    DxvkImageCreateInfo info;
    info.type        = type;
    info.format      = VK_FORMAT_R32_UINT;
    info.flags       = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    info.sampleCount = VK_SAMPLE_COUNT_1_BIT;
    info.extent      = { 1, 1, 1 };
    info.numLayers   = layers;
    info.mipLevels   = 1;
    info.usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                     | VK_IMAGE_USAGE_SAMPLED_BIT
                     | VK_IMAGE_USAGE_STORAGE_BIT;
    info.stages      = VK_PIPELINE_STAGE_TRANSFER_BIT
                     | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                     | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
                     | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT
                     | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
                     | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                     | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    info.access      = VK_ACCESS_SHADER_READ_BIT;
    info.layout      = VK_IMAGE_LAYOUT_GENERAL;
    info.tiling      = VK_IMAGE_TILING_OPTIMAL;
    
    if (type == VK_IMAGE_TYPE_2D)
      info.flags       |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    
    return dev->createImage(info,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }
  
  
  Rc<DxvkImageView> DxvkUnboundResources::createImageView(
          DxvkDevice*     dev,
    const Rc<DxvkImage>&  image,
          VkImageViewType type,
          uint32_t        layers) {
    DxvkImageViewCreateInfo info;
    info.type         = type;
    info.format       = image->info().format;
    info.usage        = VK_IMAGE_USAGE_SAMPLED_BIT
                      | VK_IMAGE_USAGE_STORAGE_BIT;
    info.aspect       = VK_IMAGE_ASPECT_COLOR_BIT;
    info.minLevel     = 0;
    info.numLevels    = 1;
    info.minLayer     = 0;
    info.numLayers    = layers;
    info.swizzle      = VkComponentMapping {
      VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO,
      VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO };
    
    return dev->createImageView(image, info);
  }
  
  
  const DxvkImageView* DxvkUnboundResources::getImageView(VkImageViewType type) const {
    switch (type) {
      case VK_IMAGE_VIEW_TYPE_1D:         return m_view1D.ptr();
      case VK_IMAGE_VIEW_TYPE_1D_ARRAY:   return m_view1DArr.ptr();
      case VK_IMAGE_VIEW_TYPE_2D:         return m_view2D.ptr();
      case VK_IMAGE_VIEW_TYPE_2D_ARRAY:   return m_view2DArr.ptr();
      case VK_IMAGE_VIEW_TYPE_CUBE:       return m_viewCube.ptr();
      case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY: return m_viewCubeArr.ptr();
      case VK_IMAGE_VIEW_TYPE_3D:         return m_view3D.ptr();
      default:                            Logger::err("null"); return nullptr;
    }
  }
  
  
  void DxvkUnboundResources::clearBuffer(
    const Rc<DxvkContext>&  ctx,
    const Rc<DxvkBuffer>&   buffer) {
    ctx->clearBuffer(buffer, 0, buffer->info().size, 0);
  }
  
  
  void DxvkUnboundResources::clearImage(
    const Rc<DxvkContext>&  ctx,
    const Rc<DxvkImage>&    image) {
    ctx->clearColorImage(image,
      VkClearColorValue { },
      VkImageSubresourceRange {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, image->info().mipLevels,
        0, image->info().numLayers });
  }
  
}