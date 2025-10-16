#pragma once

#include "ddraw_include.h"

#include <unordered_map>

namespace dxvk {

  class D3DCommonMaterial;

  class D3D7Interface;
  class D3D6Interface;
  class D3D5Interface;
  class D3D3Interface;

  class D3DCommonInterface : public ComObjectClamp<IUnknown> {

  public:

    D3DCommonInterface();

    ~D3DCommonInterface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    static D3DCommonMaterial* GetCommonMaterialFromHandle(D3DMATERIALHANDLE handle);

    static void EmplaceMaterial(D3DCommonMaterial* commonMaterial, D3DMATERIALHANDLE handle);

    static void ReleaseMaterialHandle(D3DMATERIALHANDLE handle);

    d3d9::D3DMULTISAMPLE_TYPE GetMultiSampleType(d3d9::D3DFORMAT backBufferFormat) const;

    static D3DMATERIALHANDLE GetNextMaterialHandle() {
      return ++s_materialHandle;
    }

    d3d9::IDirect3D9* GetD3D9Interface() const {
      return m_d3d9Intf.ptr();
    }

    void SetD3D7Interface(D3D7Interface* d3d7Intf) {
      m_d3d7Intf = d3d7Intf;
    }

    D3D7Interface* GetD3D7Interface() const {
      return m_d3d7Intf;
    }

    void SetD3D6Interface(D3D6Interface* d3d6Intf) {
      m_d3d6Intf = d3d6Intf;
    }

    D3D6Interface* GetD3D6Interface() const {
      return m_d3d6Intf;
    }

    void SetD3D5Interface(D3D5Interface* d3d5Intf) {
      m_d3d5Intf = d3d5Intf;
    }

    D3D5Interface* GetD3D5Interface() const {
      return m_d3d5Intf;
    }

    void SetD3D3Interface(D3D3Interface* d3d3Intf) {
      m_d3d3Intf = d3d3Intf;
    }

    D3D3Interface* GetD3D3Interface() const {
      return m_d3d3Intf;
    }

  private:

    Com<d3d9::IDirect3D9> m_d3d9Intf       = nullptr;

    D3D7Interface*        m_d3d7Intf       = nullptr;
    D3D6Interface*        m_d3d6Intf       = nullptr;
    D3D5Interface*        m_d3d5Intf       = nullptr;
    D3D3Interface*        m_d3d3Intf       = nullptr;

    // Tests have indicated that once created, material handles are shared across
    // all devices and D3D interfaces, regardless of their relation
    static std::atomic<D3DMATERIALHANDLE> s_materialHandle;
    static inline std::unordered_map<D3DMATERIALHANDLE, D3DCommonMaterial*> s_materials;

  };

}