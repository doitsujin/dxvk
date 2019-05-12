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
            IUnknown*               pContainer,
            bool                    OwnsTexture)
      : D3D9Resource<Type...> ( pDevice )
      , m_container                ( pContainer )
      , m_texture                  ( pTexture )
      , m_face                     ( Face )
      , m_mipLevel                 ( MipLevel )
      , m_ownsTexture              ( OwnsTexture ) { }

    ~D3D9Subresource() {
      if (m_ownsTexture)
        delete m_texture;
    }

    ULONG STDMETHODCALLTYPE AddRef() final {
      if (m_container != nullptr)
        m_container->AddRef();

      return D3D9Resource<Type...>::AddRef();
    }

    ULONG STDMETHODCALLTYPE Release() final {
      if (m_container != nullptr)
        m_container->Release();

      return D3D9Resource<Type...>::Release();
    }

    HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void** ppContainer) final {
      if (m_container == nullptr)
        return D3DERR_INVALIDCALL;

      return m_container->QueryInterface(riid, ppContainer);
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

    Rc<DxvkImageView> GetImageView(bool srgb) {
      return m_texture->GetImageViewLayer(m_face, srgb);
    }

    Rc<DxvkImageView> GetRenderTargetView(bool srgb) {
      return m_texture->GetRenderTargetView(m_face, srgb);
    }

    VkImageLayout GetRenderTargetLayout() {
      return m_texture->GetRenderTargetLayout();
    }

    Rc<DxvkImageView> GetDepthStencilView() {
      return m_texture->GetDepthStencilView(m_face);
    }

    VkImageLayout GetDepthLayout() {
      return m_texture->GetDepthLayout();
    }

  protected:

    IUnknown*               m_container;

    D3D9CommonTexture*      m_texture;
    UINT                    m_face;
    UINT                    m_mipLevel;

    bool                    m_ownsTexture;

  };

}