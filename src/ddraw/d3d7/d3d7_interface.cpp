#include "d3d7_interface.h"

#include "d3d7_device.h"
#include "d3d7_buffer.h"

#include "../d3d_multithread.h"

#include "../ddraw7/ddraw7_interface.h"
#include "../ddraw7/ddraw7_surface.h"

namespace dxvk {

  D3D7Interface::D3D7Interface(
        D3DCommonInterface* commonD3DIntf,
        DDrawCommonInterface* commonIntf,
        Com<IDirect3D7>&& d3d7IntfProxy,
        IUnknown* pParent)
    : DDrawWrappedObject<IUnknown, IDirect3D7>(pParent, std::move(d3d7IntfProxy))
    , m_commonD3DIntf ( commonD3DIntf )
    , m_commonIntf ( commonIntf ) {
    if (m_commonD3DIntf == nullptr)
      m_commonD3DIntf = new D3DCommonInterface();

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();
    // Retrieve and cache the base capabilities
    m_desc = GetD3D7BaseCaps(d3dOptions);

    d3d9::IDirect3D9* d3d9Intf = m_commonD3DIntf->GetD3D9Interface();

    // Get the bridge interface to D3D9
    if (unlikely(FAILED(d3d9Intf->QueryInterface(__uuidof(IDxvkLegacyD3DInterfaceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D7Interface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    m_commonD3DIntf->SetD3D7Interface(this);

    m_bridge->SetD3DCompatibility(D3DCompatibility::D3D7);
  }

  D3D7Interface::~D3D7Interface() {
    if (m_commonD3DIntf->GetD3D7Interface() == this)
      m_commonD3DIntf->SetD3D7Interface(nullptr);
  }

  // Interlocked refcount with the parent IDirectDraw7
  ULONG STDMETHODCALLTYPE D3D7Interface::AddRef() {
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

  // Interlocked refcount with the parent IDirectDraw7
  ULONG STDMETHODCALLTYPE D3D7Interface::Release() {
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

  HRESULT STDMETHODCALLTYPE D3D7Interface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IDirectDraw7)) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    // Some games query for legacy ddraw interfaces
    if (unlikely(riid == __uuidof(IDirectDraw)
              || riid == __uuidof(IDirectDraw2)
              || riid == __uuidof(IDirectDraw4))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirect3D7))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D7Interface::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::EnumDevices(LPD3DENUMDEVICESCALLBACK7 cb, void *ctx) {
    if (unlikely(cb == nullptr))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    // Ideally we should take all the adapters into account, however
    // D3D7 supports one RGB (software emulation) device, one HAL device,
    // and one HAL T&L device, all indentified via GUIDs

    // Note: The enumeration order seems to matter for some applications,
    // such as (The) Summoner, so always report RGB first, then HAL, then T&L HAL

    HRESULT hr;

    // Software emulation, this is expected to be exposed
    D3DDEVICEDESC7 desc7RGB = m_desc;
    ApplyD3D7DeviceCaps(&desc7RGB, IID_IDirect3DRGBDevice);
    if (likely(!d3dOptions->legacyDeviceNames)) {
      static char deviceDescRGB[100] = "D7VK RGB";
      static char deviceNameRGB[100] = "D7VK RGB";
      hr = cb(&deviceDescRGB[0], &deviceNameRGB[0], &desc7RGB, ctx);
    } else {
      static char legacyDeviceDescRGB[100] = "RGB Emulation";
      static char legacyDeviceNameRGB[100] = "RGB Emulation";
      hr = cb(&legacyDeviceDescRGB[0], &legacyDeviceNameRGB[0], &desc7RGB, ctx);
    }
    if (hr != D3DENUMRET_OK)
      return D3D_OK;

    // Hardware acceleration (no T&L)
    D3DDEVICEDESC7 desc7HAL = m_desc;
    ApplyD3D7DeviceCaps(&desc7HAL, IID_IDirect3DHALDevice);
    if (likely(!d3dOptions->legacyDeviceNames)) {
      static char deviceDescHAL[100] = "D7VK HAL";
      static char deviceNameHAL[100] = "D7VK HAL";
      hr = cb(&deviceDescHAL[0], &deviceNameHAL[0], &desc7HAL, ctx);
    } else {
      static char legacyDeviceDescHAL[100] = "Direct3D HAL";
      static char legacyDeviceNameHAL[100] = "Direct3D HAL";
      hr = cb(&legacyDeviceDescHAL[0], &legacyDeviceNameHAL[0], &desc7HAL, ctx);
    }
    if (hr != D3DENUMRET_OK)
      return D3D_OK;

    // Hardware acceleration with T&L
    D3DDEVICEDESC7 desc7TNL = m_desc;
    ApplyD3D7DeviceCaps(&desc7TNL, IID_IDirect3DTnLHalDevice);
    if (likely(!d3dOptions->legacyDeviceNames)) {
      static char deviceDescTNL[100] = "D7VK T&L HAL";
      static char deviceNameTNL[100] = "D7VK T&L HAL";
      hr = cb(&deviceDescTNL[0], &deviceNameTNL[0], &desc7TNL, ctx);
    } else {
      static char legacyDeviceDescTNL[100] = "Direct3D T&L HAL";
      static char legacyDeviceNameTNL[100] = "Direct3D T&L HAL";
      hr = cb(&legacyDeviceDescTNL[0], &legacyDeviceNameTNL[0], &desc7TNL, ctx);
    }
    if (hr != D3DENUMRET_OK)
      return D3D_OK;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::CreateDevice(REFCLSID rclsid, IDirectDrawSurface7 *surface, IDirect3DDevice7 **ppd3dDevice) {
    if (unlikely(ppd3dDevice == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(ppd3dDevice);

    if (unlikely(surface == nullptr))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    DWORD deviceCreationFlags9 = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    bool  isHALOrTNLHALDevice  = false;
    bool  halFallback          = false;
    bool  rgbFallback          = false;

    if (rclsid == IID_IDirect3DTnLHalDevice) {
      Logger::info("D3D7Interface::CreateDevice: Creating an IID_IDirect3DTnLHalDevice device");
      if (likely(!d3dOptions->forceSWVP))
        deviceCreationFlags9 = D3DCREATE_HARDWARE_VERTEXPROCESSING;
      isHALOrTNLHALDevice = true;
    } else if (rclsid == IID_IDirect3DHALDevice) {
      Logger::info("D3D7Interface::CreateDevice: Creating an IID_IDirect3DHALDevice device");
      if (likely(!d3dOptions->forceSWVP))
        deviceCreationFlags9 = D3DCREATE_MIXED_VERTEXPROCESSING;
      isHALOrTNLHALDevice = true;
    } else if (rclsid == IID_IDirect3DRGBDevice) {
      Logger::info("D3D7Interface::CreateDevice: Creating an IID_IDirect3DRGBDevice device");
    } else if (rclsid == IID_IDirect3DMMXDevice) {
      Logger::warn("D3D7Interface::CreateDevice: Unsupported MMX device, falling back to RGB");
      rgbFallback = true;
    } else if (rclsid == IID_IDirect3DRampDevice) {
      Logger::warn("D3D7Interface::CreateDevice: Unsupported Ramp device, falling back to RGB");
      rgbFallback = true;
    } else if (unlikely(rclsid == IID_IUnknown)) {
      Logger::warn("D3D7Interface::CreateDevice: Unsupported IID_IUnknown, falling back to RGB");
      rgbFallback = true;
    } else {
      if (unlikely(rclsid != IID_WineD3DDevice)) {
        Logger::warn("D3D7Interface::CreateDevice: Unknown device type, falling back to HAL");
        Logger::warn(str::format(rclsid));
      } else {
        Logger::info("D3D7Interface::CreateDevice: Creating an IID_WineD3DDevice HAL device");
      }
      halFallback = true;
      // Don't enforce isHALOrTNLHALDevice RT validations
    }

    const IID rclsidOverride = halFallback ? IID_IDirect3DHALDevice :
                               rgbFallback ? IID_IDirect3DRGBDevice : rclsid;

    HWND hWnd = m_commonIntf->GetHWND();
    // Needed to sometimes safely skip intro playback on legacy devices
    if (unlikely(hWnd == nullptr)) {
      Logger::debug("D3D7Interface::CreateDevice: HWND is NULL");
    }

    Com<DDraw7Surface> rt7;
    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(surface))) {
      Logger::err("D3D7Interface::CreateDevice: Unwrapped surface passed as RT");
      return DDERR_UNSUPPORTED;
    } else {
      rt7 = static_cast<DDraw7Surface*>(surface);
    }

    HRESULT hrRT = rt7->GetCommonSurface()->ValidateRTUsage7(isHALOrTNLHALDevice, true);
    if (unlikely(FAILED(hrRT)))
      return hrRT;

    Com<IDirect3DDevice7> d3d7DeviceProxy;
    HRESULT hr = m_proxy->CreateDevice(rclsidOverride, rt7->GetShadowOrProxied(), &d3d7DeviceProxy);
    if (unlikely(FAILED(hr))) {
      Logger::warn("D3D7Interface::CreateDevice: Failed to create the proxy device");
      return hr;
    }

    DDSURFACEDESC2 desc;
    desc.dwSize = sizeof(DDSURFACEDESC2);
    surface->GetSurfaceDesc(&desc);

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
          Logger::info("D3D7Interface::CreateDevice: Enforcing mode dimensions");
          backBufferWidth  = modeSize->width;
          BackBufferHeight = modeSize->height;
        }
      }
    }

    const d3d9::D3DFORMAT backBufferFormat = ConvertFormat(desc.ddpfPixelFormat);

    const DWORD cooperativeLevel = m_commonIntf->GetCooperativeLevel();

    if ((cooperativeLevel & DDSCL_MULTITHREADED) || d3dOptions->forceMultiThreaded) {
      Logger::info("D3D7Interface::CreateDevice: Using thread safe runtime synchronization");
      deviceCreationFlags9 |= D3DCREATE_MULTITHREADED;
    }
    // DDSCL_FPUSETUP was used exclusively prior to DDraw7 and had the opposite effect
    // to DDSCL_FPUPRESERVE. It is still present in DDraw7, now as the default state.
    // Some D3D7 applications still specify it explicitly, so account for that regardless.
    if (!(cooperativeLevel & DDSCL_FPUSETUP) && (cooperativeLevel & DDSCL_FPUPRESERVE))
      deviceCreationFlags9 |= D3DCREATE_FPU_PRESERVE;
    if (cooperativeLevel & DDSCL_NOWINDOWCHANGES)
      deviceCreationFlags9 |= D3DCREATE_NOWINDOWCHANGES;

    Logger::info(str::format("D3D7Interface::CreateDevice: Back buffer size: ", desc.dwWidth, "x", desc.dwHeight));

    const DWORD backBufferCount = DetermineBackBufferCount<IDirectDrawSurface7>(rt7->GetProxied());
    Logger::info(str::format("D3D7Interface::CreateDevice: Back buffer count: ", backBufferCount));

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
    hr = m_commonD3DIntf->GetD3D9Interface()->CreateDevice(
      D3DADAPTER_DEFAULT,
      d3d9::D3DDEVTYPE_HAL,
      hWnd,
      deviceCreationFlags9,
      &params,
      &device9
    );

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Interface::CreateDevice: Failed to create the D3D9 device");
      return hr;
    }

    try{
      Com<D3D7Device> device7 = new D3D7Device(nullptr, std::move(d3d7DeviceProxy), this,
                                               rclsidOverride, &params, std::move(device9),
                                               rt7.ptr(), deviceCreationFlags9);

      // Set the common device on the common interface
      m_commonIntf->SetCommonD3DDevice(device7->GetCommonD3DDevice());
      // Now that we have a valid common D3D device on the DDraw interface,
      // we can initialize the render target and depth stencil (if any)
      hr = device7->InitializeRTAndDS();
      if (unlikely(FAILED(hr)))
        return hr;

      *ppd3dDevice = device7.ref();
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::CreateVertexBuffer(D3DVERTEXBUFFERDESC *desc, IDirect3DVertexBuffer7 **ppVertexBuffer, DWORD usage) {
    if (unlikely(desc == nullptr || ppVertexBuffer == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(ppVertexBuffer);

    *ppVertexBuffer = ref(new D3D7VertexBuffer(this, desc));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Interface::EnumZBufferFormats(REFCLSID riidDevice, LPD3DENUMPIXELFORMATSCALLBACK lpEnumCallback, LPVOID lpContext) {
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

  HRESULT STDMETHODCALLTYPE D3D7Interface::EvictManagedTextures() {
    HRESULT hr = m_proxy->EvictManagedTextures();
    if (unlikely(FAILED(hr)))
      return hr;

    D3DCommonDevice* commonDevice = m_commonIntf->GetCommonD3DDevice();
    if (likely(commonDevice != nullptr)) {
      d3d9::IDirect3DDevice9* d3d9Device = commonDevice->GetD3D9Device();

      // Note: This doesn't do anything in the D3D9 backend at the moment
      hr = d3d9Device->EvictManagedResources();
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7Interface::EvictManagedTextures: Failed D3D9 managed resource eviction");
        return hr;
      }
    }

    return D3D_OK;
  }

}