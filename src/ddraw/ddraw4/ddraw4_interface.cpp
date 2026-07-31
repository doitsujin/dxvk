#include "ddraw4_interface.h"

#include "../d3d_common_device.h"

#include "ddraw4_surface.h"

#include "../ddraw_clipper.h"
#include "../ddraw_palette.h"

#include "../ddraw/ddraw_interface.h"
#include "../ddraw2/ddraw2_interface.h"
#include "../ddraw7/ddraw7_interface.h"

#include "../d3d3/d3d3_interface.h"
#include "../d3d5/d3d5_interface.h"
#include "../d3d6/d3d6_interface.h"

namespace dxvk {

  DDraw4Interface::DDraw4Interface(
        DDrawCommonInterface* commonIntf,
        Com<IDirectDraw4>&& proxyIntf)
    : DDrawWrappedObject<IUnknown, IDirectDraw4>(nullptr, std::move(proxyIntf))
    , m_commonIntf ( commonIntf ) {
    // We need a temporary D3D9 interface at this point to retrieve the adapter identifier
    Com<d3d9::IDirect3D9> d3d9Intf = d3d9::Direct3DCreate9(D3D_SDK_VERSION);

    d3d9::D3DADAPTER_IDENTIFIER9 adapterIdentifier9;
    HRESULT hr = d3d9Intf->GetAdapterIdentifier(0, 0, &adapterIdentifier9);
    if (unlikely(FAILED(hr))) {
      throw DxvkError("DDraw4Interface: ERROR! Failed to get D3D9 adapter identifier!");
    }

    m_commonIntf->SetAdapterIdentifier(adapterIdentifier9);

    // Note: IDirectDraw4 can never be the origin interface

    m_commonIntf->SetDD4Interface(this);
  }

  DDraw4Interface::~DDraw4Interface() {
    m_commonIntf->SetDD4Interface(nullptr);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Standard way of retrieving a D3D6 interface
    if (riid == __uuidof(IDirect3D3)) {
      // Initialize the IDirect3D3 interlocked object
      if (unlikely(m_d3d6Intf == nullptr)) {
        Com<IDirect3D3> ppvProxyObject;
        HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
        if (unlikely(FAILED(hr)))
          return hr;

        m_d3d6Intf = new D3D6Interface(nullptr, m_commonIntf.ptr(), std::move(ppvProxyObject), this);
      }

      *ppvObject = m_d3d6Intf.ref();

      return S_OK;
    }
    // Some games query for legacy ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw))) {
      if (m_commonIntf->GetDDInterface() != nullptr)
        return m_commonIntf->GetDDInterface()->QueryInterface(riid, ppvObject);

      Com<IDirectDraw> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDrawInterface(m_commonIntf.ptr(), std::move(ppvProxyObject)));

      return S_OK;
    }
    // Some games query for legacy ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw2))) {
      if (m_commonIntf->GetDD2Interface() != nullptr)
        return m_commonIntf->GetDD2Interface()->QueryInterface(riid, ppvObject);

      Com<IDirectDraw2> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw2Interface(m_commonIntf.ptr(), std::move(ppvProxyObject)));

      return S_OK;
    }
    // Legacy way of getting a DDraw7 interface
    if (unlikely(riid == __uuidof(IDirectDraw7))) {
      if (m_commonIntf->GetDD7Interface() != nullptr)
        return m_commonIntf->GetDD7Interface()->QueryInterface(riid, ppvObject);

      Com<IDirectDraw7> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw7Interface(m_commonIntf.ptr(), std::move(ppvProxyObject)));

      return S_OK;
    }
    // Standard way of retrieving a D3D3 interface
    if (unlikely(riid == __uuidof(IDirect3D))) {
      if (m_commonIntf->GetDDInterface() != nullptr)
        return m_commonIntf->GetDDInterface()->QueryInterface(riid, ppvObject);

      Com<D3D3Interface> d3d3Intf = new D3D3Interface(nullptr, m_commonIntf.ptr(), this);
      m_commonIntf->SetD3D3Interface(d3d3Intf.ptr());
      *ppvObject = d3d3Intf.ref();

      return S_OK;
    }
    // Standard way of retrieving a D3D5 interface
    if (unlikely(riid == __uuidof(IDirect3D2))) {
      if (m_commonIntf->GetDDInterface() != nullptr)
        return m_commonIntf->GetDDInterface()->QueryInterface(riid, ppvObject);

      Com<D3D5Interface> d3d5Intf = new D3D5Interface(nullptr, m_commonIntf.ptr(), this);
      *ppvObject = d3d5Intf.ref();

      return S_OK;
    }
    // Quite a lot of games query for this IID during intro playback
    if (unlikely(riid == GUID_IAMMediaStream)) {
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Also seen queried by some games, such as V-Rally 2: Expert Edition
    if (unlikely(riid == GUID_IMediaStream)) {
      return m_proxy->QueryInterface(riid, ppvObject);
    }

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirectDraw4))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("DDraw4Interface::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  // The documentation states: "The IDirectDraw4::Compact method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDraw4Interface::Compact() {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
    if (unlikely(lplpDDClipper == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDClipper);

    Com<IDirectDrawClipper> lplpDDClipperProxy;
    HRESULT hr = m_proxy->CreateClipper(dwFlags, &lplpDDClipperProxy, pUnkOuter);
    if (unlikely(FAILED(hr)))
      return hr;

    *lplpDDClipper = ref(new DDrawClipper(m_commonIntf.ptr(), std::move(lplpDDClipperProxy), this));

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE *lplpDDPalette, IUnknown *pUnkOuter) {
    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    Com<IDirectDrawPalette> lplpDDPaletteProxy;
    HRESULT hr = m_proxy->CreatePalette(dwFlags, lpColorTable, &lplpDDPaletteProxy, pUnkOuter);
    if (unlikely(FAILED(hr)))
      return hr;

    *lplpDDPalette = ref(new DDrawPalette(std::move(lplpDDPaletteProxy), this));

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::CreateSurface(LPDDSURFACEDESC2 lpDDSurfaceDesc, LPDIRECTDRAWSURFACE4 *lplpDDSurface, IUnknown *pUnkOuter) {
    // The cooperative level is always checked first
    if (unlikely(!m_commonIntf->IsCooperativeLevelSet()))
      return DDERR_NOCOOPERATIVELEVELSET;

    if (unlikely(lpDDSurfaceDesc == nullptr || lplpDDSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDSurface);

    if (likely(lpDDSurfaceDesc->dwFlags & DDSD_CAPS)) {
      // Because we are removing the DDSCAPS_WRITEONLY flag below, we need
      // to first validate the combinations that would otherwise cause issues
      HRESULT hr = ValidateSurfaceFlags(lpDDSurfaceDesc);
      if (unlikely(FAILED(hr)))
        return hr;

      // We need to ensure we can always read from surfaces for upload to
      // D3D9, so always strip the DDSCAPS_WRITEONLY flag on creation
      lpDDSurfaceDesc->ddsCaps.dwCaps &= ~DDSCAPS_WRITEONLY;
      // Similarly strip the DDSCAPS2_OPAQUE flag on texture creation
      lpDDSurfaceDesc->ddsCaps.dwCaps2 &= ~DDSCAPS2_OPAQUE;
    }

    if (likely(lpDDSurfaceDesc->dwFlags & DDSD_PIXELFORMAT)) {
      // WineD3D will fail to create 8-bit texture surfaces at times, for whatever reason...
      if (unlikely((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_TEXTURE)
               &&  (lpDDSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_RGB)
               &&   lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount == 8
               && !(lpDDSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8))) {
        static bool s_8bitTextureWarningShown;

        if (!std::exchange(s_8bitTextureWarningShown, true))
          Logger::warn("DDraw4Interface::CreateSurface: Use of potentially unsupported 8-bit texture surface");
      }

      // Work around a WineD3D bug/limitation that prevents
      // read back from L6V5U5 and X8L8V8U8 video memory surfaces
      //
      // Note: Doing this for other surfaces/formats is a bad idea,
      // because some games expect these flags to remain in place, and
      // may crash in case they find that's not the case
      if (unlikely((lpDDSurfaceDesc->ddpfPixelFormat.dwFlags == (DDPF_BUMPDUDV | DDPF_BUMPLUMINANCE))
                && (lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY))) {
        Logger::warn("DDraw4Interface::CreateSurface: Video memory DDPF_BUMPLUMINANCE surface");
        lpDDSurfaceDesc->ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY &
                                           ~DDSCAPS_LOCALVIDMEM &
                                           ~DDSCAPS_NONLOCALVIDMEM;
        lpDDSurfaceDesc->ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
      }

      if (unlikely(lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_ZBUFFER)) {
        if (unlikely(m_commonIntf->GetOptions()->useD16forD24X8
                  && lpDDSurfaceDesc->ddpfPixelFormat.dwZBitMask == 0xFFFFFF
                  && lpDDSurfaceDesc->ddpfPixelFormat.dwStencilBitMask == 0x0)) {
          // Games such as Need for Speed: Porsche are broken with 32-bit color
          // on night tracks with "projected" lights, because they clearly were
          // designed with 16-bit Z buffers in mind. Fix it up by silently swapping
          // D16 for D24X8 on depth stencil creation.
          Logger::debug("DDraw4Interface::CreateSurface: Using D16 instead of D24X8");
          lpDDSurfaceDesc->ddpfPixelFormat.dwZBufferBitDepth = 16;
          lpDDSurfaceDesc->ddpfPixelFormat.dwZBitMask = 0xFFFF;
        } else if (unlikely(lpDDSurfaceDesc->ddpfPixelFormat.dwZBitMask == 0xFFFFFFFF)) {
          if (m_commonIntf->GetOptions()->useD24X8forD32) {
            // In case of up-front unsupported and unadvertised D32 depth stencil use,
            // replace it with D24X8, as some games, such as Sacrifice, rely on it
            // to properly enable 32-bit display modes (and revert to 16-bit otherwise)
            Logger::debug("DDraw4Interface::CreateSurface: Using D24X8 instead of D32");
            lpDDSurfaceDesc->ddpfPixelFormat.dwZBitMask = 0xFFFFFF;
          } else {
            Logger::warn("DDraw4Interface::CreateSurface: Use of unsupported D32");
          }
        }
      }
    }

    Com<IDirectDrawSurface4> ddrawSurface4Proxied;
    HRESULT hr = m_proxy->CreateSurface(lpDDSurfaceDesc, &ddrawSurface4Proxied, pUnkOuter);
    // Some games simply try creating surfaces with various formats until something works...
    if (unlikely(FAILED(hr)))
      return hr;

    try{
      Com<DDraw4Surface> surface4 = new DDraw4Surface(nullptr, std::move(ddrawSurface4Proxied), this, nullptr, true);

      if (unlikely(lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)) {
        m_commonIntf->SetPrimarySurface(surface4->GetCommonSurface());

        // Shadow surface creation for the primary surface
        // (it needs to be based on the same incoming desc)
        if (unlikely(!surface4->GetCommonSurface()->Is8BitFormat() &&
                      m_commonIntf->GetOptions()->forceLegacyPresent)) {
          DDSURFACEDESC2 shadowDesc = *lpDDSurfaceDesc;
          const DDSURFACEDESC2* primaryDesc = surface4->GetCommonSurface()->GetDesc2();

          shadowDesc.ddsCaps.dwCaps &= ~DDSCAPS_PRIMARYSURFACE & ~DDSCAPS_COMPLEX & ~DDSCAPS_FLIP;
          shadowDesc.ddsCaps.dwCaps |= DDSCAPS_OFFSCREENPLAIN;
          shadowDesc.dwFlags &= ~DDSD_BACKBUFFERCOUNT;
          // Dimensions aren't specified in the incoming desc,
          // but are explicitly needed for non-primary surfaces
          shadowDesc.dwFlags |= DDSD_WIDTH | DDSD_HEIGHT;
          shadowDesc.dwWidth  = primaryDesc->dwWidth;
          shadowDesc.dwHeight = primaryDesc->dwHeight;

          Com<IDirectDrawSurface4> ddraw4SurfaceShadow;
          hr = m_proxy->CreateSurface(&shadowDesc, &ddraw4SurfaceShadow, pUnkOuter);
          if (unlikely(FAILED(hr))) {
            Logger::warn("DDraw4Interface::CreateSurface: Failed to create shadow surface");
          } else {
            Com<DDraw4Surface> shadowSurf = new DDraw4Surface(nullptr, std::move(ddraw4SurfaceShadow),
                                                              this, nullptr, false);
            surface4->SetShadowSurface(std::move(shadowSurf));
          }
        }
      }

      *lplpDDSurface = surface4.ref();
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::DuplicateSurface(LPDIRECTDRAWSURFACE4 lpDDSurface, LPDIRECTDRAWSURFACE4 *lplpDupDDSurface) {
    if (unlikely(lpDDSurface == nullptr || lplpDupDDSurface == nullptr))
      return DDERR_CANTDUPLICATE;

    InitReturnPtr(lplpDupDDSurface);

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(lpDDSurface))) {
      Logger::err("DDraw4Interface::DDraw4Interface: Received an unwrapped surface");
      return DDERR_CANTDUPLICATE;
    }

    DDraw4Surface* ddraw4Surface = static_cast<DDraw4Surface*>(lpDDSurface);

    Com<IDirectDrawSurface4> dupSurface4;
    HRESULT hr = m_proxy->DuplicateSurface(ddraw4Surface->GetProxied(), &dupSurface4);
    if (unlikely(FAILED(hr)))
      return hr;

    try {
      *lplpDupDDSurface = ref(new DDraw4Surface(nullptr, std::move(dupSurface4), this, nullptr, false));
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK2 lpEnumModesCallback) {
    if (unlikely(lpEnumModesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    std::vector<DDSURFACEDESC2> displayModes;
    HRESULT hr = m_proxy->EnumDisplayModes(dwFlags, lpDDSurfaceDesc, reinterpret_cast<void*>(&displayModes), EnumDisplayModesCallback2);
    if (unlikely(FAILED(hr)))
      return hr;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    hr = DDENUMRET_OK;

    auto displayModeIt = displayModes.begin();
    while (displayModeIt != displayModes.end() && hr == DDENUMRET_OK) {
      DDSURFACEDESC2 dmDesc = *displayModeIt;

      if (unlikely(d3dOptions->mask8BitModes && dmDesc.ddpfPixelFormat.dwRGBBitCount == 8)) {
        static bool s_maskModeWarningShown;

        if (!std::exchange(s_maskModeWarningShown, true))
          Logger::warn("DDraw4Interface::EnumDisplayModes: Masking 8-bit display modes");
      } else {
        hr = lpEnumModesCallback(&dmDesc, lpContext);
      }

      ++displayModeIt;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC2 lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK2 lpEnumSurfacesCallback) {
    if (unlikely(lpEnumSurfacesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    std::vector<AttachedSurface4> attachedSurfaces;
    HRESULT hr = m_proxy->EnumSurfaces(dwFlags, lpDDSD, reinterpret_cast<void*>(&attachedSurfaces), EnumAttachedSurfaces4Callback);
    if (unlikely(FAILED(hr)))
      return hr;

    hr = DDENUMRET_OK;

    auto surfaceIt = attachedSurfaces.begin();
    while (surfaceIt != attachedSurfaces.end() && hr == DDENUMRET_OK) {
      Com<IDirectDrawSurface4> surface4 = surfaceIt->surface4;

      Com<DDraw4Surface> ddraw4Surface;
      try {
        ddraw4Surface = new DDraw4Surface(nullptr, std::move(surface4), this, nullptr, false);
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return DDERR_GENERIC;
      }
      hr = lpEnumSurfacesCallback(ddraw4Surface.ref(), &surfaceIt->desc2, lpContext);

      ++surfaceIt;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::FlipToGDISurface() {
    DDrawCommonSurface* ps = m_commonIntf->GetPrimarySurface();

    // A primary surface must exist for a GDI flip to be possible
    if (unlikely(ps == nullptr))
      return DDERR_NOTFOUND;

    if (unlikely(!ps->IsFlippable()))
      return DDERR_NOTFLIPPABLE;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) {
    if (unlikely(lpDDDriverCaps == nullptr && lpDDHELCaps == nullptr))
      return DDERR_INVALIDPARAMS;

    // Interstate '76 sends invalid dwSizes part of the structs,
    // and that explodes in Wine, so validate it before proxying
    if (unlikely(lpDDDriverCaps != nullptr && !IsValidDDrawCapsSize(lpDDDriverCaps->dwSize)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpDDHELCaps != nullptr && !IsValidDDrawCapsSize(lpDDHELCaps->dwSize)))
      return DDERR_INVALIDPARAMS;

    HRESULT hr = m_proxy->GetCaps(lpDDDriverCaps, lpDDHELCaps);
    if (unlikely(FAILED(hr)))
      return hr;

    static constexpr DWORD Megabytes = 1024 * 1024;
    static constexpr DWORD MaxMemory = ddrawCaps::MaxTextureMemory * Megabytes;
    static constexpr DWORD ReservedMemory = ddrawCaps::ReservedTextureMemory * Megabytes;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();
    // Properly fill in the dwVidMemTotal / dwVidMemFree fields
    DWORD total9 = 0;
    DWORD free9  = 0;

    D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();
    if (likely(commonDevice != nullptr)) {
      d3d9::IDirect3DDevice9* d3d9Device = commonDevice->GetD3D9Device();

      total9 = static_cast<DWORD>(commonDevice->GetTotalTextureMemory());
      free9  = static_cast<DWORD>(d3d9Device->GetAvailableTextureMem());

      if (likely(total9 >= MaxMemory)) {
        const DWORD delta = total9 - MaxMemory;
        total9 = MaxMemory - ReservedMemory;
        free9 = free9 > delta + ReservedMemory ? free9 - (delta + ReservedMemory) : 0;
      }

      //Logger::debug(str::format("DDraw4Interface::GetCaps: Total: ", total9));
      //Logger::debug(str::format("DDraw4Interface::GetCaps: Free : ", free9));
    } else {
      const DWORD total6 = lpDDDriverCaps != nullptr ? lpDDDriverCaps->dwVidMemTotal : 0;
      const DWORD free6  = lpDDDriverCaps != nullptr ? lpDDDriverCaps->dwVidMemFree  : 0;

      //Logger::debug(str::format("DDraw4Interface::GetCaps: DDraw Total: ", total6));
      //Logger::debug(str::format("DDraw4Interface::GetCaps: DDraw Free : ", free6));

      if (unlikely(total6 < MaxMemory)) {
        total9 = total6;
        free9 = free6;
      } else {
        const DWORD delta = total6 - MaxMemory;
        total9 = MaxMemory - ReservedMemory;
        free9 = free6 > delta + ReservedMemory ? free6 - (delta + ReservedMemory) : 0;
      }

      //Logger::debug(str::format("DDraw4Interface::GetCaps: Total: ", total9));
      //Logger::debug(str::format("DDraw4Interface::GetCaps: Free : ", free9));
    }

    if (lpDDDriverCaps != nullptr) {
      lpDDDriverCaps->dwCaps2 &= ~DDCAPS2_CANCALIBRATEGAMMA;
      lpDDDriverCaps->dwCaps2 |= DDCAPS2_FLIPINTERVAL | DDCAPS2_FLIPNOVSYNC;
      lpDDDriverCaps->dwZBufferBitDepths = d3dOptions->supportD16 ? DDBD_16 | DDBD_24 : DDBD_24;
      lpDDDriverCaps->dwVidMemTotal = total9;
      lpDDDriverCaps->dwVidMemFree  = free9;
      lpDDDriverCaps->dwNumFourCCCodes = ddrawCaps::NumberOfFOURCCCodes;
    }
    if (lpDDHELCaps != nullptr) {
      lpDDHELCaps->dwCaps2 &= ~DDCAPS2_CANCALIBRATEGAMMA;
      lpDDHELCaps->dwCaps2 |= DDCAPS2_FLIPINTERVAL | DDCAPS2_FLIPNOVSYNC;
      lpDDHELCaps->dwZBufferBitDepths = d3dOptions->supportD16 ? DDBD_16 | DDBD_24 : DDBD_24;
      lpDDHELCaps->dwVidMemTotal = total9;
      lpDDHELCaps->dwVidMemFree  = free9;
      lpDDHELCaps->dwNumFourCCCodes = ddrawCaps::NumberOfFOURCCCodes;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetDisplayMode(LPDDSURFACEDESC2 lpDDSurfaceDesc) {
    if (unlikely(lpDDSurfaceDesc == nullptr))
      return DDERR_INVALIDPARAMS;

    HRESULT hr = m_proxy->GetDisplayMode(lpDDSurfaceDesc);
    if (unlikely(FAILED(hr)))
      return hr;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    if (unlikely(d3dOptions->mask8BitModes
              && lpDDSurfaceDesc->dwFlags & DDSD_PIXELFORMAT
              && lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount == 8)) {
      // Report a fake D3DFMT_R5G6B5 back buffer scenario
      lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
      lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = 16;
      lpDDSurfaceDesc->ddpfPixelFormat.dwRGBAlphaBitMask = 0x0000;
      lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = 0xf800;
      lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = 0x07e0;
      lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = 0x001f;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes) {
    if (likely(lpNumCodes != nullptr && lpCodes != nullptr)) {
      const uint32_t copyNumCodes = std::min<uint32_t>(ddrawCaps::NumberOfFOURCCCodes, *lpNumCodes);
      for (uint32_t i = 0; i < copyNumCodes; i++) {
        lpCodes[i] = ddrawCaps::SupportedFourCCs[i];
      }
    }

    if (lpNumCodes != nullptr)
      *lpNumCodes = ddrawCaps::NumberOfFOURCCCodes;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetGDISurface(LPDIRECTDRAWSURFACE4 *lplpGDIDDSurface) {
    if (unlikely(lplpGDIDDSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpGDIDDSurface);

    Com<IDirectDrawSurface4> gdiSurface;
    HRESULT hr = m_proxy->GetGDISurface(&gdiSurface);
    if (unlikely(FAILED(hr)))
      return hr;

    if (unlikely(DDrawCommonInterface::IsWrappedSurface(gdiSurface.ptr()))) {
      *lplpGDIDDSurface = gdiSurface.ref();
    } else {
      try {
        *lplpGDIDDSurface = ref(new DDraw4Surface(nullptr, std::move(gdiSurface),
                                                  this, nullptr, false));
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return DDERR_GENERIC;
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetMonitorFrequency(LPDWORD lpdwFrequency) {
    return m_proxy->GetMonitorFrequency(lpdwFrequency);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetScanLine(LPDWORD lpdwScanLine) {
    return m_proxy->GetScanLine(lpdwScanLine);
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetVerticalBlankStatus(LPBOOL lpbIsInVB) {
    return m_proxy->GetVerticalBlankStatus(lpbIsInVB);
  }

  // Should technically always return DDERR_ALREADYINITIALIZED, unless the
  // interface is created via IClassFactory, however Requiem: Avenging Angel
  // expects it to work on a regular interface too, after initially creating
  // and releasing an interface through IClassFactory (but never initializing it).
  // On native DDraw the initial interface most likely gets reused. In practice,
  // applications that don't use IClassFactory won't call this, so keep it simple.
  HRESULT STDMETHODCALLTYPE DDraw4Interface::Initialize(GUID* lpGUID) {
    if (unlikely(m_commonIntf->IsInitialized()))
      return DDERR_ALREADYINITIALIZED;

    m_commonIntf->MarkAsInitialized();

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::RestoreDisplayMode() {
    return m_proxy->RestoreDisplayMode();
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::SetCooperativeLevel(HWND hWnd, DWORD dwFlags) {
    // DDSCL_CREATEDEVICEWINDOW doesn't appear to behave properly in
    // Wine, so use the cached hWnd to set the device window instead
    if (unlikely((dwFlags & DDSCL_CREATEDEVICEWINDOW) && hWnd == nullptr
               && m_commonIntf->GetHWND() != nullptr)) {
      dwFlags &= ~DDSCL_CREATEDEVICEWINDOW;
      dwFlags |= DDSCL_SETDEVICEWINDOW;
      hWnd = m_commonIntf->GetHWND();
    }

    HRESULT hr = m_proxy->SetCooperativeLevel(hWnd, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonIntf->SetCooperativeLevel(hWnd, dwFlags);

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags) {
    Logger::debug(str::format("DDraw4Interface::SetDisplayMode: ", dwWidth, "x", dwHeight, ":", dwBPP, "@", dwRefreshRate));

    HRESULT hr = m_proxy->SetDisplayMode(dwWidth, dwHeight, dwBPP, dwRefreshRate, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    DDrawCommonSurface* ps = m_commonIntf->GetPrimarySurface();

    if (likely(ps != nullptr)) {
      hr = ps->RefreshSurfaceDescripton(true);
      if (unlikely(FAILED(hr)))
        Logger::warn("DDraw4Interface::SetDisplayMode: Failed to update primary surface desc");
    }

    if (likely(m_commonIntf->GetOptions()->backBufferResize)) {
      const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Ignore any mode size dimensions when in windowed present mode
      if (exclusiveMode) {
        Logger::debug("DDraw4Interface::SetDisplayMode: Exclusive full-screen present mode in use");
        DDrawModeSize* modeSize = m_commonIntf->GetModeSize();
        if (modeSize->width != dwWidth || modeSize->height != dwHeight) {
          modeSize->width  = dwWidth;
          modeSize->height = dwHeight;
        }
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) {
    if (unlikely(dwFlags & DDWAITVB_BLOCKBEGINEVENT))
      return DDERR_UNSUPPORTED;

    // Switch to a default presentation interval when an application
    // tries to wait for vertical blank, if we're not already doing so
    D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();
    if (unlikely(commonDevice != nullptr && !m_commonIntf->GetWaitForVBlank())) {
      Logger::info("DDraw4Interface::WaitForVerticalBlank: Switching to D3DPRESENT_INTERVAL_DEFAULT for presentation");

      d3d9::D3DPRESENT_PARAMETERS resetParams = *commonDevice->GetPresentParameters();
      resetParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
      HRESULT hr = commonDevice->ResetD3D9Swapchain(&resetParams);
      if (likely(SUCCEEDED(hr)))
        m_commonIntf->SetWaitForVBlank(true);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetAvailableVidMem(LPDDSCAPS2 lpDDCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree) {
    if (unlikely(lpdwTotal == nullptr && lpdwFree == nullptr))
      return DD_OK;

    static constexpr DWORD Megabytes = 1024 * 1024;
    static constexpr DWORD MaxMemory = ddrawCaps::MaxTextureMemory * Megabytes;
    static constexpr DWORD ReservedMemory = ddrawCaps::ReservedTextureMemory * Megabytes;

    D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();
    if (likely(commonDevice != nullptr)) {
      d3d9::IDirect3DDevice9* d3d9Device = commonDevice->GetD3D9Device();

      DWORD total9 = static_cast<DWORD>(commonDevice->GetTotalTextureMemory());
      DWORD free9  = static_cast<DWORD>(d3d9Device->GetAvailableTextureMem());

      if (likely(total9 >= MaxMemory)) {
        const DWORD delta = total9 - MaxMemory;
        total9 = MaxMemory - ReservedMemory;
        free9 = free9 > delta + ReservedMemory ? free9 - (delta + ReservedMemory) : 0;
      }

      //Logger::debug(str::format("DDraw4Interface::GetAvailableVidMem: Total: ", total9));
      //Logger::debug(str::format("DDraw4Interface::GetAvailableVidMem: Free : ", free9));

      if (lpdwTotal != nullptr)
        *lpdwTotal = total9;
      if (lpdwFree != nullptr)
        *lpdwFree  = free9;

    } else {
      DWORD total6 = 0;
      DWORD free6  = 0;

      HRESULT hr = m_proxy->GetAvailableVidMem(lpDDCaps, &total6, &free6);
      if (unlikely(FAILED(hr))) {
        Logger::err("DDraw4Interface::GetAvailableVidMem: Failed proxied call");
        if (lpdwTotal != nullptr)
          *lpdwTotal = 0;
        if (lpdwFree != nullptr)
          *lpdwFree  = 0;
        return hr;
      }

      //Logger::debug(str::format("DDraw4Interface::GetAvailableVidMem: DDraw Total: ", total6));
      //Logger::debug(str::format("DDraw4Interface::GetAvailableVidMem: DDraw Free : ", free6));

      DWORD total9 = 0;
      DWORD free9  = 0;

      if (unlikely(total6 < MaxMemory)) {
        total9 = total6;
        free9 = free6;
      } else {
        const DWORD delta = total6 - MaxMemory;
        total9 = MaxMemory - ReservedMemory;
        free9 = free6 > delta + ReservedMemory ? free6 - (delta + ReservedMemory) : 0;
      }

      //Logger::debug(str::format("DDraw4Interface::GetAvailableVidMem: Total: ", total9));
      //Logger::debug(str::format("DDraw4Interface::GetAvailableVidMem: Free : ", free9));

      if (lpdwTotal != nullptr)
        *lpdwTotal = total9;
      if (lpdwFree != nullptr)
        *lpdwFree  = free9;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetSurfaceFromDC(HDC hdc, LPDIRECTDRAWSURFACE4 *pSurf) {
    if (unlikely(pSurf == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(pSurf);

    Com<IDirectDrawSurface4> surface;
    HRESULT hr = m_proxy->GetSurfaceFromDC(hdc, &surface);
    if (unlikely(FAILED(hr)))
      return hr;

    try {
      *pSurf = ref(new DDraw4Surface(nullptr, std::move(surface), this, nullptr, false));
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::RestoreAllSurfaces() {
    return m_proxy->RestoreAllSurfaces();
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::TestCooperativeLevel() {
    D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();

    if (unlikely(commonDevice == nullptr))
      return m_proxy->TestCooperativeLevel();

    d3d9::IDirect3DDevice9* d3d9Device = commonDevice->GetD3D9Device();

    HRESULT hr = d3d9Device->TestCooperativeLevel();
    if (unlikely(FAILED(hr)))
      return DDERR_NOEXCLUSIVEMODE;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDraw4Interface::GetDeviceIdentifier(LPDDDEVICEIDENTIFIER pDDDI, DWORD dwFlags) {
    if (unlikely(pDDDI == nullptr))
      return DDERR_INVALIDPARAMS;

    // We need to share the device identifier with the underlying
    // DDraw implementation, as some games validate it in various places
    DDDEVICEIDENTIFIER pDDDIproxy = { };
    HRESULT hr = m_proxy->GetDeviceIdentifier(&pDDDIproxy, dwFlags);
    if (unlikely(FAILED(hr)))
      return hr;

    const d3d9::D3DADAPTER_IDENTIFIER9* adapterIdentifier9 = m_commonIntf->GetAdapterIdentifier();
    // This is typically the "2D accelerator" in the system
    if (unlikely(dwFlags & DDGDI_GETHOSTIDENTIFIER)) {
      CopyToStringArray(pDDDI->szDriver, "vga.dll");
      // The Matrox G400 TechDemo expects the description to be aligned with native
      CopyToStringArray(pDDDI->szDescription, "DirectDraw HAL");
      pDDDI->liDriverVersion.QuadPart = 0;
      pDDDI->dwVendorId               = 0;
      pDDDI->dwDeviceId               = 0;
      pDDDI->dwSubSysId               = 0;
      pDDDI->dwRevision               = 0;
      pDDDI->guidDeviceIdentifier     = pDDDIproxy.guidDeviceIdentifier;
    } else {
      memcpy(&pDDDI->szDriver,      &adapterIdentifier9->Driver,      sizeof(adapterIdentifier9->Driver));
      memcpy(&pDDDI->szDescription, &adapterIdentifier9->Description, sizeof(adapterIdentifier9->Description));
      // Neither ATI, nor Nvidia (Windows XP) native drivers at the time reported a version
      pDDDI->liDriverVersion.QuadPart = 0;
      pDDDI->dwVendorId               = adapterIdentifier9->VendorId;
      pDDDI->dwDeviceId               = adapterIdentifier9->DeviceId;
      pDDDI->dwSubSysId               = adapterIdentifier9->SubSysId;
      pDDDI->dwRevision               = adapterIdentifier9->Revision;
      pDDDI->guidDeviceIdentifier     = pDDDIproxy.guidDeviceIdentifier;
    }

    Logger::info(str::format("DDraw4Interface::GetDeviceIdentifier: Reporting: ", pDDDI->szDescription));

    return DD_OK;
  }

}