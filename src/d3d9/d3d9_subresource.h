#pragma once

#include "d3d9_resource.h"
#include "d3d9_common_texture.h"

namespace dxvk {

  template <typename... Type>
  class D3D9Subresource : public D3D9Resource<Type...> {

  public:

    D3D9Subresource(
            D3D9DeviceEx*           pDevice,
      const bool                    Extended,
            D3D9CommonTexture*      pTexture,
            UINT                    Face,
            UINT                    MipLevel,
            IDirect3DBaseTexture9*  pBaseTexture,
            IUnknown*               pContainer)
    : D3D9Resource<Type...>(pDevice, pTexture->GetPool(), Extended),
      m_container          (pContainer),
      m_baseTexture        (pBaseTexture),
      m_texture            (pTexture),
      m_face               (Face),
      m_mipLevel           (MipLevel),
      m_isSrgbCompatible   (pTexture->IsSrgbCompatible()),
      m_isNull             (pTexture->IsNull()) {

    }

    ~D3D9Subresource() {
      // We own the texture!
      if (m_baseTexture == nullptr)
        delete m_texture;
    }

    ULONG STDMETHODCALLTYPE AddRef() final {
      if (m_baseTexture != nullptr)
        return m_baseTexture->AddRef();

      return D3D9Resource<Type...>::AddRef();
    }

    ULONG STDMETHODCALLTYPE Release() final {
      if (m_baseTexture != nullptr)
        return m_baseTexture->Release();

      return D3D9Resource<Type...>::Release();
    }

    HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void** ppContainer) final {
      if (m_container != nullptr)
        return m_container->QueryInterface(riid, ppContainer);

      return this->GetDevice()->QueryInterface(riid, ppContainer);
    }

    void STDMETHODCALLTYPE PreLoad() {
      m_texture->PreLoadSubresource(GetSubresource());
    }

    inline D3D9CommonTexture* GetCommonTexture() {
      return m_texture;
    }

    inline UINT GetFace() const {
      return m_face;
    }

    inline UINT GetMipLevel() const {
      return m_mipLevel;
    }

    inline UINT GetSubresource() const {
      return m_texture->CalcSubresource(m_face, m_mipLevel);
    }

    inline const Rc<DxvkImageView>& GetRenderTargetView(bool Srgb) {
      Rc<DxvkImageView>& view = m_renderTargetView.Pick(Srgb);

      if (unlikely(!view && !IsNull())) {
        // The backend will ignore the view layout anyway for images
        // that have GENERAL (or FEEDBACK_LOOP) as their layout.
        // Because of that, we don't need to pay special attention here
        // to whether the image was transitioned because of a feedback loop.

        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (m_texture->GetImage()->info().usage & VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT)
          usage |= VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT | VK_IMAGE_USAGE_SAMPLED_BIT;

        view = m_texture->CreateView(m_face, m_mipLevel,
          usage,
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          Srgb && m_isSrgbCompatible);
      }

      return view;
    }

    inline const Rc<DxvkImageView>& GetDepthStencilView(bool Writable) {
      Rc<DxvkImageView>& view = Writable
        ? m_dsvReadWrite
        : m_dsvReadOnly;

      if (unlikely(!view)) {
        // The backend will ignore the view layout anyway for images
        // that have GENERAL (or FEEDBACK_LOOP) as their layout.
        // Because of that, we don't need to pay special attention here
        // to whether the image was transitioned because of a feedback loop.

        VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (m_texture->GetImage()->info().usage & VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT)
          usage |= VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT | VK_IMAGE_USAGE_SAMPLED_BIT;

        VkImageLayout layout = Writable
          ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
          : VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;

        view = m_texture->CreateView(m_face, m_mipLevel,
          usage, layout, false);
      }

      return view;
    }

    inline bool IsNull() {
      return m_isNull;
    }

    inline IDirect3DBaseTexture9* GetBaseTexture() {
      return m_baseTexture;
    }

    inline void Swap(D3D9Subresource* Other) {
      // Only used for swap chain back buffers that don't
      // have a container and all have identical properties
      std::swap(m_texture,          Other->m_texture);
      std::swap(m_renderTargetView, Other->m_renderTargetView);
    }

  protected:

    IUnknown*               m_container;
    IDirect3DBaseTexture9*  m_baseTexture;

    D3D9CommonTexture*      m_texture;

    UINT                    m_face             : 8;
    UINT                    m_mipLevel         : 16;
    UINT                    m_isSrgbCompatible : 1;
    UINT                    m_isNull           : 1;

    D3D9ColorView           m_renderTargetView;

    Rc<DxvkImageView>       m_dsvReadWrite;
    Rc<DxvkImageView>       m_dsvReadOnly;

  };

}
