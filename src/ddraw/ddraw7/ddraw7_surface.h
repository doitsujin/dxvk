#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../ddraw_common_interface.h"
#include "../ddraw_common_surface.h"

#include "ddraw7_interface.h"

#include "../d3d3/d3d3_texture.h"
#include "../d3d5/d3d5_texture.h"

#include <array>
#include <unordered_map>

namespace dxvk {

  /**
  * \brief IDirectDrawSurface7 interface implementation
  */
  class DDraw7Surface final : public DDrawWrappedObject<DDraw7Interface, IDirectDrawSurface7> {

  public:

    DDraw7Surface(
          DDrawCommonSurface* commonSurf,
          Com<IDirectDrawSurface7>&& surfProxy,
          DDraw7Interface* pParent,
          DDraw7Surface* pParentSurf,
          bool isChildObject);

    ~DDraw7Surface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE AddAttachedSurface(LPDIRECTDRAWSURFACE7 lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE AddOverlayDirtyRect(LPRECT lpRect);

    HRESULT STDMETHODCALLTYPE Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx);

    HRESULT STDMETHODCALLTYPE BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans);

    HRESULT STDMETHODCALLTYPE DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE7 lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpEnumSurfacesCallback);

    HRESULT STDMETHODCALLTYPE EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpfnCallback);

    HRESULT STDMETHODCALLTYPE Flip(LPDIRECTDRAWSURFACE7 lpDDSurfaceTargetOverride, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE7 *lplpDDAttachedSurface);

    HRESULT STDMETHODCALLTYPE GetBltStatus(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetCaps(LPDDSCAPS2 lpDDSCaps);

    HRESULT STDMETHODCALLTYPE GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper);

    HRESULT STDMETHODCALLTYPE GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey);

    HRESULT STDMETHODCALLTYPE GetDC(HDC *lphDC);

    HRESULT STDMETHODCALLTYPE GetFlipStatus(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetOverlayPosition(LPLONG lplX, LPLONG lplY);

    HRESULT STDMETHODCALLTYPE GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette);

    HRESULT STDMETHODCALLTYPE GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat);

    HRESULT STDMETHODCALLTYPE GetSurfaceDesc(LPDDSURFACEDESC2 lpDDSurfaceDesc);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC2 lpDDSurfaceDesc);

    HRESULT STDMETHODCALLTYPE IsLost();

    HRESULT STDMETHODCALLTYPE Lock(LPRECT lpDestRect, LPDDSURFACEDESC2 lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent);

    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC hDC);

    HRESULT STDMETHODCALLTYPE Restore();

    HRESULT STDMETHODCALLTYPE SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper);

    HRESULT STDMETHODCALLTYPE SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey);

    HRESULT STDMETHODCALLTYPE SetOverlayPosition(LONG lX, LONG lY);

    HRESULT STDMETHODCALLTYPE SetPalette(LPDIRECTDRAWPALETTE lpDDPalette);

    HRESULT STDMETHODCALLTYPE Unlock(LPRECT lpSurfaceData);

    HRESULT STDMETHODCALLTYPE UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE7 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx);

    HRESULT STDMETHODCALLTYPE UpdateOverlayDisplay(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE7 lpDDSReference);

    HRESULT STDMETHODCALLTYPE GetDDInterface(LPVOID *lplpDD);

    HRESULT STDMETHODCALLTYPE PageLock(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE PageUnlock(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetSurfaceDesc(LPDDSURFACEDESC2 lpDDSD, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID tag, LPVOID pData, DWORD cbSize, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID tag, LPVOID pBuffer, LPDWORD pcbBufferSize);

    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID tag);

    HRESULT STDMETHODCALLTYPE GetUniquenessValue(LPDWORD pValue);

    HRESULT STDMETHODCALLTYPE ChangeUniquenessValue();

    HRESULT STDMETHODCALLTYPE SetPriority(DWORD prio);

    HRESULT STDMETHODCALLTYPE GetPriority(LPDWORD prio);

    HRESULT STDMETHODCALLTYPE SetLOD(DWORD lod);

    HRESULT STDMETHODCALLTYPE GetLOD(LPDWORD lod);

    IDirectDrawSurface7* GetShadowOrProxied();

    HRESULT InitializeD3D9RenderTarget();

    HRESULT InitializeD3D9DepthStencil();

    HRESULT InitializeOrUploadD3D9();

    void DownloadSurfaceData();

    void SetShadowSurface(Com<DDraw7Surface>&& shadowSurf) {
      m_shadowSurf = shadowSurf;
    }

    DDraw7Surface* GetShadowSurface() const {
      return m_shadowSurf.ptr();
    }

    DDrawCommonSurface* GetCommonSurface() const {
      return m_commonSurf.ptr();
    }

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf;
    }

    DDraw7Surface* GetNextFlippable() const {
      return m_nextFlippable;
    }

    void SetAttachedDepthStencil(Com<DDraw7Surface>&& depthStencil) {
      m_depthStencil = depthStencil;
    }

    DDraw7Surface* GetAttachedDepthStencil() {
      // Fast path, since in most cases we already store the required surface
      if (likely(m_depthStencil.ptr() != nullptr))
        return m_depthStencil.ptr();

      DDSCAPS2 caps2;
      caps2.dwCaps = DDSCAPS_ZBUFFER;
      IDirectDrawSurface7* surface = nullptr;
      HRESULT hr = GetAttachedSurface(&caps2, &surface);
      if (unlikely(FAILED(hr)))
        return nullptr;

      m_depthStencil = reinterpret_cast<DDraw7Surface*>(surface);

      return m_depthStencil.ptr();
    }

    void SetParentSurface(DDraw7Surface* surface) {
      m_parentSurf = surface;

      if (m_parentSurf != nullptr)
        m_commonSurf->SetIsAttached(true);
      else
        m_commonSurf->SetIsAttached(false);
    }

    DDraw7Surface* GetParentSurface() const {
      return m_parentSurf;
    }

  private:

    inline void UpdateMipMapCount();

    inline void InitializeAndAttachCubeFace(
        IDirectDrawSurface7* surf,
        d3d9::IDirect3DCubeTexture9* cubeTex9,
        d3d9::D3DCUBEMAP_FACES face);

    inline void InitializeAllCubeMapSurfaces();

    inline HRESULT UploadSurfaceData();

    bool                                m_isChildObject   = false;

    Com<DDrawCommonSurface>             m_commonSurf;
    DDrawCommonInterface*               m_commonIntf      = nullptr;

    DDraw7Surface*                      m_parentSurf      = nullptr;

    std::array<IDirectDrawSurface7*, 6> m_cubeMapSurfaces;

    DDraw7Surface*                      m_nextFlippable   = nullptr;

    // Offscreen plain surface we use to mask unwanted DDraw interactions, such
    // as forced swapchain presents caused by blits/locks on primary surfaces
    Com<DDraw7Surface>                  m_shadowSurf;

    // Back buffers will have depth stencil surfaces as attachments (in practice
    // I have never seen more than one depth stencil being attached at a time)
    Com<DDraw7Surface>                  m_depthStencil;

    // These are attached surfaces, which are typically mips or other types of generated
    // surfaces, which need to exist for the entire lifecycle of their parent surface.
    // They are implemented with linked list, so for example only one mip level
    // will be held in a parent texture, and the next mip level will be held in the previous mip.
    std::unordered_map<IDirectDrawSurface7*, Com<DDraw7Surface, false>> m_attachedSurfaces;

    uint32_t                            m_surfCount       = 0;
    static std::atomic<uint32_t>        s_surfCount;

  };

}
