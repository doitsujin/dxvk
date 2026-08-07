#pragma once

#include "../ddraw_include.h"
#include "../ddraw_child_object.h"
#include "../ddraw_options.h"

#include "../d3d_common_device.h"

#include "../d3d_multithread.h"

#include "../../d3d9/d3d9_bridge.h"

#include "d3d3_interface.h"
#include "d3d3_viewport.h"

#include <vector>
#include <unordered_map>

namespace dxvk {

  class D3DCommonDevice;
  class DDrawCommonInterface;
  class DDrawSurface;

  /**
  * \brief D3D3 device implementation
  */
  class D3D3Device final : public DDrawChildObject<DDrawSurface, IDirect3DDevice> {

  public:
    D3D3Device(
          D3DCommonDevice* commonD3DDevice,
          DDrawSurface* pParent,
          GUID deviceGUID,
          const d3d9::D3DPRESENT_PARAMETERS* pParams9,
          Com<d3d9::IDirect3DDevice9>&& pDevice9,
          DWORD CreationFlags9);

    ~D3D3Device();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetCaps(D3DDEVICEDESC *hal_desc, D3DDEVICEDESC *hel_desc);

    HRESULT STDMETHODCALLTYPE SwapTextureHandles(IDirect3DTexture *tex1, IDirect3DTexture *tex2);

    HRESULT STDMETHODCALLTYPE GetStats(D3DSTATS *stats);

    HRESULT STDMETHODCALLTYPE AddViewport(IDirect3DViewport *viewport);

    HRESULT STDMETHODCALLTYPE DeleteViewport(IDirect3DViewport *viewport);

    HRESULT STDMETHODCALLTYPE NextViewport(IDirect3DViewport *lpDirect3DViewport, IDirect3DViewport **lplpAnotherViewport, DWORD flags);

    HRESULT STDMETHODCALLTYPE EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK cb, void *ctx);

    HRESULT STDMETHODCALLTYPE BeginScene();

    HRESULT STDMETHODCALLTYPE EndScene();

    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D **d3d);

    HRESULT STDMETHODCALLTYPE Initialize(IDirect3D *d3d, GUID *lpGUID, D3DDEVICEDESC *desc);

    HRESULT STDMETHODCALLTYPE CreateExecuteBuffer(D3DEXECUTEBUFFERDESC *desc, IDirect3DExecuteBuffer **buffer, IUnknown *pkOuter);

    HRESULT STDMETHODCALLTYPE Execute(IDirect3DExecuteBuffer *buffer, IDirect3DViewport *viewport, DWORD flags);

    HRESULT STDMETHODCALLTYPE Pick(IDirect3DExecuteBuffer *buffer, IDirect3DViewport *viewport, DWORD flags, D3DRECT *rect);

    HRESULT STDMETHODCALLTYPE GetPickRecords(DWORD *count, D3DPICKRECORD *records);

    HRESULT STDMETHODCALLTYPE CreateMatrix(D3DMATRIXHANDLE *matrix);

    HRESULT STDMETHODCALLTYPE SetMatrix(D3DMATRIXHANDLE handle, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE GetMatrix(D3DMATRIXHANDLE handle, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE DeleteMatrix(D3DMATRIXHANDLE D3DMatHandle);

    void InitializeDS();

    void UpdateSurfaceDirtyTracking(bool dirtyRenderTarget, bool dirtyDepthStencil, bool dirtyPrimarySurface);

    D3DCommonDevice* GetCommonD3DDevice() {
      return m_commonD3DDevice.ptr();
    }

    D3DDeviceLock LockDevice() {
      return m_multithread.AcquireLock();
    }

    DDrawSurface* GetRenderTarget() const {
      return m_rt.ptr();
    }

    DDrawSurface* GetDepthStencil() const {
      return m_ds.ptr();
    }

    D3D3Viewport* GetCurrentViewportInternal() const {
      return m_currentViewport.ptr();
    }

  private:

    inline void DDrawDirtySurfaceUpload();

    inline HRESULT SetTextureInternal(DDrawSurface* surface, DWORD textureHandle);

    inline HRESULT SetRenderStateInternal(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState);

    inline HRESULT SetLightStateInternal(D3DLIGHTSTATETYPE dwLightStateType, DWORD dwLightState);

    inline void DrawTriangleInternal(D3DTRIANGLE* triangle, uint16_t count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer);

    inline void DrawLineInternal(D3DLINE* line, uint16_t count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer);

    inline void DrawPointInternal(D3DPOINT* point, uint16_t count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer);

    inline void DrawSpanInternal(D3DSPAN* span, uint16_t count, DWORD vertexCount, const D3DTLVERTEX* vertexBuffer);

    inline void TextureLoadInternal(D3DTEXTURELOAD* textureLoad, uint16_t count);

    inline void RefreshLastUsedDevice() {
      if (unlikely(m_commonIntf->GetCommonD3DDevice() != m_commonD3DDevice.ptr()))
        m_commonIntf->SetCommonD3DDevice(m_commonD3DDevice.ptr());
    }

    Com<D3DCommonDevice>            m_commonD3DDevice;

    DDrawCommonInterface*           m_commonIntf       = nullptr;

    Com<IDxvkLegacyD3DDeviceBridge> m_bridge;

    D3DMultithread                  m_multithread;

    D3DDEVICEDESC3                  m_desc;

    Com<DDrawSurface>               m_rt;
    Com<DDrawSurface, false>        m_ds;

    Com<D3D3Viewport>               m_currentViewport;
    std::vector<Com<D3D3Viewport>>  m_viewports;

    D3DSTATS                        m_stats            = { };

    D3DMATRIXHANDLE                 m_worldHandle      = 0;
    D3DMATRIXHANDLE                 m_viewHandle       = 0;
    D3DMATRIXHANDLE                 m_projectionHandle = 0;

    std::atomic<D3DMATRIXHANDLE>    m_matrixHandle     = 0;
    std::unordered_map<D3DMATRIXHANDLE, D3DMATRIX> m_matrices;

  };

}