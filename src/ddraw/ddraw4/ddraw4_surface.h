#pragma once

#include "../ddraw_include.h"
#include "../ddraw_wrapped_object.h"

#include "../ddraw_common_interface.h"
#include "../ddraw_common_surface.h"

#include "ddraw4_interface.h"

#include "../d3d3/d3d3_texture.h"
#include "../d3d5/d3d5_texture.h"

#include <unordered_map>

namespace dxvk {

  /**
  * \brief IDirectDrawSurface4 interface implementation
  */
  class DDraw4Surface final : public DDrawWrappedObject<DDraw4Interface, IDirectDrawSurface4> {

  public:

    DDraw4Surface(
          DDrawCommonSurface* commonSurf,
          Com<IDirectDrawSurface4>&& surfProxy,
          DDraw4Interface* pParent,
          DDraw4Surface* pParentSurf,
          bool isChildObject);

    ~DDraw4Surface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE AddAttachedSurface(LPDIRECTDRAWSURFACE4 lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE AddOverlayDirtyRect(LPRECT lpRect);

    HRESULT STDMETHODCALLTYPE Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx);

    HRESULT STDMETHODCALLTYPE BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans);

    HRESULT STDMETHODCALLTYPE DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSAttachedSurface);

    HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpEnumSurfacesCallback);

    HRESULT STDMETHODCALLTYPE EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpfnCallback);

    HRESULT STDMETHODCALLTYPE Flip(LPDIRECTDRAWSURFACE4 lpDDSurfaceTargetOverride, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE4 *lplpDDAttachedSurface);

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

    HRESULT STDMETHODCALLTYPE UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE4 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx);

    HRESULT STDMETHODCALLTYPE UpdateOverlayDisplay(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSReference);

    HRESULT STDMETHODCALLTYPE GetDDInterface(LPVOID *lplpDD);

    HRESULT STDMETHODCALLTYPE PageLock(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE PageUnlock(DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetSurfaceDesc(LPDDSURFACEDESC2 lpDDSD, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID tag, LPVOID pData, DWORD cbSize, DWORD dwFlags);

    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID tag, LPVOID pBuffer, LPDWORD pcbBufferSize);

    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID tag);

    HRESULT STDMETHODCALLTYPE GetUniquenessValue(LPDWORD pValue);

    HRESULT STDMETHODCALLTYPE ChangeUniquenessValue();

    IDirectDrawSurface4* GetShadowOrProxied();

    HRESULT InitializeD3D9RenderTarget();

    HRESULT InitializeD3D9DepthStencil();

    HRESULT InitializeOrUploadD3D9();

    void DownloadSurfaceData();

    void SetShadowSurface(Com<DDraw4Surface>&& shadowSurf) {
      m_shadowSurf = shadowSurf;
    }

    DDraw4Surface* GetShadowSurface() const {
      return m_shadowSurf.ptr();
    }

    DDrawCommonSurface* GetCommonSurface() const {
      return m_commonSurf.ptr();
    }

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf;
    }

    DDraw4Surface* GetNextFlippable() const {
      return m_nextFlippable;
    }

    void SetAttachedDepthStencil(Com<DDraw4Surface>&& depthStencil) {
      m_depthStencil = depthStencil;
    }

    DDraw4Surface* GetAttachedDepthStencil() {
      // Fast path, since in most cases we already store the required surface
      if (likely(m_depthStencil.ptr() != nullptr))
        return m_depthStencil.ptr();

      DDSCAPS2 caps2;
      caps2.dwCaps = DDSCAPS_ZBUFFER;
      IDirectDrawSurface4* surface = nullptr;
      HRESULT hr = GetAttachedSurface(&caps2, &surface);
      if (unlikely(FAILED(hr)))
        return nullptr;

      m_depthStencil = reinterpret_cast<DDraw4Surface*>(surface);

      return m_depthStencil.ptr();
    }

    void SetParentSurface(DDraw4Surface* surface) {
      m_parentSurf = surface;

      if (m_parentSurf != nullptr)
        m_commonSurf->SetIsAttached(true);
      else
        m_commonSurf->SetIsAttached(false);
    }

    DDraw4Surface* GetParentSurface() const {
      return m_parentSurf;
    }

  private:

    inline void UpdateMipMapCount();

    inline HRESULT UploadSurfaceData();

    bool                    m_isChildObject = true;

    Com<DDrawCommonSurface> m_commonSurf;
    DDrawCommonInterface*   m_commonIntf    = nullptr;

    DDraw4Surface*          m_parentSurf    = nullptr;

    Com<D3D3Texture, false> m_texture3;
    // D3D5Texture (aka IDirect3DTexture2) is shared between D3D5 and D3D6
    Com<D3D5Texture, false> m_texture6;

    DDraw4Surface*          m_nextFlippable = nullptr;

    // Offscreen plain surface we use to mask unwanted DDraw interactions, such
    // as forced swapchain presents caused by blits/locks on primary surfaces
    Com<DDraw4Surface>      m_shadowSurf;

    // Back buffers will have depth stencil surfaces as attachments (in practice
    // I have never seen more than one depth stencil being attached at a time)
    Com<DDraw4Surface>      m_depthStencil;

    // These are attached surfaces, which are typically mips or other types of generated
    // surfaces, which need to exist for the entire lifecycle of their parent surface.
    // They are implemented with linked list, so for example only one mip level
    // will be held in a parent texture, and the next mip level will be held in the previous mip.
    std::unordered_map<IDirectDrawSurface4*, Com<DDraw4Surface, false>> m_attachedSurfaces;

    uint32_t                m_surfCount     = 0;
    static std::atomic<uint32_t> s_surfCount;

  };

}
