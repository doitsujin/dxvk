#pragma once

#include "../ddraw_include.h"
#include "../ddraw_child_object.h"

#include "../d3d_common_material.h"

namespace dxvk {

  class D3D6Interface;

  class D3D6Material final : public DDrawChildObject<D3D6Interface, IDirect3DMaterial3> {

  public:

    D3D6Material(D3D6Interface* pParent);

    ~D3D6Material();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE SetMaterial(D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetHandle(IDirect3DDevice3 *device, D3DMATERIALHANDLE *handle);

    D3DCommonMaterial* GetCommonMaterial() const {
      return m_commonMaterial.ptr();
    }

  private:

    Com<D3DCommonMaterial> m_commonMaterial;

    uint32_t               m_materialCount = 0;
    static std::atomic<uint32_t> s_materialCount;

  };

}
