#include "dxvk_image.h"

namespace dxvk {
  
  DxvkImage::DxvkImage(
    const Rc<vk::DeviceFn>&     vkd,
    const DxvkImageCreateInfo&  createInfo,
          DxvkMemoryAllocator&  memAlloc,
          VkMemoryPropertyFlags memFlags)
  : m_vkd(vkd), m_info(createInfo), m_memFlags(memFlags) {
    
    VkImageCreateInfo info;
    info.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext                 = nullptr;
    info.flags                 = createInfo.flags;
    info.imageType             = createInfo.type;
    info.format                = createInfo.format;
    info.extent                = createInfo.extent;
    info.mipLevels             = createInfo.mipLevels;
    info.arrayLayers           = createInfo.numLayers;
    info.samples               = createInfo.sampleCount;
    info.tiling                = createInfo.tiling;
    info.usage                 = createInfo.usage;
    info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices   = nullptr;
    info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    
    if (m_vkd->vkCreateImage(m_vkd->device(),
          &info, nullptr, &m_image) != VK_SUCCESS)
      throw DxvkError("DxvkImage::DxvkImage: Failed to create image");
    
    VkMemoryRequirements memReq;
    m_vkd->vkGetImageMemoryRequirements(
      m_vkd->device(), m_image, &memReq);
    
    m_memory = memAlloc.alloc(memReq, memFlags);
    
    if (m_vkd->vkBindImageMemory(m_vkd->device(),
          m_image, m_memory.memory(), m_memory.offset()) != VK_SUCCESS)
      throw DxvkError("DxvkImage::DxvkImage: Failed to bind device memory");
  }
  
  
  DxvkImage::DxvkImage(
    const Rc<vk::DeviceFn>&     vkd,
    const DxvkImageCreateInfo&  info,
          VkImage               image)
  : m_vkd(vkd), m_info(info), m_image(image) {
    
  }
  
  
  DxvkImage::~DxvkImage() {
    // This is a bit of a hack to determine whether
    // the image is implementation-handled or not
    if (m_memory.memory() != VK_NULL_HANDLE)
      m_vkd->vkDestroyImage(m_vkd->device(), m_image, nullptr);
  }
  
  
  DxvkImageView::DxvkImageView(
    const Rc<vk::DeviceFn>&         vkd,
    const Rc<DxvkImage>&            image,
    const DxvkImageViewCreateInfo&  info)
  : m_vkd(vkd), m_image(image), m_info(info) {
    VkImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask     = info.aspect;
    subresourceRange.baseMipLevel   = info.minLevel;
    subresourceRange.levelCount     = info.numLevels;
    subresourceRange.baseArrayLayer = info.minLayer;
    subresourceRange.layerCount     = info.numLayers;
    
    VkImageViewCreateInfo viewInfo;
    viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.pNext            = nullptr;
    viewInfo.flags            = 0;
    viewInfo.image            = image->handle();
    viewInfo.viewType         = info.type;
    viewInfo.format           = info.format;
    viewInfo.components       = info.swizzle;
    viewInfo.subresourceRange = subresourceRange;
    
    if (m_vkd->vkCreateImageView(m_vkd->device(), &viewInfo, nullptr, &m_view) != VK_SUCCESS)
      throw DxvkError("DxvkImageView::DxvkImageView: Failed to create image view");
  }
  
  
  DxvkImageView::~DxvkImageView() {
    m_vkd->vkDestroyImageView(
      m_vkd->device(), m_view, nullptr);
  }
  
}