#pragma once

#include "../ddraw_include.h"
#include "../ddraw_child_object.h"

#include "../d3d_common_material.h"

namespace dxvk {

  class D3D5Interface;

  class D3D5Material final : public DDrawChildObject<D3D5Interface, IDirect3DMaterial2> {

  public:

    D3D5Material(D3D5Interface* pParent);

    ~D3D5Material();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE SetMaterial(D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL *data);

    HRESULT STDMETHODCALLTYPE GetHandle(IDirect3DDevice2 *device, D3DMATERIALHANDLE *handle);

    D3DCommonMaterial* GetCommonMaterial() const {
      return m_commonMaterial.ptr();
    }

  private:

    Com<D3DCommonMaterial> m_commonMaterial;

    uint32_t               m_materialCount = 0;
    static std::atomic<uint32_t> s_materialCount;

  };

}
