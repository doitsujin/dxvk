#pragma once

#include "d3d8_include.h"

namespace dxvk {

  template <typename D3D9Type, typename D3D8Type>
  class D3D8WrappedObject : public ComObjectClamp<D3D8Type> {

  public:

    using D3D9 = D3D9Type;
    using D3D8 = D3D8Type;

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

    virtual IUnknown* GetInterface(REFIID riid) {
      if (riid == __uuidof(IUnknown))
        return this;
      if (riid == __uuidof(D3D8))
        return this;

      throw DxvkError("D3D8WrappedObject::QueryInterface: Unknown interface query");
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) final {
      if (ppvObject == nullptr)
        return E_POINTER;

      *ppvObject = nullptr;

      try {
        *ppvObject = ref(this->GetInterface(riid));
        return S_OK;
      } catch (const DxvkError& e) {
        Logger::warn(e.message());
        Logger::warn(str::format(riid));
        return E_NOINTERFACE;
      }
    }

  private:

    Com<D3D9> m_d3d9;

  };

}