#pragma once

#include "../ddraw_include.h"
#include "../ddraw_child_object.h"
#include "../ddraw_options.h"
#include "../ddraw_util.h"
#include "../ddraw_caps.h"

#include "../d3d_common_device.h"

#include "../d3d_multithread.h"

#include "../../d3d9/d3d9_bridge.h"

#include "../d3d5/d3d5_texture.h"

#include "d3d6_interface.h"
#include "d3d6_viewport.h"

#include <array>
#include <vector>

namespace dxvk {

  class D3DCommonDevice;
  class DDrawCommonInterface;
  class DDraw4Surface;
  class DDraw4Interface;
  class D3D5Device;
  class D3D3Device;

  /**
  * \brief D3D6 device implementation
  */
  class D3D6Device final : public DDrawChildObject<D3D6Interface, IDirect3DDevice3> {

  public:
    D3D6Device(
          D3DCommonDevice* commonD3DDevice,
          D3D6Interface* pParent,
          GUID deviceGUID,
          const d3d9::D3DPRESENT_PARAMETERS* pParams9,
          Com<d3d9::IDirect3DDevice9>&& pDevice9,
          DDraw4Surface* pRT,
          DWORD CreationFlags9);

    ~D3D6Device();

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetCaps(D3DDEVICEDESC *hal_desc, D3DDEVICEDESC *hel_desc);

    HRESULT STDMETHODCALLTYPE GetStats(D3DSTATS *stats);

    HRESULT STDMETHODCALLTYPE AddViewport(IDirect3DViewport3 *viewport);

    HRESULT STDMETHODCALLTYPE DeleteViewport(IDirect3DViewport3 *viewport);

    HRESULT STDMETHODCALLTYPE NextViewport(IDirect3DViewport3 *lpDirect3DViewport, IDirect3DViewport3 **lplpAnotherViewport, DWORD flags);

    HRESULT STDMETHODCALLTYPE EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK cb, void *ctx);

    HRESULT STDMETHODCALLTYPE BeginScene();

    HRESULT STDMETHODCALLTYPE EndScene();

    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D3 **d3d);

    HRESULT STDMETHODCALLTYPE SetCurrentViewport(IDirect3DViewport3 *viewport);

    HRESULT STDMETHODCALLTYPE GetCurrentViewport(IDirect3DViewport3 **viewport);

    HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirectDrawSurface4 *surface, DWORD flags);

    HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirectDrawSurface4 **surface);

    HRESULT STDMETHODCALLTYPE Begin(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE BeginIndexed(D3DPRIMITIVETYPE primitive_type, DWORD fvf, void *vertices, DWORD vertex_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE Vertex(void *vertex);

    HRESULT STDMETHODCALLTYPE Index(WORD wVertexIndex);

    HRESULT STDMETHODCALLTYPE End(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE dwRenderStateType, LPDWORD lpdwRenderState);

    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState);

    HRESULT STDMETHODCALLTYPE GetLightState(D3DLIGHTSTATETYPE dwLightStateType, LPDWORD lpdwLightState);

    HRESULT STDMETHODCALLTYPE SetLightState(D3DLIGHTSTATETYPE dwLightStateType, DWORD dwLightState);

    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE primitive_type, DWORD vertex_type, void *vertices, DWORD vertex_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE primitive_type, DWORD fvf, void *vertices, DWORD vertex_count, WORD *indices, DWORD index_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE SetClipStatus(D3DCLIPSTATUS *clip_status);

    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS *clip_status);

    HRESULT STDMETHODCALLTYPE DrawPrimitiveStrided(D3DPRIMITIVETYPE primitive_type, DWORD fvf, D3DDRAWPRIMITIVESTRIDEDDATA *strided_data, DWORD vertex_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveStrided(D3DPRIMITIVETYPE primitive_type, DWORD fvf, D3DDRAWPRIMITIVESTRIDEDDATA *strided_data, DWORD vertex_count, WORD *indices, DWORD index_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawPrimitiveVB(D3DPRIMITIVETYPE primitive_type, IDirect3DVertexBuffer *vb, DWORD start_vertex, DWORD vertex_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE primitive_type, IDirect3DVertexBuffer *vb, WORD *indices, DWORD index_count, DWORD flags);

    HRESULT STDMETHODCALLTYPE ComputeSphereVisibility(D3DVECTOR *lpCenters, D3DVALUE *lpRadii, DWORD dwNumSpheres, DWORD dwFlags, DWORD *lpdwReturnValues);

    HRESULT STDMETHODCALLTYPE GetTexture(DWORD stage, IDirect3DTexture2 **texture);

    HRESULT STDMETHODCALLTYPE SetTexture(DWORD stage, IDirect3DTexture2 *texture);

    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, LPDWORD lpdwState);

    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, DWORD dwState);

    HRESULT STDMETHODCALLTYPE ValidateDevice(LPDWORD lpdwPasses);

    void InitializeDS();

    void UpdateSurfaceDirtyTracking(bool dirtyRenderTarget, bool dirtyDepthStencil, bool dirtyPrimarySurface);

    HRESULT ResetD3D9Swapchain(d3d9::D3DPRESENT_PARAMETERS* params);

    D3DCommonDevice* GetCommonD3DDevice() {
      return m_commonD3DDevice.ptr();
    }

    D3DDeviceLock LockDevice() {
      return m_multithread.AcquireLock();
    }

    DDraw4Surface* GetRenderTarget() const {
      return m_rt.ptr();
    }

    DDraw4Surface* GetDepthStencil() const {
      return m_ds.ptr();
    }

    D3D6Viewport* GetCurrentViewportInternal() const {
      return m_currentViewport.ptr();
    }

  private:

    inline HRESULT InitializeIndexBuffers();

    inline void UploadIndices(d3d9::IDirect3DIndexBuffer9* ib9, WORD* indices, DWORD indexCount);

    inline void DDrawDirtySurfaceUpload();

    inline void AddViewportInternal(IDirect3DViewport3* viewport);

    inline void DeleteViewportInternal(IDirect3DViewport3* viewport);

    inline HRESULT SetTextureWithHandle(DDraw4Surface* surface, DWORD textureHandle);

    inline void RefreshLastUsedDevice() {
      if (unlikely(m_commonIntf->GetCommonD3DDevice() != m_commonD3DDevice.ptr()))
        m_commonIntf->SetCommonD3DDevice(m_commonD3DDevice.ptr());
    }

    inline void HandlePreDrawLegacyProjection(d3d9::IDirect3DDevice9* device9, DWORD drawFlags) {
      if (likely(m_currentViewport != nullptr)) {
        m_legacyProjection = m_currentViewport->GetCommonViewport()->GetLegacyProjectionMatrix(drawFlags);

        if (m_legacyProjection != nullptr) {
          device9->GetTransform(d3d9::D3DTS_PROJECTION, &m_projectionMatrix);
          device9->MultiplyTransform(d3d9::D3DTS_PROJECTION, m_legacyProjection);
        }
      }
    }

    inline void HandlePostDrawLegacyProjection(d3d9::IDirect3DDevice9* device9) {
      if (m_legacyProjection != nullptr) {
        device9->SetTransform(d3d9::D3DTS_PROJECTION, &m_projectionMatrix);
      }
    }

    bool                            m_alphaOpSet         = false;

    Com<D3DCommonDevice>            m_commonD3DDevice;

    DDrawCommonInterface*           m_commonIntf         = nullptr;

    Com<IDxvkLegacyD3DDeviceBridge> m_bridge;

    Com<D3D5Device, false>          m_device5;
    Com<D3D3Device, false>          m_device3;

    D3DMultithread                  m_multithread;

    D3DDEVICEDESC                   m_desc;

    Com<DDraw4Surface>              m_rt;
    Com<DDraw4Surface, false>       m_ds;

    Com<D3D6Viewport>               m_currentViewport;
    std::vector<Com<D3D6Viewport>>  m_viewports;

    VertexStreamInfo                m_vertexStreamInfo;
    std::vector<D3DVERTEX>          m_vertexStream;
    std::vector<D3DLVERTEX>         m_lvertexStream;
    std::vector<D3DTLVERTEX>        m_tlvertexStream;

    // D3D5Texture (aka IDirect3DTexture2) is shared between D3D5 and D3D6
    std::array<Com<D3D5Texture, false>, ddrawCaps::TextureStageCount> m_textures;

    D3DMATRIX                       m_projectionMatrix   = { };
    const D3DMATRIX*                m_legacyProjection   = nullptr;

    std::array<Com<d3d9::IDirect3DIndexBuffer9>, ddrawCaps::IndexBufferCount> m_ib9;

    uint32_t                        m_deviceCount        = 0;
    static std::atomic<uint32_t>    s_deviceCount;

  };

}