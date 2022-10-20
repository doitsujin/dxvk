#pragma once

#include "d3d8_include.h"

namespace dxvk {

  template <typename D3D9, typename D3D8>
  class D3D8WrappedObject : public ComObjectClamp<D3D8> {

  public:

    D3D8WrappedObject(Com<D3D9>&& object)
      : m_d3d9(std::move(object)) {
    }

    D3D9* GetD3D9() {
      return m_d3d9.ptr();
    }

    // For cases where the object may be null.
    static D3D9* GetD3D9Nullable(D3D8WrappedObject* self) {
      if (unlikely(self == NULL)) {
        return NULL;
      }
      return self->m_d3d9.ptr();
    }

    template <typename T>
    static D3D9* GetD3D9Nullable(Com<T>& self) {
      return GetD3D9Nullable(self.ptr());
    }

  private:

    Com<D3D9> m_d3d9;

  };

}