#include "d3d6_interface.h"

#include "d3d6_device.h"
#include "d3d6_buffer.h"
#include "d3d6_material.h"
#include "d3d6_viewport.h"

#include "../d3d_light.h"
#include "../d3d_multithread.h"

#include "../d3d3/d3d3_interface.h"
#include "../d3d5/d3d5_interface.h"

#include "../ddraw4/ddraw4_interface.h"
#include "../ddraw4/ddraw4_surface.h"

namespace dxvk {

  D3D6Interface::D3D6Interface(
        D3DCommonInterface* commonD3DIntf,
        DDrawCommonInterface* commonIntf,
        Com<IDirect3D3>&& d3d6IntfProxy,
        IUnknown* pParent)
    : DDrawWrappedObject<IUnknown, IDirect3D3>(pParent, std::move(d3d6IntfProxy))
    , m_commonD3DIntf ( commonD3DIntf )
    , m_commonIntf ( commonIntf ) {
    if (m_commonD3DIntf == nullptr)
      m_commonD3DIntf = new D3DCommonInterface();

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();
    // Retrieve and cache the base capabilities
    m_desc = GetD3D6BaseCaps(d3dOptions);

    d3d9::IDirect3D9* d3d9Intf = m_commonD3DIntf->GetD3D9Interface();

    // Get the bridge interface to D3D9
    if (unlikely(FAILED(d3d9Intf->QueryInterface(__uuidof(IDxvkLegacyD3DInterfaceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D6Interface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    m_commonD3DIntf->SetD3D6Interface(this);

    m_bridge->SetD3DCompatibility(D3DCompatibility::D3D6);
  }

  D3D6Interface::~D3D6Interface() {
    if (m_commonD3DIntf->GetD3D6Interface() == this)
      m_commonD3DIntf->SetD3D6Interface(nullptr);
  }

  // Interlocked refcount with the parent IDirectDraw4
  ULONG STDMETHODCALLTYPE D3D6Interface::AddRef() {
    if (likely(m_parent != nullptr)) {
      IUnknown* origin = m_commonIntf->GetOrigin();
      if (likely(origin != nullptr))
        return origin->AddRef();
      else
        return m_parent->AddRef();
    } else {
      return ComObjectClamp::AddRef();
    }
  }

  // Interlocked refcount with the parent IDirectDraw4
  ULONG STDMETHODCALLTYPE D3D6Interface::Release() {
    if (likely(m_parent != nullptr)) {
      IUnknown* origin = m_commonIntf->GetOrigin();
      if (likely(origin != nullptr))
        return origin->Release();
      else
        return m_parent->Release();
    } else {
      return ComObjectClamp::Release();
    }
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirectDraw))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDraw2))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (riid == __uuidof(IDirectDraw4)) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    // Some games query for legacy D3D interfaces
    if (unlikely(riid == __uuidof(IDirect3D))) {
      if (m_commonD3DIntf->GetD3D3Interface() != nullptr)
        return m_commonD3DIntf->GetD3D3Interface()->QueryInterface(riid, ppvObject);

      m_d3d3Intf = new D3D3Interface(m_commonD3DIntf.ptr(), m_commonIntf, m_parent);
      m_commonIntf->SetD3D3Interface(m_d3d3Intf.ptr());
      *ppvObject = m_d3d3Intf.ref();

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirect3D2))) {
      if (m_commonD3DIntf->GetD3D5Interface() != nullptr)
        return m_commonD3DIntf->GetD3D5Interface()->QueryInterface(riid, ppvObject);

      m_d3d5Intf = new D3D5Interface(m_commonD3DIntf.ptr(), m_commonIntf, m_parent);
      *ppvObject = m_d3d5Intf.ref();

      return S_OK;
    }

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirect3D3))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D6Interface::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg) {
    if (unlikely(lpEnumDevicesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    // D3D6 reports both HAL and HEL caps for any time of device,
    // with minor differences between the two. Note that the
    // device listing order matters, so list RGB first, HAL second.

    HRESULT hr;

    // Software emulation, this is expected to be exposed
    GUID guidRGB = IID_IDirect3DRGBDevice;
    D3DDEVICEDESC descRGB_HAL = m_desc;
    ApplyD3D6DeviceCaps(&descRGB_HAL, guidRGB);
    D3DDEVICEDESC descRGB_HEL = m_desc;
    ApplyD3D6DeviceCaps(&descRGB_HEL, guidRGB);
    descRGB_HAL.dwFlags = 0;
    descRGB_HAL.dcmColorModel = 0;
    // Some applications apparently care about HAL texture caps
    descRGB_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_POW2
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    descRGB_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_POW2
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;

    if (likely(!d3dOptions->legacyDeviceNames)) {
      static char deviceDescRGB[100] = "D6VK RGB";
      static char deviceNameRGB[100] = "D6VK RGB";
      hr = lpEnumDevicesCallback(&guidRGB, &deviceDescRGB[0], &deviceNameRGB[0],
                                 &descRGB_HAL, &descRGB_HEL, lpUserArg);
    } else {
      static char legacyDeviceDescRGB[100] = "RGB Emulation";
      static char legacyDeviceNameRGB[100] = "RGB Emulation";
      hr = lpEnumDevicesCallback(&guidRGB, &legacyDeviceDescRGB[0], &legacyDeviceNameRGB[0],
                                 &descRGB_HAL, &descRGB_HEL, lpUserArg);
    }
    if (hr != D3DENUMRET_OK)
      return D3D_OK;

    // Hardware acceleration
    GUID guidHAL = IID_IDirect3DHALDevice;
    D3DDEVICEDESC descHAL_HAL = m_desc;
    ApplyD3D6DeviceCaps(&descHAL_HAL, guidHAL);
    D3DDEVICEDESC descHAL_HEL = m_desc;
    ApplyD3D6DeviceCaps(&descHAL_HEL, guidHAL);
    descHAL_HEL.dcmColorModel = 0;
    // Some applications apparently care about RGB texture caps
    descHAL_HEL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_POW2
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    descHAL_HEL.dpcTriCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                          & ~D3DPTEXTURECAPS_POW2
                                          & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    descHAL_HEL.dwDevCaps &= ~D3DDEVCAPS_HWTRANSFORMANDLIGHT
                           & ~D3DDEVCAPS_DRAWPRIMITIVES2
                           & ~D3DDEVCAPS_DRAWPRIMITIVES2EX;

    if (likely(!d3dOptions->legacyDeviceNames)) {
      static char deviceDescHAL[100] = "D6VK HAL";
      static char deviceNameHAL[100] = "D6VK HAL";
      hr = lpEnumDevicesCallback(&guidHAL, &deviceDescHAL[0], &deviceNameHAL[0],
                                 &descHAL_HAL, &descHAL_HEL, lpUserArg);
    } else {
      static char legacyDeviceDescHAL[100] = "Direct3D HAL";
      static char legacyDeviceNameHAL[100] = "Direct3D HAL";
      hr = lpEnumDevicesCallback(&guidHAL, &legacyDeviceDescHAL[0], &legacyDeviceNameHAL[0],
                                 &descHAL_HAL, &descHAL_HEL, lpUserArg);
    }
    if (hr != D3DENUMRET_OK)
      return D3D_OK;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateLight(LPDIRECT3DLIGHT *lplpDirect3DLight, IUnknown *pUnkOuter) {
    if (unlikely(lplpDirect3DLight == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDirect3DLight);

    *lplpDirect3DLight = ref(new D3DLight(this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateMaterial(LPDIRECT3DMATERIAL3 *lplpDirect3DMaterial, IUnknown *pUnkOuter) {
    if (unlikely(lplpDirect3DMaterial == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDirect3DMaterial);

    *lplpDirect3DMaterial = ref(new D3D6Material(this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateViewport(LPDIRECT3DVIEWPORT3 *lplpD3DViewport, IUnknown *pUnkOuter) {
    InitReturnPtr(lplpD3DViewport);

    *lplpD3DViewport = ref(new D3D6Viewport(nullptr, this));

    return D3D_OK;
  }

  // Minimal implementation which should suffice in most cases
  HRESULT STDMETHODCALLTYPE D3D6Interface::FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR) {
    if (unlikely(lpD3DFDS == nullptr || lpD3DFDR == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpD3DFDS->dwSize != sizeof(D3DFINDDEVICESEARCH)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!IsValidFindDeviceResultSize(lpD3DFDR->dwSize)))
      return DDERR_INVALIDPARAMS;

    // Software emulation, this is expected to be exposed
    D3DDEVICEDESC descRGB_HAL = m_desc;
    ApplyD3D6DeviceCaps(&descRGB_HAL, IID_IDirect3DRGBDevice);
    D3DDEVICEDESC descRGB_HEL = m_desc;
    ApplyD3D6DeviceCaps(&descRGB_HEL, IID_IDirect3DRGBDevice);
    descRGB_HAL.dwFlags = 0;
    descRGB_HAL.dcmColorModel = 0;
    // Some applications apparently care about HAL texture caps
    descRGB_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_POW2
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    descRGB_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_POW2
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;

    // Hardware acceleration
    D3DDEVICEDESC descHAL_HAL = m_desc;
    ApplyD3D6DeviceCaps(&descHAL_HAL, IID_IDirect3DHALDevice);
    D3DDEVICEDESC descHAL_HEL = m_desc;
    ApplyD3D6DeviceCaps(&descHAL_HEL, IID_IDirect3DHALDevice);
    descHAL_HEL.dcmColorModel = 0;
    // Some applications apparently care about HEL texture caps
    descHAL_HEL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_POW2
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    descHAL_HEL.dpcTriCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                          & ~D3DPTEXTURECAPS_POW2
                                          & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    descHAL_HEL.dwDevCaps &= ~D3DDEVCAPS_HWTRANSFORMANDLIGHT
                           & ~D3DDEVCAPS_DRAWPRIMITIVES2
                           & ~D3DDEVCAPS_DRAWPRIMITIVES2EX;

    lpD3DFDR->dwSize = sizeof(D3DFINDDEVICERESULT);

    if (lpD3DFDS->dwFlags & D3DFDS_GUID) {
      // IID_IDirect3DRampDevice and IID_IDirect3DMMXDevice return DDERR_NOTFOUND in D3D6
      if (lpD3DFDS->guid == IID_IDirect3DRGBDevice) {
        lpD3DFDR->guid = IID_IDirect3DRGBDevice;
        lpD3DFDR->ddHwDesc = descRGB_HAL;
        lpD3DFDR->ddSwDesc = descRGB_HEL;
      } else if (lpD3DFDS->guid == IID_IDirect3DHALDevice) {
        lpD3DFDR->guid = IID_IDirect3DHALDevice;
        lpD3DFDR->ddHwDesc = descHAL_HAL;
        lpD3DFDR->ddSwDesc = descHAL_HEL;
      } else {
        Logger::err(str::format("D3D6Interface::FindDevice: Unknown device type: ", lpD3DFDS->guid));
        return DDERR_NOTFOUND;
      }
    } else if (lpD3DFDS->dwFlags & D3DFDS_HARDWARE) {
      if (likely(lpD3DFDS->bHardware == TRUE)) {
        lpD3DFDR->guid = IID_IDirect3DHALDevice;
        lpD3DFDR->ddHwDesc = descHAL_HAL;
        lpD3DFDR->ddSwDesc = descHAL_HEL;
      } else {
        lpD3DFDR->guid = IID_IDirect3DRGBDevice;
        lpD3DFDR->ddHwDesc = descRGB_HAL;
        lpD3DFDR->ddSwDesc = descRGB_HEL;
      }
    } else if (lpD3DFDS->dwFlags & D3DFDS_COLORMODEL) {
      lpD3DFDR->guid = IID_IDirect3DHALDevice;
      lpD3DFDR->ddHwDesc = descHAL_HAL;
      lpD3DFDR->ddSwDesc = descHAL_HEL;
    } else if (lpD3DFDS->dwFlags == 0) {
      lpD3DFDR->guid = IID_IDirect3DHALDevice;
      lpD3DFDR->ddHwDesc = descHAL_HAL;
      lpD3DFDR->ddSwDesc = descHAL_HEL;
    } else {
      Logger::err("D3D6Interface::FindDevice: Unhandled matching type");
      return DDERR_NOTFOUND;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateDevice(REFCLSID rclsid, LPDIRECTDRAWSURFACE4 lpDDS, LPDIRECT3DDEVICE3 *lplpD3DDevice, IUnknown *pUnkOuter) {
    if (unlikely(lplpD3DDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpD3DDevice);

    if (unlikely(lpDDS == nullptr))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    DWORD deviceCreationFlags9 = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    bool  isHALDevice          = false;
    bool  halFallback          = false;
    bool  rgbFallback          = false;

    if (likely(!d3dOptions->forceSWVP)) {
      if (rclsid == IID_IDirect3DHALDevice) {
        Logger::info("D3D6Interface::CreateDevice: Creating an IID_IDirect3DHALDevice device");
        deviceCreationFlags9 = D3DCREATE_MIXED_VERTEXPROCESSING;
        isHALDevice = true;
      } else if (rclsid == IID_IDirect3DRGBDevice) {
        Logger::info("D3D6Interface::CreateDevice: Creating an IID_IDirect3DRGBDevice device");
      } else if (rclsid == IID_IDirect3DMMXDevice) {
        Logger::warn("D3D6Interface::CreateDevice: Unsupported MMX device, falling back to RGB");
        rgbFallback = true;
      } else if (rclsid == IID_IDirect3DRampDevice) {
        Logger::warn("D3D6Interface::CreateDevice: Unsupported Ramp device, falling back to RGB");
        rgbFallback = true;
      } else {
        // Revenant uses a bogus rclsid (not WineD3D's), so fall back to HAL in these cases
        if (unlikely(rclsid != IID_WineD3DDevice)) {
          Logger::warn("D3D6Interface::CreateDevice: Unknown device type, falling back to HAL");
          Logger::warn(str::format(rclsid));
        } else {
          Logger::info("D3D6Interface::CreateDevice: Creating an IID_WineD3DDevice HAL device");
        }
        halFallback = true;
        // Don't enforce isHALDevice RT validations
      }
    }

    const IID rclsidOverride = halFallback ? IID_IDirect3DHALDevice :
                               rgbFallback ? IID_IDirect3DRGBDevice : rclsid;

    HWND hWnd = m_commonIntf->GetHWND();
    // Needed to sometimes safely skip intro playback on legacy devices
    if (unlikely(hWnd == nullptr)) {
      Logger::debug("D3D6Interface::CreateDevice: HWND is NULL");
    }

    Com<DDraw4Surface> rt4;
    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(lpDDS))) {
      Logger::err("D3D6Interface::CreateDevice: Unwrapped surface passed as RT");
      return DDERR_UNSUPPORTED;
    } else {
      rt4 = static_cast<DDraw4Surface*>(lpDDS);
    }

    HRESULT hrRT = rt4->GetCommonSurface()->ValidateRTUsage(isHALDevice, true);
    if (unlikely(FAILED(hrRT)))
      return hrRT;

    DDSURFACEDESC2 desc;
    desc.dwSize = sizeof(DDSURFACEDESC2);
    lpDDS->GetSurfaceDesc(&desc);

    DWORD backBufferWidth  = desc.dwWidth;
    DWORD BackBufferHeight = desc.dwHeight;

    if (likely(d3dOptions->backBufferResize)) {
      const bool exclusiveMode = m_commonIntf->GetCooperativeLevel() & DDSCL_EXCLUSIVE;

      // Ignore any mode size dimensions when in windowed present mode
      if (exclusiveMode) {
        DDrawModeSize* modeSize = m_commonIntf->GetModeSize();
        // Wayland apparently needs this for somewhat proper back buffer sizing
        if ((modeSize->width  && modeSize->width  < desc.dwWidth)
         || (modeSize->height && modeSize->height < desc.dwHeight)) {
          Logger::info("D3D6Interface::CreateDevice: Enforcing mode dimensions");
          backBufferWidth  = modeSize->width;
          BackBufferHeight = modeSize->height;
        }
      }
    }

    const d3d9::D3DFORMAT backBufferFormat = ConvertFormat(desc.ddpfPixelFormat);

    const DWORD cooperativeLevel = m_commonIntf->GetCooperativeLevel();

    if ((cooperativeLevel & DDSCL_MULTITHREADED) || d3dOptions->forceMultiThreaded) {
      Logger::info("D3D6Interface::CreateDevice: Using thread safe runtime synchronization");
      deviceCreationFlags9 |= D3DCREATE_MULTITHREADED;
    }
    // DDSCL_FPUPRESERVE does not exist prior to DDraw7,
    // and DDSCL_FPUSETUP is NOT the default state
    if (!(cooperativeLevel & DDSCL_FPUSETUP))
      deviceCreationFlags9 |= D3DCREATE_FPU_PRESERVE;
    if (cooperativeLevel & DDSCL_NOWINDOWCHANGES)
      deviceCreationFlags9 |= D3DCREATE_NOWINDOWCHANGES;

    Logger::info(str::format("D3D6Interface::CreateDevice: Back buffer size: ", desc.dwWidth, "x", desc.dwHeight));

    const DWORD backBufferCount = DetermineBackBufferCount<IDirectDrawSurface4>(rt4->GetProxied());
    Logger::info(str::format("D3D6Interface::CreateDevice: Back buffer count: ", backBufferCount));

    // Determine the supported AA sample count by querying the D3D9 interface
    const d3d9::D3DMULTISAMPLE_TYPE multiSampleType = d3dOptions->emulateFSAA != FSAAEmulation::Disabled ?
                                                      m_commonD3DIntf->GetMultiSampleType(backBufferFormat) :
                                                      d3d9::D3DMULTISAMPLE_NONE;

    // Always appears to be enabled when running in non-exclusive mode
    const bool vBlankStatus = m_commonIntf->GetWaitForVBlank();

    d3d9::D3DPRESENT_PARAMETERS params;
    params.BackBufferWidth    = backBufferWidth;
    params.BackBufferHeight   = BackBufferHeight;
    params.BackBufferFormat   = backBufferFormat;
    params.BackBufferCount    = backBufferCount;
    params.MultiSampleType    = multiSampleType; // Controlled through D3DRENDERSTATE_ANTIALIAS
    params.MultiSampleQuality = 0;
    params.SwapEffect         = d3d9::D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow      = hWnd;
    params.Windowed           = TRUE; // Always use windowed, so that we can delegate mode switching to DDraw
    params.EnableAutoDepthStencil     = FALSE;
    params.AutoDepthStencilFormat     = d3d9::D3DFMT_UNKNOWN;
    params.Flags                      = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER; // Needed for back buffer locks
    params.FullScreen_RefreshRateInHz = 0; // We'll get the right mode/refresh rate set by DDraw, just play along
    params.PresentationInterval       = vBlankStatus ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;

    Com<d3d9::IDirect3DDevice9> device9;
    HRESULT hr = m_commonD3DIntf->GetD3D9Interface()->CreateDevice(
      D3DADAPTER_DEFAULT,
      d3d9::D3DDEVTYPE_HAL,
      hWnd,
      deviceCreationFlags9,
      &params,
      &device9
    );

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Interface::CreateDevice: Failed to create the D3D9 device");
      return hr;
    }

    try{
      Com<D3D6Device> device6 = new D3D6Device(nullptr, this, rclsidOverride, &params,
                                               std::move(device9), rt4.ptr(), deviceCreationFlags9);

      // Set the common device on the common interface
      m_commonIntf->SetCommonD3DDevice(device6->GetCommonD3DDevice());
      // Now that we have a valid D3D9 device pointer, we can initialize the depth stencil (if any)
      device6->InitializeDS();

      *lplpD3DDevice = device6.ref();
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::CreateVertexBuffer(LPD3DVERTEXBUFFERDESC lpVBDesc, LPDIRECT3DVERTEXBUFFER *lpD3DVertexBuffer, DWORD dwFlags, IUnknown *pUnkOuter) {
    if (unlikely(lpVBDesc == nullptr || lpD3DVertexBuffer == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lpD3DVertexBuffer);

    *lpD3DVertexBuffer = ref(new D3D6VertexBuffer(this, dwFlags, lpVBDesc));

    return D3D_OK;
  }

  // Total Club Manager 2003 uses a D3D6 interface to query for supported Z buffer formats,
  // so report what we know is supported by D3D9, otherwise the game will error out on startup
  HRESULT STDMETHODCALLTYPE D3D6Interface::EnumZBufferFormats(REFCLSID riidDevice, LPD3DENUMPIXELFORMATSCALLBACK lpEnumCallback, LPVOID lpContext) {
    if (unlikely(lpEnumCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    // There are just 3 supported depth stencil formats to worry about
    // in D3D9, so let's just enumerate them liniarly, for better clarity
    DDPIXELFORMAT depthFormat;
    HRESULT hr;

    if (likely(d3dOptions->supportD16)) {
      depthFormat = GetZBufferFormat(d3d9::D3DFMT_D16);
      hr = lpEnumCallback(&depthFormat, lpContext);
      if (unlikely(hr != D3DENUMRET_OK))
        return D3D_OK;
    }

    // Apparently some games expect D3DFMT_D24X8 to have a 24-bit
    // dwZBufferBitDepth, so we have to enumerate both variants.
    // According to Wine tests, Windows Vista and newer also enumerate both.
    depthFormat = GetZBufferFormat(d3d9::D3DFMT_D24X8);
    depthFormat.dwZBufferBitDepth = 24;
    hr = lpEnumCallback(&depthFormat, lpContext);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    // Expendable relies on having only the 24-bit dwZBufferBitDepth variant
    // of D3DFMT_D24X8 enumerated in order to have working projected shadows
    if (likely(d3dOptions->support32BitDepth)) {
      depthFormat = GetZBufferFormat(d3d9::D3DFMT_D24X8);
      hr = lpEnumCallback(&depthFormat, lpContext);
      if (unlikely(hr != D3DENUMRET_OK))
        return D3D_OK;

      depthFormat = GetZBufferFormat(d3d9::D3DFMT_D24S8);
      hr = lpEnumCallback(&depthFormat, lpContext);
      if (unlikely(hr != D3DENUMRET_OK))
        return D3D_OK;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Interface::EvictManagedTextures() {
    HRESULT hr = m_proxy->EvictManagedTextures();
    if (unlikely(FAILED(hr)))
      return hr;

    D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();
    if (likely(commonDevice != nullptr)) {
      d3d9::IDirect3DDevice9* d3d9Device = commonDevice->GetD3D9Device();

      // Note: This doesn't do anything in the D3D9 backend at the moment
      hr = d3d9Device->EvictManagedResources();
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D6Interface::EvictManagedTextures: Failed D3D9 managed resource eviction");
        return hr;
      }
    }

    return D3D_OK;
  }

}