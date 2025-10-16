#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../d3d_common_texture.h"

namespace dxvk {

  class DDrawCommonInterface;
  class DDrawCommonSurface;
  class D3DCommonTexture;

  class D3D5Texture final : public DDrawWrappedObject<IUnknown, IDirect3DTexture2> {

  public:

    // D3D5Texture (aka IDirect3DTexture2) is shared between D3D5 and D3D6
    D3D5Texture(
          D3DCommonTexture* commonTex,
          DDrawCommonSurface* commonSurf,
          Com<IDirect3DTexture2>&& proxyTexture,
          // This can be either an IDirectDrawSurface or an IDirectDrawSurface4
          IUnknown* pParent,
          bool isD3D6Texture);

    ~D3D5Texture();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetHandle(LPDIRECT3DDEVICE2 lpDirect3DDevice2, LPD3DTEXTUREHANDLE lpHandle);

    HRESULT STDMETHODCALLTYPE PaletteChanged(DWORD dwStart, DWORD dwCount);

    HRESULT STDMETHODCALLTYPE Load(LPDIRECT3DTEXTURE2 lpD3DTexture2);

    D3DCommonTexture* GetCommonTexture() const {
      return m_commonTex.ptr();
    }

  private:

    Com<D3DCommonTexture> m_commonTex;

    DDrawCommonInterface* m_commonIntf = nullptr;

    const char*           m_objectType = "D3D5Texture";

    uint32_t              m_texCount   = 0;
    static std::atomic<uint32_t> s_texCount;

  };

}
