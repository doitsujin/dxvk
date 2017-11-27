#pragma once

#include "dxvk_memory.h"
#include "dxvk_resource.h"

namespace dxvk {
  
  struct DxvkImageCreateInfo {
    VkImageType type;
    VkFormat format;
    VkSampleCountFlagBits sampleCount;
    VkExtent3D extent;
    uint32_t numLayers;
    uint32_t mipLevels;
    VkImageUsageFlags usage;
    VkPipelineStageFlags stages;
    VkAccessFlags access;
    VkImageTiling tiling;
  };
  
  struct DxvkImageViewCreateInfo {
    VkImageViewType type;
    VkFormat format;
    VkImageAspectFlags aspect;
    uint32_t minLevel;
    uint32_t numLevels;
    uint32_t minLayer;
    uint32_t numLayers;
  };
  
  
  /**
   * \brief DXVK image
   * 
   * An image resource consisting of various subresources.
   * Cannot be mapped to host memory, the only way to access
   * image data is through buffer transfer operations.
   */
  class DxvkImage : public DxvkResource {
    
  public:
    
    DxvkImage(
      const Rc<vk::DeviceFn>&     vkd,
      const DxvkImageCreateInfo&  createInfo,
            DxvkMemoryAllocator&  memAlloc,
            VkMemoryPropertyFlags memFlags);
    
    /**
     * \brief Creates image object from existing image
     * 
     * This can be used to create an image object for
     * an implementation-managed image. Make sure to
     * provide the correct image properties, since
     * otherwise some image operations may fail.
     */
    DxvkImage(
      const Rc<vk::DeviceFn>&     vkd,
      const DxvkImageCreateInfo&  info,
            VkImage               image);
    
    /**
     * \brief Destroys image
     * 
     * If this is an implementation-managed image,
     * this will not destroy the Vulkan image.
     */
    ~DxvkImage();
    
    /**
     * \brief Image handle
     * 
     * Internal use only.
     * \returns Image handle
     */
    VkImage handle() const {
      return m_image;
    }
    
    /**
     * \brief Image properties
     * 
     * The image create info structure.
     * \returns Image properties
     */
    const DxvkImageCreateInfo& info() const {
      return m_info;
    }
    
  private:
    
    Rc<vk::DeviceFn>    m_vkd;
    DxvkImageCreateInfo m_info;
    DxvkMemory          m_memory;
    VkImage             m_image = VK_NULL_HANDLE;
    
  };
  
  
  /**
   * \brief DXVK image view
   */
  class DxvkImageView : public DxvkResource {
    
  public:
    
    DxvkImageView(
      const Rc<vk::DeviceFn>&         vkd,
      const Rc<DxvkImage>&            image,
      const DxvkImageViewCreateInfo&  info);
    
    ~DxvkImageView();
    
    /**
     * \brief Image view handle
     * 
     * Internal use only.
     * \returns Image view handle
     */
    VkImageView handle() const {
      return m_view;
    }
    
    /**
     * \brief Image view properties
     * \returns Image view properties
     */
    const DxvkImageViewCreateInfo& info() const {
      return m_info;
    }
    
    /**
     * \brief Image
     * \returns Image
     */
    Rc<DxvkImage> image() const {
      return m_image;
    }
    
  private:
    
    Rc<vk::DeviceFn>  m_vkd;
    Rc<DxvkImage>     m_image;
    
    DxvkImageViewCreateInfo m_info;
    VkImageView             m_view;
    
  };
  
}