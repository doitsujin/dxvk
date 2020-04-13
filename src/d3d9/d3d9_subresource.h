#pragma once

#include "d3d9_resource.h"
#include "d3d9_common_texture.h"

namespace dxvk {

  template <typename... Type>
  class D3D9Subresource : public D3D9Resource<Type...> {

  public:

    D3D9Subresource(
            D3D9DeviceEx*           pDevice,
            D3D9CommonTexture*      pTexture,
            UINT                    Face,
            UINT                    MipLevel,
            IDirect3DBaseTexture9*  pBaseTexture,
            IUnknown*               pContainer)
      : D3D9Resource<Type...>      ( pDevice )
      , m_container                ( pContainer )
      , m_baseTexture              ( pBaseTexture )
      , m_texture                  ( pTexture )
      , m_face                     ( Face )
      , m_mipLevel                 ( MipLevel )
      , m_isSrgbCompatible         ( pTexture->IsSrgbCompatible() ) { }

    ~D3D9Subresource() {
      // We own the texture!
      if (m_baseTexture == nullptr)
        delete m_texture;
    }

    ULONG STDMETHODCALLTYPE AddRef() final {
      if (m_container != nullptr)
        return m_container->AddRef();

      return D3D9Resource<Type...>::AddRef();
    }

    ULONG STDMETHODCALLTYPE Release() final {
      if (m_container != nullptr)
        return m_container->Release();

      return D3D9Resource<Type...>::Release();
    }

    HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void** ppContainer) final {
      if (m_container != nullptr)
        return m_container->QueryInterface(riid, ppContainer);

      return this->GetDevice()->QueryInterface(riid, ppContainer);
    }

    D3D9CommonTexture* GetCommonTexture() {
      return m_texture;
    }

    UINT GetFace() const {
      return m_face;
    }

    UINT GetMipLevel() const {
      return m_mipLevel;
    }

    UINT GetSubresource() const {
      return m_texture->CalcSubresource(m_face, m_mipLevel);
    }

    Rc<DxvkImageView> GetImageView(bool Srgb) {
      Srgb &= m_isSrgbCompatible;
      Rc<DxvkImageView>& view = m_sampleView.Pick(Srgb);

      if (unlikely(view == nullptr && !IsNull()))
        view = m_texture->CreateView(m_face, m_mipLevel, VK_IMAGE_USAGE_SAMPLED_BIT, Srgb);

      return view;
    }

    Rc<DxvkImageView> GetRenderTargetView(bool Srgb) {
      Srgb &= m_isSrgbCompatible;
      Rc<DxvkImageView>& view = m_renderTargetView.Pick(Srgb);

      if (unlikely(view == nullptr && !IsNull()))
        view = m_texture->CreateView(m_face, m_mipLevel, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, Srgb);

      return view;
    }

    VkImageLayout GetRenderTargetLayout() const {
      return m_texture->DetermineRenderTargetLayout();
    }

    Rc<DxvkImageView> GetDepthStencilView() {
      Rc<DxvkImageView>& view = m_depthStencilView;

      if (unlikely(view == nullptr))
        view = m_texture->CreateView(m_face, m_mipLevel, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, false);

      return view;
    }

    VkImageLayout GetDepthStencilLayout(bool write, bool hazardous) const {
      return m_texture->DetermineDepthStencilLayout(write, hazardous);
    }

    bool IsNull() {
      return m_texture->Desc()->Format == D3D9Format::NULL_FORMAT;
    }

    IDirect3DBaseTexture9* GetBaseTexture() {
      return m_baseTexture;
    }

    void Swap(D3D9Subresource* Other) {
      // Only used for swap chain back buffers that don't
      // have a container and all have identical properties
      std::swap(m_texture,          Other->m_texture);
      std::swap(m_sampleView,       Other->m_sampleView);
      std::swap(m_renderTargetView, Other->m_renderTargetView);
    }

  protected:

    IUnknown*               m_container;
    IDirect3DBaseTexture9*  m_baseTexture;

    D3D9CommonTexture*      m_texture;
    UINT                    m_face;
    UINT                    m_mipLevel;

    bool                    m_isSrgbCompatible;
    D3D9ColorView           m_sampleView;
    D3D9ColorView           m_renderTargetView;
    Rc<DxvkImageView>       m_depthStencilView;

  };

}