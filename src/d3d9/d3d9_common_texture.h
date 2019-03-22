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
    BOOL Lockable;
  };

  class Direct3DCommonTexture9 : public RcObject {

  public:

    Direct3DCommonTexture9(
      Direct3DDevice9Ex*      pDevice,
      const D3D9TextureDesc*        pDesc);

    Direct3DCommonTexture9(
            Direct3DDevice9Ex*      pDevice,
            Rc<DxvkImage>           Image,
            Rc<DxvkImageView>       ImageView,
            Rc<DxvkImageView>       ImageViewSrgb,
      const D3D9TextureDesc*        pDesc);

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

    /**
     * \brief The DXVK buffer
     * \returns The DXVK buffer
     */
    Rc<DxvkBuffer> GetMappedBuffer() const {
      return m_buffer;
    }

    /**
     * \brief Currently mapped subresource
     * \returns Mapped subresource
     */
    VkImageSubresource GetMappedSubresource() const {
      return m_mappedSubresource;
    }

    /**
     * \brief Current map flags
     */
    DWORD GetMapFlags() const {
      return m_mapFlags;
    }

    /**
     * \brief Sets mapped subresource
     * \param [in] Subresource The subresource
     */
    void SetMappedSubresource(VkImageSubresource Subresource, DWORD Flags) {
      m_mappedSubresource = Subresource;
      m_mapFlags = Flags;
    }

    /**
     * \brief Resets mapped subresource
     * Marks the texture as not mapped.
     */
    void ClearMappedSubresource() {
      m_mappedSubresource = VkImageSubresource{ };
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
      UINT            Subresource,
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
    HRESULT Unlock(UINT     Subresource);

    Rc<DxvkImageView> GetImageView(bool srgb) const {
      return srgb ? m_imageViewSrgb : m_imageView;
    }

    Rc<DxvkImageView> GetRenderTargetView(bool srgb) const {
      return srgb ? m_renderTargetViewSrgb : m_renderTargetView;
    }

    Rc<DxvkImageView> GetDepthStencilView() const {
      return m_depthStencilView;
    }

    UINT GetLayerCount() const {
      return m_desc.Type == D3DRTYPE_CUBETEXTURE ? 6 : 1;
    }

    VkImageViewType GetImageViewType() const;

    void RecreateImageView(UINT Lod);
    void CreateDepthStencilView();
    void CreateRenderTargetView();

    VkImageLayout GetDepthLayout() const {
      if (m_depthStencilView->imageInfo().tiling == VK_IMAGE_TILING_OPTIMAL)
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      else
        return VK_IMAGE_LAYOUT_GENERAL;
    }

    VkImageLayout GetRenderTargetLayout(bool srgb) const {
      return GetRenderTargetView(srgb)->imageInfo().tiling == VK_IMAGE_TILING_OPTIMAL
        ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_GENERAL;
    }

  private:

    Rc<DxvkImageView>                 m_imageView;
    Rc<DxvkImageView>                 m_imageViewSrgb;

    Rc<DxvkImageView>                 m_renderTargetView;
    Rc<DxvkImageView>                 m_renderTargetViewSrgb;

    Rc<DxvkImageView>                 m_depthStencilView;

    Direct3DDevice9Ex*   m_device;
    D3D9TextureDesc      m_desc;
    D3D9_COMMON_TEXTURE_MAP_MODE m_mapMode;

    Rc<DxvkImage>   m_image;
    Rc<DxvkBuffer>  m_buffer;

    VkImageSubresource m_mappedSubresource
      = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    DWORD m_mapFlags = 0;

    Rc<DxvkBuffer> CreateMappedBuffer() const;

    BOOL CheckImageSupport(
      const DxvkImageCreateInfo*  pImageInfo,
      VkImageTiling         Tiling) const;

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
      VkImageUsageFlags UsageFlags,
      bool              srgb,
      UINT              Lod);

  };

}