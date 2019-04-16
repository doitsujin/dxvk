#pragma once

#include "d3d9_resource.h"
#include "d3d9_common_texture.h"

namespace dxvk {

  template <typename... Type>
  class Direct3DSubresource9 : public Direct3DResource9<Type...> {

  public:

    Direct3DSubresource9(
            Direct3DDevice9Ex*      pDevice,
            Direct3DCommonTexture9* pTexture,
            UINT                    Face,
            UINT                    MipLevel,
            IUnknown*               pContainer,
            bool                    OwnsTexture)
      : Direct3DResource9<Type...> ( pDevice )
      , m_container                ( pContainer )
      , m_texture                  ( pTexture )
      , m_face                     ( Face )
      , m_mipLevel                 ( MipLevel )
      , m_ownsTexture              ( OwnsTexture ) { }

    ~Direct3DSubresource9() {
      if (m_ownsTexture)
        delete m_texture;
    }

    ULONG STDMETHODCALLTYPE AddRef() final {
      if (m_container != nullptr)
        m_container->AddRef();

      return Direct3DResource9<Type...>::AddRef();
    }

    ULONG STDMETHODCALLTYPE Release() final {
      if (m_container != nullptr)
        m_container->Release();

      return Direct3DResource9<Type...>::Release();
    }

    HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void** ppContainer) final {
      if (m_container == nullptr)
        return D3DERR_INVALIDCALL;

      return m_container->QueryInterface(riid, ppContainer);
    }

    Direct3DCommonTexture9* GetCommonTexture() {
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
      return m_texture->GetImageView(srgb);
    }

    Rc<DxvkImageView> GetRenderTargetView(bool srgb) {
      return m_texture->GetRenderTargetView(srgb);
    }

    VkImageLayout GetRenderTargetLayout() {
      return m_texture->GetRenderTargetLayout();
    }

    Rc<DxvkImageView> GetDepthStencilView() {
      return m_texture->GetDepthStencilView();
    }

    VkImageLayout GetDepthLayout() {
      return m_texture->GetDepthLayout();
    }

  protected:

    IUnknown*               m_container;

    Direct3DCommonTexture9* m_texture;
    UINT                    m_face;
    UINT                    m_mipLevel;

    bool                    m_ownsTexture;

  };

}