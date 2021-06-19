#pragma once

#include "d3d9_format.h"
#include "d3d9_util.h"
#include "d3d9_caps.h"

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
    BOOL                IsBackBuffer;
    BOOL                IsAttachmentOnly;
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
    const D3D9_VK_FORMAT_MAPPING GetFormatMapping() const {
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
            D3D9DeviceEx*              pDevice,
            D3D9_COMMON_TEXTURE_DESC*  pDesc);

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
      SetWrittenByGPU(Subresource, true);
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

    bool MarkHazardous() {
      return std::exchange(m_hazardous, true);
    }

    D3DRESOURCETYPE GetType() {
      return m_type;
    }

    const D3D9_VK_FORMAT_MAPPING& GetMapping() { return m_mapping; }

    void SetLocked(UINT Subresource, bool value) { m_locked.set(Subresource, value); }

    bool GetLocked(UINT Subresource) const { return m_locked.get(Subresource); }

    bool IsAnySubresourceLocked() const { return m_locked.any(); }

    void SetWrittenByGPU(UINT Subresource, bool value) { m_wasWrittenByGPU.set(Subresource, value); }

    bool WasWrittenByGPU(UINT Subresource) const { return m_wasWrittenByGPU.get(Subresource); }

    void MarkAllWrittenByGPU() { m_wasWrittenByGPU.setAll(); }

    void SetReadOnlyLocked(UINT Subresource, bool readOnly) { return m_readOnly.set(Subresource, readOnly); }

    bool GetReadOnlyLocked(UINT Subresource) const { return m_readOnly.get(Subresource); }

    const Rc<DxvkImageView>& GetSampleView(bool srgb) const {
      return m_sampleView.Pick(srgb && IsSrgbCompatible());
    }

    VkImageLayout DetermineRenderTargetLayout() const {
      return m_image != nullptr &&
             m_image->info().tiling == VK_IMAGE_TILING_OPTIMAL &&
            !m_hazardous
        ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_GENERAL;
    }

    VkImageLayout DetermineDepthStencilLayout(bool write, bool hazardous) const {
      VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

      if (unlikely(hazardous)) {
        layout = write
          ? VK_IMAGE_LAYOUT_GENERAL
          : VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
      }

      if (unlikely(m_image->info().tiling != VK_IMAGE_TILING_OPTIMAL))
        layout = VK_IMAGE_LAYOUT_GENERAL;

      return layout;
    }

    Rc<DxvkImageView> CreateView(
            UINT                   Layer,
            UINT                   Lod,
            VkImageUsageFlags      UsageFlags,
            bool                   Srgb);
    D3D9SubresourceBitset& GetUploadBitmask() { return m_needsUpload; }

    void SetNeedsUpload(UINT Subresource, bool upload) { m_needsUpload.set(Subresource, upload); }
    bool NeedsUpload(UINT Subresource) const { return m_needsUpload.get(Subresource); }
    bool NeedsAnyUpload() { return m_needsUpload.any(); }
    void ClearNeedsUpload() { return m_needsUpload.clearAll();  }
    bool DoesStagingBufferUploads(UINT Subresource) const { return m_uploadUsingStaging.get(Subresource); }

    void EnableStagingBufferUploads(UINT Subresource) {
      m_uploadUsingStaging.set(Subresource, true);
    }

    void SetNeedsMipGen(bool value) { m_needsMipGen = value; }
    bool NeedsMipGen() const { return m_needsMipGen; }

    DWORD ExposedMipLevels() { return m_exposedMipLevels; }

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

    D3D9_VK_FORMAT_MAPPING        m_mapping;

    bool                          m_shadow; //< Depth Compare-ness

    int64_t                       m_size = 0;

    bool                          m_systemmemModified = false;

    bool                          m_hazardous = false;

    D3D9ColorView                 m_sampleView;

    D3D9SubresourceBitset         m_locked = { };

    D3D9SubresourceBitset         m_readOnly = { };

    D3D9SubresourceBitset         m_wasWrittenByGPU = { };

    D3D9SubresourceBitset         m_needsUpload = { };

    D3D9SubresourceBitset         m_uploadUsingStaging = { };

    DWORD                         m_exposedMipLevels = 0;

    bool                          m_needsMipGen = false;

    D3DTEXTUREFILTERTYPE          m_mipFilter = D3DTEXF_LINEAR;

    std::array<D3DBOX, 6>         m_dirtyBoxes;

    /**
     * \brief Mip level
     * \returns Size of packed mip level in bytes
     */
    VkDeviceSize GetMipSize(UINT Subresource) const;

    Rc<DxvkImage> CreatePrimaryImage(D3DRESOURCETYPE ResourceType, bool TryOffscreenRT) const;

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

    VkImageLayout OptimizeLayout(
            VkImageUsageFlags         Usage) const;

    static VkImageType GetImageTypeFromResourceType(
            D3DRESOURCETYPE  Dimension);

    static VkImageViewType GetImageViewTypeFromResourceType(
            D3DRESOURCETYPE  Dimension,
            UINT             Layer);

    static constexpr UINT AllLayers = UINT32_MAX;

  };

}