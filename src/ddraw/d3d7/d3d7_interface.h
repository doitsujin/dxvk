#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_options.h"
#include "../ddraw_util.h"

#include "../ddraw_common_interface.h"
#include "../d3d_common_interface.h"

#include "../../d3d9/d3d9_bridge.h"

namespace dxvk {

  /**
  * \brief D3D7 interface implementation
  */
  class D3D7Interface final : public DDrawWrappedObject<IUnknown, IDirect3D7> {

  public:
    D3D7Interface(
          D3DCommonInterface* commonD3DIntf,
          DDrawCommonInterface* commonIntf,
          Com<IDirect3D7>&& d3d7Intf,
          IUnknown* pParent);

    ~D3D7Interface();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE EnumDevices(LPD3DENUMDEVICESCALLBACK7 cb, void *ctx);

    HRESULT STDMETHODCALLTYPE CreateDevice(REFCLSID rclsid, IDirectDrawSurface7 *surface, IDirect3DDevice7 **ppd3dDevice);

    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(D3DVERTEXBUFFERDESC *desc, IDirect3DVertexBuffer7 **ppVertexBuffer, DWORD usage);

    HRESULT STDMETHODCALLTYPE EnumZBufferFormats(REFCLSID riidDevice, LPD3DENUMPIXELFORMATSCALLBACK lpEnumCallback, LPVOID lpContext);

    HRESULT STDMETHODCALLTYPE EvictManagedTextures();

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf;
    }

    D3DCommonInterface* GetCommonD3DInterface() const {
      return m_commonD3DIntf.ptr();
    }

  private:

    inline DWORD DetermineBackBufferCount(IDirectDrawSurface7* renderTarget);

    Com<IDxvkLegacyD3DInterfaceBridge> m_bridge;

    Com<D3DCommonInterface>            m_commonD3DIntf;

    DDrawCommonInterface*              m_commonIntf = nullptr;

    uint32_t                           m_intfCount  = 0;
    static std::atomic<uint32_t>       s_intfCount;

  };

}