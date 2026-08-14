#include "ddraw4_surface.h"

#include "../d3d_common_device.h"

#include "../ddraw_gamma.h"
#include "../ddraw/ddraw_surface.h"
#include "../ddraw2/ddraw2_surface.h"
#include "../ddraw2/ddraw3_surface.h"
#include "../ddraw7/ddraw7_surface.h"

namespace dxvk {

  DDraw4Surface::DDraw4Surface(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawSurface4>&& surfProxy,
        DDraw4Interface* pParent,
        DDraw4Surface* pParentSurf,
        bool isChildObject)
    : DDrawWrappedObject<DDraw4Interface, IDirectDrawSurface4>(pParent, std::move(surfProxy))
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
      throw DxvkError("DDraw4Surface: ERROR! Failed to retrieve the common interface!");
    }

    if (m_commonSurf == nullptr)
      m_commonSurf = new DDrawCommonSurface(m_commonIntf);

    // Retrieve and cache the proxy surface desc
    if (!m_commonSurf->IsDesc2Set()) {
      DDSURFACEDESC2 desc2;
      desc2.dwSize = sizeof(DDSURFACEDESC2);
      HRESULT hr = m_proxy->GetSurfaceDesc(&desc2);

      if (unlikely(FAILED(hr))) {
        throw DxvkError("DDraw4Surface: ERROR! Failed to retrieve new surface desc!");
      } else {
        m_commonSurf->SetDesc2(desc2);
      }
    }

    // Retrieve and cache the next surface in a flippable chain
    if (unlikely(m_commonSurf->IsFlippable() && !m_commonSurf->IsBackBuffer())) {
      IDirectDrawSurface4* nextFlippable = nullptr;
      EnumAttachedSurfaces(&nextFlippable, ListBackBufferSurfaces4Callback);
      m_nextFlippable = reinterpret_cast<DDraw4Surface*>(nextFlippable);
      if (likely(m_nextFlippable != nullptr)) {
        // The call to EnumAttachedSurfaces has incremented the public ref
        m_nextFlippable->Release();
        //Logger::debug("DDraw4Surface: Retrieved the next swapchain surface");
      }
    }

    DDrawCommonInterface::AddWrappedSurface(this);

    m_commonSurf->SetDD4Surface(this);

    if (m_parentSurf != nullptr
     && m_parentSurf->GetCommonSurface()->IsBackBufferOrFlippable()) {
      const uint32_t index = m_parentSurf->GetCommonSurface()->GetBackBufferIndex();
      m_commonSurf->IncrementBackBufferIndex(index);
    }

    if (m_parent != nullptr && m_isChildObject)
      m_parent->AddRef();

    //Logger::debug(str::format("DDraw4Surface: Created a new surface nr. [[4-", std::hex, this, "]]"));

    if (m_commonSurf->GetOrigin() == nullptr) {
      m_commonSurf->SetOrigin(this);
      m_commonSurf->SetIsAttached(m_parentSurf != nullptr);
      //m_commonSurf->ListSurfaceDetails();
    }
  }

  DDraw4Surface::~DDraw4Surface() {
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

    m_commonSurf->SetDD4Surface(nullptr);

    //Logger::debug(str::format("DDraw4Surface: Surface nr. [[4-", std::hex, this, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Shouldn't ever be called in practice
    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
      if (unlikely(m_texture3 == nullptr)) {
        Com<IDirect3DTexture> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        m_texture3 = new D3D3Texture(m_texture6 != nullptr ? m_texture6->GetCommonTexture() : nullptr,
                                     m_commonSurf.ptr(), std::move(ppvProxyObject), this);
      }

      *ppvObject = m_texture3.ref();

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture2))) {
      if (unlikely(m_texture6 == nullptr)) {
        Com<IDirect3DTexture2> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        // D3D5Texture (aka IDirect3DTexture2) is shared between D3D5 and D3D6
        m_texture6 = new D3D5Texture(m_texture3 != nullptr ? m_texture3->GetCommonTexture() : nullptr,
                                     m_commonSurf.ptr(), std::move(ppvProxyObject), this, true);
      }

      *ppvObject = m_texture6.ref();

      return S_OK;
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
    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      if (m_commonSurf->GetDDSurface() != nullptr)
        return m_commonSurf->GetDDSurface()->QueryInterface(riid, ppvObject);

      Com<IDirectDrawSurface> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      Com<DDrawSurface> surf;
      try {
        surf = new DDrawSurface(m_commonSurf.ptr(), std::move(ppvProxyObject),
                                m_commonIntf->GetDDInterface(), nullptr, false);
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return E_NOINTERFACE;
      }

      // The Sims is an absolute joke of a game that goes out of its way to
      // release the parent IDirectDrawSurface4 object, after querying
      // for a IDirect3DTexture2 object on IDirectDrawSurface. Just because.
      // Make sure we keep a private reference to prevent UAF in this case.
      if (unlikely(m_commonIntf->GetOptions()->robustTextureLifeCycle))
        surf->SetParentSurface4(this);

      *ppvObject = surf.ref();

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

    if (likely(riid == __uuidof(IDirectDrawSurface4))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("DDraw4Surface::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::AddAttachedSurface(LPDIRECTDRAWSURFACE4 lpDDSAttachedSurface) {
    if (unlikely(lpDDSAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(lpDDSAttachedSurface))) {
      Logger::err("DDraw4Surface::AddAttachedSurface: Received an unwrapped surface");
      return DDERR_CANNOTATTACHSURFACE;
    }

    DDraw4Surface* attachedSurf = static_cast<DDraw4Surface*>(lpDDSAttachedSurface);

    HRESULT hr = m_proxy->AddAttachedSurface(attachedSurf->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    attachedSurf->SetParentSurface(this);

    if (likely(attachedSurf->GetCommonSurface()->IsDepthStencil())) {
      m_depthStencil = attachedSurf;
    // If a flippable surface is attached, mark it as the next flippable surface
    } else if (unlikely(attachedSurf->GetCommonSurface()->IsBackBufferOrFlippable())) {
      m_nextFlippable = attachedSurf;
    }

    return DD_OK;
  }

  // Docs: "The IDirectDrawSurface4::AddOverlayDirtyRect method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw4Surface::AddOverlayDirtyRect(LPRECT lpRect) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    if (unlikely(lpDDSrcSurface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSrcSurface))) {
      Logger::err("DDraw4Surface::Blt: Received an unwrapped source surface");
      return DDERR_UNSUPPORTED;
    }

    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    if (likely(lpDDSrcSurface != nullptr)) {
      DDraw4Surface* sourceSurface = static_cast<DDraw4Surface*>(lpDDSrcSurface);
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
        DDraw4Surface* sourceSurface = static_cast<DDraw4Surface*>(lpDDSrcSurface);
        DDraw4Surface* renderTarget = m_commonSurf->GetCommonD3DDevice()->GetCurrentRenderTarget4();

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
      DDraw4Surface* sourceSurface = static_cast<DDraw4Surface*>(lpDDSrcSurface);
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

  // Docs: "The IDirectDrawSurface4::BltBatch method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw4Surface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE4 lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    if (unlikely(lpDDSrcSurface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSrcSurface))) {
      Logger::err("DDraw4Surface::BltFast: Received an unwrapped source surface");
      return DDERR_UNSUPPORTED;
    }

    const RECT* sourceFullSurfaceRect = nullptr;
    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    if (likely(lpDDSrcSurface != nullptr)) {
      DDraw4Surface* sourceSurface = static_cast<DDraw4Surface*>(lpDDSrcSurface);
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
        DDraw4Surface* sourceSurface = static_cast<DDraw4Surface*>(lpDDSrcSurface);
        DDraw4Surface* renderTarget = m_commonSurf->GetCommonD3DDevice()->GetCurrentRenderTarget4();

        if (sourceSurface == renderTarget) {
          renderTarget->InitializeOrUploadD3D9();
          d3d9Device->Present(NULL, NULL, NULL, NULL);
          return DD_OK;
        }
      }
    }

    HRESULT hr;
    if (lpDDSrcSurface == nullptr) {
      hr = GetShadowOrProxied()->BltFast(dwX, dwY, lpDDSrcSurface, lpSrcRect, dwTrans);
    } else {
      DDraw4Surface* sourceSurface = static_cast<DDraw4Surface*>(lpDDSrcSurface);
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
  HRESULT STDMETHODCALLTYPE DDraw4Surface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSAttachedSurface) {
    if (unlikely(lpDDSAttachedSurface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSAttachedSurface))) {
      Logger::err("DDraw4Surface::DeleteAttachedSurface: Received an unwrapped surface");
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

    DDraw4Surface* attachedSurf = static_cast<DDraw4Surface*>(lpDDSAttachedSurface);

    HRESULT hr = m_proxy->DeleteAttachedSurface(dwFlags, attachedSurf->GetProxied());
    if (unlikely(FAILED(hr)))
      return hr;

    attachedSurf->SetParentSurface(nullptr);

    if (likely(m_depthStencil == attachedSurf)) {
      m_depthStencil = nullptr;
    // Clear the next flippable surface or flippable surface detachment
    } else if (unlikely(attachedSurf->GetCommonSurface()->IsBackBufferOrFlippable() &&
                        attachedSurf == m_nextFlippable)) {
      m_nextFlippable = nullptr;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpEnumSurfacesCallback) {
    if (unlikely(lpEnumSurfacesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    std::vector<AttachedSurface4> attachedSurfaces;
    // Enumerate all attached surfaces from the underlying DDraw implementation
    HRESULT hr = m_proxy->EnumAttachedSurfaces(reinterpret_cast<void*>(&attachedSurfaces), EnumAttachedSurfaces4Callback);
    if (unlikely(FAILED(hr)))
      return hr;

    hr = DDENUMRET_OK;

    // Wrap surfaces as needed and perform the actual callback the application is requesting
    auto surfaceIt = attachedSurfaces.begin();
    while (surfaceIt != attachedSurfaces.end() && hr == DDENUMRET_OK) {
      Com<IDirectDrawSurface4> surface4 = surfaceIt->surface4;

      try {
        auto attachedSurfaceIter = m_attachedSurfaces.find(surface4.ptr());
        if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
          // Return the already attached depth surface if it exists
          if (unlikely(m_depthStencil != nullptr && surface4.ptr() == m_depthStencil->GetProxied())) {
            hr = lpEnumSurfacesCallback(m_depthStencil.ref(), &surfaceIt->desc2, lpContext);
          } else {
            Com<DDraw4Surface> ddraw4Surface = new DDraw4Surface(nullptr, std::move(surface4), m_commonIntf->GetDD4Interface(), this, false);
            m_attachedSurfaces.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(ddraw4Surface->GetProxied()),
                                       std::forward_as_tuple(ddraw4Surface.ref()));
            hr = lpEnumSurfacesCallback(ddraw4Surface.ref(), &surfaceIt->desc2, lpContext);
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

  HRESULT STDMETHODCALLTYPE DDraw4Surface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpfnCallback) {
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Flip(LPDIRECTDRAWSURFACE4 lpDDSurfaceTargetOverride, DWORD dwFlags) {
    if (unlikely(lpDDSurfaceTargetOverride != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSurfaceTargetOverride))) {
      Logger::err("DDraw4Surface::Flip: Received an unwrapped override surface");
      return DDERR_UNSUPPORTED;
    }

    Com<DDraw4Surface> overrideSurf = static_cast<DDraw4Surface*>(lpDDSurfaceTargetOverride);

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
        Logger::info("DDraw4Surface::Flip: Switching to D3DPRESENT_INTERVAL_IMMEDIATE for presentation");

        D3DCommonDevice* commonD3DDevice = m_commonSurf->GetCommonD3DDevice();

        d3d9::D3DPRESENT_PARAMETERS resetParams = *commonD3DDevice->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        HRESULT hrReset = commonD3DDevice->ResetD3D9Swapchain(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw4Surface::Flip: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(false);
        }
      // If the interface is not waiting for VBlank and we stop getting DDFLIP_NOVSYNC
      // flags, reset the swapchain in order to return to VSync presentation using DEFAULT
      } else if (unlikely(!m_commonIntf->GetWaitForVBlank() && IsVSyncFlipFlag(dwFlags))) {
        Logger::info("DDraw4Surface::Flip: Switching to D3DPRESENT_INTERVAL_DEFAULT for presentation");

        D3DCommonDevice* commonD3DDevice = m_commonSurf->GetCommonD3DDevice();

        d3d9::D3DPRESENT_PARAMETERS resetParams = *commonD3DDevice->GetPresentParameters();
        resetParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        HRESULT hrReset = commonD3DDevice->ResetD3D9Swapchain(&resetParams);
        if (unlikely(FAILED(hrReset))) {
          Logger::warn("DDraw4Surface::Flip: Failed D3D9 swapchain reset");
        } else {
          m_commonIntf->SetWaitForVBlank(true);
        }
      }

      // Workaround for The Sims/other games flipping a different
      // swapchain than the one set on the current D3D device
      if (unlikely(m_commonIntf->GetOptions()->forceRTFlip && m_commonSurf->IsPrimarySurface())) {
        m_nextFlippable = m_commonSurf->GetCommonD3DDevice()->GetCurrentRenderTarget4();
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

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE4 *lplpDDAttachedSurface) {
    if (unlikely(lpDDSCaps == nullptr || lplpDDAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<IDirectDrawSurface4> surface;
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
          Com<DDraw4Surface> ddraw4Surface = new DDraw4Surface(nullptr, std::move(surface), m_commonIntf->GetDD4Interface(), this, false);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(ddraw4Surface->GetProxied()),
                                     std::forward_as_tuple(ddraw4Surface.ref()));
          *lplpDDAttachedSurface = ddraw4Surface.ref();
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
  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetBltStatus(DWORD dwFlags) {
    if (likely(dwFlags == DDGBS_CANBLT || dwFlags == DDGBS_ISBLTDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetCaps(LPDDSCAPS2 lpDDSCaps) {
    if (unlikely(lpDDSCaps == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDSCaps = m_commonSurf->GetDesc2()->ddsCaps;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper) {
    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    DDrawClipper* clipper = m_commonSurf->GetClipper();

    if (unlikely(clipper == nullptr))
      return DDERR_NOCLIPPERATTACHED;

    *lplpDDClipper = ref(clipper);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetDC(HDC *lphDC) {
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
  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetFlipStatus(DWORD dwFlags) {
    if (likely(dwFlags == DDGFS_CANFLIP || dwFlags == DDGFS_ISFLIPDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetOverlayPosition(LPLONG lplX, LPLONG lplY) {
    return m_proxy->GetOverlayPosition(lplX, lplY);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette) {
    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    DDrawPalette* palette = m_commonSurf->GetPalette();

    if (unlikely(palette == nullptr))
      return DDERR_NOPALETTEATTACHED;

    *lplpDDPalette = ref(palette);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    if (unlikely(lpDDPixelFormat == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDPixelFormat = m_commonSurf->GetDesc2()->ddpfPixelFormat;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetSurfaceDesc(LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    if (unlikely(lpDDSurfaceDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDDSurfaceDesc->dwSize != sizeof(DDSURFACEDESC2)))
      return DDERR_INVALIDPARAMS;

    *lpDDSurfaceDesc = *m_commonSurf->GetDesc2();

    return DD_OK;
  }

  // According to the docs: "Because the DirectDrawSurface object is initialized
  // when it's created, this method always returns DDERR_ALREADYINITIALIZED."
  HRESULT STDMETHODCALLTYPE DDraw4Surface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::IsLost() {
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC2 lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    // Write back any dirty surface data from bound D3D9 back buffers or depth stencils
    DownloadSurfaceData();

    HRESULT hr = GetShadowOrProxied()->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
    if (unlikely(FAILED(hr)))
      return hr;

    // For single surface locks, track the READONLY flag in order to skip dirtying
    // on Unlock(). Reset the tracking in case of multiple simultaneous locks.
    if (likely(!m_lockCount)) {
      m_readOnlyLock = (dwFlags & DDLOCK_READONLY) && !(dwFlags & DDLOCK_WRITEONLY);
    } else {
      m_readOnlyLock = false;
    }

    m_lockCount++;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::ReleaseDC(HDC hDC) {
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

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Restore() {
    return m_proxy->Restore();
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
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

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
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
      Logger::err("DDraw4Surface::SetColorKey: Failed to retrieve updated surface desc");

    if (unlikely(m_shadowSurf != nullptr)) {
      hr = m_shadowSurf->GetProxied()->SetColorKey(dwFlags, lpDDColorKey);
      if (unlikely(FAILED(hr))) {
        Logger::warn("DDraw4Surface::SetColorKey: Failed to set shadow surface color key");
      } else {
        hr = m_shadowSurf->GetCommonSurface()->RefreshSurfaceDescripton(false);
        if (unlikely(FAILED(hr)))
          Logger::warn("DDraw4Surface::SetColorKey: Failed to retrieve updated shadow surface desc");
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetOverlayPosition(LONG lX, LONG lY) {
    return m_proxy->SetOverlayPosition(lX, lY);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) {
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

  HRESULT STDMETHODCALLTYPE DDraw4Surface::Unlock(LPRECT lpSurfaceData) {
    HRESULT hr = GetShadowOrProxied()->Unlock(lpSurfaceData);
    if (unlikely(FAILED(hr)))
      return hr;

    if (!m_readOnlyLock) {
      m_commonSurf->DirtyDDrawSurface();
    } else {
      m_readOnlyLock = false;
    }

    if (likely(m_lockCount))
      m_lockCount--;

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

  HRESULT STDMETHODCALLTYPE DDraw4Surface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE4 lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    if (unlikely(lpDDDestSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(lpDDDestSurface))) {
      Logger::err("DDraw4Surface::UpdateOverlay: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDDestSurface);
    return m_proxy->UpdateOverlay(lpSrcRect, ddraw4Surface->GetProxied(), lpDestRect, dwFlags, lpDDOverlayFx);
  }

  // Docs: "The IDirectDrawSurface4::UpdateOverlayDisplay method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw4Surface::UpdateOverlayDisplay(DWORD dwFlags) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE4 lpDDSReference) {
    if (unlikely(lpDDSReference != nullptr
              && !DDrawCommonInterface::IsWrappedSurface(lpDDSReference))) {
      Logger::err("DDraw4Surface::UpdateOverlayZOrder: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    if (lpDDSReference == nullptr) {
      return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
    } else {
      DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSReference);
      return m_proxy->UpdateOverlayZOrder(dwFlags, ddraw4Surface->GetProxied());
    }
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetDDInterface(LPVOID *lplpDD) {
    if (unlikely(lplpDD == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDD);

    if (unlikely(m_commonIntf->GetDD4Interface() == nullptr)) {
      Logger::err("DDraw4Surface::GetDDInterface: Found no valid parent interface");
      return DDERR_NOTFOUND;
    }

    *lplpDD = ref(m_commonIntf->GetDD4Interface());

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::PageLock(DWORD dwFlags) {
    return m_proxy->PageLock(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::PageUnlock(DWORD dwFlags) {
    return m_proxy->PageUnlock(dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetSurfaceDesc(LPDDSURFACEDESC2 lpDDSD, DWORD dwFlags) {
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
    m_commonSurf->ResetD3D9Objects();

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::SetPrivateData(REFGUID tag, LPVOID pData, DWORD cbSize, DWORD dwFlags) {
    return m_proxy->SetPrivateData(tag, pData, cbSize, dwFlags);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetPrivateData(REFGUID tag, LPVOID pBuffer, LPDWORD pcbBufferSize) {
    return m_proxy->GetPrivateData(tag, pBuffer, pcbBufferSize);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::FreePrivateData(REFGUID tag) {
    return m_proxy->FreePrivateData(tag);
  }

  // Docs: "The only defined uniqueness value is 0, indicating that the surface
  // is likely to be changing beyond the control of DirectDraw."
  HRESULT STDMETHODCALLTYPE DDraw4Surface::GetUniquenessValue(LPDWORD pValue) {
    if (unlikely(pValue == nullptr))
      return DDERR_INVALIDPARAMS;

    *pValue = 0;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Surface::ChangeUniquenessValue() {
    return DD_OK;
  }

  HRESULT DDraw4Surface::InitializeD3D9RenderTarget() {
    // Currently ignores all P8 surfaces
    if (unlikely(m_commonSurf->SkipD3D9Operations()))
      return DD_OK;

    m_commonSurf->RefreshD3D9Device();

    if (unlikely(!m_commonSurf->IsInitialized())) {
      HRESULT hr = m_commonSurf->InitializeD3D9(true);
      if (unlikely(FAILED(hr))) {
        Logger::err(str::format("DDraw4Surface::InitializeD3D9RenderTarget: Failed to initialize surface nr. [[4-", std::hex, this, "]]"));
        return hr;
      }

      return UploadSurfaceData();
    }

    return DD_OK;
  }

  HRESULT DDraw4Surface::InitializeD3D9DepthStencil() {
    m_commonSurf->RefreshD3D9Device();

    if (unlikely(!m_commonSurf->IsInitialized())) {
      HRESULT hr = m_commonSurf->InitializeD3D9(false);
      if (unlikely(FAILED(hr))) {
        Logger::err(str::format("DDraw4Surface::InitializeD3D9DepthStencil: Failed to initialize surface nr. [[4-", std::hex, this, "]]"));
        return hr;
      }

      return UploadSurfaceData();
    }

    return DD_OK;
  }

  HRESULT DDraw4Surface::InitializeOrUploadD3D9() {
    // Currently ignores all P8 surfaces
    if (unlikely(m_commonSurf->SkipD3D9Operations()))
      return DD_OK;

    d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->GetRefreshedD3D9Device();

    // Fast skip
    if (unlikely(d3d9Device == nullptr))
      return DD_OK;

    if (unlikely(!m_commonSurf->IsInitialized())) {
      const bool initRenderTarget = m_commonSurf->GetCommonD3DDevice()->IsCurrentRenderTarget(m_commonSurf.ptr());

      HRESULT hr = m_commonSurf->InitializeD3D9(initRenderTarget);
      if (unlikely(FAILED(hr))) {
        Logger::err(str::format("DDraw4Surface::InitializeOrUploadD3D9: Failed to initialize surface nr. [[4-", std::hex, this, "]]"));
        return hr;
      }
    }

    return UploadSurfaceData();
  }

  void DDraw4Surface::DownloadSurfaceData() {
    // Some games, like The Settlers IV, use multiple devices for rendering, one to handle
    // terrain and the overall 3D scene, and one to create textures/sprites to overlay on
    // top of it. Since DXVK's D3D9 backend does not restrict cross-device surface/texture
    // use, simply skip changing assigned surface devices during downloads. This is essentially
    // a hack, which by some miracle works well enough in some cases, though may explode in others.
    if (likely(!m_commonIntf->GetOptions()->deviceResourceSharing))
      m_commonSurf->RefreshD3D9Device();

    // TODO: We are technically ignoring mip maps as is, though that will probably never be an issue
    if (m_commonSurf->IsD3D9SurfaceDirty() && m_commonSurf->IsInitialized()) {
      //Logger::debug(str::format("DDraw4Surface::DownloadSurfaceData: Downloading nr. [[4-", std::hex, this, "]]"));
      BlitToDDrawSurface<IDirectDrawSurface4, DDSURFACEDESC2>(GetShadowOrProxied(), m_commonSurf->GetD3D9Surface(),
                                                              m_commonSurf->IsDXTFormat());
      m_commonSurf->UnDirtyD3D9Surface();
    }
  }

  inline HRESULT DDraw4Surface::UploadSurfaceData() {
    // Fast skip
    if (!m_commonSurf->IsDDrawSurfaceDirty())
      return DD_OK;

    //Logger::debug(str::format("DDraw4Surface::UploadSurfaceData: Uploading nr. [[4-", std::hex, this, "]]"));

    D3D9SurfaceType d3d9SurfaceType = m_commonSurf->GetD3D9SurfaceType();

    switch (d3d9SurfaceType) {
      case D3D9SurfaceType::Texture:
        BlitToD3D9Texture<IDirectDrawSurface4, DDSURFACEDESC2>(m_commonSurf->GetD3D9Texture(), m_proxy.ptr(),
                                                               m_commonSurf->GetMipCount(), m_commonSurf->IsDXTFormat());
        break;
      default:
        BlitToD3D9Surface<IDirectDrawSurface4, DDSURFACEDESC2>(m_commonSurf->GetD3D9Surface(), GetShadowOrProxied(),
                                                               m_commonSurf->IsDXTFormat());
        break;
    }

    m_commonSurf->UnDirtyDDrawSurface();

    return DD_OK;
  }

}
