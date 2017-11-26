#include "dxvk_image.h"

namespace dxvk {
  
//   DxvkImage::DxvkImage(
//     const Rc<vk::DeviceFn>&     vkd,
//     const DxvkImageCreateInfo&  info,
//           DxvkMemory&&          memory)
//   : m_vkd(vkd), m_info(info), m_memory(std::move(memory)) {
//     
//   }
  
  
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
    VkComponentMapping componentMapping;
    componentMapping.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    componentMapping.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    componentMapping.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    componentMapping.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    
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
    viewInfo.components       = componentMapping;
    viewInfo.subresourceRange = subresourceRange;
    
    if (m_vkd->vkCreateImageView(m_vkd->device(), &viewInfo, nullptr, &m_view) != VK_SUCCESS)
      throw DxvkError("DxvkImageView::DxvkImageView: Failed to create image view");
  }
  
  
  DxvkImageView::~DxvkImageView() {
    m_vkd->vkDestroyImageView(
      m_vkd->device(), m_view, nullptr);
  }
  
}