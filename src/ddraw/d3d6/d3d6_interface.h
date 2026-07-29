#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_options.h"
#include "../ddraw_util.h"
#include "../ddraw_format.h"

#include "../ddraw_common_interface.h"
#include "../d3d_common_interface.h"

#include "../../d3d9/d3d9_bridge.h"

namespace dxvk {

  /**
  * \brief D3D6 interface implementation
  */
  class D3D6Interface final : public DDrawWrappedObject<IUnknown, IDirect3D3> {

  public:
    D3D6Interface(
          D3DCommonInterface* commonD3DIntf,
          DDrawCommonInterface* commonIntf,
          Com<IDirect3D3>&& d3d6Intf,
          IUnknown* pParent);

    ~D3D6Interface();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg);

    HRESULT STDMETHODCALLTYPE CreateLight(LPDIRECT3DLIGHT *lplpDirect3DLight, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE CreateMaterial(LPDIRECT3DMATERIAL3 *lplpDirect3DMaterial, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE CreateViewport(LPDIRECT3DVIEWPORT3 *lplpD3DViewport, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR);

    HRESULT STDMETHODCALLTYPE CreateDevice(REFCLSID rclsid, LPDIRECTDRAWSURFACE4 lpDDS, LPDIRECT3DDEVICE3 *lplpD3DDevice, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(LPD3DVERTEXBUFFERDESC lpVBDesc, LPDIRECT3DVERTEXBUFFER *lpD3DVertexBuffer, DWORD dwFlags, IUnknown *pUnkOuter);

    HRESULT STDMETHODCALLTYPE EnumZBufferFormats(REFCLSID riidDevice, LPD3DENUMPIXELFORMATSCALLBACK lpEnumCallback, LPVOID lpContext);

    HRESULT STDMETHODCALLTYPE EvictManagedTextures();

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf;
    }

    D3DCommonInterface* GetCommonD3DInterface() const {
      return m_commonD3DIntf.ptr();
    }

  private:

    inline DWORD DetermineBackBufferCount(IDirectDrawSurface4* renderTarget);

    Com<IDxvkLegacyD3DInterfaceBridge> m_bridge;

    Com<D3DCommonInterface>            m_commonD3DIntf;

    DDrawCommonInterface*              m_commonIntf = nullptr;

    Com<D3D5Interface, false>          m_d3d5Intf;
    Com<D3D3Interface, false>          m_d3d3Intf;

  };

}