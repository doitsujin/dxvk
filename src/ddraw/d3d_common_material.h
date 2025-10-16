#pragma once

#include "ddraw_include.h"

#include "d3d_common_interface.h"

namespace dxvk {

  class D3D6Material;
  class D3D5Material;
  class D3D3Material;

  class D3DCommonMaterial : public ComObjectClamp<IUnknown> {

  public:

    D3DCommonMaterial();

    ~D3DCommonMaterial();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    d3d9::D3DMATERIAL9* GetD3D9Material() {
      return &m_material9;
    }

    void DirtyMaterialColor() {
      m_dirtyColor = true;
    }

    void SetMaterialHandle(D3DMATERIALHANDLE materialHandle) {
      m_materialHandle = materialHandle;
    }

    D3DMATERIALHANDLE GetMaterialHandle() const {
      return m_materialHandle;
    }

    D3DCOLOR GetMaterialColor() {
      if (unlikely(m_dirtyColor)) {
        m_materialColor = D3DCOLOR_COLORVALUE(m_material9.Diffuse.r, m_material9.Diffuse.g,
                                              m_material9.Diffuse.b, m_material9.Diffuse.a);
        m_dirtyColor = false;
      }

      return m_materialColor;
    }

    void SetD3D6Material(D3D6Material* material6) {
      m_material6 = material6;
    }

    D3D6Material* GetD3D6Material() const {
      return m_material6;
    }

    void SetD3D5Material(D3D5Material* material5) {
      m_material5 = material5;
    }

    D3D5Material* GetD3D5Material() const {
      return m_material5;
    }

    void SetD3D3Material(D3D3Material* material3) {
      m_material3 = material3;
    }

    D3D3Material* GetD3D3Material() const {
      return m_material3;
    }

  private:

    bool               m_dirtyColor     = false;

    D3DMATERIALHANDLE  m_materialHandle = 0;

    D3DCOLOR           m_materialColor  = D3DCOLOR_ARGB(0, 0, 0, 0);

    d3d9::D3DMATERIAL9 m_material9      = { };

    D3D6Material*      m_material6      = nullptr;
    D3D5Material*      m_material5      = nullptr;
    D3D3Material*      m_material3      = nullptr;

  };

}