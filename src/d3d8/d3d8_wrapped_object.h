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

  private:

    Com<D3D9> m_d3d9;

  };

}