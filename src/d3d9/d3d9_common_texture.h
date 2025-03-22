#pragma once

#include "d3d9_format.h"
#include "d3d9_util.h"
#include "d3d9_caps.h"
#include "d3d9_mem.h"
#include "d3d9_interop.h"

#include "../dxvk/dxvk_device.h"

#include "../util/util_bit.h"

namespace dxvk {

  class D3D9DeviceEx;

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
    D3D9_COMMON_TEXTURE_MAP_MODE_UNMAPPABLE,   ///< Non-Vulkan memory that can be unmapped
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
    D3DMULTISAMPLE_TYPE MultiSample;
    DWORD               MultisampleQuality;
    bool                Discard;
    bool                IsBackBuffer;
    bool                IsAttachmentOnly;
    bool                IsLockable;

    // Additional parameters for ID3D9VkInteropDevice
    VkImageUsageFlags   ImageUsage = 0;
  };

  struct D3D9ColorView {
    inline Rc<DxvkImageView>& Pick(bool Srgb) {
      return Srgb ? this->Srgb : this->Color;
    }

    inline const Rc<DxvkImageView>& Pick(bool Srgb) const {
      return Srgb ? this->Srgb : this->Color;
    }

    Rc<DxvkImageView> Color;
    Rc<DxvkImageView> Srgb;
  };

  template <typename T>
  using D3D9SubresourceArray = std::array<T, caps::MaxSubresources>;

  using D3D9SubresourceBitset = bit::bitset<caps::MaxSubresources>;

  class D3D9CommonTexture {

  public:

    static constexpr UINT AllLayers = std::numeric_limits<uint32_t>::max();

    D3D9CommonTexture(
            D3D9DeviceEx*             pDevice,
            IUnknown*                 pInterface,
      const D3D9_COMMON_TEXTURE_DESC* pDesc,
            D3DRESOURCETYPE           ResourceType,
            HANDLE*                   pSharedHandle);

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
    const D3D9_VK_FORMAT_MAPPING& GetFormatMapping() const {
      return m_mapping;
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
    const Rc<DxvkImage>& GetImage() const {
      return m_image;
    }

    /**
     * \brief Get a copy of the main image, but with a single sample
     * This function will allocate/reuse an image with the same info
     * as the main image
     * \returns An image with identical info, but 1 sample
     */
    const Rc<DxvkImage>& GetResolveImage() {
      if (unlikely(m_resolveImage == nullptr))
        m_resolveImage = CreateResolveImage();

      return m_resolveImage;
    }

    /**
     * \brief Returns a pointer to the internal data used for LockRect/LockBox
     *
     * This works regardless of the map mode used by this texture
     * and will map the memory if necessary.
     * \param [in] Subresource Subresource index
     * @return Pointer to locking data
     */
    void* GetData(UINT Subresource);

    const Rc<DxvkBuffer>& GetBuffer();

    DxvkBufferSlice GetBufferSlice(UINT Subresource);

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
     * \param [in] pDevice D3D9 device
     * \param [in] ResourceType Resource type
     * \param [in,out] pDesc Texture description
     * \returns \c S_OK if the parameters are valid
     */
    static HRESULT NormalizeTextureProperties(
            D3D9DeviceEx*              pDevice,
            D3DRESOURCETYPE            ResourceType,
            D3D9_COMMON_TEXTURE_DESC*  pDesc);

    /**
     * \brief Shadow
     * \returns Whether the texture is to be depth compared
     */
    bool IsShadow() const {
      return m_shadow;
    }

    /**
     * \brief Dref Clamp
     * \returns Whether the texture emulates an UNORM format with D32f
     */
    bool IsUpgradedToD32f() const {
      return m_upgradedToD32f;
    }

    /**
     * \brief FETCH4 compatibility
     * \returns Whether the format of the texture supports the FETCH4 hack
     */
    bool SupportsFetch4() const {
      return m_supportsFetch4;
    }

    /**
     * \brief Null
     * \returns Whether the texture is D3DFMT_NULL or not
     */
    bool IsNull() const {
      return m_desc.Format == D3D9Format::NULL_FORMAT;
    }

    /**
     * \brief Subresource
     * \returns The subresource idx of a given face and mip level
     */
    UINT CalcSubresource(UINT Face, UINT MipLevel) const {
      return Face * m_desc.MipLevels + MipLevel;
    }

    void UnmapData() {
      m_data.Unmap();
    }

    /**
     * \brief Destroys a buffer
     * Destroys mapping and staging buffers for a given subresource
     */
    void DestroyBuffer() {
      m_buffer = nullptr;
      MarkAllNeedReadback();
    }

    bool IsDynamic() const {
      return m_desc.Usage & D3DUSAGE_DYNAMIC;
    }

    /**
     * \brief Managed
     * \returns Whether a resource is managed (pool) or not
     */
    bool IsManaged() const {
      return IsPoolManaged(m_desc.Pool);
    }

    /**
     * \brief Render Target
     * \returns Whether a resource is a render target or not
     */
    bool IsRenderTarget() const {
      return m_desc.Usage & D3DUSAGE_RENDERTARGET;
    }

    /**
     * \brief Depth stencil
     * \returns Whether a resource is a depth stencil or not
     */
    bool IsDepthStencil() const {
      return m_desc.Usage & D3DUSAGE_DEPTHSTENCIL;
    }

    /**
     * \brief Autogen Mipmap
     * \returns Whether the texture is to have automatic mip generation
     */
    bool IsAutomaticMip() const {
      return m_desc.Usage & D3DUSAGE_AUTOGENMIPMAP;
    }

    /**
     * \brief Checks whether sRGB views can be created
     * \returns Whether the format is sRGB compatible.
     */
    bool IsSrgbCompatible() const {
      return m_mapping.FormatSrgb;
    }

    /**
     * \brief Recreate main image view
     * Recreates the main view of the sampler w/ a specific LOD.
     * SetLOD only works on MANAGED textures so this is A-okay.
     */
    void CreateSampleView(UINT Lod);

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

    bool MarkTransitionedToHazardLayout() {
      return std::exchange(m_transitionedToHazardLayout, true);
    }

    D3DRESOURCETYPE GetType() const {
      return m_type;
    }

    uint32_t GetPlaneCount() const;

    D3DPOOL GetPool() const { return m_desc.Pool; }

    const D3D9_VK_FORMAT_MAPPING& GetMapping() { return m_mapping; }

    void SetLocked(UINT Subresource, bool value) { m_locked.set(Subresource, value); }

    bool GetLocked(UINT Subresource) const { return m_locked.get(Subresource); }

    bool IsAnySubresourceLocked() const { return m_locked.any(); }

    void SetNeedsReadback(UINT Subresource, bool value) { m_needsReadback.set(Subresource, value); }

    bool NeedsReadback(UINT Subresource) const { return m_needsReadback.get(Subresource); }

    void MarkAllNeedReadback() { m_needsReadback.setAll(); }

    const Rc<DxvkImageView>& GetSampleView(bool srgb) const {
      return m_sampleView.Pick(srgb && IsSrgbCompatible());
    }

    VkImageLayout DetermineRenderTargetLayout(VkImageLayout hazardLayout) const {
      if (unlikely(m_transitionedToHazardLayout))
        return hazardLayout;

      return m_image != nullptr &&
             m_image->info().tiling == VK_IMAGE_TILING_OPTIMAL
        ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_GENERAL;
    }

    VkImageLayout DetermineDepthStencilLayout(bool write, bool hazardous, VkImageLayout hazardLayout) const {
      if (unlikely(m_transitionedToHazardLayout))
        return hazardLayout;

      if (unlikely(m_image->info().tiling != VK_IMAGE_TILING_OPTIMAL))
        return VK_IMAGE_LAYOUT_GENERAL;

      if (unlikely(hazardous && !write))
        return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;

      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    Rc<DxvkImageView> CreateView(
            UINT                   Layer,
            UINT                   Lod,
            VkImageUsageFlags      UsageFlags,
            bool                   Srgb);
    D3D9SubresourceBitset& GetUploadBitmask() { return m_needsUpload; }

    void SetAllNeedUpload() {
      if (likely(!IsAutomaticMip())) {
        m_needsUpload.setAll();
      } else {
        for (uint32_t a = 0; a < m_desc.ArraySize; a++) {
          for (uint32_t m = 0; m < ExposedMipLevels(); m++) {
            SetNeedsUpload(CalcSubresource(a, m), true);
          }
        }
      }
    }
    void SetNeedsUpload(UINT Subresource, bool upload) { m_needsUpload.set(Subresource, upload); }
    bool NeedsUpload(UINT Subresource) const { return m_needsUpload.get(Subresource); }
    bool NeedsAnyUpload() { return m_needsUpload.any(); }
    void ClearNeedsUpload() { return m_needsUpload.clearAll();  }

    void SetNeedsMipGen(bool value) { m_needsMipGen = value; }
    bool NeedsMipGen() const { return m_needsMipGen; }

    DWORD ExposedMipLevels() const { return m_exposedMipLevels; }

    void SetMipFilter(D3DTEXTUREFILTERTYPE filter) { m_mipFilter = filter; }
    D3DTEXTUREFILTERTYPE GetMipFilter() const { return m_mipFilter; }

    void PreLoadAll();
    void PreLoadSubresource(UINT Subresource);

    void AddDirtyBox(CONST D3DBOX* pDirtyBox, uint32_t layer) {
      if (pDirtyBox) {
        D3DBOX box = *pDirtyBox;
        if (box.Right <= box.Left
          || box.Bottom <= box.Top
          || box.Back <= box.Front)
          return;

        box.Right = std::min(box.Right, m_desc.Width);
        box.Bottom = std::min(box.Bottom, m_desc.Height);
        box.Back = std::min(box.Back, m_desc.Depth);

        D3DBOX& dirtyBox = m_dirtyBoxes[layer];
        if (dirtyBox.Left == dirtyBox.Right) {
          dirtyBox = box;
        } else {
          dirtyBox.Left    = std::min(dirtyBox.Left,   box.Left);
          dirtyBox.Right   = std::max(dirtyBox.Right,  box.Right);
          dirtyBox.Top     = std::min(dirtyBox.Top,    box.Top);
          dirtyBox.Bottom  = std::max(dirtyBox.Bottom, box.Bottom);
          dirtyBox.Front   = std::min(dirtyBox.Front,  box.Front);
          dirtyBox.Back    = std::max(dirtyBox.Back,   box.Back);
        }
      } else {
        m_dirtyBoxes[layer] = { 0, 0, m_desc.Width, m_desc.Height, 0, m_desc.Depth };
      }
    }

    void ClearDirtyBoxes() {
      for (uint32_t i = 0; i < m_dirtyBoxes.size(); i++) {
        m_dirtyBoxes[i] = { 0, 0, 0, 0, 0, 0 };
      }
    }

    const D3DBOX& GetDirtyBox(uint32_t layer) const {
      return m_dirtyBoxes[layer];
    }

    static VkImageType GetImageTypeFromResourceType(
            D3DRESOURCETYPE  Dimension);

    static VkImageViewType GetImageViewTypeFromResourceType(
            D3DRESOURCETYPE  Dimension,
            UINT             Layer);

     /**
     * \brief Tracks sequence number for a given subresource
     *
     * Stores which CS chunk the resource was last used on.
     * \param [in] Subresource Subresource index
     * \param [in] Seq Sequence number
     */
    void TrackMappingBufferSequenceNumber(UINT Subresource, uint64_t Seq) {
      if (Subresource < m_seqs.size())
        m_seqs[Subresource] = Seq;
    }

    /**
     * \brief Queries sequence number for a given subresource
     *
     * Returns which CS chunk the resource was last used on.
     * \param [in] Subresource Subresource index
     * \returns Sequence number for the given subresource
     */
    uint64_t GetMappingBufferSequenceNumber(UINT Subresource) {
      return Subresource < m_seqs.size()
        ? m_seqs[Subresource]
        : 0ull;
    }

    /**
     * \brief Mip level
     * \returns Size of packed mip level in bytes
     */
    VkDeviceSize GetMipSize(UINT Subresource) const;

    uint32_t GetTotalSize() const {
      return m_totalSize;
    }

    /**
     * \brief Creates a buffer
     * Creates the mapping buffer if necessary
     * \param [in] Initialize Whether to copy over existing data (or clear if there is no data)
     * \returns Whether an allocation happened
     */
    void CreateBuffer(bool Initialize);

    ID3D9VkInteropTexture* GetVkInterop() { return &m_d3d9Interop; }

  private:

    D3D9DeviceEx*                 m_device;
    D3D9_COMMON_TEXTURE_DESC      m_desc;
    D3DRESOURCETYPE               m_type;
    D3D9_COMMON_TEXTURE_MAP_MODE  m_mapMode;

    Rc<DxvkImage>                 m_image;
    Rc<DxvkImage>                 m_resolveImage;
    Rc<DxvkBuffer>                m_buffer;
    D3D9Memory                    m_data = { };

    D3D9SubresourceArray<
      uint64_t>                   m_seqs = { };

    D3D9SubresourceArray<
      uint32_t>                   m_memoryOffset = { };
    
    uint32_t                      m_totalSize = 0;

    D3D9_VK_FORMAT_MAPPING        m_mapping;

    bool                          m_shadow; //< Depth Compare-ness
    bool                          m_upgradedToD32f; // Dref Clamp
    bool                          m_supportsFetch4;

    int64_t                       m_size = 0;

    bool                          m_transitionedToHazardLayout = false;

    D3D9ColorView                 m_sampleView;

    D3D9SubresourceBitset         m_locked = { };

    D3D9SubresourceBitset         m_needsReadback = { };

    D3D9SubresourceBitset         m_needsUpload = { };

    DWORD                         m_exposedMipLevels = 0;

    bool                          m_needsMipGen = false;

    D3DTEXTUREFILTERTYPE          m_mipFilter = D3DTEXF_LINEAR;

    std::array<D3DBOX, 6>         m_dirtyBoxes;

    D3D9VkInteropTexture          m_d3d9Interop;

    Rc<DxvkImage> CreatePrimaryImage(D3DRESOURCETYPE ResourceType, bool TryOffscreenRT, HANDLE* pSharedHandle) const;

    Rc<DxvkImage> CreateResolveImage() const;

    BOOL DetermineShadowState() const;

    BOOL DetermineFetch4Compatibility() const;

    BOOL CheckImageSupport(
      const DxvkImageCreateInfo*  pImageInfo,
            VkImageTiling         Tiling) const;

    D3D9_COMMON_TEXTURE_MAP_MODE DetermineMapMode() const;

    VkImageLayout OptimizeLayout(
            VkImageUsageFlags         Usage) const;

    void ExportImageInfo();

  };

}
