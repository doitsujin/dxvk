#include "ddraw2_surface.h"

#include "../d3d_common_device.h"

#include "../ddraw_gamma.h"
#include "../ddraw2/ddraw3_surface.h"
#include "../ddraw4/ddraw4_surface.h"
#include "../ddraw7/ddraw7_surface.h"

namespace dxvk {

  DDraw2Surface::DDraw2Surface(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawSurface2>&& surfProxy,
        DDrawSurface* pParent,
        DDraw2Surface* pParentSurf)
    : DDrawWrappedObject<DDrawSurface, IDirectDrawSurface2>(pParent, std::move(surfProxy))
    , m_commonSurf ( commonSurf )
    , m_parentSurf ( pParentSurf ) {
    if (m_parent != nullptr) {
      m_commonIntf = m_parent->GetCommonInterface();
    } else if (m_parentSurf != nullptr) {
      m_commonIntf = m_parentSurf->GetCommonInterface();
    } else if (m_commonSurf != nullptr) {
      m_commonIntf = m_commonSurf->GetCommonInterface();
    } else {
      throw DxvkError("DDraw2Surface: ERROR! Failed to retrieve the common interface!");
    }

    if (m_commonSurf == nullptr)
      m_commonSurf = new DDrawCommonSurface(m_commonIntf);

    // Retrieve and cache the proxy surface desc
    if (unlikely(!m_commonSurf->IsDescSet())) {
      DDSURFACEDESC desc;
      desc.dwSize = sizeof(DDSURFACEDESC);
      HRESULT hr = m_proxy->GetSurfaceDesc(&desc);

      if (unlikely(FAILED(hr))) {
        throw DxvkError("DDraw2Surface: ERROR! Failed to retrieve new surface desc!");
      } else {
        m_commonSurf->SetDesc(desc);
      }
    }

    // We need to keep the IDirectDrawSurface parent around, since we're entirely dependent on it.
    // If one doesn't exist (e.g. attached surfaces), then use QueryInterface and cache it.
    if (m_parent == nullptr) {
      Com<IDirectDrawSurface> originSurf;
      HRESULT hr = m_proxy->QueryInterface(__uuidof(IDirectDrawSurface), reinterpret_cast<void**>(&originSurf));
      if (unlikely(FAILED(hr)))
        throw DxvkError("DDraw2Surface: ERROR! Failed to retrieve an IDirectDrawSurface interface!");

      try {
        m_originSurf = new DDrawSurface(m_commonSurf.ptr(), std::move(originSurf),
                                        m_commonIntf->GetDDInterface(), nullptr, false);
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        throw DxvkError("DDraw2Surface: ERROR! Failed to create a DDrawSurface origin!");
      }
      m_parent = m_originSurf.ptr();
    } else {
      m_originSurf = m_parent;
    }

    // Retrieve and cache the shadow surface
    if (likely(m_shadowSurf == nullptr)) {
      IUnknown* shadowSurfaceProxiedOrigin = m_commonSurf->GetShadowSurfaceProxied();
      if (likely(shadowSurfaceProxiedOrigin != nullptr)) {
        Com<IDirectDrawSurface2> shadowSurfProxy;
        HRESULT hr = shadowSurfaceProxiedOrigin->QueryInterface(__uuidof(IDirectDrawSurface2),
                                                                reinterpret_cast<void**>(&shadowSurfProxy));
        if (unlikely(FAILED(hr))) {
          throw DxvkError("DDraw2Surface: ERROR! Failed to retrieve the shadow surface!");
        } else {
          if (shadowSurfProxy != nullptr) {
            try {
              m_shadowSurf = new DDraw2Surface(m_commonSurf->GetShadowCommonSurface(), std::move(shadowSurfProxy),
                                               m_commonSurf->GetDDSurface(), nullptr);
            } catch (const DxvkError& e) {
              Logger::err(e.message());
              throw DxvkError("DDraw2Surface: ERROR! Failed to create the shadow surface!");
            }
          }
        }
      }
    }

    DDrawCommonInterface::AddWrappedSurface(this);

    m_commonSurf->SetDD2Surface(this);

    //Logger::debug(str::format("DDraw2Surface: Created a new surface nr. [[2-", std::hex, this, "]]"));

    // Note: IDirectDrawSurface2 can't ever be the origin surface
  }

  DDraw2Surface::~DDraw2Surface() {
    // Clear the cached depth stencil on the parent if matched
    if (unlikely(m_parentSurf != nullptr && m_commonSurf->IsDepthStencil()
      && m_parentSurf->GetAttachedDepthStencil() == this)) {
      m_parentSurf->SetAttachedDepthStencil(nullptr);
    }

    DDrawCommonInterface::RemoveWrappedSurface(this);

    if (m_depthStencil != nullptr)
      m_depthStencil->SetParentSurface(nullptr);

    // Release all public references on all attached surfaces
    for (auto & attachedSurface : m_attachedSurfaces) {
      attachedSurface.second->SetParentSurface(nullptr);
      uint32_t attachedRef;
      do {
        attachedRef = attachedSurface.second->Release();
      } while (attachedRef > 0);
    }

    m_commonSurf->SetDD2Surface(nullptr);

    //Logger::debug(str::format("DDraw2Surface: Surface nr. [[2-", std::hex, this, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IDirect3DTexture)) {
      IDirect3DTexture* texture3 = m_parent->GetD3D3Texture();

      if (unlikely(texture3 == nullptr))
        return m_parent->QueryInterface(riid, ppvObject);

      *ppvObject = ref(texture3);

      return S_OK;
    }
    if (riid == __uuidof(IDirect3DTexture2)) {
      IDirect3DTexture2* texture5 = m_parent->GetD3D5Texture();

      if (unlikely(texture5 == nullptr))
        return m_parent->QueryInterface(riid, ppvObject);

      *ppvObject = ref(texture5);

      return S_OK;
    }
    if (riid == __uuidof(IDirectDrawGammaControl)) {
      void* gammaControlProxiedVoid = nullptr;
      // This can never reasonably fail
      m_proxy->QueryInterface(__uuidof(IDirectDrawGammaControl), &gammaControlProxiedVoid);
      Com<IDirectDrawGammaControl> gammaControlProxied = static_cast<IDirectDrawGammaControl*>(gammaControlProxiedVoid);
      *ppvObject = ref(new DDrawGammaControl(m_commonSurf.ptr(), std::move(gammaControlProxied), m_commonSurf->GetDDSurface()));
      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      return E_NOINTERFACE;
    }
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      if (m_commonSurf->GetDDSurface() != nullptr)
        return m_commonSurf->GetDDSurface()->QueryInterface(riid, ppvObject);

      Com<IDirectDrawSurface> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      try {
        *ppvObject = ref(new DDrawSurface(m_commonSurf.ptr(), std::move(ppvProxyObject),
                                          m_commonIntf->GetDDInterface(), nullptr, false));
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return E_NOINTERFACE;
      }

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      if (m_commonSurf->GetDD3Surface() != nullptr)
        return m_commonSurf->GetDD3Surface()->QueryInterface(riid, ppvObject);

      Com<IDirectDrawSurface3> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      try {
        *ppvObject = ref(new DDraw3Surface(m_commonSurf.ptr(), std::move(ppvProxyObject),
                                           m_commonSurf->GetDDSurface(), nullptr));
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return E_NOINTERFACE;
      }

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      if (m_commonSurf->GetDD4Surface() != nullptr)
        return m_commonSurf->GetDD4Surface()->QueryInterface(riid, ppvObject);

      Com<IDirectDrawSurface4> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      try {
        *ppvObject = ref(new DDraw4Surface(m_commonSurf.ptr(), std::move(ppvProxyObject),
                                           m_commonIntf->GetDD4Interface(), nullptr, false));
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return E_NOINTERFACE;
      }

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      if (m_commonSurf->GetDD7Surface() != nullptr)
        return m_commonSurf->GetDD7Surface()->QueryInterface(riid, ppvObject);

      Com<IDirectDrawSurface7> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      try {
        *ppvObject = ref(new DDraw7Surface(m_commonSurf.ptr(), std::move(ppvProxyObject),
                                           m_commonIntf->GetDD7Interface(), nullptr, false));
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return E_NOINTERFACE;
      }

      return S_OK;
    }

    if (likely(riid == __uuidof(IDirectDrawSurface2))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("DDraw2Surface::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::AddAttachedSurface(LPDIRECTDRAWSURFACE2 lpDDSAttachedSurface) {
    if (unlikely(lpDDSAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(lpDDSAttachedSurface))) {
      // Some games, like Nightmare Creatures, try to attach an IDirectDrawSurface depth stencil directly...
      if (likely(DDrawCommonInterface::IsWrappedSurface(reinterpret_cast<IDirectDrawSurface*>(lpDDSAttachedSurface))))
        return m_parent->AddAttachedSurface(reinterpret_cast<IDirectDrawSurface*>(lpDDSAttachedSurface));

      Logger::err("DDraw2Surface::AddAttachedSurface: Received an unwrapped surface");
      return DDERR_CANNOTATTACHSURFACE;
    }

    DDraw2Surface* attachedSurf = static_cast<DDraw2Surface*>(lpDDSAttachedSurface);

    HRESULT hr = m_proxy->AddAttachedSurface(attachedSurf->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    attachedSurf->SetParentSurface(this);

    if (likely(attachedSurf->GetCommonSurface()->IsDepthStencil())) {
      m_depthStencil = attachedSurf;
    // If a flippable surface is attached, mark it as the next flippable surface
    } else if (unlikely(attachedSurf->GetCommonSurface()->IsBackBufferOrFlippable())) {
      DDrawSurface* attachedSurfParent = attachedSurf->GetCommonSurface()->GetDDSurface();
      m_parent->SetNextFlippable(attachedSurfParent);
    }

    return DD_OK;
  }

  // Docs: "This method is used for the software implementation.
  // It is not needed if the overlay support is provided by the hardware."
  HRESULT STDMETHODCALLTYPE DDraw2Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE2 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    if (unlikely(lpDDSrcSurface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSrcSurface))) {
      if (likely(DDrawCommonInterface::IsWrappedSurface(reinterpret_cast<IDirectDrawSurface*>(lpDDSrcSurface))))
        return m_parent->Blt(lpDestRect, reinterpret_cast<IDirectDrawSurface*>(lpDDSrcSurface), lpSrcRect, dwFlags, lpDDBltFx);

      Logger::err("DDraw2Surface::Blt: Received an unwrapped source surface");
      return DDERR_UNSUPPORTED;
    }

    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    if (likely(lpDDSrcSurface != nullptr)) {
      DDraw2Surface* sourceSurf = static_cast<DDraw2Surface*>(lpDDSrcSurface);
      sourceSurf->DownloadSurfaceData();
    }
    // No point in downloading the destination surface if it's going to be overwritten
    if ((lpDDBltFx == nullptr || (dwFlags & DDBLT_COLORFILL) || (dwFlags & DDBLT_DEPTHFILL)) &&
         m_commonSurf->IsFullSurfaceLock(lpDestRect, nullptr)) {
      m_commonSurf->UnDirtyD3D9Surface();
    } else {
      DownloadSurfaceData();
    }

    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->GetRefreshedD3D9Device();
    if (likely(d3d9Device != nullptr)) {
      const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Windowed mode presentation path
      if (!exclusiveMode && lpDDSrcSurface != nullptr && m_commonSurf->IsPrimarySurface()) {
        // TODO: Handle this properly, not by uploading the RT again
        DDraw2Surface* sourceSurf = static_cast<DDraw2Surface*>(lpDDSrcSurface);
        DDrawSurface* sourceSurfOrig = sourceSurf->GetCommonSurface()->GetDDSurface();
        DDrawSurface* renderTarget = m_commonSurf->GetCommonD3DDevice()->GetCurrentRenderTarget();

        if (sourceSurfOrig == renderTarget) {
          renderTarget->InitializeOrUploadD3D9();
          d3d9Device->Present(NULL, NULL, NULL, NULL);
          return DD_OK;
        }
      }
    }

    HRESULT hr;
    if (unlikely(lpDDSrcSurface == nullptr)) {
      hr = GetShadowOrProxied()->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
    } else {
      DDraw2Surface* sourceSurf = static_cast<DDraw2Surface*>(lpDDSrcSurface);
      hr = GetShadowOrProxied()->Blt(lpDestRect, sourceSurf->GetShadowOrProxied(), lpSrcRect, dwFlags, lpDDBltFx);
    }
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonSurf->DirtyDDrawSurface();

    if (unlikely(m_shadowSurf != nullptr && d3d9Device != nullptr)) {
      const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                !m_commonSurf->GetCommonD3DDevice()->IsInScene() :
                                 m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                 false : true;
      if (shouldPresent) {
        InitializeOrUploadD3D9();
        d3d9Device->Present(NULL, NULL, NULL, NULL);
      }
    }

    return DD_OK;
  }

  // Docs: "The IDirectDrawSurface2::BltBatch method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw2Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE2 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    if (unlikely(lpDDSrcSurface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSrcSurface))) {
      if (likely(DDrawCommonInterface::IsWrappedSurface(reinterpret_cast<IDirectDrawSurface*>(lpDDSrcSurface))))
        return m_parent->BltFast(dwX, dwY, reinterpret_cast<IDirectDrawSurface*>(lpDDSrcSurface), lpSrcRect, dwTrans);

      Logger::err("DDraw2Surface::BltFast: Received an unwrapped source surface");
      return DDERR_UNSUPPORTED;
    }

    const RECT* sourceFullSurfaceRect = nullptr;
    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    if (likely(lpDDSrcSurface != nullptr)) {
      DDraw2Surface* sourceSurf = static_cast<DDraw2Surface*>(lpDDSrcSurface);
      sourceSurf->DownloadSurfaceData();
      sourceFullSurfaceRect = sourceSurf->GetCommonSurface()->GetFullSurfaceRect();
    }
    // No point in downloading the destination surface if it's going to be overwritten
    if (dwX == 0 && dwY == 0 && (dwTrans & DDBLTFAST_NOCOLORKEY) &&
        m_commonSurf->IsFullSurfaceLock(lpSrcRect, sourceFullSurfaceRect)) {
      m_commonSurf->UnDirtyD3D9Surface();
    } else {
      DownloadSurfaceData();
    }

    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->GetRefreshedD3D9Device();
    if (likely(d3d9Device != nullptr)) {
      const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Windowed mode presentation path
      if (!exclusiveMode && lpDDSrcSurface != nullptr && m_commonSurf->IsPrimarySurface()) {
        // TODO: Handle this properly, not by uploading the RT again
        DDraw2Surface* sourceSurf = static_cast<DDraw2Surface*>(lpDDSrcSurface);
        DDrawSurface* sourceSurfOrig = sourceSurf->GetCommonSurface()->GetDDSurface();
        DDrawSurface* renderTarget = m_commonSurf->GetCommonD3DDevice()->GetCurrentRenderTarget();

        if (sourceSurfOrig == renderTarget) {
          renderTarget->InitializeOrUploadD3D9();
          d3d9Device->Present(NULL, NULL, NULL, NULL);
          return DD_OK;
        }
      }
    }

    HRESULT hr;
    if (unlikely(lpDDSrcSurface == nullptr)) {
      hr = GetShadowOrProxied()->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    } else {
      DDraw2Surface* sourceSurf = static_cast<DDraw2Surface*>(lpDDSrcSurface);
      hr = GetShadowOrProxied()->BltFast(dwX, dwY, sourceSurf->GetShadowOrProxied(), lpSrcRect, dwTrans);
    }
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonSurf->DirtyDDrawSurface();

    if (unlikely(m_shadowSurf != nullptr && d3d9Device != nullptr)) {
      const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                !m_commonSurf->GetCommonD3DDevice()->IsInScene() :
                                 m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                 false : true;
      if (shouldPresent) {
        InitializeOrUploadD3D9();
        d3d9Device->Present(NULL, NULL, NULL, NULL);
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE2 lpDDSAttachedSurface) {
    if (unlikely(lpDDSAttachedSurface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSAttachedSurface))) {
      // Some games, like Nightmare Creatures, try to attach an IDirectDrawSurface depth stencil directly...
      if (likely(DDrawCommonInterface::IsWrappedSurface(reinterpret_cast<IDirectDrawSurface*>(lpDDSAttachedSurface))))
        return m_parent->DeleteAttachedSurface(dwFlags, reinterpret_cast<IDirectDrawSurface*>(lpDDSAttachedSurface));

      Logger::err("DDraw2Surface::DeleteAttachedSurface: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    if (lpDDSAttachedSurface == nullptr) {
      HRESULT hrProxy = m_proxy->DeleteAttachedSurface(dwFlags, lpDDSAttachedSurface);
      if (unlikely(FAILED(hrProxy)))
        return hrProxy;

      // If lpDDSAttachedSurface is NULL, then all surfaces are detached
      m_depthStencil = nullptr;

      return DD_OK;
    }

    DDraw2Surface* attachedSurf = static_cast<DDraw2Surface*>(lpDDSAttachedSurface);

    HRESULT hr = m_proxy->DeleteAttachedSurface(dwFlags, attachedSurf->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    attachedSurf->SetParentSurface(nullptr);

    if (likely(m_depthStencil == attachedSurf)) {
      m_depthStencil = nullptr;
    // Clear the next flippable surface or flippable surface detachment
    } else if (unlikely(attachedSurf->GetCommonSurface()->IsBackBufferOrFlippable() &&
                        attachedSurf->GetCommonSurface()->GetDDSurface() == m_parent->GetNextFlippable())) {
      m_parent->SetNextFlippable(nullptr);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    return m_parent->EnumAttachedSurfaces(lpContext, lpEnumSurfacesCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpfnCallback) {
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Flip(LPDIRECTDRAWSURFACE2 lpDDSurfaceTargetOverride, DWORD dwFlags) {
    if (unlikely(lpDDSurfaceTargetOverride != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSurfaceTargetOverride))) {
      Logger::err("DDraw2Surface::Flip: Received an unwrapped override surface");
      return DDERR_UNSUPPORTED;
    }

    Com<DDraw2Surface> overrideSurf = static_cast<DDraw2Surface*>(lpDDSurfaceTargetOverride);

    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->GetRefreshedD3D9Device();
    // Overlays can have odd video formats which DXVK doesn't support for RT
    // use, so let DDraw present in case the flipped surface is an overlay
    if (likely(d3d9Device != nullptr && !m_commonSurf->IsOverlay())) {
      // Lost surfaces are not flippable
      HRESULT hr = m_proxy->IsLost();
      if (unlikely(FAILED(hr)))
        return hr;

      if (unlikely(!(m_commonSurf->IsFrontBuffer() || m_commonSurf->IsBackBufferOrFlippable())))
        return DDERR_NOTFLIPPABLE;

      const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Non-exclusive mode validations
      if (unlikely(m_commonSurf->IsPrimarySurface() && !exclusiveMode))
        return DDERR_NOEXCLUSIVEMODE;

      // Exclusive mode validations
      if (unlikely(m_commonSurf->IsBackBufferOrFlippable() && exclusiveMode))
        return DDERR_NOTFLIPPABLE;

      if (unlikely(overrideSurf != nullptr && !overrideSurf->GetParent()->GetCommonSurface()->IsBackBufferOrFlippable()))
        return DDERR_NOTFLIPPABLE;

      DDrawSurface* nextFlippable;
      // Workaround for The Sims/other games flipping a different
      // swapchain than the one set on the current D3D device
      if (unlikely(m_commonIntf->GetOptions()->forceRTFlip && m_commonSurf->IsPrimarySurface())) {
        nextFlippable = m_commonSurf->GetCommonD3DDevice()->GetCurrentRenderTarget();
      } else {
        nextFlippable = m_parent->GetNextFlippable();
      }

      if (likely(nextFlippable != nullptr)) {
        if (m_commonIntf->GetOptions()->emulateFrontBuffer) {
          InitializeOrUploadD3D9();
          // Workaround for front buffer image retention issues
          if (m_shadowSurf != nullptr && nextFlippable->GetCommonSurface()->IsDDrawSurfaceDirty())
            m_parent->GetShadowOrProxied()->BltFast(0, 0, nextFlippable->GetShadowOrProxied(), nullptr, DDBLTFAST_NOCOLORKEY);
        }
        nextFlippable->InitializeOrUploadD3D9();
      } else {
        InitializeOrUploadD3D9();
      }

      d3d9Device->Present(NULL, NULL, NULL, NULL);

    } else {
      if (overrideSurf == nullptr) {
        return m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
      } else {
        return m_proxy->Flip(overrideSurf->GetShadowOrProxied(), dwFlags);
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE2 *lplpDDAttachedSurface) {
    if (unlikely(lpDDSCaps == nullptr || lplpDDAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<IDirectDrawSurface2> surface;
    HRESULT hr = m_proxy->GetAttachedSurface(lpDDSCaps, &surface);
    // These are rather common, as some games query expecting to get nothing in return, for
    // example it's a common use case to query the mip attach chain until nothing is returned
    if (FAILED(hr)) {
      *lplpDDAttachedSurface = surface.ptr();
      return hr;
    }

    try {
      auto attachedSurfaceIter = m_attachedSurfaces.find(surface.ptr());
      if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
        // Return the already attached depth surface if it exists
        if (unlikely(m_depthStencil != nullptr && surface.ptr() == m_depthStencil->GetProxied())) {
          *lplpDDAttachedSurface = m_depthStencil.ref();
        } else {
          Com<DDraw2Surface> ddraw2Surface = new DDraw2Surface(nullptr, std::move(surface), nullptr, this);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(ddraw2Surface->GetProxied()),
                                     std::forward_as_tuple(ddraw2Surface.ref()));
          *lplpDDAttachedSurface = ddraw2Surface.ref();
        }
      } else {
        *lplpDDAttachedSurface = attachedSurfaceIter->second.ref();
      }
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      *lplpDDAttachedSurface = nullptr;
      return DDERR_GENERIC;
    }

    return DD_OK;
  }

  // Blitting can be done at any time and completes within its call frame
  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetBltStatus(DWORD dwFlags) {
    if (likely(dwFlags == DDGBS_CANBLT || dwFlags == DDGBS_ISBLTDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetCaps(LPDDSCAPS lpDDSCaps) {
    if (unlikely(lpDDSCaps == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDSCaps = m_commonSurf->GetDesc()->ddsCaps;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper) {
    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    DDrawClipper* clipper = m_commonSurf->GetClipper();

    if (unlikely(clipper == nullptr))
      return DDERR_NOCLIPPERATTACHED;

    *lplpDDClipper = ref(clipper);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetDC(HDC *lphDC) {
    // Direct D3D9 path which can sometimes be faster (and other times slower)
    if (unlikely(m_commonIntf->GetOptions()->forceDCForwarding && m_commonSurf->IsInitialized())) {
      InitializeOrUploadD3D9();

      HRESULT hr = m_commonSurf->GetD3D9Surface()->GetDC(lphDC);
      if (unlikely(FAILED(hr)))
        return DDERR_INVALIDPARAMS;

      return DD_OK;
    }

    // Write back any dirty surface data from bound D3D9 back buffers or depth stencils
    DownloadSurfaceData();

    return GetShadowOrProxied()->GetDC(lphDC);
  }

  // Flipping can be done at any time and completes within its call frame
  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetFlipStatus(DWORD dwFlags) {
    if (likely(dwFlags == DDGFS_CANFLIP || dwFlags == DDGFS_ISFLIPDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetOverlayPosition(LPLONG lplX, LPLONG lplY) {
    return m_proxy->GetOverlayPosition(lplX, lplY);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette) {
    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    DDrawPalette* palette = m_commonSurf->GetPalette();

    if (unlikely(palette == nullptr))
      return DDERR_NOPALETTEATTACHED;

    *lplpDDPalette = ref(palette);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    if (unlikely(lpDDPixelFormat == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDPixelFormat = m_commonSurf->GetDesc()->ddpfPixelFormat;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) {
    if (unlikely(lpDDSurfaceDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDDSurfaceDesc->dwSize != sizeof(DDSURFACEDESC)))
      return DDERR_INVALIDPARAMS;

    *lpDDSurfaceDesc = *m_commonSurf->GetDesc();

    return DD_OK;
  }

  // According to the docs: "Because the DirectDrawSurface object is initialized
  // when it's created, this method always returns DDERR_ALREADYINITIALIZED."
  HRESULT STDMETHODCALLTYPE DDraw2Surface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc) {
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::IsLost() {
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    // Write back any dirty surface data from bound D3D9 back buffers or depth stencils
    DownloadSurfaceData();

    return GetShadowOrProxied()->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::ReleaseDC(HDC hDC) {
    // Direct D3D9 path which can sometimes be faster (and other times slower)
    if (unlikely(m_commonIntf->GetOptions()->forceDCForwarding && m_commonSurf->IsInitialized())) {
      HRESULT hr = m_commonSurf->GetD3D9Surface()->ReleaseDC(hDC);
      if (unlikely(FAILED(hr)))
        return DDERR_INVALIDPARAMS;

      m_commonSurf->DirtyD3D9Surface();

      return DD_OK;
    }

    HRESULT hr = GetShadowOrProxied()->ReleaseDC(hDC);
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonSurf->DirtyDDrawSurface();

    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->GetRefreshedD3D9Device();
    if (unlikely(m_shadowSurf != nullptr && d3d9Device != nullptr)) {
      const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                !m_commonSurf->GetCommonD3DDevice()->IsInScene() :
                                 m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                 false : true;
      if (shouldPresent) {
        InitializeOrUploadD3D9();
        d3d9Device->Present(NULL, NULL, NULL, NULL);
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Restore() {
    return m_proxy->Restore();
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
    // A nullptr lpDDClipper gets the current clipper detached
    if (lpDDClipper == nullptr) {
      HRESULT hr = m_proxy->SetClipper(lpDDClipper);
      if (unlikely(FAILED(hr)))
        return hr;

      m_commonSurf->SetClipper(nullptr);
    } else {
      DDrawClipper* ddrawClipper = static_cast<DDrawClipper*>(lpDDClipper);

      HRESULT hr = m_proxy->SetClipper(ddrawClipper->GetProxied());
      if (unlikely(FAILED(hr)))
        return hr;

      m_commonSurf->SetClipper(ddrawClipper);

      // Retrieve a hWnd, if needed, during clipper attachment
      HWND hWnd = nullptr;
      hr = ddrawClipper->GetProxied()->GetHWnd(&hWnd);
      if (unlikely(FAILED(hr)))
        return DD_OK;

      m_commonIntf->SetHWND(hWnd);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    // The Combat Mission series of games set a color key which is
    // outside the color range of the surface they are setting it on...
    // clamp it to the surface color depth in that case. This doesn't
    // appear to work well universally, however, so only apply when needed.
    if (unlikely(m_commonIntf->GetOptions()->colorKeyMasking && lpDDColorKey != nullptr)) {
      const uint8_t colorBitCount = m_commonSurf->GetColorBitCount();
      if (likely(colorBitCount < 32u)) {
        lpDDColorKey->dwColorSpaceLowValue  &= (1 << colorBitCount) - 1;
        lpDDColorKey->dwColorSpaceHighValue &= (1 << colorBitCount) - 1;
      }
    }

    HRESULT hr = m_proxy->SetColorKey(dwFlags, lpDDColorKey);
    if (unlikely(FAILED(hr)))
      return hr;

    hr = m_commonSurf->RefreshSurfaceDescripton(false);
    if (unlikely(FAILED(hr)))
      Logger::err("DDraw2Surface::SetColorKey: Failed to retrieve updated surface desc");

    if (unlikely(m_shadowSurf != nullptr)) {
      hr = m_shadowSurf->GetProxied()->SetColorKey(dwFlags, lpDDColorKey);
      if (unlikely(FAILED(hr))) {
        Logger::warn("DDraw2Surface::SetColorKey: Failed to set shadow surface color key");
      } else {
        hr = m_shadowSurf->GetCommonSurface()->RefreshSurfaceDescripton(false);
        if (unlikely(FAILED(hr)))
          Logger::warn("DDraw2Surface::SetColorKey: Failed to retrieve updated shadow surface desc");
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::SetOverlayPosition(LONG lX, LONG lY) {
    return m_proxy->SetOverlayPosition(lX, lY);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) {
    // A nullptr lpDDPalette gets the current palette detached
    if (lpDDPalette == nullptr) {
      HRESULT hr = GetShadowOrProxied()->SetPalette(lpDDPalette);
      if (unlikely(FAILED(hr)))
        return hr;

      m_commonSurf->SetPalette(nullptr);
    } else {
      DDrawPalette* ddrawPalette = static_cast<DDrawPalette*>(lpDDPalette);

      HRESULT hr = GetShadowOrProxied()->SetPalette(ddrawPalette->GetProxied());
      if (unlikely(FAILED(hr)))
        return hr;

      m_commonSurf->SetPalette(ddrawPalette);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::Unlock(LPVOID lpSurfaceData) {
    HRESULT hr = GetShadowOrProxied()->Unlock(lpSurfaceData);
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonSurf->DirtyDDrawSurface();

    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->GetRefreshedD3D9Device();
    if (unlikely(m_shadowSurf != nullptr && d3d9Device != nullptr)) {
      const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                !m_commonSurf->GetCommonD3DDevice()->IsInScene() :
                                 m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                 false : true;
      if (shouldPresent) {
        InitializeOrUploadD3D9();
        d3d9Device->Present(NULL, NULL, NULL, NULL);
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE2 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    if (unlikely(lpDDDestSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(lpDDDestSurface))) {
      Logger::err("DDraw2Surface::UpdateOverlay: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    DDraw2Surface* ddraw2Surface = static_cast<DDraw2Surface*>(lpDDDestSurface);
    return m_proxy->UpdateOverlay(lpSrcRect, ddraw2Surface->GetProxied(), lpDestRect, dwFlags, lpDDOverlayFx);
  }

  // Docs: "This method is for software emulation only; it does nothing if the hardware supports overlays."
  HRESULT STDMETHODCALLTYPE DDraw2Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE2 lpDDSReference) {
    if (unlikely(lpDDSReference != nullptr
              && !DDrawCommonInterface::IsWrappedSurface(lpDDSReference))) {
      Logger::err("DDraw2Surface::UpdateOverlayZOrder: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    if (lpDDSReference == nullptr) {
      return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
    } else {
      DDraw2Surface* ddraw2Surface = static_cast<DDraw2Surface*>(lpDDSReference);
      return m_proxy->UpdateOverlayZOrder(dwFlags, ddraw2Surface->GetProxied());
    }
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::GetDDInterface(LPVOID *lplpDD) {
    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDD);

    if (unlikely(m_commonIntf->GetDDInterface() == nullptr)) {
      Logger::err("DDraw2Surface::GetDDInterface: Found no valid parent interface");
      return DDERR_NOTFOUND;
    }

    *lplpDD = ref(m_commonIntf->GetDDInterface());

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::PageLock(DWORD dwFlags) {
    return m_proxy->PageLock(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw2Surface::PageUnlock(DWORD dwFlags) {
    return m_proxy->PageUnlock(dwFlags);
  }

  IDirectDrawSurface2* DDraw2Surface::GetShadowOrProxied() {
    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->GetRefreshedD3D9Device();

    if (unlikely(m_shadowSurf != nullptr && d3d9Device != nullptr))
      return m_shadowSurf->GetProxied();

    return m_proxy.ptr();
  }

  HRESULT DDraw2Surface::InitializeOrUploadD3D9() {
    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->GetRefreshedD3D9Device();

    // Fast skip
    if (unlikely(d3d9Device == nullptr))
      return DD_OK;

    if (unlikely(!m_commonSurf->IsInitialized())) {
      if (m_commonSurf->IsTexture())
        m_parent->UpdateMipMapCount();

      const bool initRenderTarget = m_commonSurf->GetCommonD3DDevice()->IsCurrentRenderTarget(m_commonSurf.ptr());

      HRESULT hr = m_commonSurf->InitializeD3D9(initRenderTarget);
      if (unlikely(FAILED(hr))) {
        Logger::err(str::format("DDraw2Surface::InitializeOrUploadD3D9: Failed to initialize surface nr. [[2-", std::hex, this, "]]"));
        return hr;
      }
    }

    if (likely(m_commonSurf->IsInitialized()))
      return UploadSurfaceData();

    return DD_OK;
  }

  void DDraw2Surface::DownloadSurfaceData() {
    // Some games, like The Settlers IV, use multiple devices for rendering, one to handle
    // terrain and the overall 3D scene, and one to create textures/sprites to overlay on
    // top of it. Since DXVK's D3D9 backend does not restrict cross-device surface/texture
    // use, simply skip changing assigned surface devices during downloads. This is essentially
    // a hack, which by some miracle works well enough in some cases, though may explode in others.
    if (likely(!m_commonIntf->GetOptions()->deviceResourceSharing))
      m_commonSurf->RefreshD3D9Device();

    if (unlikely(m_commonSurf->IsD3D9BackBuffer())) {
      if (m_commonSurf->IsInitialized() && m_commonSurf->IsD3D9SurfaceDirty()) {
        //Logger::debug(str::format("DDraw2Surface::DownloadSurfaceData: Downloading nr. [[2-", std::hex, this, "]]"));
        BlitToDDrawSurface<IDirectDrawSurface2, DDSURFACEDESC>(GetShadowOrProxied(), m_commonSurf->GetD3D9Surface(),
                                                               m_commonSurf->IsDXTFormat());
        m_commonSurf->UnDirtyD3D9Surface();
      }
    } else if (unlikely(m_commonSurf->IsD3D9DepthStencil())) {
      if (m_commonSurf->IsInitialized() && m_commonSurf->IsD3D9SurfaceDirty()) {
        //Logger::debug(str::format("DDraw2Surface::DownloadSurfaceData: Downloading nr. [[2-", std::hex, this, "]]"));
        BlitToDDrawSurface<IDirectDrawSurface2, DDSURFACEDESC>(m_proxy.ptr(), m_commonSurf->GetD3D9Surface(),
                                                               m_commonSurf->IsDXTFormat());
        m_commonSurf->UnDirtyD3D9Surface();
      }
    }
  }

  inline HRESULT DDraw2Surface::UploadSurfaceData() {
    // Fast skip
    if (!m_commonSurf->IsDDrawSurfaceDirty())
      return DD_OK;

    //Logger::debug(str::format("DDraw2Surface::UploadSurfaceData: Uploading nr. [[2-", std::hex, this, "]]"));

    if (m_commonSurf->IsTexture()) {
      BlitToD3D9Texture<IDirectDrawSurface2, DDSURFACEDESC>(m_commonSurf->GetD3D9Texture(), m_proxy.ptr(),
                                                            m_commonSurf->GetMipCount(), m_commonSurf->IsDXTFormat());
    // Blit surfaces directly
    } else {
      BlitToD3D9Surface<IDirectDrawSurface2, DDSURFACEDESC>(m_commonSurf->GetD3D9Surface(), GetShadowOrProxied(),
                                                            m_commonSurf->IsDXTFormat());
    }

    m_commonSurf->UnDirtyDDrawSurface();

    return DD_OK;
  }

}
