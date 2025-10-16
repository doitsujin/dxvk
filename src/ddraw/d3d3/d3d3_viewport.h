#pragma once

#include "../ddraw_include.h"
#include "../ddraw_child_object.h"

#include "../d3d_common_viewport.h"

#include "d3d3_interface.h"

namespace dxvk {

  class D3DLight;

  class DDrawSurface;

  class D3D6Viewport;
  class D3D5Viewport;

  class D3D3Viewport final : public DDrawChildObject<D3D3Interface, IDirect3DViewport> {

  public:

    D3D3Viewport(
          D3DCommonViewport* commonViewport,
          D3D3Interface* pParent);

    ~D3D3Viewport();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECT3D lpDirect3D);

    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT *data);

    HRESULT STDMETHODCALLTYPE SetViewport(D3DVIEWPORT *data);

    HRESULT STDMETHODCALLTYPE TransformVertices(DWORD vertex_count, D3DTRANSFORMDATA *data, DWORD flags, DWORD *offscreen);

    HRESULT STDMETHODCALLTYPE LightElements(DWORD element_count, D3DLIGHTDATA *data);

    HRESULT STDMETHODCALLTYPE SetBackground(D3DMATERIALHANDLE hMat);

    HRESULT STDMETHODCALLTYPE GetBackground(D3DMATERIALHANDLE *material, BOOL *valid);

    HRESULT STDMETHODCALLTYPE SetBackgroundDepth(IDirectDrawSurface *surface);

    HRESULT STDMETHODCALLTYPE GetBackgroundDepth(IDirectDrawSurface **surface, BOOL *valid);

    HRESULT STDMETHODCALLTYPE Clear(DWORD count, D3DRECT *rects, DWORD flags);

    HRESULT STDMETHODCALLTYPE AddLight(IDirect3DLight *light);

    HRESULT STDMETHODCALLTYPE DeleteLight(IDirect3DLight *light);

    HRESULT STDMETHODCALLTYPE NextLight(IDirect3DLight *lpDirect3DLight, IDirect3DLight **lplpDirect3DLight, DWORD flags);

    HRESULT ApplyViewport();

    HRESULT ApplyAndActivateLights();

    HRESULT DeactivateLights();

    HRESULT ApplyAndActivateLight(DWORD index, D3DLight* light);

    D3DCommonViewport* GetCommonViewport() const {
      return m_commonViewport.ptr();
    }

  private:

    bool                     m_isBackgroundDepthSet = false;

    Com<D3DCommonViewport>   m_commonViewport;

    Com<DDrawSurface>        m_backgroundDepth;

    Com<D3D6Viewport, false> m_viewport6;
    Com<D3D5Viewport, false> m_viewport5;

    uint32_t                 m_viewportCount        = 0;
    static std::atomic<uint32_t> s_viewportCount;

  };

}
