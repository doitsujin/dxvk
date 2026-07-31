#include "ddraw_interface.h"

#include "../d3d_common_device.h"

#include "ddraw_surface.h"

#include "../ddraw_clipper.h"
#include "../ddraw_palette.h"

#include "../ddraw7/ddraw7_interface.h"
#include "../ddraw4/ddraw4_interface.h"
#include "../ddraw2/ddraw2_interface.h"

#include "../d3d3/d3d3_interface.h"
#include "../d3d5/d3d5_interface.h"
#include "../d3d6/d3d6_interface.h"

namespace dxvk {

  DDrawInterface::DDrawInterface(
        DDrawCommonInterface* commonIntf,
        Com<IDirectDraw>&& proxyIntf)
    : DDrawWrappedObject<IUnknown, IDirectDraw>(nullptr, std::move(proxyIntf))
    , m_commonIntf ( commonIntf ) {

    if (likely(m_commonIntf == nullptr)) {
      // We need a temporary D3D9 interface to retrieve the options
      Com<d3d9::IDirect3D9> d3d9Intf = d3d9::Direct3DCreate9(D3D_SDK_VERSION);
      Com<IDxvkLegacyD3DInterfaceBridge> d3d9Bridge;

      if (unlikely(FAILED(d3d9Intf->QueryInterface(__uuidof(IDxvkLegacyD3DInterfaceBridge), reinterpret_cast<void**>(&d3d9Bridge))))) {
        throw DxvkError("DDrawInterface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
      }

      m_commonIntf = new DDrawCommonInterface(D3DOptions(*d3d9Bridge->GetConfig()));
    }

    if (m_commonIntf->GetOrigin() == nullptr)
      m_commonIntf->SetOrigin(this);

    m_commonIntf->SetDDInterface(this);
  }

  DDrawInterface::~DDrawInterface() {
    if (m_commonIntf->GetOrigin() == this)
      m_commonIntf->SetOrigin(nullptr);

    m_commonIntf->SetDDInterface(nullptr);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    // Standard way of retrieving a D3D6 interface
    if (riid == __uuidof(IDirect3D3)) {
      // GTA 2 queries for IDirect3D3 on IDirectDraw, after creating a
      // IDirectDraw4 interface and a ton of surfaces... so forward the call
      if (m_commonIntf->GetDD4Interface() != nullptr)
        return m_commonIntf->GetDD4Interface()->QueryInterface(riid, ppvObject);

      Com<IDirect3D3> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      Com<D3D6Interface> d3d6Intf = new D3D6Interface(nullptr, m_commonIntf.ptr(), std::move(ppvProxyObject), this);
      *ppvObject = d3d6Intf.ref();

      return S_OK;
    }
    // Standard way of retrieving a D3D5 interface
    if (unlikely(riid == __uuidof(IDirect3D2))) {
      // Initialize the IDirect3D2 interlocked object
      if (unlikely(m_d3d5Intf == nullptr))
        m_d3d5Intf = new D3D5Interface(nullptr, m_commonIntf.ptr(), this);

      *ppvObject = m_d3d5Intf.ref();

      return S_OK;
    }
    // Standard way of retrieving a D3D3 interface
    if (unlikely(riid == __uuidof(IDirect3D))) {
      // Initialize the IDirect3D interlocked object
      if (unlikely(m_d3d3Intf == nullptr)) {
        m_d3d3Intf = new D3D3Interface(nullptr, m_commonIntf.ptr(), this);
        m_commonIntf->SetD3D3Interface(m_d3d3Intf.ptr());
      }

      *ppvObject = m_d3d3Intf.ref();

      return S_OK;
    }
    // Standard way of getting a DDraw4 interface
    if (riid == __uuidof(IDirectDraw4)) {
      if (m_commonIntf->GetDD4Interface() != nullptr)
        return m_commonIntf->GetDD4Interface()->QueryInterface(riid, ppvObject);

      Com<IDirectDraw4> ppvProxyObject;
      HRESULT hr = m_proxy->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      *ppvObject = ref(new DDraw4Interface(m_commonIntf.ptr(), std::move(ppvProxyObject)));

      return S_OK;
    }
    // Standard way of getting a DDraw2 interface
    if (riid == __uuidof(IDirectDraw2)) {
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
    // Quite a lot of games query for this IID during intro playback
    if (unlikely(riid == GUID_IAMMediaStream)) {
      return m_proxy->QueryInterface(riid, ppvObject);
    }
    // Also seen queried by some games, such as V-Rally 2: Expert Edition
    if (unlikely(riid == GUID_IMediaStream)) {
      return m_proxy->QueryInterface(riid, ppvObject);
    }

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirectDraw))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("DDrawInterface::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  // The documentation states: "The IDirectDraw::Compact method is not currently implemented."
  HRESULT STDMETHODCALLTYPE DDrawInterface::Compact() {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER *lplpDDClipper, IUnknown *pUnkOuter) {
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

  HRESULT STDMETHODCALLTYPE DDrawInterface::CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE *lplpDDPalette, IUnknown *pUnkOuter) {
    if (unlikely(lplpDDPalette == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDDPalette);

    Com<IDirectDrawPalette> lplpDDPaletteProxy;
    HRESULT hr = m_proxy->CreatePalette(dwFlags, lpColorTable, &lplpDDPaletteProxy, pUnkOuter);
    if (unlikely(FAILED(hr)))
      return hr;

    // Palettes created from IDirectDraw and IDirectDraw2 do not ref their parent interfaces
    *lplpDDPalette = ref(new DDrawPalette(std::move(lplpDDPaletteProxy), nullptr));

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE *lplpDDSurface, IUnknown *pUnkOuter) {
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
    }

    if (likely(lpDDSurfaceDesc->dwFlags & DDSD_PIXELFORMAT)) {
      // WineD3D will fail to create 8-bit texture surfaces at times, for whatever reason...
      if (unlikely((lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_TEXTURE)
               &&  (lpDDSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_RGB)
               &&   lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount == 8
               && !(lpDDSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8))) {
        static bool s_8bitTextureWarningShown;

        if (!std::exchange(s_8bitTextureWarningShown, true))
          Logger::warn("DDrawInterface::CreateSurface: Use of potentially unsupported 8-bit texture surface");
      }

      // Work around a WineD3D bug/limitation that prevents
      // read back from L6V5U5 and X8L8V8U8 video memory surfaces
      //
      // Note: Doing this for other surfaces/formats is a bad idea,
      // because some games expect these flags to remain in place, and
      // may crash in case they find that's not the case
      if (unlikely((lpDDSurfaceDesc->ddpfPixelFormat.dwFlags == (DDPF_BUMPDUDV | DDPF_BUMPLUMINANCE))
                && (lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY))) {
        Logger::warn("DDrawInterface::CreateSurface: Video memory DDPF_BUMPLUMINANCE surface");
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
          Logger::debug("DDrawInterface::CreateSurface: Using D16 instead of D24X8");
          lpDDSurfaceDesc->ddpfPixelFormat.dwZBufferBitDepth = 16;
          lpDDSurfaceDesc->ddpfPixelFormat.dwZBitMask = 0xFFFF;
        } else if (unlikely(lpDDSurfaceDesc->ddpfPixelFormat.dwZBitMask == 0xFFFFFFFF)) {
          if (m_commonIntf->GetOptions()->useD24X8forD32) {
            // In case of up-front unsupported and unadvertised D32 depth stencil use,
            // replace it with D24X8, as some games, such as Sacrifice, rely on it
            // to properly enable 32-bit display modes (and revert to 16-bit otherwise)
            Logger::debug("DDrawInterface::CreateSurface: Using D24X8 instead of D32");
            lpDDSurfaceDesc->ddpfPixelFormat.dwZBitMask = 0xFFFFFF;
          } else {
            Logger::warn("DDrawInterface::CreateSurface: Use of unsupported D32");
          }
        }
      }
    }

    Com<IDirectDrawSurface> ddrawSurfaceProxied;
    HRESULT hr = m_proxy->CreateSurface(lpDDSurfaceDesc, &ddrawSurfaceProxied, pUnkOuter);
    // Some games simply try creating surfaces with various formats until something works...
    if (unlikely(FAILED(hr)))
      return hr;

    try{
      // Surfaces created from IDirectDraw and IDirectDraw2 do not ref their parent interfaces
      Com<DDrawSurface> surface = new DDrawSurface(nullptr, std::move(ddrawSurfaceProxied), this, nullptr, false);

      if (unlikely(lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)) {
        m_commonIntf->SetPrimarySurface(surface->GetCommonSurface());

        // Shadow surface creation for the primary surface
        // (it needs to be based on the same incoming desc)
        if (unlikely(!surface->GetCommonSurface()->Is8BitFormat() &&
                      m_commonIntf->GetOptions()->forceLegacyPresent)) {
          DDSURFACEDESC shadowDesc = *lpDDSurfaceDesc;
          const DDSURFACEDESC* primaryDesc = surface->GetCommonSurface()->GetDesc();

          shadowDesc.ddsCaps.dwCaps &= ~DDSCAPS_PRIMARYSURFACE & ~DDSCAPS_COMPLEX & ~DDSCAPS_FLIP;
          shadowDesc.ddsCaps.dwCaps |= DDSCAPS_OFFSCREENPLAIN;
          shadowDesc.dwFlags &= ~DDSD_BACKBUFFERCOUNT;
          // Dimensions aren't specified in the incoming desc,
          // but are explicitly needed for non-primary surfaces
          shadowDesc.dwFlags |= DDSD_WIDTH | DDSD_HEIGHT;
          shadowDesc.dwWidth  = primaryDesc->dwWidth;
          shadowDesc.dwHeight = primaryDesc->dwHeight;

          Com<IDirectDrawSurface> ddrawSurfaceShadow;
          hr = m_proxy->CreateSurface(&shadowDesc, &ddrawSurfaceShadow, pUnkOuter);
          if (unlikely(FAILED(hr))) {
            Logger::warn("DDrawInterface::CreateSurface: Failed to create shadow surface");
          } else {
            Com<DDrawSurface> shadowSurf = new DDrawSurface(nullptr, std::move(ddrawSurfaceShadow),
                                                            this, nullptr, false);
            surface->SetShadowSurface(std::move(shadowSurf));
          }
        }
      }

      *lplpDDSurface = surface.ref();
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::DuplicateSurface(LPDIRECTDRAWSURFACE lpDDSurface, LPDIRECTDRAWSURFACE *lplpDupDDSurface) {
    if (unlikely(lpDDSurface == nullptr || lplpDupDDSurface == nullptr))
      return DDERR_CANTDUPLICATE;

    InitReturnPtr(lplpDupDDSurface);

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(lpDDSurface))) {
      Logger::err("DDrawInterface::DuplicateSurface: Received an unwrapped surface");
      return DDERR_CANTDUPLICATE;
    }

    DDrawSurface* ddrawSurface = static_cast<DDrawSurface*>(lpDDSurface);

    Com<IDirectDrawSurface> dupSurface;
    HRESULT hr = m_proxy->DuplicateSurface(ddrawSurface->GetProxied(), &dupSurface);
    if (unlikely(FAILED(hr)))
      return hr;

    try {
      *lplpDupDDSurface = ref(new DDrawSurface(nullptr, std::move(dupSurface), this, nullptr, false));
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback) {
    if (unlikely(lpEnumModesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    std::vector<DDSURFACEDESC> displayModes;
    HRESULT hr = m_proxy->EnumDisplayModes(dwFlags, lpDDSurfaceDesc, reinterpret_cast<void*>(&displayModes), EnumDisplayModesCallback);
    if (unlikely(FAILED(hr)))
      return hr;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    hr = DDENUMRET_OK;

    auto displayModeIt = displayModes.begin();
    while (displayModeIt != displayModes.end() && hr == DDENUMRET_OK) {
      DDSURFACEDESC dmDesc = *displayModeIt;

      if (unlikely(d3dOptions->mask8BitModes && dmDesc.ddpfPixelFormat.dwRGBBitCount == 8)) {
        static bool s_maskModeWarningShown;

        if (!std::exchange(s_maskModeWarningShown, true))
          Logger::warn("DDrawInterface::EnumDisplayModes: Masking 8-bit display modes");
      } else {
        hr = lpEnumModesCallback(&dmDesc, lpContext);
      }

      ++displayModeIt;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    if (unlikely(lpEnumSurfacesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    std::vector<AttachedSurface> attachedSurfaces;
    HRESULT hr = m_proxy->EnumSurfaces(dwFlags, lpDDSD, reinterpret_cast<void*>(&attachedSurfaces), EnumAttachedSurfacesCallback);
    if (unlikely(FAILED(hr)))
      return hr;

    hr = DDENUMRET_OK;

    auto surfaceIt = attachedSurfaces.begin();
    while (surfaceIt != attachedSurfaces.end() && hr == DDENUMRET_OK) {
      Com<IDirectDrawSurface> surface = surfaceIt->surface;

      Com<DDrawSurface> ddrawSurface;
      try {
        ddrawSurface = new DDrawSurface(nullptr, std::move(surface), this, nullptr, false);
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return DDERR_GENERIC;
      }
      hr = lpEnumSurfacesCallback(ddrawSurface.ref(), &surfaceIt->desc, lpContext);

      ++surfaceIt;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::FlipToGDISurface() {
    DDrawCommonSurface* ps = m_commonIntf->GetPrimarySurface();

    // A primary surface must exist for a GDI flip to be possible
    if (unlikely(ps == nullptr))
      return DDERR_NOTFOUND;

    if (unlikely(!ps->IsFlippable()))
      return DDERR_NOTFLIPPABLE;

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) {
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

      //Logger::debug(str::format("DDrawInterface::GetCaps: Total: ", total9));
      //Logger::debug(str::format("DDrawInterface::GetCaps: Free : ", free9));
    } else {
      const DWORD total3 = lpDDDriverCaps != nullptr ? lpDDDriverCaps->dwVidMemTotal : 0;
      const DWORD free3  = lpDDDriverCaps != nullptr ? lpDDDriverCaps->dwVidMemFree  : 0;

      //Logger::debug(str::format("DDrawInterface::GetCaps: DDraw Total: ", total3));
      //Logger::debug(str::format("DDrawInterface::GetCaps: DDraw Free : ", free3));

      if (unlikely(total3 < MaxMemory)) {
        total9 = total3;
        free9 = free3;
      } else {
        const DWORD delta = total3 - MaxMemory;
        total9 = MaxMemory - ReservedMemory;
        free9 = free3 > delta + ReservedMemory ? free3 - (delta + ReservedMemory) : 0;
      }

      //Logger::debug(str::format("DDrawInterface::GetCaps: Total: ", total9));
      //Logger::debug(str::format("DDrawInterface::GetCaps: Free : ", free9));
    }

    if (lpDDDriverCaps != nullptr) {
      lpDDDriverCaps->dwZBufferBitDepths = d3dOptions->supportD16 ? DDBD_16 | DDBD_24 : DDBD_24;
      lpDDDriverCaps->dwVidMemTotal = total9;
      lpDDDriverCaps->dwVidMemFree  = free9;
      lpDDDriverCaps->dwNumFourCCCodes = ddrawCaps::NumberOfFOURCCCodes;
    }
    if (lpDDHELCaps != nullptr) {
      lpDDHELCaps->dwZBufferBitDepths = d3dOptions->supportD16 ? DDBD_16 | DDBD_24 : DDBD_24;
      lpDDHELCaps->dwVidMemTotal = total9;
      lpDDHELCaps->dwVidMemFree  = free9;
      lpDDHELCaps->dwNumFourCCCodes = ddrawCaps::NumberOfFOURCCCodes;
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc) {
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

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes) {
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

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetGDISurface(LPDIRECTDRAWSURFACE *lplpGDIDDSurface) {
    if (unlikely(lplpGDIDDSurface == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpGDIDDSurface);

    Com<IDirectDrawSurface> gdiSurface;
    HRESULT hr = m_proxy->GetGDISurface(&gdiSurface);
    if (unlikely(FAILED(hr)))
      return hr;

    if (unlikely(DDrawCommonInterface::IsWrappedSurface(gdiSurface.ptr()))) {
      *lplpGDIDDSurface = gdiSurface.ref();
    } else {
      try {
        *lplpGDIDDSurface = ref(new DDrawSurface(nullptr, std::move(gdiSurface), this, nullptr, false));
      } catch (const DxvkError& e) {
        Logger::err(e.message());
        return DDERR_GENERIC;
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetMonitorFrequency(LPDWORD lpdwFrequency) {
    return m_proxy->GetMonitorFrequency(lpdwFrequency);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetScanLine(LPDWORD lpdwScanLine) {
    return m_proxy->GetScanLine(lpdwScanLine);
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::GetVerticalBlankStatus(LPBOOL lpbIsInVB) {
    return m_proxy->GetVerticalBlankStatus(lpbIsInVB);
  }

  // Should technically always return DDERR_ALREADYINITIALIZED, unless the
  // interface is created via IClassFactory, however Requiem: Avenging Angel
  // expects it to work on a regular interface too, after initially creating
  // and releasing an interface through IClassFactory (but never initializing it).
  // On native DDraw the initial interface most likely gets reused. In practice,
  // applications that don't use IClassFactory won't call this, so keep it simple.
  HRESULT STDMETHODCALLTYPE DDrawInterface::Initialize(GUID* lpGUID) {
    if (unlikely(m_commonIntf->IsInitialized()))
      return DDERR_ALREADYINITIALIZED;

    m_commonIntf->MarkAsInitialized();

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::RestoreDisplayMode() {
    return m_proxy->RestoreDisplayMode();
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::SetCooperativeLevel(HWND hWnd, DWORD dwFlags) {
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

  HRESULT STDMETHODCALLTYPE DDrawInterface::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP) {
    Logger::debug(str::format("DDrawInterface::SetDisplayMode: ", dwWidth, "x", dwHeight, ":", dwBPP));

    HRESULT hr = m_proxy->SetDisplayMode(dwWidth, dwHeight, dwBPP);
    if (unlikely(FAILED(hr)))
      return hr;

    DDrawCommonSurface* ps = m_commonIntf->GetPrimarySurface();

    if (likely(ps != nullptr)) {
      hr = ps->RefreshSurfaceDescripton(true);
      if (unlikely(FAILED(hr)))
        Logger::warn("DDrawInterface::SetDisplayMode: Failed to update primary surface desc");
    }

    if (likely(m_commonIntf->GetOptions()->backBufferResize)) {
      const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Ignore any mode size dimensions when in windowed present mode
      if (exclusiveMode) {
        Logger::debug("DDrawInterface::SetDisplayMode: Exclusive full-screen present mode in use");
        DDrawModeSize* modeSize = m_commonIntf->GetModeSize();
        if (modeSize->width != dwWidth || modeSize->height != dwHeight) {
          modeSize->width  = dwWidth;
          modeSize->height = dwHeight;
        }
      }
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawInterface::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) {
    if (unlikely(dwFlags & DDWAITVB_BLOCKBEGINEVENT))
      return DDERR_UNSUPPORTED;

    // Switch to a default presentation interval when an application
    // tries to wait for vertical blank, if we're not already doing so
    D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();
    if (unlikely(commonDevice != nullptr && !m_commonIntf->GetWaitForVBlank())) {
      Logger::info("DDrawInterface::WaitForVerticalBlank: Switching to D3DPRESENT_INTERVAL_DEFAULT for presentation");

      d3d9::D3DPRESENT_PARAMETERS resetParams = *commonDevice->GetPresentParameters();
      resetParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
      HRESULT hr = commonDevice->ResetD3D9Swapchain(&resetParams);
      if (likely(SUCCEEDED(hr)))
        m_commonIntf->SetWaitForVBlank(true);
    }

    return DD_OK;
  }

}