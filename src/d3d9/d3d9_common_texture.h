#pragma once

#include "d3d9_device.h"
#include "d3d9_format.h"

#include "../dxvk/dxvk_device.h"

namespace dxvk {

  /**
 * \brief Image memory mapping mode
 *
 * Determines how exactly \c Map will
 * behave when mapping an image.
 */
  enum D3D9_COMMON_TEXTURE_MAP_MODE {
    D3D9_COMMON_TEXTURE_MAP_MODE_BUFFER, ///< Mapped through buffer
    D3D9_COMMON_TEXTURE_MAP_MODE_DIRECT, ///< Directly mapped to host mem
  };

  struct Direct3DView9 {
    Rc<DxvkImageView> View;
    VkImageLayout Layout;
  };

  struct D3D9TextureDesc {
    D3DRESOURCETYPE Type;
    UINT Width;
    UINT Height;
    UINT Depth;
    UINT MipLevels;
    DWORD Usage;
    D3D9Format Format;
    D3DPOOL Pool;
    BOOL Discard;
    D3DMULTISAMPLE_TYPE MultiSample;
    DWORD MultisampleQuality;
    BOOL Offscreen;
  };

  class D3D9CommonTexture {

  public:

    D3D9CommonTexture(
            D3D9DeviceEx*           pDevice,
      const D3D9TextureDesc*        pDesc);

    D3D9CommonTexture(
            D3D9DeviceEx*           pDevice,
            Rc<DxvkImage>           Image,
            Rc<DxvkImageView>       ImageView,
            Rc<DxvkImageView>       ImageViewSrgb,
      const D3D9TextureDesc*        pDesc);

    ~D3D9CommonTexture();

    /**
     * \brief Texture properties
     *
     * The returned data can be used to fill in
     * \c D3D9TextureDesc and similar structs.
     * \returns Pointer to texture description
     */
    const D3D9TextureDesc* Desc() const {
      return &m_desc;
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
     * \returns The DXVK image
     */
    Rc<DxvkImage> GetImage() const {
      return m_image;
    }

    Rc<DxvkImage> GetResolveImage();

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
     * \brief Checks whether a view can be created for this textue
     *
     * View formats are only compatible if they are either identical
     * or from the same family of typeless formats, where the resource
     * format must be typeless and the view format must be typed. This
     * will also check whether the required bind flags are supported.
     * \param [in] D3D9 usage flags for the view.
     * \param [in] Format The desired view format.
     * \param [in] If the format should be SRGB or not.
     * \returns \c true if the format is compatible
     */
    bool CheckViewCompatibility(
      DWORD               Usage,
      D3D9Format          Format,
      bool                srgb) const;

    /**
     * \brief Normalizes and validates texture description
     *
     * Fills in undefined values and validates the texture
     * parameters. Any error returned by this method should
     * be forwarded to the application.
     * \param [in,out] pDesc Texture description
     * \returns \c D3D_OK if the parameters are valid
     */
    static HRESULT NormalizeTextureProperties(
      D3D9TextureDesc* pDesc);

    /**
     * \brief Locks a subresource of an image
     *
     * Passthrough to device lock.
     * \param [in] Subresource The subresource of the image to lock
     * \param [out] pLockedBox The returned locked box of the image, containing data ptr and strides
     * \param [in] pBox The region of the subresource to lock. This offsets the returned data ptr
     * \param [in] Flags The D3DLOCK_* flags to lock the image with
     * \returns \c D3D_OK if the parameters are valid or D3DERR_INVALIDCALL if it fails.
     */
    HRESULT Lock(
            UINT            Face,
            UINT            MipLevel,
            D3DLOCKED_BOX*  pLockedBox,
      const D3DBOX*         pBox,
            DWORD           Flags);

    /**
     * \brief Unlocks a subresource of an image
     *
     * Passthrough to device unlock.
     * \param [in] Subresource The subresource of the image to unlock
     * \returns \c D3D_OK if the parameters are valid or D3DERR_INVALIDCALL if it fails.
     */
    HRESULT Unlock(
            UINT Face,
            UINT MipLevel);

    Rc<DxvkImageView> GetMipGenView() const {
      return m_mipGenView;
    }

    Rc<DxvkImageView> GetImageView(bool srgb) const {
      return srgb ? m_imageViewSrgb : m_imageView;
    }

    Rc<DxvkImageView> GetImageViewLayer(uint32_t face, bool srgb) const {
      return srgb ? m_imageViewSrgbFaces[face] : m_imageViewFaces[face];
    }

    Rc<DxvkImageView> GetRenderTargetView(uint32_t face, bool srgb) const {
      return srgb ? m_renderTargetViewSrgb[face] : m_renderTargetView[face];
    }

    Rc<DxvkImageView> GetDepthStencilView(uint32_t face) const {
      return m_depthStencilView[face];
    }

    bool RequiresFixup() const {
      return m_desc.Format == D3D9Format::R8G8B8;
          //|| m_desc.Format == D3D9Format::A8L8;
    }

    UINT GetMipCount() const {
      return m_desc.MipLevels;
    }

    UINT GetLayerCount() const {
      return m_desc.Type == D3DRTYPE_CUBETEXTURE ? 6 : 1;
    }

    UINT GetSubresourceCount() const {
      return GetLayerCount()* m_desc.MipLevels;
    }

    void GenerateMipSubLevels() {
      if (m_desc.Usage & D3DUSAGE_AUTOGENMIPMAP)
        m_device->GenerateMips(this);
    }

    VkImageViewType GetImageViewType() const;

    void RecreateImageViews(UINT Lod);
    void CreateDepthStencilViews();
    void CreateRenderTargetViews();

    VkImageLayout GetDepthLayout() const {
      return (m_depthStencilView[0] != nullptr
           && m_depthStencilView[0]->imageInfo().tiling == VK_IMAGE_TILING_OPTIMAL)
        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_GENERAL;
    }

    VkImageLayout GetRenderTargetLayout() const {
      return (m_renderTargetView[0] != nullptr
           && m_renderTargetView[0]->imageInfo().tiling == VK_IMAGE_TILING_OPTIMAL)
        ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_GENERAL;
    }

    VkDeviceSize GetMipLength(UINT MipLevel) const;

    void MarkSubresourceMapped(UINT Face, UINT MipLevel, DWORD LockFlags) {
      const uint16_t mipBit = 1u << MipLevel;

      LockFlags & D3DLOCK_READONLY
        ? m_readOnlySubresources[Face] |= mipBit
        : m_mappedSubresources[Face]   |= mipBit;
    }

    bool IsReadOnlyLock(UINT Face, UINT MipLevel) {
      const uint16_t mipBit = 1u << MipLevel;

      return m_readOnlySubresources[Face] & mipBit;
    }

    bool MarkSubresourceUnmapped(UINT Face, UINT MipLevel) {
      const uint16_t mipBit = 1u << MipLevel;

      if (!(m_readOnlySubresources[Face] & mipBit))
        m_unmappedSubresources[Face] |= mipBit;

      m_readOnlySubresources[Face] &= ~mipBit;

      return m_mappedSubresources == m_unmappedSubresources;
    }

    bool ReadLocksRemaining() {
      for (uint16_t mask : m_readOnlySubresources) {
        if (mask != 0)
          return true;
      }

      return false;
    }

    std::array<uint16_t, 6> DiscardSubresourceMasking() {
      std::array<uint16_t, 6> copy = m_mappedSubresources;

      for (uint32_t i = 0; i < m_mappedSubresources.size(); i++) {
        m_mappedSubresources.at(i) = 0;
        m_unmappedSubresources.at(i) = 0;
      }

      return copy;
    }

    UINT CalcSubresource(UINT Face, UINT MipLevel) const {
      return Face * m_desc.MipLevels + MipLevel;
    }

    bool AllocBuffers(UINT Face, UINT MipLevel);

    Rc<DxvkBuffer> GetMappedBuffer(UINT Subresource) {
      return m_mappingBuffers.at(Subresource);
    }

    Rc<DxvkBuffer> GetFixupBuffer(UINT Subresource) {
      return m_fixupBuffers.at(Subresource);
    }

    bool IsWriteOnly() {
      return m_desc.Usage & D3DUSAGE_WRITEONLY;
    }

    bool ShouldShadow() {
      return m_shadow;
    }

    void DeallocMappingBuffers();
    void DeallocFixupBuffers();
    void DeallocFixupBuffer(UINT Subresource);

  private:

    D3D9DeviceEx*        m_device;
    D3D9TextureDesc      m_desc;
    D3D9_COMMON_TEXTURE_MAP_MODE m_mapMode;

    Rc<DxvkImage>   m_image;

    Rc<DxvkImage>   m_resolveImage;

    std::vector<Rc<DxvkBuffer>>       m_mappingBuffers;
    std::vector<Rc<DxvkBuffer>>       m_fixupBuffers;

    Rc<DxvkImageView>                 m_imageView;
    Rc<DxvkImageView>                 m_imageViewSrgb;

    std::array<Rc<DxvkImageView>, 6>  m_imageViewFaces;
    std::array<Rc<DxvkImageView>, 6>  m_imageViewSrgbFaces;

    Rc<DxvkImageView>                 m_mipGenView;

    std::array<Rc<DxvkImageView>, 6>  m_renderTargetView;
    std::array<Rc<DxvkImageView>, 6>  m_renderTargetViewSrgb;

    std::array<Rc<DxvkImageView>, 6>  m_depthStencilView;

    std::array<uint16_t, 6>           m_mappedSubresources   = { 0 };
    std::array<uint16_t, 6>           m_unmappedSubresources = { 0 };
    std::array<uint16_t, 6>           m_readOnlySubresources = { 0 };

    bool                              m_shadow               = false;

    int64_t                           m_size                 = 0;

    BOOL CheckImageSupport(
      const DxvkImageCreateInfo*  pImageInfo,
      VkImageTiling         Tiling) const;

    BOOL CalcShadowState() const;

    int64_t CalcMemoryConsumption() const;

    BOOL CheckFormatFeatureSupport(
      VkFormat              Format,
      VkFormatFeatureFlags  Features) const;

    VkImageUsageFlags EnableMetaCopyUsage(
      VkFormat              Format,
      VkImageTiling         Tiling) const;

    VkImageUsageFlags EnableMetaPackUsage(
      VkFormat              Format,
      BOOL                  WriteOnly) const;

    D3D9_COMMON_TEXTURE_MAP_MODE DetermineMapMode(
      const DxvkImageCreateInfo*  pImageInfo) const;

    static VkImageType GetImageTypeFromResourceType(
      D3DRESOURCETYPE       Type);

    static VkImageLayout OptimizeLayout(
      VkImageUsageFlags         Usage);

    Rc<DxvkImageView> CreateView(
      int32_t           Index,
      VkImageUsageFlags UsageFlags,
      bool              srgb,
      UINT              Lod);

  };

}