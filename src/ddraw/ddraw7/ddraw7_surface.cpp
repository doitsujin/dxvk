#include "ddraw7_surface.h"

#include "../d3d_common_device.h"

#include "../ddraw_gamma.h"
#include "../ddraw/ddraw_surface.h"
#include "../ddraw2/ddraw2_surface.h"
#include "../ddraw2/ddraw3_surface.h"
#include "../ddraw4/ddraw4_surface.h"

namespace dxvk {

  DDraw7Surface::DDraw7Surface(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawSurface7>&& surfProxy,
        DDraw7Interface* pParent,
        DDraw7Surface* pParentSurf,
        bool isChildObject)
    : DDrawWrappedObject<DDraw7Interface, IDirectDrawSurface7>(pParent, std::move(surfProxy))
    , m_isChildObject ( isChildObject )
    , m_commonSurf ( commonSurf )
    , m_parentSurf ( pParentSurf ) {
    if (m_parent != nullptr) {
      m_commonIntf = m_parent->GetCommonInterface();
    } else if (m_parentSurf != nullptr) {
      m_commonIntf = m_parentSurf->GetCommonInterface();
    } else if (m_commonSurf != nullptr) {
      m_commonIntf = m_commonSurf->GetCommonInterface();
    } else {
      throw DxvkError("DDraw7Surface: ERROR! Failed to retrieve the common interface!");
    }

    if (m_commonSurf == nullptr)
      m_commonSurf = new DDrawCommonSurface(m_commonIntf);

    // Retrieve and cache the proxy surface desc
    if (!m_commonSurf->IsDesc2Set()) {
      DDSURFACEDESC2 desc2;
      desc2.dwSize = sizeof(DDSURFACEDESC2);
      HRESULT hr = m_proxy->GetSurfaceDesc(&desc2);

      if (unlikely(FAILED(hr))) {
        throw DxvkError("DDraw7Surface: ERROR! Failed to retrieve new surface desc!");
      } else {
        m_commonSurf->SetDesc2(desc2);
      }
    }

    // Retrieve and cache the next surface in a flippable chain
    if (unlikely(m_commonSurf->IsFlippable() && !m_commonSurf->IsBackBuffer())) {
      IDirectDrawSurface7* nextFlippable = nullptr;
      EnumAttachedSurfaces(&nextFlippable, ListBackBufferSurfaces7Callback);
      m_nextFlippable = reinterpret_cast<DDraw7Surface*>(nextFlippable);
      if (likely(m_nextFlippable != nullptr)) {
        // The call to EnumAttachedSurfaces has incremented the public ref
        m_nextFlippable->Release();
        //Logger::debug("DDraw7Surface: Retrieved the next swapchain surface");
      }
    }

    DDrawCommonInterface::AddWrappedSurface(this);

    m_commonSurf->SetDD7Surface(this);

    if (m_parentSurf != nullptr
     && m_parentSurf->GetCommonSurface()->IsBackBufferOrFlippable()) {
      const uint32_t index = m_parentSurf->GetCommonSurface()->GetBackBufferIndex();
      m_commonSurf->IncrementBackBufferIndex(index);
    }

    if (m_parent != nullptr && m_isChildObject)
      m_parent->AddRef();

    // Cube map face surfaces
    m_cubeMapSurfaces.fill(nullptr);

    //Logger::debug(str::format("DDraw7Surface: Created a new surface nr. [[7-", std::hex, this, "]]"));

    if (m_commonSurf->GetOrigin() == nullptr) {
      m_commonSurf->SetOrigin(this);
      m_commonSurf->SetIsAttached(m_parentSurf != nullptr);
      //m_commonSurf->ListSurfaceDetails();
    }
  }

  DDraw7Surface::~DDraw7Surface() {
    if (m_commonSurf->GetOrigin() == this)
      m_commonSurf->SetOrigin(nullptr);

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

    if (m_parent != nullptr && m_isChildObject)
      m_parent->Release();

    m_commonSurf->SetDD7Surface(nullptr);

    //Logger::debug(str::format("DDraw7Surface: Surface nr. [[7-", std::hex, this, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Shouldn't ever be called in practice
    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
      return E_NOINTERFACE;
    }
    // Black & White queries for IDirect3DTexture2 for whatever reason...
    if (unlikely(riid == __uuidof(IDirect3DTexture2))) {
      return E_NOINTERFACE;
    }
    // Wrap IDirectDrawGammaControl, to potentially ignore application set gamma ramps
    if (riid == __uuidof(IDirectDrawGammaControl)) {
      void* gammaControlProxiedVoid = nullptr;
      // This can never reasonably fail
      m_proxy->QueryInterface(__uuidof(IDirectDrawGammaControl), &gammaControlProxiedVoid);
      Com<IDirectDrawGammaControl> gammaControlProxied = static_cast<IDirectDrawGammaControl*>(gammaControlProxiedVoid);
      *ppvObject = ref(new DDrawGammaControl(m_commonSurf.ptr(), std::move(gammaControlProxied), this));
      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirectDrawColorControl))) {
      return E_NOINTERFACE;
    }
    // Some games query for legacy ddraw surfaces
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
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      if (m_commonSurf->GetDD2Surface() != nullptr)
        return m_commonSurf->GetDD2Surface()->QueryInterface(riid, ppvObject);

      Com<IDirectDrawSurface2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      try {
        *ppvObject = ref(new DDraw2Surface(m_commonSurf.ptr(), std::move(ppvProxyObject),
                                           m_commonSurf->GetDDSurface(), nullptr));
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

    if (likely(riid == __uuidof(IDirectDrawSurface7))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("DDraw7Surface::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  // On IDirectDrawSurface7, this call will only attach DDSCAPS_ZBUFFER
  // type surfaces and will fail if called with any other surface type.
  HRESULT STDMETHODCALLTYPE DDraw7Surface::AddAttachedSurface(LPDIRECTDRAWSURFACE7 lpDDSAttachedSurface) {
    if (unlikely(lpDDSAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(lpDDSAttachedSurface))) {
      Logger::err("DDraw7Surface::AddAttachedSurface: Received an unwrapped surface");
      return DDERR_CANNOTATTACHSURFACE;
    }

    DDraw7Surface* attachedSurf = static_cast<DDraw7Surface*>(lpDDSAttachedSurface);

    HRESULT hr = m_proxy->AddAttachedSurface(attachedSurf->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    attachedSurf->SetParentSurface(this);
    m_depthStencil = attachedSurf;

    return DD_OK;
  }

  // Docs: "The IDirectDrawSurface7::AddOverlayDirtyRect method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw7Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    if (unlikely(lpDDSrcSurface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSrcSurface))) {
      Logger::err("DDraw7Surface::Blt: Received an unwrapped source surface");
      return DDERR_UNSUPPORTED;
    }

    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    if (likely(lpDDSrcSurface != nullptr)) {
      DDraw7Surface* sourceSurface = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      sourceSurface->DownloadSurfaceData();
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
        DDraw7Surface* sourceSurface = static_cast<DDraw7Surface*>(lpDDSrcSurface);
        DDraw7Surface* renderTarget = m_commonSurf->GetCommonD3DDevice()->GetCurrentRenderTarget7();

        if (sourceSurface == renderTarget) {
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
      DDraw7Surface* sourceSurface = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      hr = GetShadowOrProxied()->Blt(lpDestRect, sourceSurface->GetShadowOrProxied(), lpSrcRect, dwFlags, lpDDBltFx);
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

  // Docs: "The IDirectDrawSurface7::BltBatch method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw7Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE7 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    if (unlikely(lpDDSrcSurface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSrcSurface))) {
      Logger::err("DDraw7Surface::BltFast: Received an unwrapped source surface");
      return DDERR_UNSUPPORTED;
    }

    const RECT* sourceFullSurfaceRect = nullptr;
    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    if (likely(lpDDSrcSurface != nullptr)) {
      DDraw7Surface* sourceSurface = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      sourceSurface->DownloadSurfaceData();
      sourceFullSurfaceRect = sourceSurface->GetCommonSurface()->GetFullSurfaceRect();
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
        DDraw7Surface* sourceSurface = static_cast<DDraw7Surface*>(lpDDSrcSurface);
        DDraw7Surface* renderTarget = m_commonSurf->GetCommonD3DDevice()->GetCurrentRenderTarget7();

        if (sourceSurface == renderTarget) {
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
      DDraw7Surface* sourceSurface = static_cast<DDraw7Surface*>(lpDDSrcSurface);
      hr = GetShadowOrProxied()->BltFast(dwX, dwY, sourceSurface->GetShadowOrProxied(), lpSrcRect, dwTrans);
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

  // This call will only detach DDSCAPS_ZBUFFER type surfaces and will reject anything else.
  HRESULT STDMETHODCALLTYPE DDraw7Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE7 lpDDSAttachedSurface) {
    if (unlikely(lpDDSAttachedSurface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSAttachedSurface))) {
      Logger::err("DDraw7Surface::DeleteAttachedSurface: Received an unwrapped surface");
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

    DDraw7Surface* attachedSurf = static_cast<DDraw7Surface*>(lpDDSAttachedSurface);

    HRESULT hr = m_proxy->DeleteAttachedSurface(dwFlags, attachedSurf->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    attachedSurf->SetParentSurface(nullptr);

    if (likely(m_depthStencil == attachedSurf))
      m_depthStencil = nullptr;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpEnumSurfacesCallback) {
    if (unlikely(lpEnumSurfacesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    std::vector<AttachedSurface7> attachedSurfaces;
    // Enumerate all attached surfaces from the underlying DDraw implementation
    HRESULT hr = m_proxy->EnumAttachedSurfaces(reinterpret_cast<void*>(&attachedSurfaces), EnumAttachedSurfaces7Callback);
    if (unlikely(FAILED(hr)))
      return hr;

    hr = DDENUMRET_OK;

    // Wrap surfaces as needed and perform the actual callback the application is requesting
    auto surfaceIt = attachedSurfaces.begin();
    while (surfaceIt != attachedSurfaces.end() && hr == DDENUMRET_OK) {
      Com<IDirectDrawSurface7> surface7 = surfaceIt->surface7;

      try {
        auto attachedSurfaceIter = m_attachedSurfaces.find(surface7.ptr());
        if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
          // Return the already attached depth surface if it exists
          if (unlikely(m_depthStencil != nullptr && surface7.ptr() == m_depthStencil->GetProxied())) {
            hr = lpEnumSurfacesCallback(m_depthStencil.ref(), &surfaceIt->desc2, lpContext);
          } else {
            Com<DDraw7Surface> ddraw7Surface = new DDraw7Surface(nullptr, std::move(surface7), m_parent, this, false);
            m_attachedSurfaces.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(ddraw7Surface->GetProxied()),
                                       std::forward_as_tuple(ddraw7Surface.ref()));
            hr = lpEnumSurfacesCallback(ddraw7Surface.ref(), &surfaceIt->desc2, lpContext);
          }
        } else {
          hr = lpEnumSurfacesCallback(attachedSurfaceIter->second.ref(), &surfaceIt->desc2, lpContext);
        }
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return DDERR_GENERIC;
      }

      ++surfaceIt;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK7 lpfnCallback) {
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Flip(LPDIRECTDRAWSURFACE7 lpDDSurfaceTargetOverride, DWORD dwFlags) {
    if (unlikely(lpDDSurfaceTargetOverride != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSurfaceTargetOverride))) {
      Logger::err("DDraw7Surface::Flip: Received an unwrapped override surface");
      return DDERR_UNSUPPORTED;
    }

    Com<DDraw7Surface> overrideSurf = static_cast<DDraw7Surface*>(lpDDSurfaceTargetOverride);

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

      if (unlikely(overrideSurf != nullptr && !overrideSurf->GetCommonSurface()->IsBackBufferOrFlippable()))
        return DDERR_NOTFLIPPABLE;

      // If the interface is waiting for VBlank and we get a no VSync flip, switch
      // to doing immediate presents by resetting the swapchain appropriately
      if (unlikely(m_commonIntf->GetWaitForVBlank() && (dwFlags & DDFLIP_NOVSYNC))) {
        Logger::info("DDraw7Surface::Flip: Switching to D3DPRESENT_INTERVAL_IMMEDIATE for presentation");

        D3DCommonDevice* commonD3DDevice = m_commonSurf->GetCommonD3DDevice();

        d3d9::D3DPRESENT_PARAMETERS resetParams = *commonD3DDevice->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        HRESULT hrReset = commonD3DDevice->ResetD3D9Swapchain(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw7Surface::Flip: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(false);
        }
      // If the interface is not waiting for VBlank and we stop getting DDFLIP_NOVSYNC
      // flags, reset the swapchain in order to return to VSync presentation using DEFAULT
      } else if (unlikely(!m_commonIntf->GetWaitForVBlank() && IsVSyncFlipFlag(dwFlags))) {
        Logger::info("DDraw7Surface::Flip: Switching to D3DPRESENT_INTERVAL_DEFAULT for presentation");

        D3DCommonDevice* commonD3DDevice = m_commonSurf->GetCommonD3DDevice();

        d3d9::D3DPRESENT_PARAMETERS resetParams = *commonD3DDevice->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        HRESULT hrReset = commonD3DDevice->ResetD3D9Swapchain(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw7Surface::Flip: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(true);
        }
      }

      // Workaround for The Sims/other games flipping a different
      // swapchain than the one set on the current D3D device
      if (unlikely(m_commonIntf->GetOptions()->forceRTFlip && m_commonSurf->IsPrimarySurface())) {
        m_nextFlippable = m_commonSurf->GetCommonD3DDevice()->GetCurrentRenderTarget7();
      }

      if (likely(m_nextFlippable != nullptr)) {
        if (m_commonIntf->GetOptions()->emulateFrontBuffer) {
          InitializeOrUploadD3D9();
          // Workaround for front buffer image retention issues
          if (m_shadowSurf != nullptr && m_nextFlippable->GetCommonSurface()->IsDDrawSurfaceDirty())
            GetShadowOrProxied()->BltFast(0, 0, m_nextFlippable->GetShadowOrProxied(), nullptr, DDBLTFAST_NOCOLORKEY);
        }
        m_nextFlippable->InitializeOrUploadD3D9();
      } else {
        InitializeOrUploadD3D9();
      }

      d3d9Device->Present(NULL, NULL, NULL, NULL);

    } else {
      // Update the VBlank wait status based on the flip flags
      m_commonIntf->SetWaitForVBlank(IsVSyncFlipFlag(dwFlags));

      if (overrideSurf == nullptr) {
        return m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
      } else {
        return m_proxy->Flip(overrideSurf->GetShadowOrProxied(), dwFlags);
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE7 *lplpDDAttachedSurface) {
    if (unlikely(lpDDSCaps == nullptr || lplpDDAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<IDirectDrawSurface7> surface;
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
          Com<DDraw7Surface> ddraw7Surface = new DDraw7Surface(nullptr, std::move(surface), m_parent, this, false);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(ddraw7Surface->GetProxied()),
                                     std::forward_as_tuple(ddraw7Surface.ref()));
          *lplpDDAttachedSurface = ddraw7Surface.ref();
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
  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetBltStatus(DWORD dwFlags) {
    if (likely(dwFlags == DDGBS_CANBLT || dwFlags == DDGBS_ISBLTDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetCaps(LPDDSCAPS2 lpDDSCaps) {
    if (unlikely(lpDDSCaps == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDSCaps = m_commonSurf->GetDesc2()->ddsCaps;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper) {
    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    DDrawClipper* clipper = m_commonSurf->GetClipper();

    if (unlikely(clipper == nullptr))
      return DDERR_NOCLIPPERATTACHED;

    *lplpDDClipper = ref(clipper);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetDC(HDC *lphDC) {
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
  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetFlipStatus(DWORD dwFlags) {
    if (likely(dwFlags == DDGFS_CANFLIP || dwFlags == DDGFS_ISFLIPDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetOverlayPosition(LPLONG lplX, LPLONG lplY) {
    return m_proxy->GetOverlayPosition(lplX, lplY);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette) {
    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    DDrawPalette* palette = m_commonSurf->GetPalette();

    if (unlikely(palette == nullptr))
      return DDERR_NOPALETTEATTACHED;

    *lplpDDPalette = ref(palette);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    if (unlikely(lpDDPixelFormat == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDPixelFormat = m_commonSurf->GetDesc2()->ddpfPixelFormat;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetSurfaceDesc(LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    if (unlikely(lpDDSurfaceDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDDSurfaceDesc->dwSize != sizeof(DDSURFACEDESC2)))
      return DDERR_INVALIDPARAMS;

    *lpDDSurfaceDesc = *m_commonSurf->GetDesc2();

    return DD_OK;
  }

  // According to the docs: "Because the DirectDrawSurface object is initialized
  // when it's created, this method always returns DDERR_ALREADYINITIALIZED."
  HRESULT STDMETHODCALLTYPE DDraw7Surface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::IsLost() {
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC2 lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    // Write back any dirty surface data from bound D3D9 back buffers or depth stencils
    DownloadSurfaceData();

    return GetShadowOrProxied()->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::ReleaseDC(HDC hDC) {
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

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Restore() {
    return m_proxy->Restore();
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
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

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
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
      Logger::err("DDraw7Surface::SetColorKey: Failed to retrieve updated surface desc");

    if (unlikely(m_shadowSurf != nullptr)) {
      hr = m_shadowSurf->GetProxied()->SetColorKey(dwFlags, lpDDColorKey);
      if (unlikely(FAILED(hr))) {
        Logger::warn("DDraw7Surface::SetColorKey: Failed to set shadow surface color key");
      } else {
        hr = m_shadowSurf->GetCommonSurface()->RefreshSurfaceDescripton(false);
        if (unlikely(FAILED(hr)))
          Logger::warn("DDraw7Surface::SetColorKey: Failed to retrieve updated shadow surface desc");
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetOverlayPosition(LONG lX, LONG lY) {
    return m_proxy->SetOverlayPosition(lX, lY);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) {
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

  HRESULT STDMETHODCALLTYPE DDraw7Surface::Unlock(LPRECT lpSurfaceData) {
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

  HRESULT STDMETHODCALLTYPE DDraw7Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE7 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    if (unlikely(lpDDDestSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(lpDDDestSurface))) {
      Logger::err("DDraw7Surface::UpdateOverlay: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDDestSurface);
    return m_proxy->UpdateOverlay(lpSrcRect, ddraw7Surface->GetProxied(), lpDestRect, dwFlags, lpDDOverlayFx);
  }

  // Docs: "The IDirectDrawSurface7::UpdateOverlayDisplay method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw7Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE7 lpDDSReference) {
    if (unlikely(lpDDSReference != nullptr
              && !DDrawCommonInterface::IsWrappedSurface(lpDDSReference))) {
      Logger::err("DDraw7Surface::UpdateOverlayZOrder: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    if (lpDDSReference == nullptr) {
      return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
    } else {
      DDraw7Surface* ddraw7Surface = static_cast<DDraw7Surface*>(lpDDSReference);
      return m_proxy->UpdateOverlayZOrder(dwFlags, ddraw7Surface->GetProxied());
    }
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetDDInterface(LPVOID *lplpDD) {
    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDD);

    if (unlikely(m_parent == nullptr)) {
      Logger::err("DDraw7Surface::GetDDInterface: Found no valid parent interface");
      return DDERR_NOTFOUND;
    }

    *lplpDD = ref(m_parent);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::PageLock(DWORD dwFlags) {
    return m_proxy->PageLock(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::PageUnlock(DWORD dwFlags) {
    return m_proxy->PageUnlock(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetSurfaceDesc(LPDDSURFACEDESC2 lpDDSD, DWORD dwFlags) {
    // Can be used only to set the surface data and pixel format
    // used by an explicit system-memory surface (will be validated)
    HRESULT hr = m_proxy->SetSurfaceDesc(lpDDSD, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    hr = m_commonSurf->RefreshSurfaceDescripton(true);
    if (unlikely(FAILED(hr))) {
      Logger::err("DDraw4Surface::SetSurfaceDesc: Failed to retrieve updated surface desc");
      return hr;
    }

    // We may need to recreate the d3d9 object based on the new desc
    m_commonSurf->SetD3D9Surface(nullptr);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetPrivateData(REFGUID tag, LPVOID pData, DWORD cbSize, DWORD dwFlags) {
    return m_proxy->SetPrivateData(tag, pData, cbSize, dwFlags);
  }

  // Silent Hunter II needs to get these from the proxy surface, as it
  // sets them otherwise... it doesn't call SetPrivateData at all
  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetPrivateData(REFGUID tag, LPVOID pBuffer, LPDWORD pcbBufferSize) {
    return m_proxy->GetPrivateData(tag, pBuffer, pcbBufferSize);
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::FreePrivateData(REFGUID tag) {
    return m_proxy->FreePrivateData(tag);
  }

  // Docs: "The only defined uniqueness value is 0, indicating that the surface
  // is likely to be changing beyond the control of DirectDraw."
  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetUniquenessValue(LPDWORD pValue) {
    if (unlikely(pValue == nullptr))
      return DDERR_INVALIDPARAMS;

    *pValue = 0;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::ChangeUniquenessValue() {
    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetPriority(DWORD prio) {
    m_commonSurf->RefreshD3D9Device();

    if (unlikely(!m_commonSurf->IsInitialized()))
      return m_proxy->SetPriority(prio);

    if (unlikely(!m_commonSurf->IsManaged()))
      return DDERR_INVALIDOBJECT;

    HRESULT hr = m_proxy->SetPriority(prio);
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonSurf->GetD3D9Surface()->SetPriority(prio);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetPriority(LPDWORD prio) {
    m_commonSurf->RefreshD3D9Device();

    if (unlikely(!m_commonSurf->IsInitialized()))
      return m_proxy->GetPriority(prio);

    if (unlikely(prio == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonSurf->IsManaged()))
      return DDERR_INVALIDOBJECT;

    *prio = m_commonSurf->GetD3D9Surface()->GetPriority();

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::SetLOD(DWORD lod) {
    m_commonSurf->RefreshD3D9Device();

    if (unlikely(!m_commonSurf->IsInitialized()))
      return m_proxy->SetLOD(lod);

    if (unlikely(!m_commonSurf->IsManaged()))
      return DDERR_INVALIDOBJECT;

    HRESULT hr = m_proxy->SetLOD(lod);
    if (unlikely(FAILED(hr)))
      return hr;

    if (m_commonSurf->GetD3D9Texture() != nullptr) {
      m_commonSurf->GetD3D9Texture()->SetLOD(lod);
    } else if (m_commonSurf->GetD3D9CubeTexture() != nullptr) {
      m_commonSurf->GetD3D9CubeTexture()->SetLOD(lod);
    } else {
      Logger::warn("DDraw7Surface::SetLOD: Failed to set D3D9 LOD");
      return DDERR_INVALIDOBJECT;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw7Surface::GetLOD(LPDWORD lod) {
    m_commonSurf->RefreshD3D9Device();

    if (unlikely(!m_commonSurf->IsInitialized()))
      return m_proxy->GetLOD(lod);

    if (unlikely(lod == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonSurf->IsManaged()))
      return DDERR_INVALIDOBJECT;

    if (likely(m_commonSurf->GetD3D9Texture() != nullptr)) {
      *lod = m_commonSurf->GetD3D9Texture()->GetLOD();
    } else if (m_commonSurf->GetD3D9CubeTexture() != nullptr) {
      *lod = m_commonSurf->GetD3D9CubeTexture()->GetLOD();
    } else {
      Logger::warn("DDraw7Surface::GetLOD: Failed to get D3D9 LOD");
      return DDERR_INVALIDOBJECT;
    }

    return DD_OK;
  }

  IDirectDrawSurface7* DDraw7Surface::GetShadowOrProxied() {
    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->GetRefreshedD3D9Device();

    if (unlikely(m_shadowSurf != nullptr && d3d9Device != nullptr))
      return m_shadowSurf->GetProxied();

    return m_proxy.ptr();
  }

  HRESULT DDraw7Surface::InitializeD3D9RenderTarget() {
    m_commonSurf->RefreshD3D9Device();

    m_commonSurf->MarkAsD3D9BackBuffer();

    if (unlikely(!m_commonSurf->IsInitialized())) {
      if (m_commonSurf->IsTextureOrCubeMap())
        UpdateMipMapCount();

      HRESULT hr = m_commonSurf->InitializeD3D9(true);
      if (unlikely(FAILED(hr))) {
        Logger::err(str::format("DDraw7Surface::InitializeD3D9RenderTarget: Failed to initialize surface nr. [[7-", std::hex, this, "]]"));
        return hr;
      }

      if (unlikely(m_commonSurf->IsCubeMap()))
        InitializeAllCubeMapSurfaces();

      return UploadSurfaceData();
    }

    return DD_OK;
  }

  HRESULT DDraw7Surface::InitializeD3D9DepthStencil() {
    m_commonSurf->RefreshD3D9Device();

    m_commonSurf->MarkAsD3D9DepthStencil();

    if (unlikely(!m_commonSurf->IsInitialized())) {
      HRESULT hr = m_commonSurf->InitializeD3D9(false);
      if (unlikely(FAILED(hr))) {
        Logger::err(str::format("DDraw7Surface::InitializeD3D9DepthStencil: Failed to initialize surface nr. [[7-", std::hex, this, "]]"));
        return hr;
      }

      return UploadSurfaceData();
    }

    return DD_OK;
  }

  HRESULT DDraw7Surface::InitializeOrUploadD3D9() {
    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->GetRefreshedD3D9Device();

    // Fast skip
    if (unlikely(d3d9Device == nullptr))
      return DD_OK;

    if (unlikely(!m_commonSurf->IsInitialized())) {
      if (m_commonSurf->IsTextureOrCubeMap())
        UpdateMipMapCount();

      const bool initRenderTarget = m_commonSurf->GetCommonD3DDevice()->IsCurrentRenderTarget(m_commonSurf.ptr());

      HRESULT hr = m_commonSurf->InitializeD3D9(initRenderTarget);
      if (unlikely(FAILED(hr))) {
        Logger::err(str::format("DDraw7Surface::InitializeOrUploadD3D9: Failed to initialize surface nr. [[7-", std::hex, this, "]]"));
        return hr;
      }

      if (unlikely(m_commonSurf->IsCubeMap()))
        InitializeAllCubeMapSurfaces();
    }

    if (likely(m_commonSurf->IsInitialized()))
      return UploadSurfaceData();

    return DD_OK;
  }

  void DDraw7Surface::DownloadSurfaceData() {
    // Some games, like The Settlers IV, use multiple devices for rendering, one to handle
    // terrain and the overall 3D scene, and one to create textures/sprites to overlay on
    // top of it. Since DXVK's D3D9 backend does not restrict cross-device surface/texture
    // use, simply skip changing assigned surface devices during downloads. This is essentially
    // a hack, which by some miracle works well enough in some cases, though may explode in others.
    if (likely(!m_commonIntf->GetOptions()->deviceResourceSharing))
      m_commonSurf->RefreshD3D9Device();

    if (unlikely(m_commonSurf->IsD3D9BackBuffer())) {
      if (m_commonSurf->IsInitialized() && m_commonSurf->IsD3D9SurfaceDirty()) {
        //Logger::debug(str::format("DDraw7Surface::DownloadSurfaceData: Downloading nr. [[7-", std::hex, this, "]]"));
        BlitToDDrawSurface<IDirectDrawSurface7, DDSURFACEDESC2>(GetShadowOrProxied(), m_commonSurf->GetD3D9Surface(),
                                                                m_commonSurf->IsDXTFormat());
        m_commonSurf->UnDirtyD3D9Surface();
      }
    } else if (unlikely(m_commonSurf->IsD3D9DepthStencil())) {
      if (m_commonSurf->IsInitialized() && m_commonSurf->IsD3D9SurfaceDirty()) {
        //Logger::debug(str::format("DDraw7Surface::DownloadSurfaceData: Downloading nr. [[7-", std::hex, this, "]]"));
        BlitToDDrawSurface<IDirectDrawSurface7, DDSURFACEDESC2>(m_proxy.ptr(), m_commonSurf->GetD3D9Surface(),
                                                                m_commonSurf->IsDXTFormat());
        m_commonSurf->UnDirtyD3D9Surface();
      }
    }
  }

  inline void DDraw7Surface::UpdateMipMapCount() {
    // We need to count the number of actual mips on initialization by going through
    // the mip chain, since the dwMipMapCount number may or may not be accurate. I am
    // guessing it was intended more as a hint, not neceesarily a set number.
    const DDSURFACEDESC2* desc2  = m_commonSurf->GetDesc2();

    IDirectDrawSurface7* mipMap = m_proxy.ptr();
    DDSURFACEDESC2 mipDesc2;
    uint16_t mipCount = 1;

    while (mipMap != nullptr) {
      IDirectDrawSurface7* parentSurface = mipMap;
      mipMap = nullptr;
      parentSurface->EnumAttachedSurfaces(&mipMap, ListMipChainSurfaces7Callback);
      if (mipMap != nullptr) {
        mipCount++;

        mipDesc2 = { };
        mipDesc2.dwSize = sizeof(DDSURFACEDESC2);
        mipMap->GetSurfaceDesc(&mipDesc2);
        // Ignore multiple 1x1 mips, which apparently can get generated if the
        // application gets the dwMipMapCount wrong vs surface dimensions.
        if (unlikely(mipDesc2.dwWidth == 1 && mipDesc2.dwHeight == 1))
          break;
      }
    }

    // Do not worry about maximum supported mip map levels validation,
    // because D3D9 will handle this for us and cap them appropriately
    if (mipCount > 1) {
      //Logger::debug(str::format("DDraw7Surface::UpdateMipMapCount: Found ", mipCount, " mip levels"));

      if (unlikely(mipCount != desc2->dwMipMapCount))
        Logger::debug(str::format("DDraw7Surface::UpdateMipMapCount: Mismatch with declared ", desc2->dwMipMapCount, " mip levels"));
    }

    if (unlikely(m_commonIntf->GetOptions()->autoGenMipMaps))
      mipCount = 0;

    m_commonSurf->SetMipCount(mipCount);
  }

  inline void DDraw7Surface::InitializeAndAttachCubeFace(
        IDirectDrawSurface7* surf,
        d3d9::IDirect3DCubeTexture9* cubeTex9,
        d3d9::D3DCUBEMAP_FACES face) {
    Com<DDraw7Surface> face7;

    Com<IDirectDrawSurface7> faceProxied = surf;
    try {
      face7 = new DDraw7Surface(nullptr, std::move(faceProxied), m_parent, this, false);
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      Logger::err("InitializeAndAttachCubeFace: Failed to create wrapped cube face surface");
    }

    if (likely(face7 != nullptr)) {
      Com<d3d9::IDirect3DSurface9> face9;
      cubeTex9->GetCubeMapSurface(face, 0, &face9);
      face7->GetCommonSurface()->SetD3D9Surface(std::move(face9));
      m_attachedSurfaces.emplace(std::piecewise_construct,
                                std::forward_as_tuple(face7->GetProxied()),
                                std::forward_as_tuple(face7.ref()));
    }
  }

  inline void DDraw7Surface::InitializeAllCubeMapSurfaces() {
    CubeMapAttachedSurfaces cubeMapAttachedSurfaces;
    m_proxy->EnumAttachedSurfaces(&cubeMapAttachedSurfaces, EnumAndAttachCubeMapFacesCallback);

    // The positive X surfaces is this surface, so nothing should be retrieved
    if (unlikely(cubeMapAttachedSurfaces.positiveX != nullptr))
      Logger::warn("DDraw7Surface::InitializeAllCubeMapSurfaces: Non-null positive X cube map face");

    m_cubeMapSurfaces[0] = m_proxy.ptr();

    // We can't know in advance which faces have been generated,
    // so check them one by one, initialize and bind as needed
    if (cubeMapAttachedSurfaces.negativeX != nullptr) {
      m_cubeMapSurfaces[1] = cubeMapAttachedSurfaces.negativeX;
      InitializeAndAttachCubeFace(cubeMapAttachedSurfaces.negativeX, m_commonSurf->GetD3D9CubeTexture(),
                                  d3d9::D3DCUBEMAP_FACE_NEGATIVE_X);
    }
    if (cubeMapAttachedSurfaces.positiveY != nullptr) {
      m_cubeMapSurfaces[2] = cubeMapAttachedSurfaces.positiveY;
      InitializeAndAttachCubeFace(cubeMapAttachedSurfaces.positiveY, m_commonSurf->GetD3D9CubeTexture(),
                                  d3d9::D3DCUBEMAP_FACE_POSITIVE_Y);
    }
    if (cubeMapAttachedSurfaces.negativeY != nullptr) {
      m_cubeMapSurfaces[3] = cubeMapAttachedSurfaces.negativeY;
      InitializeAndAttachCubeFace(cubeMapAttachedSurfaces.negativeY, m_commonSurf->GetD3D9CubeTexture(),
                                  d3d9::D3DCUBEMAP_FACE_NEGATIVE_Y);
    }
    if (cubeMapAttachedSurfaces.positiveZ != nullptr) {
      m_cubeMapSurfaces[4] = cubeMapAttachedSurfaces.positiveZ;
      InitializeAndAttachCubeFace(cubeMapAttachedSurfaces.positiveZ, m_commonSurf->GetD3D9CubeTexture(),
                                  d3d9::D3DCUBEMAP_FACE_POSITIVE_Z);
    }
    if (cubeMapAttachedSurfaces.negativeZ != nullptr) {
      m_cubeMapSurfaces[5] = cubeMapAttachedSurfaces.negativeZ;
      InitializeAndAttachCubeFace(cubeMapAttachedSurfaces.negativeZ, m_commonSurf->GetD3D9CubeTexture(),
                                  d3d9::D3DCUBEMAP_FACE_NEGATIVE_Z);
    }
  }

  inline HRESULT DDraw7Surface::UploadSurfaceData() {
    //Fast skip
    if (!m_commonSurf->IsDDrawSurfaceDirty())
      return DD_OK;

    //Logger::debug(str::format("DDraw7Surface::UploadSurfaceData: Uploading nr. [[7-", std::hex, this, "]]"));

    // Cube maps will also get marked as textures, so need to be handled first
    if (unlikely(m_commonSurf->IsCubeMap())) {
      // In theory we won't know which faces have been generated,
      // so check them one by one, and upload as needed
      const uint16_t mipCount    = m_commonSurf->GetMipCount();
      const bool     isDXTFormat = m_commonSurf->IsDXTFormat();
      if (likely(m_cubeMapSurfaces[0] != nullptr)) {
        BlitToD3D9CubeMap(m_commonSurf->GetD3D9CubeTexture(), m_cubeMapSurfaces[0], mipCount, isDXTFormat);
      }
      if (likely(m_cubeMapSurfaces[1] != nullptr)) {
        BlitToD3D9CubeMap(m_commonSurf->GetD3D9CubeTexture(), m_cubeMapSurfaces[1], mipCount, isDXTFormat);
      }
      if (likely(m_cubeMapSurfaces[2] != nullptr)) {
        BlitToD3D9CubeMap(m_commonSurf->GetD3D9CubeTexture(), m_cubeMapSurfaces[2], mipCount, isDXTFormat);
      }
      if (likely(m_cubeMapSurfaces[3] != nullptr)) {
        BlitToD3D9CubeMap(m_commonSurf->GetD3D9CubeTexture(), m_cubeMapSurfaces[3], mipCount, isDXTFormat);
      }
      if (likely(m_cubeMapSurfaces[4] != nullptr)) {
        BlitToD3D9CubeMap(m_commonSurf->GetD3D9CubeTexture(), m_cubeMapSurfaces[4], mipCount, isDXTFormat);
      }
      if (likely(m_cubeMapSurfaces[5] != nullptr)) {
        BlitToD3D9CubeMap(m_commonSurf->GetD3D9CubeTexture(), m_cubeMapSurfaces[5], mipCount, isDXTFormat);
      }
    // Blit all the mips for textures
    } else if (m_commonSurf->IsTexture()) {
      BlitToD3D9Texture<IDirectDrawSurface7, DDSURFACEDESC2>(m_commonSurf->GetD3D9Texture(), m_proxy.ptr(),
                                                             m_commonSurf->GetMipCount(), m_commonSurf->IsDXTFormat());
    // Blit surfaces directly
    } else {
      BlitToD3D9Surface<IDirectDrawSurface7, DDSURFACEDESC2>(m_commonSurf->GetD3D9Surface(), GetShadowOrProxied(),
                                                             m_commonSurf->IsDXTFormat());
    }

    m_commonSurf->UnDirtyDDrawSurface();

    return DD_OK;
  }

}
