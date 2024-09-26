#pragma once

#include "dxvk_descriptor.h"
#include "dxvk_format.h"
#include "dxvk_memory.h"
#include "dxvk_resource.h"
#include "dxvk_sparse.h"
#include "dxvk_util.h"

namespace dxvk {

  /**
   * \brief Image create info
   * 
   * The properties of an image that are
   * passed to \ref DxvkDevice::createImage
   */
  struct DxvkImageCreateInfo {
    /// Image dimension
    VkImageType type = VK_IMAGE_TYPE_2D;
    
    /// Pixel format
    VkFormat format = VK_FORMAT_UNDEFINED;
    
    /// Flags
    VkImageCreateFlags flags = 0u;
    
    /// Sample count for MSAA
    VkSampleCountFlagBits sampleCount = VkSampleCountFlagBits(0u);
    
    /// Image size, in texels
    VkExtent3D extent = { };
    
    /// Number of image array layers
    uint32_t numLayers = 0u;
    
    /// Number of mip levels
    uint32_t mipLevels = 0u;
    
    /// Image usage flags
    VkImageUsageFlags usage = 0u;
    
    /// Pipeline stages that can access
    /// the contents of the image
    VkPipelineStageFlags stages = 0u;
    
    /// Allowed access pattern
    VkAccessFlags access = 0u;
    
    /// Image tiling mode
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    
    /// Common image layout
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Initial image layout
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Image is used by multiple contexts so it needs
    // to be in its default layout after each submission
    VkBool32 shared = VK_FALSE;

    // Image view formats that can
    // be used with this image
    uint32_t viewFormatCount = 0;
    const VkFormat* viewFormats = nullptr;

    // Shared handle info
    DxvkSharedHandleInfo sharing = { };
  };
  
  
  /**
   * \brief Image create info
   * 
   * The properties of an image view that are
   * passed to \ref DxvkDevice::createImageView
   */
  struct DxvkImageViewCreateInfo {
    /// Image view dimension
    VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D;
    
    /// Pixel format
    VkFormat format = VK_FORMAT_UNDEFINED;

    /// Image view usage flags
    VkImageUsageFlags usage = 0;
    
    /// Subresources to use in the view
    VkImageAspectFlags aspect = 0;
    
    uint32_t minLevel  = 0;
    uint32_t numLevels = 0;
    uint32_t minLayer  = 0;
    uint32_t numLayers = 0;
    
    /// Component mapping. Defaults to identity.
    VkComponentMapping swizzle = {
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
      VK_COMPONENT_SWIZZLE_IDENTITY,
    };
  };
  
  
  /**
   * \brief Virtual image view
   *
   * Stores views for a number of different view types
   * that the defined view is compatible with.
   */
  class DxvkImageView {
    constexpr static uint32_t ViewCount = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY + 1;
  public:

    DxvkImageView(
            DxvkImage*                image,
      const DxvkImageViewKey&         key);

    ~DxvkImageView();

    void incRef();
    void decRef();

    /**
     * \brief Image view handle for the default type
     *
     * The default view type is guaranteed to be
     * supported by the image view, and should be
     * preferred over picking a different type.
     * \returns Image view handle
     */
    VkImageView handle() {
      return handle(m_key.viewType);
    }

    /**
     * \brief Image view handle for a given view type
     *
     * If the view does not support the requested image
     * view type, \c VK_NULL_HANDLE will be returned.
     * \param [in] viewType The requested view type
     * \returns The image view handle
     */
    VkImageView handle(VkImageViewType viewType);

    /**
     * \brief Image view type
     *
     * Convenience method to query the view type
     * in order to check for resource compatibility.
     * \returns Image view type
     */
    VkImageViewType type() const {
      return m_key.viewType;
    }

    /**
     * \brief Image view properties
     * \returns Image view properties
     */
    DxvkImageViewCreateInfo info() const {
      DxvkImageViewCreateInfo info = { };
      info.type = m_key.viewType;
      info.format = m_key.format;
      info.usage = m_key.usage;
      info.aspect = m_key.aspects;
      info.minLevel = m_key.mipIndex;
      info.numLevels = m_key.mipCount;
      info.minLayer = m_key.layerIndex;
      info.numLayers = m_key.layerCount;
      info.swizzle.r = VkComponentSwizzle((m_key.packedSwizzle >>  0) & 0xf);
      info.swizzle.g = VkComponentSwizzle((m_key.packedSwizzle >>  4) & 0xf);
      info.swizzle.b = VkComponentSwizzle((m_key.packedSwizzle >>  8) & 0xf);
      info.swizzle.a = VkComponentSwizzle((m_key.packedSwizzle >> 12) & 0xf);
      return info;
    }

    /**
     * \brief Image object
     * \returns Image object
     */
    DxvkImage* image() const {
      return m_image;
    }

    /**
     * \brief View format info
     * \returns View format info
     */
    const DxvkFormatInfo* formatInfo() const {
      return lookupFormatInfo(m_key.format);
    }

    /**
     * \brief Mip level size
     *
     * Computes the mip level size relative to
     * the first mip level that the view includes.
     * \param [in] level Mip level
     * \returns Size of that level
     */
    VkExtent3D mipLevelExtent(uint32_t level) const;

    /**
     * \brief View subresource range
     *
     * Returns the subresource range from the image
     * description. For 2D views of 3D images, this
     * will return the viewed 3D slices.
     * \returns View subresource range
     */
    VkImageSubresourceRange subresources() const {
      VkImageSubresourceRange result;
      result.aspectMask     = m_key.aspects;
      result.baseMipLevel   = m_key.mipIndex;
      result.levelCount     = m_key.mipCount;
      result.baseArrayLayer = m_key.layerIndex;
      result.layerCount     = m_key.layerCount;
      return result;
    }

    /**
     * \brief Actual image subresource range
     *
     * Handles 3D images correctly in that it only
     * returns one single array layer. Use this for
     * barriers.
     * \returns Image subresource range
     */
    VkImageSubresourceRange imageSubresources() const;

    /**
     * \brief Picks an image layout
     * \see DxvkImage::pickLayout
     */
    VkImageLayout pickLayout(VkImageLayout layout) const;

    /**
     * \brief Retrieves descriptor info
     * 
     * \param [in] type Exact view type
     * \param [in] layout Image layout
     * \returns Image descriptor
     */
    DxvkDescriptorInfo getDescriptor(VkImageViewType type, VkImageLayout layout) {
      DxvkDescriptorInfo result;
      result.image.sampler = VK_NULL_HANDLE;
      result.image.imageView = handle(type);
      result.image.imageLayout = layout;
      return result;
    }

    /**
     * \brief Checks whether this view matches another
     *
     * \param [in] view The other view to check
     * \returns \c true if the two views have the same subresources
     */
    bool matchesView(const Rc<DxvkImageView>& view) const {
      if (this == view.ptr())
        return true;

      return this->image()        == view->image()
          && this->subresources() == view->subresources()
          && this->info().type    == view->info().type
          && this->info().format  == view->info().format;
    }

    /**
     * \brief Checks whether this view overlaps with another one
     *
     * Two views overlap if they were created for the same
     * image and have at least one subresource in common.
     * \param [in] view The other view to check
     * \returns \c true if the two views overlap
     */
    bool checkSubresourceOverlap(const Rc<DxvkImageView>& view) const {
      if (likely(m_image != view->m_image))
        return false;

      return vk::checkSubresourceRangeOverlap(
        this->imageSubresources(),
        view->imageSubresources());
    }

  private:

    DxvkImage*              m_image     = nullptr;
    DxvkImageViewKey        m_key       = { };

    uint32_t                m_version   = 0u;

    std::array<VkImageView, ViewCount> m_views = { };

    VkImageView createView(VkImageViewType type) const;

    void updateViews();

  };


  /**
   * \brief Virtual image resource
   * 
   * An image resource consisting of various subresources.
   * Can be accessed by the host if allocated on a suitable
   * memory type and if created with the linear tiling option.
   */
  class DxvkImage : public DxvkPagedResource {
    friend DxvkImageView;
    friend class DxvkContext;
  public:
    
    DxvkImage(
            DxvkDevice*           device,
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
            DxvkDevice*           device,
      const DxvkImageCreateInfo&  createInfo,
            VkImage               imageHandle,
            DxvkMemoryAllocator&  memAlloc,
            VkMemoryPropertyFlags memFlags);
    
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
      return m_imageInfo.image;
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
    
    /**
     * \brief Memory type flags
     * 
     * Use this to determine whether a
     * buffer is mapped to host memory.
     * \returns Vulkan memory flags
     */
    VkMemoryPropertyFlags memFlags() const {
      return m_properties;
    }
    
    /**
     * \brief Map pointer
     * 
     * If the image has been created on a host-visible
     * memory type, its memory is mapped and can be
     * accessed by the host.
     * \param [in] offset Byte offset into mapped region
     * \returns Pointer to mapped memory region
     */
    void* mapPtr(VkDeviceSize offset) const {
      return reinterpret_cast<char*>(m_imageInfo.mapPtr) + offset;
    }
    
    /**
     * \brief Image format info
     * \returns Image format info
     */
    const DxvkFormatInfo* formatInfo() const {
      return lookupFormatInfo(m_info.format);
    }
    
    /**
     * \brief Size of a mipmap level
     * 
     * \param [in] level Mip level
     * \returns Size of that level
     */
    VkExtent3D mipLevelExtent(uint32_t level) const {
      return util::computeMipLevelExtent(m_info.extent, level);
    }
    
    /**
     * \brief Size of a mipmap level
     * 
     * \param [in] level Mip level
     * \returns Size of that level
     */
    VkExtent3D mipLevelExtent(uint32_t level, VkImageAspectFlags aspect) const {
      return util::computeMipLevelExtent(m_info.extent, level, m_info.format, aspect);
    }
    
    /**
     * \brief Picks a compatible layout
     * 
     * Under some circumstances, we have to return
     * a different layout than the one requested.
     * \param [in] layout The image layout
     * \returns A compatible image layout
     */
    VkImageLayout pickLayout(VkImageLayout layout) const {
      if (unlikely(m_info.layout == VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT)) {
        if (layout != VK_IMAGE_LAYOUT_GENERAL
         && layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
         && layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
          return VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT;
      }

      return m_info.layout == VK_IMAGE_LAYOUT_GENERAL
        ? VK_IMAGE_LAYOUT_GENERAL : layout;
    }

    /**
     * \brief Changes image layout
     * \param [in] layout New layout
     */
    void setLayout(VkImageLayout layout) {
      m_info.layout = layout;
    }

    /**
     * \brief Checks whether a subresource is entirely covered
     * 
     * This can be used to determine whether an image can or
     * should be initialized with \c VK_IMAGE_LAYOUT_UNDEFINED.
     * \param [in] subresource The image subresource
     * \param [in] extent Image extent to check
     */
    bool isFullSubresource(
      const VkImageSubresourceLayers& subresource,
            VkExtent3D                extent) const {
      return subresource.aspectMask == this->formatInfo()->aspectMask
          && extent == this->mipLevelExtent(subresource.mipLevel);
    }

    /**
     * \brief Checks view format compatibility
     * 
     * If this returns true, a view with the given
     * format can be safely created for this image.
     * \param [in] format The format to check
     * \returns \c true if the format is vompatible
     */
    bool isViewCompatible(VkFormat format) const {
      bool result = m_info.format == format;
      for (uint32_t i = 0; i < m_viewFormats.size() && !result; i++)
        result |= m_viewFormats[i] == format;
      return result;
    }

    /**
     * \brief Memory object
     * \returns Backing memory
     */
    DxvkResourceMemoryInfo getMemoryInfo() const {
      return m_storage->getMemoryInfo();
    }

    /**
     * \brief Get full subresource range of the image
     * 
     * \returns Resource range of the whole image
     */
    VkImageSubresourceRange getAvailableSubresources() const {
      VkImageSubresourceRange result;
      result.aspectMask     = formatInfo()->aspectMask;
      result.baseMipLevel   = 0;
      result.levelCount     = info().mipLevels;
      result.baseArrayLayer = 0;
      result.layerCount     = info().numLayers;
      return result;
    }

    /**
     * \brief Queries memory layout of a subresource
     *
     * Can be used to retrieve the exact pointer to a
     * subresource of a mapped image with linear tiling.
     * \param [in] subresource The image subresource
     * \returns Memory layout of that subresource
     */
    VkSubresourceLayout querySubresourceLayout(
      const VkImageSubresource& subresource) const;

    /**
     * \brief Create a new shared handle to dedicated memory backing the image
     * \returns The shared handle with the type given by DxvkSharedHandleInfo::type
     */
    HANDLE sharedHandle() const;

    /**
     * \brief Retrives sparse page table
     * \returns Page table
     */
    DxvkSparsePageTable* getSparsePageTable();

    /**
     * \brief Creates image resource
     *
     * The returned image can be used as backing storage.
     * \returns New underlying image resource
     */
    Rc<DxvkResourceAllocation> createResource();

    /**
     * \brief Assigns backing storage to the image
     *
     * Implicitly invalidates all image views.
     * \param [in] resource New backing storage
     * \returns Previous backing storage
     */
    Rc<DxvkResourceAllocation> assignResource(Rc<DxvkResourceAllocation>&& resource) {
      Rc<DxvkResourceAllocation> old = std::move(m_storage);

      m_storage = std::move(resource);
      m_imageInfo = m_storage->getImageInfo();

      m_version += 1u;
      return old;
    }

    /**
     * \brief Creates or retrieves an image view
     *
     * \param [in] info Image view create info
     * \returns Newly created image view
     */
    Rc<DxvkImageView> createView(
      const DxvkImageViewCreateInfo& info);

  private:

    Rc<vk::DeviceFn>            m_vkd;
    DxvkMemoryAllocator*        m_allocator   = nullptr;
    VkMemoryPropertyFlags       m_properties  = 0u;

    DxvkImageCreateInfo         m_info        = { };

    uint32_t                    m_version     = 0u;
    VkBool32                    m_shared      = VK_FALSE;

    DxvkResourceImageInfo       m_imageInfo   = { };

    Rc<DxvkResourceAllocation>  m_storage     = nullptr;

    small_vector<VkFormat, 4>   m_viewFormats;

    dxvk::mutex                 m_viewMutex;
    std::unordered_map<DxvkImageViewKey,
      DxvkImageView, DxvkHash, DxvkEq> m_views;

    VkImageCreateInfo getImageCreateInfo() const;

    void copyFormatList(
            uint32_t              formatCount,
      const VkFormat*             formats);

    bool canShareImage(
            DxvkDevice*           device,
      const VkImageCreateInfo&    createInfo,
      const DxvkSharedHandleInfo& sharingInfo) const;

  };




  inline void DxvkImageView::incRef() {
    m_image->incRef();
  }


  inline void DxvkImageView::decRef() {
    m_image->decRef();
  }


  inline VkImageSubresourceRange DxvkImageView::imageSubresources() const {
    VkImageSubresourceRange result = { };
    result.aspectMask     = m_key.aspects;
    result.baseMipLevel   = m_key.mipIndex;
    result.levelCount     = m_key.mipCount;

    if (likely(m_image->info().type != VK_IMAGE_TYPE_3D)) {
      result.baseArrayLayer = m_key.layerIndex;
      result.layerCount     = m_key.layerCount;
    } else {
      result.baseArrayLayer = 0;
      result.layerCount     = 1;
    }

    return result;
  }


  inline VkExtent3D DxvkImageView::mipLevelExtent(uint32_t level) const {
    return m_image->mipLevelExtent(level + m_key.mipIndex, m_key.aspects);
  }


  inline VkImageLayout DxvkImageView::pickLayout(VkImageLayout layout) const {
    return m_image->pickLayout(layout);
  }


  inline VkImageView DxvkImageView::handle(VkImageViewType viewType) {
    viewType = viewType != VK_IMAGE_VIEW_TYPE_MAX_ENUM ? viewType : m_key.viewType;

    if (unlikely(m_version < m_image->m_version))
      updateViews();

    if (unlikely(!m_views[viewType]))
      m_views[viewType] = createView(viewType);

    return m_views[viewType];
  }

}
