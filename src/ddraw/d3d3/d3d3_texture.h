#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../d3d_common_texture.h"

namespace dxvk {

  class DDrawCommonInterface;
  class DDrawCommonSurface;
  class D3DCommonTexture;

  class D3D3Texture final : public DDrawWrappedObject<IUnknown, IDirect3DTexture> {

  public:

    D3D3Texture(
          D3DCommonTexture* commonTex,
          DDrawCommonSurface* commonSurf,
          Com<IDirect3DTexture>&& proxyTexture,
          IUnknown* pParent);

    ~D3D3Texture();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetHandle(LPDIRECT3DDEVICE lpDirect3DDevice, LPD3DTEXTUREHANDLE lpHandle);

    HRESULT STDMETHODCALLTYPE PaletteChanged(DWORD dwStart, DWORD dwCount);

    HRESULT STDMETHODCALLTYPE Load(LPDIRECT3DTEXTURE lpD3DTexture);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECT3DDEVICE lpDirect3DDevice, LPDIRECTDRAWSURFACE lpDDSurface);

    HRESULT STDMETHODCALLTYPE Unload();

    D3DCommonTexture* GetCommonTexture() const {
      return m_commonTex.ptr();
    }

  private:

    Com<D3DCommonTexture> m_commonTex;

    DDrawCommonInterface* m_commonIntf = nullptr;

    uint32_t              m_texCount   = 0;
    static std::atomic<uint32_t> s_texCount;

  };

}
