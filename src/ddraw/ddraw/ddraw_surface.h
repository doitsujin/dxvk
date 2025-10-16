#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"
#include "../ddraw_util.h"

#include "../ddraw_common_surface.h"

#include "ddraw_interface.h"

#include "../ddraw4/ddraw4_surface.h"

#include "../d3d3/d3d3_texture.h"
#include "../d3d5/d3d5_texture.h"

#include <unordered_map>

namespace dxvk {

  /**
  * \brief IDirectDrawSurface interface implementation
  */
  class DDrawSurface final : public DDrawWrappedObject<DDrawInterface, IDirectDrawSurface> {

  public:

    DDrawSurface(
          DDrawCommonSurface* commonSurf,
          Com<IDirectDrawSurface>&& surfProxy,
          DDrawInterface* pParent,
          DDrawSurface* pParentSurf,
          bool isChildObject);

    ~DDrawSurface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE AddOverlayDirtyRect(LPRECT lpRect);

    HRESULT STDMETHODCALLTYPE Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx);

    HRESULT STDMETHODCALLTYPE BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans);

    HRESULT STDMETHODCALLTYPE DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback);

    HRESULT STDMETHODCALLTYPE EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpfnCallback);

    HRESULT STDMETHODCALLTYPE Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE *lplpDDAttachedSurface);

    HRESULT STDMETHODCALLTYPE GetBltStatus(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetCaps(LPDDSCAPS lpDDSCaps);

    HRESULT STDMETHODCALLTYPE GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper);

    HRESULT STDMETHODCALLTYPE GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey);

    HRESULT STDMETHODCALLTYPE GetDC(HDC *lphDC);

    HRESULT STDMETHODCALLTYPE GetFlipStatus(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetOverlayPosition(LPLONG lplX, LPLONG lplY);

    HRESULT STDMETHODCALLTYPE GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette);

    HRESULT STDMETHODCALLTYPE GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat);

    HRESULT STDMETHODCALLTYPE GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc);

    HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc);

    HRESULT STDMETHODCALLTYPE IsLost();

    HRESULT STDMETHODCALLTYPE Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent);

    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC hDC);

    HRESULT STDMETHODCALLTYPE Restore();

    HRESULT STDMETHODCALLTYPE SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper);

    HRESULT STDMETHODCALLTYPE SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey);

    HRESULT STDMETHODCALLTYPE SetOverlayPosition(LONG lX, LONG lY);

    HRESULT STDMETHODCALLTYPE SetPalette(LPDIRECTDRAWPALETTE lpDDPalette);

    HRESULT STDMETHODCALLTYPE Unlock(LPVOID lpSurfaceData);

    HRESULT STDMETHODCALLTYPE UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx);

    HRESULT STDMETHODCALLTYPE UpdateOverlayDisplay(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSReference);

    IDirectDrawSurface* GetShadowOrProxied();

    HRESULT InitializeD3D9RenderTarget();

    HRESULT InitializeD3D9DepthStencil();

    HRESULT InitializeOrUploadD3D9();

    void DownloadSurfaceData();

    void UpdateMipMapCount();

    void SetShadowSurface(Com<DDrawSurface>&& shadowSurf) {
      m_shadowSurf = shadowSurf;
    }

    DDrawSurface* GetShadowSurface() const {
      return m_shadowSurf.ptr();
    }

    DDrawCommonSurface* GetCommonSurface() const {
      return m_commonSurf.ptr();
    }

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf;
    }

    void SetNextFlippable(DDrawSurface* nextFlippable) {
      m_nextFlippable = nextFlippable;
    }

    DDrawSurface* GetNextFlippable() const {
      return m_nextFlippable;
    }

    D3D3Texture* GetD3D3Texture() const {
      return m_texture3.ptr();
    }

    void SetD3D3Texture(D3D3Texture* texture3) {
      m_texture3 = texture3;
    }

    D3D5Texture* GetD3D5Texture() const {
      return m_texture5.ptr();
    }

    void SetD3D5Texture(D3D5Texture* texture5) {
      m_texture5 = texture5;
    }

    void SetAttachedDepthStencil(Com<DDrawSurface>&& depthStencil) {
      m_depthStencil = depthStencil;
    }

    DDrawSurface* GetAttachedDepthStencil() {
      // Fast path, since in most cases we already store the required surface
      if (likely(m_depthStencil.ptr() != nullptr))
        return m_depthStencil.ptr();

      DDSCAPS caps;
      caps.dwCaps = DDSCAPS_ZBUFFER;
      IDirectDrawSurface* surface = nullptr;
      HRESULT hr = GetAttachedSurface(&caps, &surface);
      if (unlikely(FAILED(hr)))
        return nullptr;

      m_depthStencil = reinterpret_cast<DDrawSurface*>(surface);

      return m_depthStencil.ptr();
    }

    void SetParentSurface(DDrawSurface* surface) {
      m_parentSurf = surface;

      if (m_parentSurf != nullptr)
        m_commonSurf->SetIsAttached(true);
      else
        m_commonSurf->SetIsAttached(false);
    }

    DDrawSurface* GetParentSurface() const {
      return m_parentSurf;
    }

    void SetParentSurface4(DDraw4Surface* surface4) {
      m_surface4 = surface4;
    }

  private:

    inline HRESULT UploadSurfaceData();

    inline HRESULT CreateDeviceInternal(REFIID riid, void** ppvObject);

    inline DWORD DetermineBackBufferCount(IDirectDrawSurface* renderTarget);

    bool                      m_isChildObject = true;

    Com<DDrawCommonSurface>   m_commonSurf;
    DDrawCommonInterface*     m_commonIntf    = nullptr;

    DDrawSurface*             m_parentSurf    = nullptr;

    Com<D3D3Texture, false>   m_texture3;
    Com<D3D5Texture, false>   m_texture5;

    Com<DDraw4Surface, false> m_surface4;

    DDrawSurface*             m_nextFlippable = nullptr;

    // Offscreen plain surface we use to mask unwanted DDraw interactions, such
    // as forced swapchain presents caused by blits/locks on primary surfaces
    Com<DDrawSurface>         m_shadowSurf;

    // Back buffers will have depth stencil surfaces as attachments (in practice
    // I have never seen more than one depth stencil being attached at a time)
    Com<DDrawSurface>         m_depthStencil;

    // These are attached surfaces, which are typically mips or other types of generated
    // surfaces, which need to exist for the entire lifecycle of their parent surface.
    // They are implemented with linked list, so for example only one mip level
    // will be held in a parent texture, and the next mip level will be held in the previous mip.
    std::unordered_map<IDirectDrawSurface*, Com<DDrawSurface, false>> m_attachedSurfaces;

    uint32_t                  m_surfCount     = 0;
    static std::atomic<uint32_t> s_surfCount;

  };

}
