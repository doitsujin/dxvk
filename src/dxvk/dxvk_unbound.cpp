#include "dxvk_device.h"

namespace dxvk {
  
  DxvkUnboundResources::DxvkUnboundResources(DxvkDevice* dev)
  : m_sampler       (createSampler(dev)),
    m_buffer        (createBuffer(dev)),
    m_bufferView    (createBufferView(dev, m_buffer)),
    m_image1D       (createImage(dev, VK_IMAGE_TYPE_1D, 1)),
    m_image2D       (createImage(dev, VK_IMAGE_TYPE_2D, 6)),
    m_image3D       (createImage(dev, VK_IMAGE_TYPE_3D, 1)),
    m_viewsSampled  (createImageViews(dev, VK_FORMAT_R32_SFLOAT)),
    m_viewsStorage  (createImageViews(dev, VK_FORMAT_R32_UINT)) {
    
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
      VK_NULL_HANDLE,
      VK_NULL_HANDLE);
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
    info.borderColor    = VkClearColorValue();
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
                    | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
                    | VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT;
    info.stages     = VK_PIPELINE_STAGE_TRANSFER_BIT
                    | dev->getShaderPipelineStages();
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
                     | dev->getShaderPipelineStages();
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
          VkFormat        format,
          VkImageViewType type,
          uint32_t        layers) {
    DxvkImageViewCreateInfo info;
    info.type         = type;
    info.format       = format;
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
  
  
  DxvkUnboundResources::UnboundViews DxvkUnboundResources::createImageViews(DxvkDevice* dev, VkFormat format) {
    UnboundViews result;
    result.view1D      = createImageView(dev, m_image1D, format, VK_IMAGE_VIEW_TYPE_1D,         1);
    result.view1DArr   = createImageView(dev, m_image1D, format, VK_IMAGE_VIEW_TYPE_1D_ARRAY,   1);
    result.view2D      = createImageView(dev, m_image2D, format, VK_IMAGE_VIEW_TYPE_2D,         1);
    result.view2DArr   = createImageView(dev, m_image2D, format, VK_IMAGE_VIEW_TYPE_2D_ARRAY,   1);
    result.viewCube    = createImageView(dev, m_image2D, format, VK_IMAGE_VIEW_TYPE_CUBE,       6);
    result.viewCubeArr = createImageView(dev, m_image2D, format, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY, 6);
    result.view3D      = createImageView(dev, m_image3D, format, VK_IMAGE_VIEW_TYPE_3D,         1);
    return result;
  }


  const DxvkImageView* DxvkUnboundResources::getImageView(VkImageViewType type, bool sampled) const {
    auto views = sampled ? &m_viewsSampled : &m_viewsStorage;

    switch (type) {
      case VK_IMAGE_VIEW_TYPE_1D:         return views->view1D.ptr();
      case VK_IMAGE_VIEW_TYPE_1D_ARRAY:   return views->view1DArr.ptr();
      // When implicit samplers are unbound -- we assume 2D in the shader.
      case VK_IMAGE_VIEW_TYPE_MAX_ENUM:
      case VK_IMAGE_VIEW_TYPE_2D:         return views->view2D.ptr();
      case VK_IMAGE_VIEW_TYPE_2D_ARRAY:   return views->view2DArr.ptr();
      case VK_IMAGE_VIEW_TYPE_CUBE:       return views->viewCube.ptr();
      case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY: return views->viewCubeArr.ptr();
      case VK_IMAGE_VIEW_TYPE_3D:         return views->view3D.ptr();
      default:                            return nullptr;
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