#pragma once

#include "dxvk_descriptor_pool.h"
#include "dxvk_fence.h"
#include "dxvk_format.h"
#include "dxvk_memory.h"
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

    // Color space to interpret image data with. This
    // is only meaningful for swap chain back buffers.
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_MAX_ENUM_KHR;

    // Image is used by multiple contexts so it needs
    // to be in its default layout after each submission
    VkBool32 shared = VK_FALSE;

    // Image is likely to have a short lifetime
    VkBool32 transient = VK_FALSE;

    // Image view formats that can
    // be used with this image
    uint32_t viewFormatCount = 0;
    const VkFormat* viewFormats = nullptr;

    // Shared handle info
    DxvkSharedHandleInfo sharing = { };

    // Debug name
    const char* debugName = nullptr;
  };
  
  
  /**
   * \brief Extra image usage info
   *
   * Useful when recreating an image with different usage flags.
   */
  struct DxvkImageUsageInfo {
    // New image flags to add.
    VkImageCreateFlags flags = 0u;
    // Usage flags to add to the image
    VkImageUsageFlags usage = 0u;
    // Stage flags to add to the image
    VkPipelineStageFlags stages = 0u;
    // Access flags to add to the image
    VkAccessFlags access = 0u;
    // New image layout. If undefined, the
    // default layout will not be changed.
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    // Color space to interpret the image in
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_MAX_ENUM_KHR;
    // Number of new view formats to add
    uint32_t viewFormatCount = 0u;
    // View formats to add to the compatibility list
    const VkFormat* viewFormats = nullptr;
    // Requtes the image to not be relocated in the future
    VkBool32 stableGpuAddress = VK_FALSE;
  };


  /**
   * \brief Image properties stored in the view
   *
   * Used to reduce some pointer chasing.
   */
  struct DxvkImageViewImageProperties {
    VkImageLayout         layout  = VK_IMAGE_LAYOUT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
    VkAccessFlags         access  = 0u;
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
     * \brief Image view descriptor for the default type
     *
     * The default view type is guaranteed to be
     * supported by the image view, and should be
     * preferred over picking a different type.
     * \returns Image view handle
     */
    const DxvkDescriptor* getDescriptor() {
      return getDescriptor(m_key.viewType);
    }

    /**
     * \brief Image view handle for a given view type
     *
     * If the view does not support the requested image
     * view type, \c VK_NULL_HANDLE will be returned.
     * \param [in] viewType The requested view type
     * \returns The image view handle
     */
    const DxvkDescriptor* getDescriptor(VkImageViewType viewType);

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
    VkImageView handle(VkImageViewType viewType) {
      auto descriptor = getDescriptor(viewType);
      return likely(descriptor) ? descriptor->legacy.image.imageView : VK_NULL_HANDLE;
    }

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
    DxvkImageViewKey info() const {
      return m_key;
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
     * \brief Checks whether this view matches another
     *
     * \param [in] view The other view to check
     * \returns \c true if the two views have the same subresources
     */
    bool matchesView(const Rc<DxvkImageView>& view) const {
      if (this == view.ptr())
        return true;

      return this->image()         == view->image()
          && this->subresources()  == view->subresources()
          && this->info().viewType == view->info().viewType
          && this->info().format   == view->info().format;
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

    /**
     * \brief Queries the view layout
     *
     * If no layout was explicitly specified for the view, this
     * will return a suitable layout for the given usage.
     * \returns Image view layout
     */
    VkImageLayout getLayout() const;

    /**
     * \brief Checks whether the image is multisampled
     * \returns \c true if the image is multisampled
     */
    bool isMultisampled() const {
      return m_properties.samples > VK_SAMPLE_COUNT_1_BIT;
    }

    /**
     * \brief Checks whether the image has graphics stores
     *
     * This may include attachment access for render passes.
     * \returns \c true if the image has graphics pipeline stores
     */
    bool hasGfxStores() const;

  private:

    DxvkImage*              m_image     = nullptr;
    DxvkImageViewKey        m_key       = { };

    uint32_t                m_version   = 0u;

    DxvkImageViewImageProperties m_properties = { };

    std::array<const DxvkDescriptor*, ViewCount> m_views = { };

    const DxvkDescriptor* createView(VkImageViewType type) const;

    void updateViews();

    void updateProperties();

  };

  class DxvkKeyedMutex : public RcObject {
  public:

    /**
     * \brief Creates a new shared keyed mutex
     */
    DxvkKeyedMutex(
      const Rc<DxvkDevice>& device,
            uint64_t        initialValue,
            bool            ntShared);

    /**
     * \brief Opens a shared keyed mutex from its D3DKMT handles
     */
    DxvkKeyedMutex(
      const Rc<DxvkDevice>& device,
            Rc<DxvkFence>&& fence,
            D3DKMT_HANDLE   kmtLocal,
            D3DKMT_HANDLE   kmtGlobal);
    
    ~DxvkKeyedMutex();
    
    /**
     * \brief D3DKMT keyed mutex local handle
     * \returns The keyed mutex D3DKMT local handle
     * \returns \c 0 if fence is not shared
     */
    D3DKMT_HANDLE kmtLocal() const {
      return m_kmtLocal;
    }

    /**
     * \brief D3DKMT keyed mutex global handle
     * \returns The keyed mutex D3DKMT global handle
     * \returns \c 0 if keyed mutex is not shared or shared with NT handle
     */
    D3DKMT_HANDLE kmtGlobal() const {
      return m_kmtGlobal;
    }

    /**
     * \brief Retrieves current sync object
     * \returns sync object guarding this image
     */
    Rc<DxvkFence> getSyncObject() const {
      return m_fence;
    }

    /**
     * \brief Try to acquire the keyed mutex
     * \returns DXGI_ERROR_INVALID_CALL if owned already or on error
     * \returns WAIT_TIMEOUT on timeout
     * \returns S_OK on success
     */
    HRESULT AcquireSync(UINT64 key, DWORD milliseconds);

    /**
     * \brief Release the keyed mutex
     * \returns DXGI_ERROR_INVALID_CALL if not owned or on error
     * \returns S_OK on success
     */
    HRESULT ReleaseSync(UINT64 key);

  private:

    Rc<vk::DeviceFn>            m_vkd;
    Rc<DxvkFence>               m_fence;
    D3DKMT_HANDLE               m_kmtLocal  = 0;
    D3DKMT_HANDLE               m_kmtGlobal = 0;

    uint64_t                    m_fenceValue = 0;
    std::atomic<bool>           m_owned = { false };

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
            DxvkMemoryAllocator&  allocator,
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
            DxvkMemoryAllocator&  allocator,
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
     * \brief Queries shader stages that can access this image
     *
     * Derived from the pipeline stage mask passed in during creation.
     * \returns Shader stages that may access this image
     */
    VkShaderStageFlags getShaderStages() const {
      return m_shaderStages;
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
     * \brief Checks whether the image can be relocated
     *
     * Images that are shared, imported from a different API
     * or mapped to host address space cannot be relocated.
     * \returns \c true if the image can be relocated
     */
    bool canRelocate() const;

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
     * \brief Allocates new backing storage with constraints
     *
     * \param [in] mode Allocation mode flags
     * \returns Operation status and allocation
     */
    Rc<DxvkResourceAllocation> relocateStorage(
            DxvkAllocationModes         mode);

    /**
     * \brief Creates image resource
     *
     * The returned image can be used as backing storage.
     * \returns New underlying image resource
     */
    Rc<DxvkResourceAllocation> allocateStorage();

    /**
     * \brief Creates image resource with extra usage
     *
     * Creates new backing storage with additional usage flags
     * enabled. Useful to expand on usage flags after creation.
     * \param [in] usage Usage flags to add
     * \param [in] mode Allocation constraints
     * \returns New underlying image resource
     */
    Rc<DxvkResourceAllocation> allocateStorageWithUsage(
      const DxvkImageUsageInfo&         usage,
            DxvkAllocationModes         mode);

    /**
     * \brief Assigns backing storage to the image
     *
     * Implicitly invalidates all image views.
     * \param [in] resource New backing storage
     * \returns Previous backing storage
     */
    Rc<DxvkResourceAllocation> assignStorage(
            Rc<DxvkResourceAllocation>&& resource);

    /**
     * \brief Assigns backing storage to the image with extra usage
     *
     * Implicitly invalidates all image views.
     * \param [in] resource New backing storage
     * \param [in] usageInfo Added usage info
     * \returns Previous backing storage
     */
    Rc<DxvkResourceAllocation> assignStorageWithUsage(
            Rc<DxvkResourceAllocation>&& resource,
      const DxvkImageUsageInfo&         usageInfo);

    /**
     * \brief Retrieves current backing storage
     * \returns Backing storage for this image
     */
    Rc<DxvkResourceAllocation> storage() const {
      return m_storage;
    }

    /**
     * \brief Retrieves current keyed mutex
     * \returns Keyed mutex guarding this image
     */
    Rc<DxvkKeyedMutex> getKeyedMutex() const {
      return m_mutex;
    }

    /**
     * \brief Sets the image keyed mutex and sync object
     */
    void setKeyedMutex(Rc<DxvkKeyedMutex>&& mutex) {
      m_mutex = mutex;
    }

    /**
     * \brief Retrieves resource ID for barrier tracking
     * \returns Unique resource ID
     */
    bit::uint48_t getResourceId() const {
      constexpr static size_t Align = alignof(DxvkResourceAllocation);
      return bit::uint48_t(reinterpret_cast<uintptr_t>(m_storage.ptr()) / (Align & -Align));
    }

    /**
     * \brief Computes virtual offset of a subresource
     *
     * Used for hazard tracking. Ignores the aspect mask and
     * only takes the mip level and array layer into account.
     * \param [in] mip Mip level index
     * \param [in] layer Array layer index
     */
    uint64_t getTrackingAddress(uint32_t mip, uint32_t layer) const {
      // Put layers within the same mip into a contiguous range. This works well
      // for not only transfer operations but also most image view use cases.
      return uint64_t((m_info.numLayers * mip) + layer) << 48u;
    }

    /**
     * \brief Computes virtual offset of a specific image region
     *
     * Used for more granular hazard tracking. This interleaves coordinate
     * bits in order to compute a unique address for each pixel.
     * \param [in] mip Mip level index
     * \param [in] layer Array layer index
     * \param [in] coord Pixel coordinate within the subresource
     */
    uint64_t getTrackingAddress(uint32_t mip, uint32_t layer, VkOffset3D coord) const;

    /**
     * \brief Creates or retrieves an image view
     *
     * \param [in] info Image view create info
     * \returns Newly created image view
     */
    Rc<DxvkImageView> createView(
      const DxvkImageViewKey& info);

    /**
     * \brief Checks whether an image subresource is initialized
     *
     * \param [in] subresource The subresource to query
     * \returns \c true if the given subresource is initialized
     */
    bool isInitialized(
      const VkImageSubresource& subresource) const;

    /**
     * \brief Checks whether subresources are initialized
     *
     * \param [in] subresources Subresource range
     * \returns \c true if the subresources are initialized
     */
    bool isInitialized(
      const VkImageSubresourceRange& subresources) const;

    /**
     * \brief Queries current layout of an image subresource
     *
     * \param [in] subresource The subresource. Note that the aspect
     *    mask must not have multiple planes set for planar images.
     * \returns Current layout of the given image subresource
     */
    VkImageLayout queryLayout(const VkImageSubresource& subresource) const {
      if (m_globalLayout != VK_IMAGE_LAYOUT_MAX_ENUM)
        return m_globalLayout;

      uint32_t index = computeSubresourceIndex(subresource);
      return m_localLayouts[index];
    }

    /**
     * \biref Queries current layout of a subresource range
     *
     * If layouts diverge, this returns \c VK_IMAGE_LAYOUT_MAX_ENUM,
     * and individual subresources must be queried manually.
     * \param [in] subresources Subresource range to query
     * \returns Current layout of the subresource range
     */
    VkImageLayout queryLayout(const VkImageSubresourceRange& subresources) const;

    /**
     * \brief Updates per-subresource layout tracking
     *
     * \param [in] subresources Subresource range to transition
     * \param [in] layout New layout for the subresource range
     */
    void trackLayout(const VkImageSubresourceRange& subresources, VkImageLayout layout);

    /**
     * \brief Sets debug name for the backing resource
     * \param [in] name New debug name
     */
    void setDebugName(const char* name);

    /**
     * \brief Retrieves debug name
     * \returns Debug name
     */
    const char* getDebugName() const {
      return m_debugName.c_str();
    }

  private:

    Rc<vk::DeviceFn>            m_vkd;
    VkMemoryPropertyFlags       m_properties  = 0u;
    VkShaderStageFlags          m_shaderStages = 0u;

    DxvkImageCreateInfo         m_info        = { };

    uint32_t                    m_version     = 0u;
    VkBool32                    m_shared      = VK_FALSE;
    VkBool32                    m_stableAddress = VK_FALSE;

    Rc<DxvkKeyedMutex>          m_mutex       = nullptr;

    DxvkResourceImageInfo       m_imageInfo   = { };

    Rc<DxvkResourceAllocation>  m_storage     = nullptr;

    small_vector<VkFormat, 4>   m_viewFormats;

    VkImageLayout               m_globalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    small_vector<VkImageLayout, 16> m_localLayouts;

    dxvk::mutex                 m_viewMutex;
    std::unordered_map<DxvkImageViewKey,
      DxvkImageView, DxvkHash, DxvkEq> m_views;

    std::string                 m_debugName;

    void updateDebugName();

    std::string createDebugName(const char* name) const;

    VkImageCreateInfo getImageCreateInfo(
      const DxvkImageUsageInfo&         usageInfo) const;

    void copyFormatList(
            uint32_t              formatCount,
      const VkFormat*             formats);

    bool canShareImage(
            DxvkDevice*           device,
      const VkImageCreateInfo&    createInfo,
      const DxvkSharedHandleInfo& sharingInfo) const;

    uint32_t computeSubresourceIndex(const VkImageSubresource& subresource) const {
      return subresource.arrayLayer
        + m_info.numLayers * (subresource.mipLevel
        + m_info.mipLevels * vk::getPlaneIndex(subresource.aspectMask));
    }

    uint32_t computeSubresourceCount() const {
      return m_info.numLayers * m_info.mipLevels * vk::getPlaneCount(formatInfo()->aspectMask);
    }

  };


  /**
   * \brief Image relocation info
   */
  struct DxvkRelocateImageInfo {
    /// Buffer object. Stores metadata.
    Rc<DxvkImage> image;
    /// Backing storage to copy to
    Rc<DxvkResourceAllocation> storage;
    /// Additional image usage
    DxvkImageUsageInfo usageInfo;
  };




  force_inline void DxvkImageView::incRef() {
    m_image->incRef();
  }


  force_inline void DxvkImageView::decRef() {
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


  inline const DxvkDescriptor* DxvkImageView::getDescriptor(VkImageViewType viewType) {
    viewType = viewType != VK_IMAGE_VIEW_TYPE_MAX_ENUM ? viewType : m_key.viewType;

    if (unlikely(m_version < m_image->m_version))
      updateViews();

    if (unlikely(!m_views[viewType]))
      m_views[viewType] = createView(viewType);

    return m_views[viewType];
  }


  inline VkImageLayout DxvkImageView::getLayout() const {
    return m_image->pickLayout(m_key.layout);
  }


  inline bool DxvkImageView::hasGfxStores() const {
    return (m_properties.access & (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT))
        && (m_image->hasGfxStores());
  }

}
