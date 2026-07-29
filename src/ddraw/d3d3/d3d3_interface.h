#pragma once

#include "../ddraw_include.h"
#include "../ddraw_child_object.h"
#include "../ddraw_options.h"
#include "../ddraw_util.h"
#include "../ddraw_format.h"

#include "../ddraw_common_interface.h"
#include "../d3d_common_interface.h"

#include "../../d3d9/d3d9_bridge.h"

namespace dxvk {

  /**
  * \brief D3D3 interface implementation
  */
  class D3D3Interface final : public DDrawChildObject<IUnknown, IDirect3D> {

  public:
    D3D3Interface(
          D3DCommonInterface* commonD3DIntf,
          DDrawCommonInterface* commonIntf,
          IUnknown* pParent);

    ~D3D3Interface();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Initialize(REFIID riid);

    HRESULT STDMETHODCALLTYPE EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg);

    HRESULT STDMETHODCALLTYPE CreateLight(LPDIRECT3DLIGHT *lplpDirect3DLight, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE CreateMaterial(LPDIRECT3DMATERIAL *lplpDirect3DMaterial, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE CreateViewport(LPDIRECT3DVIEWPORT *lplpD3DViewport, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR);

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf;
    }

    D3DCommonInterface* GetCommonD3DInterface() const {
      return m_commonD3DIntf.ptr();
    }

  private:

    Com<IDxvkLegacyD3DInterfaceBridge> m_bridge;

    Com<D3DCommonInterface>            m_commonD3DIntf;

    DDrawCommonInterface*              m_commonIntf = nullptr;

    Com<D3D6Interface, false>          m_d3d6Intf;
    Com<D3D5Interface, false>          m_d3d5Intf;

  };

}