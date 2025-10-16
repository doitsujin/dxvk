#pragma once

#include "../ddraw_include.h"
#include "../ddraw_child_object.h"
#include "../ddraw_util.h"

#include "../d3d_common_viewport.h"

#include "d3d6_interface.h"

namespace dxvk {

  class D3DLight;

  class DDrawSurface;
  class DDraw4Surface;

  class D3D5Viewport;
  class D3D3Viewport;

  class D3D6Viewport final : public DDrawChildObject<D3D6Interface, IDirect3DViewport3> {

  public:

    D3D6Viewport(
          D3DCommonViewport* commonViewport,
          D3D6Interface* pParent);

    ~D3D6Viewport();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE Initialize(IDirect3D *d3d);

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

    HRESULT STDMETHODCALLTYPE GetViewport2(D3DVIEWPORT2 *data);

    HRESULT STDMETHODCALLTYPE SetViewport2(D3DVIEWPORT2 *data);

    HRESULT STDMETHODCALLTYPE SetBackgroundDepth2(IDirectDrawSurface4 *surface);

    HRESULT STDMETHODCALLTYPE GetBackgroundDepth2(IDirectDrawSurface4 **surface, BOOL *valid);

    HRESULT STDMETHODCALLTYPE Clear2(DWORD count, D3DRECT *rects, DWORD flags, DWORD color, D3DVALUE z, DWORD stencil);

    HRESULT ApplyViewport();

    HRESULT ApplyAndActivateLights();

    HRESULT DeactivateLights();

    HRESULT ApplyAndActivateLight(DWORD index, D3DLight* light);

    D3DCommonViewport* GetCommonViewport() const {
      return m_commonViewport.ptr();
    }

  private:

    bool                     m_isBackgroundDepthSet  = false;
    bool                     m_isBackgroundDepth4Set = false;

    Com<D3DCommonViewport>   m_commonViewport;

    Com<DDrawSurface>        m_backgroundDepth;
    Com<DDraw4Surface>       m_backgroundDepth4;

    Com<D3D5Viewport, false> m_viewport5;
    Com<D3D3Viewport, false> m_viewport3;

    uint32_t                 m_viewportCount         = 0;
    static std::atomic<uint32_t> s_viewportCount;

  };

}
