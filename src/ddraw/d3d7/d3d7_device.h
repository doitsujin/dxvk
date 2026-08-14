#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_options.h"
#include "../ddraw_util.h"
#include "../ddraw_caps.h"

#include "../d3d_common_device.h"
#include "../d3d_light.h"

#include "../d3d_multithread.h"

#include "../../d3d9/d3d9_bridge.h"

#include "d3d7_interface.h"

#include <array>
#include <unordered_map>

namespace dxvk {

  class D3DCommonDevice;
  class DDrawCommonInterface;
  class DDraw7Surface;
  class D3D7StateBlock;

  /**
  * \brief D3D7 device implementation
  */
  class D3D7Device final : public DDrawWrappedObject<D3D7Interface, IDirect3DDevice7> {

  friend class D3D7StateBlock;

  public:
    D3D7Device(
          D3DCommonDevice* commonD3DDevice,
          Com<IDirect3DDevice7>&& d3d7DeviceProxy,
          D3D7Interface* pParent,
          GUID deviceGUID,
          const d3d9::D3DPRESENT_PARAMETERS* pParams9,
          Com<d3d9::IDirect3DDevice9>&& pDevice9,
          DDraw7Surface* pRT,
          DWORD CreationFlags9);

    ~D3D7Device();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE GetCaps(D3DDEVICEDESC7 *desc);

    HRESULT STDMETHODCALLTYPE EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK cb, void *ctx);

    HRESULT STDMETHODCALLTYPE BeginScene();

    HRESULT STDMETHODCALLTYPE EndScene();

    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D7 **d3d);

    HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirectDrawSurface7 *surface, DWORD flags);

    HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirectDrawSurface7 **surface);

    HRESULT STDMETHODCALLTYPE Clear(DWORD count, D3DRECT *rects, DWORD flags, D3DCOLOR color, D3DVALUE z, DWORD stencil);

    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE SetViewport(D3DVIEWPORT7 *data);

    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix);

    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT7 *data);

    HRESULT STDMETHODCALLTYPE SetMaterial(D3DMATERIAL7 *data);

    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL7 *data);

    HRESULT STDMETHODCALLTYPE SetLight(DWORD idx, D3DLIGHT7 *data);

    HRESULT STDMETHODCALLTYPE GetLight(DWORD idx, D3DLIGHT7 *data);

    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState);

    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE dwRenderStateType, LPDWORD lpdwRenderState);

    HRESULT STDMETHODCALLTYPE BeginStateBlock();

    HRESULT STDMETHODCALLTYPE EndStateBlock(LPDWORD lpdwBlockHandle);

    HRESULT STDMETHODCALLTYPE PreLoad(IDirectDrawSurface7 *surface);

    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE primitive_type, DWORD fvf, void *pVertex, DWORD vertexCount, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE primitive_type, DWORD fvf, void *pVertex, DWORD vertexCount, WORD *pIndex, DWORD indexCount, DWORD flags);

    HRESULT STDMETHODCALLTYPE SetClipStatus(D3DCLIPSTATUS *clip_status);

    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS *clip_status);

    HRESULT STDMETHODCALLTYPE DrawPrimitiveStrided(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwVertexCount, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveStrided(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD  dwVertexCount, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE DrawPrimitiveVB(D3DPRIMITIVETYPE primitive_type, IDirect3DVertexBuffer7 *vb, DWORD startVertex, DWORD primitiveCount, DWORD flags);

    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE primitive_type, IDirect3DVertexBuffer7 *vb, DWORD startVertex, DWORD primitiveCount, WORD *pIndex, DWORD indexCount, DWORD flags);

    HRESULT STDMETHODCALLTYPE ComputeSphereVisibility(D3DVECTOR *lpCenters, D3DVALUE *lpRadii, DWORD dwNumSpheres, DWORD dwFlags, DWORD *lpdwReturnValues);

    HRESULT STDMETHODCALLTYPE GetTexture(DWORD stage, IDirectDrawSurface7 **surface);

    HRESULT STDMETHODCALLTYPE SetTexture(DWORD stage, IDirectDrawSurface7 *surface);

    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, LPDWORD lpdwState);

    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, DWORD dwState);

    HRESULT STDMETHODCALLTYPE ValidateDevice(LPDWORD lpdwPasses);

    HRESULT STDMETHODCALLTYPE ApplyStateBlock(DWORD dwBlockHandle);

    HRESULT STDMETHODCALLTYPE CaptureStateBlock(DWORD dwBlockHandle);

    HRESULT STDMETHODCALLTYPE DeleteStateBlock(DWORD dwBlockHandle);

    HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE d3dsbType, LPDWORD lpdwBlockHandle);

    HRESULT STDMETHODCALLTYPE Load(IDirectDrawSurface7 *dst_surface, POINT *dst_point, IDirectDrawSurface7 *src_surface, RECT *src_rect, DWORD flags);

    HRESULT STDMETHODCALLTYPE LightEnable(DWORD dwLightIndex, BOOL bEnable);

    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD dwLightIndex, BOOL *pbEnable);

    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD dwIndex, D3DVALUE *pPlaneEquation);

    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD dwIndex, D3DVALUE *pPlaneEquation);

    HRESULT STDMETHODCALLTYPE GetInfo(DWORD info_id, void *info, DWORD info_size);

    HRESULT InitializeRTAndDS();

    void UpdateSurfaceDirtyTracking(bool dirtyRenderTarget, bool dirtyDepthStencil, bool dirtyPrimarySurface);

    HRESULT ResetD3D9Swapchain(d3d9::D3DPRESENT_PARAMETERS* params);

    D3DCommonDevice* GetCommonD3DDevice() {
      return m_commonD3DDevice.ptr();
    }

    D3DDeviceLock LockDevice() {
      return m_multithread.AcquireLock();
    }

    DDraw7Surface* GetRenderTarget() const {
      return m_rt.ptr();
    }

    DDraw7Surface* GetDepthStencil() const {
      return m_ds.ptr();
    }

    void GetD3D9ActiveLights(std::vector<d3d9::D3DLIGHT9>* lights9) {
      for (const auto& [idx, light9] : m_lights) {
        if (m_lightsStates[idx])
          lights9->push_back(light9);
      }
    }

  private:

    inline void DDrawDirtySurfaceUpload();

    inline bool ShouldRecord() const { return m_recorder != nullptr; }

    inline void RefreshLastUsedDevice() {
      if (unlikely(m_commonIntf->GetCommonD3DDevice() != m_commonD3DDevice.ptr()))
        m_commonIntf->SetCommonD3DDevice(m_commonD3DDevice.ptr());
    }

    Com<D3DCommonDevice>            m_commonD3DDevice;

    DDrawCommonInterface*           m_commonIntf            = nullptr;

    Com<IDxvkLegacyD3DDeviceBridge> m_bridge;

    D3DMultithread                  m_multithread;

    D3DDEVICEDESC7                  m_desc;

    Com<DDraw7Surface>              m_rt;
    Com<DDraw7Surface, false>       m_ds;

    std::array<Com<DDraw7Surface, false>, ddrawCaps::TextureStageCount> m_textures;

    std::unordered_map<DWORD, d3d9::D3DLIGHT9> m_lights;
    std::unordered_map<DWORD, BOOL>            m_lightsStates;

    D3D7StateBlock*                 m_recorder              = nullptr;
    DWORD                           m_recorderHandle        = 0;
    DWORD                           m_handle                = 0;
    std::unordered_map<DWORD, D3D7StateBlock> m_stateBlocks;

    std::array<Com<d3d9::IDirect3DIndexBuffer9>, ddrawCaps::IndexBufferCount> m_ib9;

  };

}