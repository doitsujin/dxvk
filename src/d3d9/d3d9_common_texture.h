#pragma once

#include "d3d9_device.h"
#include "d3d9_format.h"

#include "../dxvk/dxvk_device.h"

namespace dxvk {

  /**
   * \brief Image memory mapping mode
   * 
   * Determines how exactly \c LockBox will
   * behave when mapping an image.
   */
  enum D3D9_COMMON_TEXTURE_MAP_MODE {
    D3D9_COMMON_TEXTURE_MAP_MODE_NONE,      ///< No mapping available
    D3D9_COMMON_TEXTURE_MAP_MODE_BACKED,    ///< Mapped image through buffer
    D3D9_COMMON_TEXTURE_MAP_MODE_SYSTEMMEM, ///< Only a buffer - no image
  };
  
  /**
   * \brief Common texture description
   * 
   * Contains all members that can be
   * defined for 2D, Cube and 3D textures.
   */
  struct D3D9_COMMON_TEXTURE_DESC {
    UINT                Width;
    UINT                Height;
    UINT                Depth;
    UINT                ArraySize;
    UINT                MipLevels;
    DWORD               Usage;
    D3D9Format          Format;
    D3DPOOL             Pool;
    BOOL                Discard;
    D3DMULTISAMPLE_TYPE MultiSample;
    DWORD               MultisampleQuality;
  };

  struct D3D9ColorView {
    inline Rc<DxvkImageView> Pick(bool Srgb) const {
      return Srgb ? this->Srgb : this->Color;
    }

    Rc<DxvkImageView> Color;
    Rc<DxvkImageView> Srgb;
  };

  struct D3D9ViewSet {
    D3D9ColorView                    Sample;
    Rc<DxvkImageView>                MipGenRT;

    std::array<
      std::array<D3D9ColorView, 15>, 6>     SubresourceSample;
    std::array<
      std::array<D3D9ColorView, 15>, 6>     SubresourceRenderTarget;
    std::array<
      std::array<Rc<DxvkImageView>, 15>, 6> SubresourceDepth;

    bool                             Hazardous = false;

    VkImageLayout GetRTLayout() const {
      return SubresourceRenderTarget[0][0].Color != nullptr
          && SubresourceRenderTarget[0][0].Color->imageInfo().tiling == VK_IMAGE_TILING_OPTIMAL
          && !Hazardous
        ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_GENERAL;
    }

    VkImageLayout GetDepthLayout() const {
      return SubresourceDepth[0][0] != nullptr
          && SubresourceDepth[0][0]->imageInfo().tiling == VK_IMAGE_TILING_OPTIMAL
        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_GENERAL;
    }
  };

  template <typename T>
  using D3D9SubresourceArray = std::array<T, caps::MaxSubresources>;

  class D3D9CommonTexture {

  public:

    D3D9CommonTexture(
            D3D9DeviceEx*             pDevice,
      const D3D9_COMMON_TEXTURE_DESC* pDesc,
            D3DRESOURCETYPE           ResourceType);

    ~D3D9CommonTexture();

    /**
     * \brief Device
     * \returns The parent device
     */
    D3D9DeviceEx* Device() const {
      return m_device;
    }

    /**
      * \brief Texture properties
      *
      * The returned data can be used to fill in
      * \c D3D11_TEXTURE2D_DESC and similar structs.
      * \returns Pointer to texture description
      */
    const D3D9_COMMON_TEXTURE_DESC* Desc() const {
      return &m_desc;
    }

    /**
     * \brief Vulkan Format
     * \returns The Vulkan format of the resource
     */
    VkFormat Format() const {
      return m_format;
    }

    /**
     * \brief Counts number of subresources
     * \returns Number of subresources
     */
    UINT CountSubresources() const {
      return m_desc.ArraySize * m_desc.MipLevels;
    }

    /**
     * \brief Map mode
     * \returns Map mode
     */
    D3D9_COMMON_TEXTURE_MAP_MODE GetMapMode() const {
      return m_mapMode;
    }

    /**
     * \brief The DXVK image
     * Note, this will be nullptr if the map mode is D3D9_COMMON_TEXTURE_MAP_MODE_SYSTEMMEM
     * \returns The DXVK image
     */
    Rc<DxvkImage> GetImage() const {
      return m_image;
    }

    /**
     * \brief Get a copy of the main image, but with a single sample
     * This function will allocate/reuse an image with the same info
     * as the main image
     * \returns An image with identical info, but 1 sample
     */
    Rc<DxvkImage> GetResolveImage() {
      if (unlikely(m_resolveImage == nullptr))
        m_resolveImage = CreateResolveImage();

      return m_resolveImage;
    }

    Rc<DxvkBuffer> GetBuffer(UINT Subresource) {
      return m_buffers[Subresource];
    }


    DxvkBufferSliceHandle GetMappedSlice(UINT Subresource) {
      return m_mappedSlices[Subresource];
    }


    DxvkBufferSliceHandle DiscardMapSlice(UINT Subresource) {
      DxvkBufferSliceHandle handle = m_buffers[Subresource]->allocSlice();
      m_mappedSlices[Subresource] = handle;
      return handle;
    }

    /**
     * \brief Computes subresource from the subresource index
     *
     * Used by some functions that operate on only
     * one subresource, such as \c UpdateSurface.
     * \param [in] Aspect The image aspect
     * \param [in] Subresource Subresource index
     * \returns The Vulkan image subresource
     */
    VkImageSubresource GetSubresourceFromIndex(
            VkImageAspectFlags    Aspect,
            UINT                  Subresource) const;

    /**
     * \brief Normalizes and validates texture description
     * 
     * Fills in undefined values and validates the texture
     * parameters. Any error returned by this method should
     * be forwarded to the application.
     * \param [in,out] pDesc Texture description
     * \returns \c S_OK if the parameters are valid
     */
    static HRESULT NormalizeTextureProperties(
            D3D9_COMMON_TEXTURE_DESC*  pDesc);

    /**
     * \brief Lock Flags
     * Set the lock flags for a given subresource
     */
    void SetLockFlags(UINT Subresource, DWORD Flags) {
      m_lockFlags[Subresource] = Flags;
    }

    /**
     * \brief Lock Flags
     * \returns The log flags for a given subresource
     */
    DWORD GetLockFlags(UINT Subresource) const {
      return m_lockFlags[Subresource];
    }

    /**
     * \brief Shadow
     * \returns Whether the texture is to be depth compared
     */
    bool IsShadow() const {
      return m_shadow;
    }

    /**
     * \brief Subresource
     * \returns The subresource idx of a given face and mip level
     */
    UINT CalcSubresource(UINT Face, UINT MipLevel) const {
      return Face * m_desc.MipLevels + MipLevel;
    }

    /**
     * \brief Creates buffers
     * Creates mapping and staging buffers for all subresources
     * allocates new buffers if necessary
     */
    void CreateBuffers() {
      const uint32_t count = CountSubresources();
      for (uint32_t i = 0; i < count; i++)
        CreateBufferSubresource(i);
    }

    /**
     * \brief Creates a buffer
     * Creates mapping and staging buffers for a given subresource
     * allocates new buffers if necessary
     * \returns Whether an allocation happened
     */
    bool CreateBufferSubresource(UINT Subresource);

    /**
     * \brief Destroys a buffer
     * Destroys mapping and staging buffers for a given subresource
     */
    void DestroyBufferSubresource(UINT Subresource) {
      m_buffers[Subresource] = nullptr;
    }

    /**
     * \brief Managed
     * \returns Whether a resource is managed (pool) or not
     */
    bool IsManaged() const {
      return IsPoolManaged(m_desc.Pool);
    }

    /**
     * \brief Autogen Mipmap
     * \returns Whether the texture is to have automatic mip generation
     */
    bool IsAutomaticMip() const {
      return m_desc.Usage & D3DUSAGE_AUTOGENMIPMAP;
    }

    /**
     * \brief Autogen Mipmap
     * \returns Whether the texture is to have automatic mip generation
     */
    const D3D9ViewSet& GetViews() const {
      return m_views;
    }

    /**
     * \brief Recreate main image view
     * Recreates the main view of the sampler w/ a specific LOD.
     * SetLOD only works on MANAGED textures so this is A-okay.
     */
    void RecreateSampledView(UINT Lod) {
      const D3D9_VK_FORMAT_MAPPING formatInfo = m_device->LookupFormat(m_desc.Format);

      m_views.Sample = CreateColorViewPair(formatInfo, AllLayers, VK_IMAGE_USAGE_SAMPLED_BIT, Lod);
    }

    /**
     * \brief Extent
     * \returns The extent of the top-level mip
     */
    VkExtent3D GetExtent() const {
      return VkExtent3D{ m_desc.Width, m_desc.Height, m_desc.Depth };
    }

    /**
     * \brief Mip Extent
     * \returns The extent of a mip or subresource
     */
    VkExtent3D GetExtentMip(UINT Subresource) const {
      UINT MipLevel = Subresource % m_desc.MipLevels;
      return util::computeMipLevelExtent(GetExtent(), MipLevel);
    }

    bool MarkHazardous() {
      return std::exchange(m_views.Hazardous, true);
    }

    D3DRESOURCETYPE GetType() {
      return m_type;
    }

    void MarkSystemMemGPUModified() {
      m_systemmemModified = true;
    }
    void UnmarkSystemMemGPUModified() {
      m_systemmemModified = false;
    }

    bool GetSystemMemGPUModified() {
      return m_systemmemModified;
    }

    bool MarkLocked(UINT Subresource, bool value) { return std::exchange(m_locked[Subresource], value); }

  private:

    D3D9DeviceEx*                 m_device;
    D3D9_COMMON_TEXTURE_DESC      m_desc;
    D3DRESOURCETYPE               m_type;
    D3D9_COMMON_TEXTURE_MAP_MODE  m_mapMode;

    Rc<DxvkImage>                 m_image;
    Rc<DxvkImage>                 m_resolveImage;
    D3D9SubresourceArray<
      Rc<DxvkBuffer>>             m_buffers;
    D3D9SubresourceArray<
      DxvkBufferSliceHandle>      m_mappedSlices;
    D3D9SubresourceArray<DWORD>   m_lockFlags;

    D3D9ViewSet                   m_views;

    VkFormat                      m_format;

    bool                          m_shadow; //< Depth Compare-ness

    int64_t                       m_size = 0;

    bool                          m_systemmemModified = false;

    D3D9SubresourceArray<
      bool>                       m_locked = { };

    /**
     * \brief Mip level
     * \returns Size of packed mip level in bytes
     */
    VkDeviceSize GetMipSize(UINT Subresource) const;

    Rc<DxvkImage> CreatePrimaryImage(D3DRESOURCETYPE ResourceType) const;

    Rc<DxvkImage> CreateResolveImage() const;

    BOOL DetermineShadowState() const;

    BOOL CheckImageSupport(
      const DxvkImageCreateInfo*  pImageInfo,
            VkImageTiling         Tiling) const;

    VkImageUsageFlags EnableMetaCopyUsage(
            VkFormat              Format,
            VkImageTiling         Tiling) const;

    D3D9_COMMON_TEXTURE_MAP_MODE DetermineMapMode() const {
      if (m_desc.Format == D3D9Format::NULL_FORMAT)
        return D3D9_COMMON_TEXTURE_MAP_MODE_NONE;

      if (m_desc.Pool == D3DPOOL_SYSTEMMEM || m_desc.Pool == D3DPOOL_SCRATCH)
        return D3D9_COMMON_TEXTURE_MAP_MODE_SYSTEMMEM;

      return D3D9_COMMON_TEXTURE_MAP_MODE_BACKED;
    }

    static VkImageType GetImageTypeFromResourceType(
            D3DRESOURCETYPE  Dimension);

    static VkImageViewType GetImageViewTypeFromResourceType(
            D3DRESOURCETYPE  Dimension,
            UINT             Layer);

    static VkImageLayout OptimizeLayout(
            VkImageUsageFlags         Usage);

    static constexpr UINT AllLayers = UINT32_MAX;

    Rc<DxvkImageView> CreateView(
            D3D9_VK_FORMAT_MAPPING FormatInfo,
            UINT                   Layer,
            VkImageUsageFlags      UsageFlags,
            UINT                   Lod,
            BOOL                   Srgb);

    D3D9ColorView CreateColorViewPair(
            D3D9_VK_FORMAT_MAPPING FormatInfo,
            UINT                   Layer,
            VkImageUsageFlags      UsageFlags,
            UINT                   Lod);

    void CreateInitialViews();
  };

}