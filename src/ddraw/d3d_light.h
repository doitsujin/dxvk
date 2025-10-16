#pragma once

#include "ddraw_include.h"
#include "ddraw_child_object.h"

namespace dxvk {

  class D3D6Viewport;
  class D3D5Viewport;
  class D3D3Viewport;

  class D3DLight final : public DDrawChildObject<IUnknown, IDirect3DLight> {

  public:

    D3DLight(IUnknown* pParent);

    ~D3DLight();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Initialize(IDirect3D *d3d);

    HRESULT STDMETHODCALLTYPE SetLight(D3DLIGHT *data);

    HRESULT STDMETHODCALLTYPE GetLight(D3DLIGHT *data);

    const d3d9::D3DLIGHT9* GetD3D9Light() const {
      return &m_light9;
    }

    void SetViewport6(D3D6Viewport* viewport6) {
      m_viewport6 = viewport6;
    }

    void SetViewport5(D3D5Viewport* viewport5) {
      m_viewport5 = viewport5;
    }

    void SetViewport3(D3D3Viewport* viewport3) {
      m_viewport3 = viewport3;
    }

    bool HasViewport() const {
      return m_viewport6 != nullptr || m_viewport5 != nullptr || m_viewport3 != nullptr;
    }

    DWORD GetIndex() const {
      return m_lightCount;
    }

    bool IsActive() const {
      return m_isActive;
    }

  private:

    bool             m_isActive        = false;
    bool             m_isParallelPoint = false;

    DWORD            m_flags           = 0;

    D3D6Viewport*    m_viewport6       = nullptr;
    D3D5Viewport*    m_viewport5       = nullptr;
    D3D3Viewport*    m_viewport3       = nullptr;

    d3d9::D3DLIGHT9  m_light9          = { };

    uint32_t         m_lightCount      = 0;
    static std::atomic<uint32_t> s_lightCount;

  };

}
