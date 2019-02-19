#pragma once

#include "d3d9_resource.h"
#include "d3d9_common_texture.h"

namespace dxvk {

  template <typename... Type>
  class Direct3DSubresource9 : public Direct3DResource9<Type...> {

  public:

    Direct3DSubresource9(
      Direct3DDevice9Ex* device,
      Rc<Direct3DCommonTexture9> texture,
      UINT subresource,
      IUnknown* container)
      : Direct3DResource9<Type...>{ device }
      , m_texture{ texture }
      , m_subresource{ subresource }
      , m_container{ container } {
    }

    ULONG STDMETHODCALLTYPE AddRef() final {
      ULONG refCount = Direct3DResource9<Type...>::AddRef();

      // If this is our first reference, add a ref to our container
      // so it doesn't get released during my lifespan.
      if (refCount == 0 && m_container != nullptr)
        m_container->AddRef();

      return refCount;
    }

    ULONG STDMETHODCALLTYPE Release() final {
      ULONG refCount = Direct3DResource9<Type...>::Release();

      // If that was our last public reference gone, dereference our container
      // so that it can be deleted now this subresource is no longer publically referenced.
      if (refCount == 0 && m_container != nullptr)
        m_container->Release();

      return refCount;
    }

    HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid, void** ppContainer) final {
      if (m_container == nullptr)
        return D3DERR_INVALIDCALL;

      return m_container->QueryInterface(riid, ppContainer);
    }

  protected:

    Rc<Direct3DCommonTexture9> m_texture;
    UINT m_subresource;

    IUnknown* m_container;

  };

}