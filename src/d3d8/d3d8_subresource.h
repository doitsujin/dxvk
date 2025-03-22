#pragma once

#include "d3d8_resource.h"

namespace dxvk {

  // Base class for Surfaces and Volumes,
  // which can be attached to Textures.

  template <typename D3D9, typename D3D8>
  class D3D8Subresource : public D3D8Resource<D3D9, D3D8> {

    using Resource = D3D8Resource<D3D9, D3D8>;

  public:

    D3D8Subresource(
            D3D8Device*             pDevice,
      const D3DPOOL                 Pool,
            Com<D3D9>&&             Object,
            IDirect3DBaseTexture8*  pBaseTexture)
    : Resource(pDevice, Pool, std::move(Object)),
      m_container(pBaseTexture) {
    }

    // Refing subresources implicitly refs the container texture,
    ULONG STDMETHODCALLTYPE AddRef() final {
      if (m_container != nullptr)
        return m_container->AddRef();

      return Resource::AddRef();
    }

    // and releasing them implicitly releases the texture.
    ULONG STDMETHODCALLTYPE Release() final {
      if (m_container != nullptr)
        return m_container->Release();

      return Resource::Release();
    }

    // Clients can grab the container if they want
    HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void** ppContainer) final {
      if (m_container != nullptr)
        return m_container->QueryInterface(riid, ppContainer);

      return this->GetDevice()->QueryInterface(riid, ppContainer);
    }

    inline IDirect3DBaseTexture8* GetBaseTexture() {
      return m_container;
    }

  protected:

    IDirect3DBaseTexture8*  m_container;

  };

}