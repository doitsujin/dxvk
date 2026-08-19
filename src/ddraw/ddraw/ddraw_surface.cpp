#include "ddraw_surface.h"

#include "../d3d_common_device.h"

#include "../ddraw_gamma.h"
#include "../ddraw2/ddraw2_surface.h"
#include "../ddraw2/ddraw3_surface.h"
#include "../ddraw4/ddraw4_surface.h"
#include "../ddraw7/ddraw7_surface.h"

#include "../d3d3/d3d3_device.h"
#include "../d3d3/d3d3_interface.h"

namespace dxvk {

  DDrawSurface::DDrawSurface(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawSurface>&& surfProxy,
        DDrawInterface* pParent,
        DDrawSurface* pParentSurf,
        bool isChildObject)
    : DDrawWrappedObject<DDrawInterface, IDirectDrawSurface>(pParent, std::move(surfProxy))
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
      throw DxvkError("DDrawSurface: ERROR! Failed to retrieve the common interface!");
    }

    if (m_commonSurf == nullptr)
      m_commonSurf = new DDrawCommonSurface(m_commonIntf);

    // Retrieve and cache the proxy surface desc
    if (!m_commonSurf->IsDescSet()) {
      DDSURFACEDESC desc;
      desc.dwSize = sizeof(DDSURFACEDESC);
      HRESULT hr = m_proxy->GetSurfaceDesc(&desc);

      if (unlikely(FAILED(hr))) {
        throw DxvkError("DDrawSurface: ERROR! Failed to retrieve new surface desc!");
      } else {
        m_commonSurf->SetDesc(desc);
      }
    }

    // Retrieve and cache the next surface in a flippable chain
    if (unlikely(m_commonSurf->IsFlippable() && !m_commonSurf->IsBackBuffer())) {
      IDirectDrawSurface* nextFlippable = nullptr;
      EnumAttachedSurfaces(&nextFlippable, ListBackBufferSurfacesCallback);
      m_nextFlippable = reinterpret_cast<DDrawSurface*>(nextFlippable);
      if (likely(m_nextFlippable != nullptr)) {
        // The call to EnumAttachedSurfaces has incremented the public ref
        m_nextFlippable->Release();
        //Logger::debug("DDrawSurface: Retrieved the next swapchain surface");
      }
    }

    DDrawCommonInterface::AddWrappedSurface(this);

    m_commonSurf->SetDDSurface(this);

    if (m_parentSurf != nullptr
     && m_parentSurf->GetCommonSurface()->IsBackBufferOrFlippable()) {
      const uint32_t index = m_parentSurf->GetCommonSurface()->GetBackBufferIndex();
      m_commonSurf->IncrementBackBufferIndex(index);
    }

    if (m_parent != nullptr && m_isChildObject)
      m_parent->AddRef();

    //Logger::debug(str::format("DDrawSurface: Created a new surface nr. [[1-", std::hex, this, "]]"));

    if (m_commonSurf->GetOrigin() == nullptr) {
      m_commonSurf->SetOrigin(this);
      m_commonSurf->SetIsAttached(m_parentSurf != nullptr);
      //m_commonSurf->ListSurfaceDetails();
    }
  }

  DDrawSurface::~DDrawSurface() {
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

    m_commonSurf->SetDDSurface(nullptr);

    //Logger::debug(str::format("DDrawSurface: Surface nr. [[1-", std::hex, this, "]] bites the dust"));
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IDirect3DTexture)) {
      if (unlikely(m_texture3 == nullptr)) {
        Com<IDirect3DTexture> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        m_texture3 = new D3D3Texture(m_texture5 != nullptr ? m_texture5->GetCommonTexture() : nullptr,
                                     m_commonSurf.ptr(), std::move(ppvProxyObject), this);
      }

      *ppvObject = m_texture3.ref();

      return S_OK;
    }
    if (riid == __uuidof(IDirect3DTexture2)) {
      if (unlikely(m_texture5 == nullptr)) {
        Com<IDirect3DTexture2> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        // If any games query for IDirect3DTexture2 from IDirectDrawSurface, they will need
        // a valid IDirectDrawSurface4 object in order to use IDirect3DTexture2 with a D3D6
        // device, or when calling IDirect3DTexture2::Load, which is a known use in D3D5/3.
        // Query for one and cache it, to keep it alive as long as needed.
        //
        // Note: Doing only this for The Sims won't be enough as, for some reason,
        // it goes out of its way to release the originating IDirectDrawSurface4
        // object AFTER it queries for IDirect3DTexture2 from IDirectDrawSurface...
        if (unlikely(m_commonSurf->GetDD4Surface() == nullptr)) {
          Com<IDirectDrawSurface4> surface4ProxyObject;
          HRESULT hr = m_proxy->QueryInterface(__uuidof(IDirectDrawSurface4),
                                               reinterpret_cast<void**>(&surface4ProxyObject));
          if (unlikely(FAILED(hr)))
            return hr;

          try {
            m_surface4 = new DDraw4Surface(m_commonSurf.ptr(), std::move(surface4ProxyObject),
                                           m_commonIntf->GetDD4Interface(), nullptr, false);
          } catch (const DxvkError& e) {
            Logger::err(e.message());
            return E_NOINTERFACE;
          }
        }

        m_texture5 = new D3D5Texture(m_texture3 != nullptr ? m_texture3->GetCommonTexture() : nullptr,
                                     m_commonSurf.ptr(), std::move(ppvProxyObject), this, false);
      }

      *ppvObject = m_texture5.ref();

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
    // The standard way of creating a new D3D3 device. Outside of RAMP, MMX, RGB and HAL,
    // some applications (e.g. Dark Rift) query for Wine's advertised custom device IID.
    if (riid == IID_IDirect3DHALDevice  || riid == IID_IDirect3DRGBDevice  ||
        riid == IID_IDirect3DMMXDevice  || riid == IID_IDirect3DRampDevice ||
        riid == IID_WineD3DDevice) {
      // Surfaces which have been queried from an IDirectDrawSurface7
      // object are unable to create a D3D3 device on this legacy path
      if (unlikely(m_commonSurf->GetDD7Surface() == m_commonSurf->GetOrigin()))
        return E_NOINTERFACE;

      HRESULT hr = CreateDeviceInternal(riid, ppvObject);
      if (unlikely(FAILED(hr)))
        return E_NOINTERFACE;

      return S_OK;
    }
    // Some applications check the supported API level by querying the various newer surface GUIDs...
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      if (m_commonSurf->GetDD2Surface() != nullptr)
        return m_commonSurf->GetDD2Surface()->QueryInterface(riid, ppvObject);

      Com<IDirectDrawSurface2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      try {
        *ppvObject = ref(new DDraw2Surface(m_commonSurf.ptr(), std::move(ppvProxyObject),
                                           this, nullptr));
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
                                           this, nullptr));
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

      Com<DDraw4Surface> surface4;
      try {
        surface4 = new DDraw4Surface(m_commonSurf.ptr(), std::move(ppvProxyObject),
                                     m_commonIntf->GetDD4Interface(), nullptr, false);
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return E_NOINTERFACE;
      }

      // Dungeon Keeper 2 creates and attaches a IDirectDrawSurface4 depth stencil,
      // but then keeps using clears from the IDirectDrawSurface object...
      if (m_depthStencil != nullptr) {
        Com<IDirectDrawSurface4> dsProxyObject;
        hr = m_depthStencil->GetProxied()->QueryInterface(riid, reinterpret_cast<void**>(&dsProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        Com<DDraw4Surface> depthStencil4;
        try {
          depthStencil4 = new DDraw4Surface(m_depthStencil->GetCommonSurface(), std::move(dsProxyObject),
                                            m_commonIntf->GetDD4Interface(), nullptr, false);
        } catch (const DxvkError& e) {
          Logger::err(e.message());
          return E_NOINTERFACE;
        }

        surface4->SetAttachedDepthStencil(std::move(depthStencil4));
      }

      *ppvObject = surface4.ref();

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
    // Some games are known to query the clipper from the surface,
    // though that won't work and GetClipper exists anyway...
    if (unlikely(riid == __uuidof(IDirectDrawClipper))) {
      return E_NOINTERFACE;
    }

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirectDrawSurface))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("DDrawSurface::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDSAttachedSurface) {
    if (unlikely(lpDDSAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(lpDDSAttachedSurface))) {
      Logger::err("DDrawSurface::AddAttachedSurface: Received an unwrapped surface");
      return DDERR_CANNOTATTACHSURFACE;
    }

    DDrawSurface* attachedSurf = static_cast<DDrawSurface*>(lpDDSAttachedSurface);

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

  // Docs: "This method is used for the software implementation.
  // It is not needed if the overlay support is provided by the hardware."
  HRESULT STDMETHODCALLTYPE DDrawSurface::AddOverlayDirtyRect(LPRECT lpRect) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    if (unlikely(lpDDSrcSurface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSrcSurface))) {
      Logger::err("DDrawSurface::Blt: Received an unwrapped source surface");
      return DDERR_UNSUPPORTED;
    }

    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    if (likely(lpDDSrcSurface != nullptr)) {
      DDrawSurface* sourceSurface = static_cast<DDrawSurface*>(lpDDSrcSurface);
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
        DDrawSurface* sourceSurface = static_cast<DDrawSurface*>(lpDDSrcSurface);
        DDrawSurface* renderTarget = m_commonSurf->GetCommonD3DDevice()->GetCurrentRenderTarget();

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
      DDrawSurface* sourceSurface = static_cast<DDrawSurface*>(lpDDSrcSurface);
      hr = GetShadowOrProxied()->Blt(lpDestRect, sourceSurface->GetShadowOrProxied(), lpSrcRect, dwFlags, lpDDBltFx);
    }
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonSurf->DirtyDDrawSurface();

    if (m_shadowSurf != nullptr && d3d9Device != nullptr) {
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

  // Docs: "The IDirectDrawSurface::BltBatch method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDrawSurface::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    if (unlikely(lpDDSrcSurface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSrcSurface))) {
      Logger::err("DDrawSurface::BltFast: Received an unwrapped source surface");
      return DDERR_UNSUPPORTED;
    }

    const RECT* sourceFullSurfaceRect = nullptr;
    // Write back any dirty surface data from bound D3D9 back buffers or
    // depth stencils, for both the source surface and the current surface
    if (likely(lpDDSrcSurface != nullptr)) {
      DDrawSurface* sourceSurface = static_cast<DDrawSurface*>(lpDDSrcSurface);
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
        DDrawSurface* sourceSurface = static_cast<DDrawSurface*>(lpDDSrcSurface);
        DDrawSurface* renderTarget = m_commonSurf->GetCommonD3DDevice()->GetCurrentRenderTarget();

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
      DDrawSurface* sourceSurface = static_cast<DDrawSurface*>(lpDDSrcSurface);
      hr = GetShadowOrProxied()->BltFast(dwX, dwY, sourceSurface->GetShadowOrProxied(), lpSrcRect, dwTrans);
    }
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonSurf->DirtyDDrawSurface();

    if (m_shadowSurf != nullptr && d3d9Device != nullptr) {
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

  HRESULT STDMETHODCALLTYPE DDrawSurface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSAttachedSurface) {
    if (unlikely(lpDDSAttachedSurface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSAttachedSurface))) {
      Logger::err("DDrawSurface::DeleteAttachedSurface: Received an unwrapped surface");
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

    DDrawSurface* attachedSurf = static_cast<DDrawSurface*>(lpDDSAttachedSurface);

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

  HRESULT STDMETHODCALLTYPE DDrawSurface::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    if (unlikely(lpEnumSurfacesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    std::vector<AttachedSurface> attachedSurfaces;
    // Enumerate all attached surfaces from the underlying DDraw implementation
    HRESULT hr = m_proxy->EnumAttachedSurfaces(reinterpret_cast<void*>(&attachedSurfaces), EnumAttachedSurfacesCallback);
    if (unlikely(FAILED(hr)))
      return hr;

    hr = DDENUMRET_OK;

    // Wrap surfaces as needed and perform the actual callback the application is requesting
    auto surfaceIt = attachedSurfaces.begin();
    while (surfaceIt != attachedSurfaces.end() && hr == DDENUMRET_OK) {
      Com<IDirectDrawSurface> surface = surfaceIt->surface;

      try {
        auto attachedSurfaceIter = m_attachedSurfaces.find(surface.ptr());
        if (unlikely(attachedSurfaceIter == m_attachedSurfaces.end())) {
          // Return the already attached depth surface if it exists
          if (unlikely(m_depthStencil != nullptr && surface.ptr() == m_depthStencil->GetProxied())) {
            hr = lpEnumSurfacesCallback(m_depthStencil.ref(), &surfaceIt->desc, lpContext);
          } else {
            Com<DDrawSurface> ddrawSurface = new DDrawSurface(nullptr, std::move(surface), m_parent, this, false);
            m_attachedSurfaces.emplace(std::piecewise_construct,
                                       std::forward_as_tuple(ddrawSurface->GetProxied()),
                                       std::forward_as_tuple(ddrawSurface.ref()));
            hr = lpEnumSurfacesCallback(ddrawSurface.ref(), &surfaceIt->desc, lpContext);
          }
        } else {
          hr = lpEnumSurfacesCallback(attachedSurfaceIter->second.ref(), &surfaceIt->desc, lpContext);
        }
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return DDERR_GENERIC;
      }

      ++surfaceIt;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpfnCallback) {
    return m_proxy->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DWORD dwFlags) {
    if (unlikely(lpDDSurfaceTargetOverride != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(lpDDSurfaceTargetOverride))) {
      Logger::err("DDrawSurface::Flip: Received an unwrapped override surface");
      return DDERR_UNSUPPORTED;
    }

    Com<DDrawSurface> overrideSurf = static_cast<DDrawSurface*>(lpDDSurfaceTargetOverride);

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

      // Workaround for The Sims/other games flipping a different
      // swapchain than the one set on the current D3D device
      if (unlikely(m_commonIntf->GetOptions()->forceRTFlip && m_commonSurf->IsPrimarySurface())) {
        m_nextFlippable = m_commonSurf->GetCommonD3DDevice()->GetCurrentRenderTarget();
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
      if (overrideSurf == nullptr) {
        return m_proxy->Flip(lpDDSurfaceTargetOverride, dwFlags);
      } else {
        return m_proxy->Flip(overrideSurf->GetShadowOrProxied(), dwFlags);
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE *lplpDDAttachedSurface) {
    if (unlikely(lpDDSCaps == nullptr || lplpDDAttachedSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<IDirectDrawSurface> surface;
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
          Com<DDrawSurface> ddrawSurface = new DDrawSurface(nullptr, std::move(surface), m_parent, this, false);
          m_attachedSurfaces.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(ddrawSurface->GetProxied()),
                                     std::forward_as_tuple(ddrawSurface.ref()));
          *lplpDDAttachedSurface = ddrawSurface.ref();
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
  HRESULT STDMETHODCALLTYPE DDrawSurface::GetBltStatus(DWORD dwFlags) {
    if (likely(dwFlags == DDGBS_CANBLT || dwFlags == DDGBS_ISBLTDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetCaps(LPDDSCAPS lpDDSCaps) {
    if (unlikely(lpDDSCaps == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDSCaps = m_commonSurf->GetDesc()->ddsCaps;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetClipper(LPDIRECTDRAWCLIPPER *lplpDDClipper) {
    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    DDrawClipper* clipper = m_commonSurf->GetClipper();

    if (unlikely(clipper == nullptr))
      return DDERR_NOCLIPPERATTACHED;

    *lplpDDClipper = ref(clipper);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    return m_proxy->GetColorKey(dwFlags, lpDDColorKey);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetDC(HDC *lphDC) {
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
  HRESULT STDMETHODCALLTYPE DDrawSurface::GetFlipStatus(DWORD dwFlags) {
    if (likely(dwFlags == DDGFS_CANFLIP || dwFlags == DDGFS_ISFLIPDONE))
      return DD_OK;

    return DDERR_INVALIDPARAMS;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetOverlayPosition(LPLONG lplX, LPLONG lplY) {
    return m_proxy->GetOverlayPosition(lplX, lplY);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetPalette(LPDIRECTDRAWPALETTE *lplpDDPalette) {
    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    DDrawPalette* palette = m_commonSurf->GetPalette();

    if (unlikely(palette == nullptr))
      return DDERR_NOPALETTEATTACHED;

    *lplpDDPalette = ref(palette);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) {
    if (unlikely(lpDDPixelFormat == nullptr))
      return DDERR_INVALIDPARAMS;

    *lpDDPixelFormat = m_commonSurf->GetDesc()->ddpfPixelFormat;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) {
    if (unlikely(lpDDSurfaceDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDDSurfaceDesc->dwSize != sizeof(DDSURFACEDESC)))
      return DDERR_INVALIDPARAMS;

    *lpDDSurfaceDesc = *m_commonSurf->GetDesc();

    return DD_OK;
  }

  // According to the docs: "Because the DirectDrawSurface object is initialized
  // when it's created, this method always returns DDERR_ALREADYINITIALIZED."
  HRESULT STDMETHODCALLTYPE DDrawSurface::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc) {
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::IsLost() {
    return m_proxy->IsLost();
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    // Write back any dirty surface data from bound D3D9 back buffers or depth stencils
    DownloadSurfaceData();

    HRESULT hr = GetShadowOrProxied()->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
    if (unlikely(FAILED(hr)))
      return hr;

    // For single surface locks, track the READONLY flag in order to skip dirtying
    // on Unlock(). Reset flag tracking in case of multiple simultaneous locks,
    // which are technically possible but extremely rare in practice.
    //
    // Note: Using lpDestRect as a key for tracking and/or matching Lock() to Unlock()
    // calls, as the documentation suggests, isn't feasible, as there are applications
    // which use nullptr during Lock() calls and then a non-null pointer on Unlock().
    if (likely(!m_readOnlyLock)) {
      m_readOnlyLock = (dwFlags & DDLOCK_READONLY) && !(dwFlags & DDLOCK_WRITEONLY);
    } else {
      m_readOnlyLock = false;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::ReleaseDC(HDC hDC) {
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

    if (m_shadowSurf != nullptr) {
      d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->GetRefreshedD3D9Device();
      if (likely(d3d9Device != nullptr)) {
        const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                  !m_commonSurf->GetCommonD3DDevice()->IsInScene() :
                                   m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                   false : true;
        if (shouldPresent) {
          InitializeOrUploadD3D9();
          d3d9Device->Present(NULL, NULL, NULL, NULL);
        }
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Restore() {
    return m_proxy->Restore();
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) {
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

  HRESULT STDMETHODCALLTYPE DDrawSurface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
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
      Logger::err("DDrawSurface::SetColorKey: Failed to retrieve updated surface desc");

    if (unlikely(m_shadowSurf != nullptr)) {
      hr = m_shadowSurf->GetProxied()->SetColorKey(dwFlags, lpDDColorKey);
      if (unlikely(FAILED(hr))) {
        Logger::warn("DDrawSurface::SetColorKey: Failed to set shadow surface color key");
      } else {
        hr = m_shadowSurf->GetCommonSurface()->RefreshSurfaceDescripton(false);
        if (unlikely(FAILED(hr)))
          Logger::warn("DDrawSurface::SetColorKey: Failed to retrieve updated shadow surface desc");
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::SetOverlayPosition(LONG lX, LONG lY) {
    return m_proxy->SetOverlayPosition(lX, lY);
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) {
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

    // Note: A palette update on a primary surface would cause immediate
    // presentation, however we don't support P8 primary surfaces

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::Unlock(LPVOID lpSurfaceData) {
    HRESULT hr = GetShadowOrProxied()->Unlock(lpSurfaceData);
    if (unlikely(FAILED(hr)))
      return hr;

    if (!m_readOnlyLock) {
      m_commonSurf->DirtyDDrawSurface();

      if (m_shadowSurf != nullptr) {
        d3d9::IDirect3DDevice9* d3d9Device = m_commonSurf->GetRefreshedD3D9Device();
        if (likely(d3d9Device != nullptr)) {
          const bool shouldPresent = m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Auto ?
                                    !m_commonSurf->GetCommonD3DDevice()->IsInScene() :
                                     m_commonIntf->GetOptions()->legacyPresentGuard == D3DLegacyPresentGuard::Strict ?
                                     false : true;
          if (shouldPresent) {
            InitializeOrUploadD3D9();
            d3d9Device->Present(NULL, NULL, NULL, NULL);
          }
        }
      }
    } else {
      m_readOnlyLock = false;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    if (unlikely(lpDDDestSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(lpDDDestSurface))) {
      Logger::err("DDrawSurface::UpdateOverlay: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDDestSurface);
    return m_proxy->UpdateOverlay(lpSrcRect, ddrawSurface->GetProxied(), lpDestRect, dwFlags, lpDDOverlayFx);
  }

  // Docs: "This method is for software emulation only; it does nothing if the hardware supports overlays."
  HRESULT STDMETHODCALLTYPE DDrawSurface::UpdateOverlayDisplay(DWORD dwFlags) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDrawSurface::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSReference) {
    if (unlikely(lpDDSReference != nullptr
              && !DDrawCommonInterface::IsWrappedSurface(lpDDSReference))) {
      Logger::err("DDrawSurface::UpdateOverlayZOrder: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    if (lpDDSReference == nullptr) {
      return m_proxy->UpdateOverlayZOrder(dwFlags, lpDDSReference);
    } else {
      DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSReference);
      return m_proxy->UpdateOverlayZOrder(dwFlags, ddrawSurface->GetProxied());
    }
  }

  HRESULT DDrawSurface::InitializeD3D9RenderTarget() {
    // Currently ignores all P8 surfaces
    if (unlikely(m_commonSurf->SkipD3D9Operations()))
      return DD_OK;

    m_commonSurf->RefreshD3D9Device();

    if (unlikely(!m_commonSurf->IsInitialized())) {
      HRESULT hr = m_commonSurf->InitializeD3D9(true);
      if (unlikely(FAILED(hr))) {
        Logger::err(str::format("DDrawSurface::InitializeD3D9RenderTarget: Failed to initialize surface nr. [[1-", std::hex, this, "]]"));
        return hr;
      }

      return UploadSurfaceData();
    }

    return DD_OK;
  }

  HRESULT DDrawSurface::InitializeD3D9DepthStencil() {
    m_commonSurf->RefreshD3D9Device();

    if (unlikely(!m_commonSurf->IsInitialized())) {
      HRESULT hr = m_commonSurf->InitializeD3D9(false);
      if (unlikely(FAILED(hr))) {
        Logger::err(str::format("DDrawSurface::InitializeD3D9DepthStencil: Failed to initialize surface nr. [[1-", std::hex, this, "]]"));
        return hr;
      }

      return UploadSurfaceData();
    }

    return DD_OK;
  }

  HRESULT DDrawSurface::InitializeOrUploadD3D9() {
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
        Logger::err(str::format("DDrawSurface::InitializeOrUploadD3D9: Failed to initialize surface nr. [[1-", std::hex, this, "]]"));
        return hr;
      }
    }

    return UploadSurfaceData();
  }

  void DDrawSurface::DownloadSurfaceData() {
    // Some games, like The Settlers IV, use multiple devices for rendering, one to handle
    // terrain and the overall 3D scene, and one to create textures/sprites to overlay on
    // top of it. Since DXVK's D3D9 backend does not restrict cross-device surface/texture
    // use, simply skip changing assigned surface devices during downloads. This is essentially
    // a hack, which by some miracle works well enough in some cases, though may explode in others.
    if (likely(!m_commonIntf->GetOptions()->deviceResourceSharing))
      m_commonSurf->RefreshD3D9Device();

    // TODO: We are technically ignoring mip maps as is, though that will probably never be an issue
    if (m_commonSurf->IsD3D9SurfaceDirty() && m_commonSurf->IsInitialized()) {
      //Logger::debug(str::format("DDrawSurface::DownloadSurfaceData: Downloading nr. [[1-", std::hex, this, "]]"));
      BlitToDDrawSurface<IDirectDrawSurface, DDSURFACEDESC>(GetShadowOrProxied(), m_commonSurf->GetD3D9Surface(),
                                                            m_commonSurf->IsDXTFormat());
      m_commonSurf->UnDirtyD3D9Surface();
    }
  }

  inline HRESULT DDrawSurface::UploadSurfaceData() {
    // Fast skip
    if (!m_commonSurf->IsDDrawSurfaceDirty())
      return DD_OK;

    //Logger::debug(str::format("DDrawSurface::UploadSurfaceData: Uploading nr. [[1-", std::hex, this, "]]"));

    const D3D9SurfaceType d3d9SurfaceType = m_commonSurf->GetD3D9SurfaceType();

    switch (d3d9SurfaceType) {
      case D3D9SurfaceType::Texture:
        BlitToD3D9Texture<IDirectDrawSurface, DDSURFACEDESC>(m_commonSurf->GetD3D9Texture(), m_proxy.ptr(),
                                                             m_commonSurf->GetMipCount(), m_commonSurf->IsDXTFormat());
        break;
      default:
        BlitToD3D9Surface<IDirectDrawSurface, DDSURFACEDESC>(m_commonSurf->GetD3D9Surface(), GetShadowOrProxied(),
                                                             m_commonSurf->IsDXTFormat());
        break;
    }

    m_commonSurf->UnDirtyDDrawSurface();

    return DD_OK;
  }

  inline HRESULT DDrawSurface::CreateDeviceInternal(REFIID riid, void** ppvObject) {
    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    DWORD deviceCreationFlags9 = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    bool  isHALDevice          = false;
    bool  halFallback          = false;
    bool  rgbFallback          = false;

    if (riid == IID_IDirect3DHALDevice) {
      Logger::info("DDrawSurface::CreateDeviceInternal: Creating an IID_IDirect3DHALDevice device");
      if (likely(!d3dOptions->forceSWVP))
        deviceCreationFlags9 = D3DCREATE_MIXED_VERTEXPROCESSING;
      isHALDevice = true;
    } else if (riid == IID_IDirect3DRGBDevice) {
      Logger::info("DDrawSurface::CreateDeviceInternal: Creating an IID_IDirect3DRGBDevice device");
    } else if (riid == IID_IDirect3DMMXDevice) {
      Logger::warn("DDrawSurface::CreateDeviceInternal: Unsupported MMX device, falling back to RGB");
      rgbFallback = true;
    } else if (riid == IID_IDirect3DRampDevice) {
      Logger::warn("DDrawSurface::CreateDeviceInternal: Unsupported Ramp device, falling back to RGB");
      rgbFallback = true;
    } else if (unlikely(riid == IID_IUnknown)) {
      Logger::warn("DDrawSurface::CreateDeviceInternal: Unsupported IID_IUnknown, falling back to RGB");
      rgbFallback = true;
    } else {
      if (unlikely(riid != IID_WineD3DDevice)) {
        Logger::warn("DDrawSurface::CreateDeviceInternal: Unknown device type, falling back to HAL");
        Logger::warn(str::format(riid));
      } else {
        Logger::info("DDrawSurface::CreateDeviceInternal: Creating an IID_WineD3DDevice HAL device");
      }
      halFallback = true;
      // Don't enforce isHALDevice RT validations
    }

    const IID rclsidOverride = halFallback ? IID_IDirect3DHALDevice :
                               rgbFallback ? IID_IDirect3DRGBDevice : riid;

    HWND hWnd = m_commonIntf->GetHWND();
    // Needed to sometimes safely skip intro playback on legacy devices
    if (unlikely(hWnd == nullptr)) {
      Logger::debug("DDrawSurface::CreateDeviceInternal: HWND is NULL");
    }

    HRESULT hrRT = m_commonSurf->ValidateRTUsage(isHALDevice, true);
    if (unlikely(FAILED(hrRT)))
      return hrRT;

    const DDSURFACEDESC* desc = m_commonSurf->GetDesc();

    DWORD backBufferWidth  = desc->dwWidth;
    DWORD BackBufferHeight = desc->dwHeight;

    if (likely(d3dOptions->backBufferResize)) {
      const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Ignore any mode size dimensions when in windowed present mode
      if (exclusiveMode) {
        DDrawModeSize* modeSize = m_commonIntf->GetModeSize();
        // Wayland apparently needs this for somewhat proper back buffer sizing
        if ((modeSize->width  && modeSize->width  < desc->dwWidth)
         || (modeSize->height && modeSize->height < desc->dwHeight)) {
          Logger::info("DDrawSurface::CreateDeviceInternal: Enforcing mode dimensions");
          backBufferWidth  = modeSize->width;
          BackBufferHeight = modeSize->height;
        }
      }
    }

    const d3d9::D3DFORMAT backBufferFormat = m_commonSurf->GetD3D9Format();

    const DWORD cooperativeLevel = m_commonIntf->GetCooperativeLevel();

    if ((cooperativeLevel & DDSCL_MULTITHREADED) || d3dOptions->forceMultiThreaded) {
      Logger::info("DDrawSurface::CreateDeviceInternal: Using thread safe runtime synchronization");
      deviceCreationFlags9 |= D3DCREATE_MULTITHREADED;
    }
    // DDSCL_FPUPRESERVE does not exist prior to DDraw7,
    // and DDSCL_FPUSETUP is NOT the default state
    if (!(cooperativeLevel & DDSCL_FPUSETUP))
      deviceCreationFlags9 |= D3DCREATE_FPU_PRESERVE;
    if (cooperativeLevel & DDSCL_NOWINDOWCHANGES)
      deviceCreationFlags9 |= D3DCREATE_NOWINDOWCHANGES;

    Logger::info(str::format("DDrawSurface::CreateDeviceInternal: Back buffer size: ", desc->dwWidth, "x", desc->dwHeight));

    const DWORD backBufferCount = DetermineBackBufferCount<IDirectDrawSurface>(m_proxy.ptr());
    Logger::info(str::format("DDrawSurface::CreateDeviceInternal: Back buffer count: ", backBufferCount));

    D3D3Interface* d3d3Intf = m_commonIntf->GetOrCreateD3D3Interface();
    // D3D3 is "special", so in odd cases we might not have a valid D3D3 interface to work with
    if (unlikely(d3d3Intf == nullptr)) {
      Logger::err("DDrawSurface::CreateDeviceInternal: Unable to retrieve a valid D3D3 interface");
      return DDERR_UNSUPPORTED;
    }

    D3DCommonInterface* commonD3DIntf = d3d3Intf->GetCommonD3DInterface();

    // Determine the supported AA sample count by querying the D3D9 interface
    const d3d9::D3DMULTISAMPLE_TYPE multiSampleType = d3dOptions->emulateFSAA != FSAAEmulation::Disabled ?
                                                      commonD3DIntf->GetMultiSampleType(backBufferFormat) :
                                                      d3d9::D3DMULTISAMPLE_NONE;

    d3d9::D3DPRESENT_PARAMETERS params;
    params.BackBufferWidth    = backBufferWidth;
    params.BackBufferHeight   = BackBufferHeight;
    params.BackBufferFormat   = backBufferFormat;
    params.BackBufferCount    = backBufferCount;
    params.MultiSampleType    = multiSampleType;
    params.MultiSampleQuality = 0;
    params.SwapEffect         = d3d9::D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow      = hWnd;
    params.Windowed           = TRUE; // Always use windowed, so that we can delegate mode switching to DDraw
    params.EnableAutoDepthStencil     = FALSE;
    params.AutoDepthStencilFormat     = d3d9::D3DFMT_UNKNOWN;
    params.Flags                      = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER; // Needed for back buffer locks
    params.FullScreen_RefreshRateInHz = 0; // We'll get the right mode/refresh rate set by DDraw, just play along
    params.PresentationInterval       = D3DPRESENT_INTERVAL_DEFAULT; // A D3D3 device always uses VSync

    Com<d3d9::IDirect3DDevice9> device9;
    HRESULT hr = commonD3DIntf->GetD3D9Interface()->CreateDevice(
      D3DADAPTER_DEFAULT,
      d3d9::D3DDEVTYPE_HAL,
      hWnd,
      deviceCreationFlags9,
      &params,
      &device9
    );

    if (unlikely(FAILED(hr))) {
      Logger::err("DDrawSurface::CreateDeviceInternal: Failed to create the D3D9 device");
      return hr;
    }

    try{
      Com<D3D3Device> device3 = new D3D3Device(nullptr, this, rclsidOverride, &params,
                                               std::move(device9), deviceCreationFlags9);

      // Set the common device on the common interface
      m_commonIntf->SetCommonD3DDevice(device3->GetCommonD3DDevice());
      // Now that we have a valid common D3D device on the DDraw interface,
      // we can initialize the render target and depth stencil (if any)
      hr = device3->InitializeRTAndDS();
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = device3.ref();
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return DD_OK;
  }

}
