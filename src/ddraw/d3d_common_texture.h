#pragma once

#include "ddraw_include.h"
#include "ddraw_format.h"

#include "ddraw_common_interface.h"
#include "ddraw_common_surface.h"

namespace dxvk {

  class DDrawSurface;

  class D3D5Texture;
  class D3D3Texture;

  class D3DCommonTexture : public ComObjectClamp<IUnknown> {

  public:

    D3DCommonTexture(DDrawCommonSurface* commonSurf);

    ~D3DCommonTexture();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    DDrawCommonSurface* GetCommonSurface() const {
      return m_commonSurf;
    }

    void SetTextureHandle(D3DTEXTUREHANDLE handle) {
      m_textureHandle = handle;
    }

    D3DTEXTUREHANDLE GetTextureHandle() const {
      return m_textureHandle;
    }

    void SetD3D5Texture(D3D5Texture* texture5) {
      m_texture5 = texture5;
    }

    D3D5Texture* GetD3D5Texture() const {
      return m_texture5;
    }

    void SetD3D3Texture(D3D3Texture* texture3) {
      m_texture3 = texture3;
    }

    D3D3Texture* GetD3D3Texture() const {
      return m_texture3;
    }

    DDrawSurface* GetDDSurface() const {
      return m_commonSurf->GetDDSurface();
    }

    DDraw4Surface* GetDD4Surface() const {
      return m_commonSurf->GetDD4Surface();
    }

  private:

    DDrawCommonSurface* m_commonSurf    = nullptr;

    D3DTEXTUREHANDLE    m_textureHandle = 0;

    D3D5Texture*        m_texture5      = nullptr;
    D3D3Texture*        m_texture3      = nullptr;

  };

}