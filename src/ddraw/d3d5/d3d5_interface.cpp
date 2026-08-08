#include "d3d5_interface.h"

#include "d3d5_device.h"
#include "d3d5_material.h"
#include "d3d5_viewport.h"

#include "../d3d_light.h"

#include "../d3d3/d3d3_interface.h"
#include "../d3d6/d3d6_interface.h"

#include "../ddraw/ddraw_interface.h"
#include "../ddraw/ddraw_surface.h"
#include "../ddraw2/ddraw2_interface.h"
#include "../ddraw2/ddraw3_surface.h"

namespace dxvk {

  D3D5Interface::D3D5Interface(
        D3DCommonInterface* commonD3DIntf,
        DDrawCommonInterface* m_commonIntf,
        IUnknown* pParent)
    : DDrawChildObject<IUnknown, IDirect3D2>(pParent)
    , m_commonD3DIntf ( commonD3DIntf )
    , m_commonIntf ( m_commonIntf ) {
    if (m_commonD3DIntf == nullptr)
      m_commonD3DIntf = new D3DCommonInterface();

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();
    // Retrieve and cache the base capabilities
    m_desc = GetD3D5BaseCaps(d3dOptions);

    d3d9::IDirect3D9* d3d9Intf = m_commonD3DIntf->GetD3D9Interface();

    // Get the bridge interface to D3D9
    if (unlikely(FAILED(d3d9Intf->QueryInterface(__uuidof(IDxvkLegacyD3DInterfaceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D5Interface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    m_commonD3DIntf->SetD3D5Interface(this);

    m_bridge->SetD3DCompatibility(D3DCompatibility::D3D5);
  }

  D3D5Interface::~D3D5Interface() {
    if (m_commonD3DIntf->GetD3D5Interface() == this)
      m_commonD3DIntf->SetD3D5Interface(nullptr);
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D5Interface::AddRef() {
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

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D5Interface::Release() {
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

  HRESULT STDMETHODCALLTYPE D3D5Interface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IDirectDraw)) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (riid == __uuidof(IDirectDraw2)) {
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
    if (unlikely(riid == __uuidof(IDirect3D3))) {
      if (m_commonD3DIntf->GetD3D6Interface() != nullptr)
        return m_commonD3DIntf->GetD3D6Interface()->QueryInterface(riid, ppvObject);

      // We don't have a proxied object on D3D5Interface, so use the parent
      // DDraw interface to get a proxied object for the queried D3D6Interface
      Com<IDirect3D3> ppvProxyObject;
      HRESULT hr = m_parent->QueryInterface(riid, reinterpret_cast<void**>(&ppvProxyObject));
      if (unlikely(FAILED(hr)))
        return hr;

      m_d3d6Intf = new D3D6Interface(m_commonD3DIntf.ptr(), m_commonIntf, std::move(ppvProxyObject), m_parent);
      *ppvObject = m_d3d6Intf.ref();

      return S_OK;
    }

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirect3D2))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D5Interface::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D5Interface::EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg) {
    if (unlikely(lpEnumDevicesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    // D3D5 reports both HAL and HEL caps for any type of device,
    // with minor differences between the two. Note that the
    // device listing order matters, so list RAMP first, RGB second,
    // and HAL last. A RAMP device also needs to be advertised in D3D5,
    // since some games like Resident Evil expect it to be present.

    HRESULT hr;

    // RAMP device (monochrome), this is expected to be exposed
    GUID guidRAMP = IID_IDirect3DRampDevice;
    // The caps of a RAMP device are mostly identical to an RGB device
    D3DDEVICEDESC2 desc2RAMP_HAL = m_desc;
    ApplyD3D5DeviceCaps(&desc2RAMP_HAL, guidRAMP);
    D3DDEVICEDESC2 desc2RAMP_HEL = m_desc;
    ApplyD3D5DeviceCaps(&desc2RAMP_HEL, guidRAMP);
    D3DDEVICEDESC descRAMP_HAL = { };
    D3DDEVICEDESC descRAMP_HEL = { };
    desc2RAMP_HAL.dwFlags = 0;
    desc2RAMP_HAL.dcmColorModel = 0;
    // RAMP devices use a monochrome color model
    desc2RAMP_HEL.dcmColorModel = D3DCOLOR_MONO;
    // Some applications apparently care about HAL texture caps
    desc2RAMP_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                             & ~D3DPTEXTURECAPS_POW2
                                             & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    desc2RAMP_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                             & ~D3DPTEXTURECAPS_POW2
                                             & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    memcpy(&descRAMP_HAL, &desc2RAMP_HAL, sizeof(D3DDEVICEDESC2));
    memcpy(&descRAMP_HEL, &desc2RAMP_HEL, sizeof(D3DDEVICEDESC2));
    if (likely(!d3dOptions->legacyDeviceNames)) {
      static char deviceDescRAMP[100] = "D5VK Ramp";
      static char deviceNameRAMP[100] = "D5VK Ramp";
      hr = lpEnumDevicesCallback(&guidRAMP, &deviceDescRAMP[0], &deviceNameRAMP[0],
                                 &descRAMP_HAL, &descRAMP_HEL, lpUserArg);
    } else {
      static char legacyDeviceDescRAMP[100] = "Ramp Emulation";
      static char legacyDeviceNameRAMP[100] = "Ramp Emulation";
      hr = lpEnumDevicesCallback(&guidRAMP, &legacyDeviceDescRAMP[0], &legacyDeviceNameRAMP[0],
                                 &descRAMP_HAL, &descRAMP_HEL, lpUserArg);
    }
    if (hr != D3DENUMRET_OK)
      return D3D_OK;

    // Software emulation, this is expected to be exposed
    GUID guidRGB = IID_IDirect3DRGBDevice;
    D3DDEVICEDESC2 desc2RGB_HAL = m_desc;
    ApplyD3D5DeviceCaps(&desc2RGB_HAL, guidRGB);
    D3DDEVICEDESC2 desc2RGB_HEL = m_desc;
    ApplyD3D5DeviceCaps(&desc2RGB_HEL, guidRGB);
    D3DDEVICEDESC descRGB_HAL = { };
    D3DDEVICEDESC descRGB_HEL = { };
    desc2RGB_HAL.dwFlags = 0;
    desc2RGB_HAL.dcmColorModel = 0;
    // Some applications apparently care about HAL texture caps
    desc2RGB_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                            & ~D3DPTEXTURECAPS_POW2
                                            & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    desc2RGB_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                            & ~D3DPTEXTURECAPS_POW2
                                            & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    memcpy(&descRGB_HAL, &desc2RGB_HAL, sizeof(D3DDEVICEDESC2));
    memcpy(&descRGB_HEL, &desc2RGB_HEL, sizeof(D3DDEVICEDESC2));

    if (likely(!d3dOptions->legacyDeviceNames)) {
      static char deviceDescRGB[100] = "D5VK RGB";
      static char deviceNameRGB[100] = "D5VK RGB";
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
    D3DDEVICEDESC2 desc2HAL_HAL = m_desc;
    ApplyD3D5DeviceCaps(&desc2HAL_HAL, guidHAL);
    D3DDEVICEDESC2 desc2HAL_HEL = m_desc;
    ApplyD3D5DeviceCaps(&desc2HAL_HEL, guidHAL);
    D3DDEVICEDESC descHAL_HAL = { };
    D3DDEVICEDESC descHAL_HEL = { };
    desc2HAL_HEL.dcmColorModel = 0;
    // Some applications apparently care about HEL texture caps
    desc2HAL_HEL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                            & ~D3DPTEXTURECAPS_POW2
                                            & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    desc2HAL_HEL.dpcTriCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_POW2
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    desc2HAL_HEL.dwDevCaps &= ~D3DDEVCAPS_HWTRANSFORMANDLIGHT
                            & ~D3DDEVCAPS_DRAWPRIMITIVES2
                            & ~D3DDEVCAPS_DRAWPRIMITIVES2EX;
    memcpy(&descHAL_HAL, &desc2HAL_HAL, sizeof(D3DDEVICEDESC2));
    memcpy(&descHAL_HEL, &desc2HAL_HEL, sizeof(D3DDEVICEDESC2));

    if (likely(!d3dOptions->legacyDeviceNames)) {
      static char deviceDescHAL[100] = "D5VK HAL";
      static char deviceNameHAL[100] = "D5VK HAL";
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

  HRESULT STDMETHODCALLTYPE D3D5Interface::CreateLight(LPDIRECT3DLIGHT *lplpDirect3DLight, IUnknown *pUnkOuter) {
    if (unlikely(lplpDirect3DLight == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDirect3DLight);

    *lplpDirect3DLight = ref(new D3DLight(this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Interface::CreateMaterial(LPDIRECT3DMATERIAL2 *lplpDirect3DMaterial, IUnknown *pUnkOuter) {
    if (unlikely(lplpDirect3DMaterial == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDirect3DMaterial);

    *lplpDirect3DMaterial = ref(new D3D5Material(this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Interface::CreateViewport(LPDIRECT3DVIEWPORT2 *lplpD3DViewport, IUnknown *pUnkOuter) {
    InitReturnPtr(lplpD3DViewport);

    *lplpD3DViewport = ref(new D3D5Viewport(nullptr, this));

    return D3D_OK;
  }

  // Minimal implementation which should suffice in most cases
  HRESULT STDMETHODCALLTYPE D3D5Interface::FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR) {
    if (unlikely(lpD3DFDS == nullptr || lpD3DFDR == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpD3DFDS->dwSize != sizeof(D3DFINDDEVICESEARCH)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!IsValidFindDeviceResultSize(lpD3DFDR->dwSize)))
      return DDERR_INVALIDPARAMS;

    // Software emulation, this is expected to be exposed
    D3DDEVICEDESC2 descRGB_HAL = m_desc;
    ApplyD3D5DeviceCaps(&descRGB_HAL, IID_IDirect3DRGBDevice);
    D3DDEVICEDESC2 descRGB_HEL = m_desc;
    ApplyD3D5DeviceCaps(&descRGB_HEL, IID_IDirect3DRGBDevice);
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
    D3DDEVICEDESC2 descHAL_HAL = m_desc;
    ApplyD3D5DeviceCaps(&descHAL_HAL, IID_IDirect3DHALDevice);
    D3DDEVICEDESC2 descHAL_HEL = m_desc;
    ApplyD3D5DeviceCaps(&descHAL_HEL, IID_IDirect3DHALDevice);
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

    D3DFINDDEVICERESULT2 lpD3DFRD2 = { };
    lpD3DFRD2.dwSize = sizeof(D3DFINDDEVICERESULT2);

    if (lpD3DFDS->dwFlags & D3DFDS_GUID) {
      if (lpD3DFDS->guid == IID_IDirect3DRGBDevice ||
          lpD3DFDS->guid == IID_IDirect3DMMXDevice ||
          lpD3DFDS->guid == IID_IDirect3DRampDevice) {
        lpD3DFRD2.guid = IID_IDirect3DRGBDevice;
        lpD3DFRD2.ddHwDesc = descRGB_HAL;
        lpD3DFRD2.ddSwDesc = descRGB_HEL;
      } else if (lpD3DFDS->guid == IID_IDirect3DHALDevice) {
        lpD3DFRD2.guid = IID_IDirect3DHALDevice;
        lpD3DFRD2.ddHwDesc = descHAL_HAL;
        lpD3DFRD2.ddSwDesc = descHAL_HEL;
      } else {
        Logger::err(str::format("D3D5Interface::FindDevice: Unknown device type: ", lpD3DFDS->guid));
        return DDERR_NOTFOUND;
      }

      memcpy(lpD3DFDR, &lpD3DFRD2, sizeof(D3DFINDDEVICERESULT2));
    } else if (lpD3DFDS->dwFlags & D3DFDS_HARDWARE) {
      if (likely(lpD3DFDS->bHardware == TRUE)) {
        lpD3DFRD2.guid = IID_IDirect3DHALDevice;
        lpD3DFRD2.ddHwDesc = descHAL_HAL;
        lpD3DFRD2.ddSwDesc = descHAL_HEL;
      } else {
        lpD3DFRD2.guid = IID_IDirect3DRGBDevice;
        lpD3DFRD2.ddHwDesc = descRGB_HAL;
        lpD3DFRD2.ddSwDesc = descRGB_HEL;
      }

      memcpy(lpD3DFDR, &lpD3DFRD2, sizeof(D3DFINDDEVICERESULT2));
    } else if (lpD3DFDS->dwFlags & D3DFDS_COLORMODEL) {
      lpD3DFRD2.guid = IID_IDirect3DHALDevice;
      lpD3DFRD2.ddHwDesc = descHAL_HAL;
      lpD3DFRD2.ddSwDesc = descHAL_HEL;

      memcpy(lpD3DFDR, &lpD3DFRD2, sizeof(D3DFINDDEVICERESULT2));
    } else if (lpD3DFDS->dwFlags == 0) {
      lpD3DFRD2.guid = IID_IDirect3DHALDevice;
      lpD3DFRD2.ddHwDesc = descHAL_HAL;
      lpD3DFRD2.ddSwDesc = descHAL_HEL;

      memcpy(lpD3DFDR, &lpD3DFRD2, sizeof(D3DFINDDEVICERESULT2));
    } else {
      Logger::err("D3D5Interface::FindDevice: Unhandled matching type");
      return DDERR_NOTFOUND;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D5Interface::CreateDevice(REFCLSID rclsid, LPDIRECTDRAWSURFACE lpDDS, LPDIRECT3DDEVICE2 *lplpD3DDevice) {
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
        Logger::info("D3D5Interface::CreateDevice: Creating an IID_IDirect3DHALDevice device");
        deviceCreationFlags9 = D3DCREATE_MIXED_VERTEXPROCESSING;
        isHALDevice = true;
      } else if (rclsid == IID_IDirect3DRGBDevice) {
        Logger::info("D3D5Interface::CreateDevice: Creating an IID_IDirect3DRGBDevice device");
      } else if (rclsid == IID_IDirect3DMMXDevice) {
        Logger::warn("D3D5Interface::CreateDevice: Unsupported MMX device, falling back to RGB");
        rgbFallback = true;
      } else if (rclsid == IID_IDirect3DRampDevice) {
        Logger::warn("D3D5Interface::CreateDevice: Unsupported Ramp device, falling back to RGB");
        rgbFallback = true;
      } else {
        if (unlikely(rclsid != IID_WineD3DDevice)) {
          Logger::warn("D3D5Interface::CreateDevice: Unknown device type, falling back to HAL");
          Logger::warn(str::format(rclsid));
        } else {
          Logger::info("D3D5Interface::CreateDevice: Creating an IID_WineD3DDevice HAL device");
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
      Logger::debug("D3D5Interface::CreateDevice: HWND is NULL");
    }

    Com<DDrawSurface> rt;
    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(lpDDS))) {
      // Nightmare Creatures passes an IDirectDrawSurface3 surface as RT
      if (unlikely(DDrawCommonInterface::IsWrappedSurface(reinterpret_cast<IDirectDrawSurface3*>(lpDDS)))) {
        DDraw3Surface* ddraw3Surface = reinterpret_cast<DDraw3Surface*>(lpDDS);
        // A DDrawSurface usually exists, because a DDraw3Surface is obtained from it via
        // QueryInterface, however the passed surface can be obtained by GetAttachedSurface() calls
        // on IDirectDrawSurface3, in which case it will NOT have a preexisting DDrawSurface
        rt = ddraw3Surface->GetCommonSurface()->GetDDSurface();
        if (unlikely(rt == nullptr)) {
          Com<IDirectDrawSurface> surface;
          ddraw3Surface->GetProxied()->QueryInterface(__uuidof(IDirectDrawSurface), reinterpret_cast<void**>(&surface));
          try {
            rt = new DDrawSurface(ddraw3Surface->GetCommonSurface(), std::move(surface),
                                  ddraw3Surface->GetCommonInterface()->GetDDInterface(), nullptr, false);
          } catch (const DxvkError& e) {
            Logger::err(e.message());
            return DDERR_UNSUPPORTED;
          }
          // Treat the new surface as the previously non-existent parent for our DDraw3Surface
          ddraw3Surface->UpdateParent(rt.ptr());
        }
      } else {
        Logger::err("D3D5Interface::CreateDevice: Unwrapped surface passed as RT");
        return DDERR_UNSUPPORTED;
      }
    } else {
      rt = static_cast<DDrawSurface*>(lpDDS);
    }

    HRESULT hrRT = rt->GetCommonSurface()->ValidateRTUsage(isHALDevice, true);
    if (unlikely(FAILED(hrRT)))
      return hrRT;

    DDSURFACEDESC desc;
    desc.dwSize = sizeof(DDSURFACEDESC);
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
          Logger::info("D3D5Interface::CreateDevice: Enforcing mode dimensions");
          backBufferWidth  = modeSize->width;
          BackBufferHeight = modeSize->height;
        }
      }
    }

    const d3d9::D3DFORMAT backBufferFormat = ConvertFormat(desc.ddpfPixelFormat);

    const DWORD cooperativeLevel = m_commonIntf->GetCooperativeLevel();

    if ((cooperativeLevel & DDSCL_MULTITHREADED) || d3dOptions->forceMultiThreaded) {
      Logger::info("D3D5Interface::CreateDevice: Using thread safe runtime synchronization");
      deviceCreationFlags9 |= D3DCREATE_MULTITHREADED;
    }
    // DDSCL_FPUPRESERVE does not exist prior to DDraw7,
    // and DDSCL_FPUSETUP is NOT the default state
    if (!(cooperativeLevel & DDSCL_FPUSETUP))
      deviceCreationFlags9 |= D3DCREATE_FPU_PRESERVE;
    if (cooperativeLevel & DDSCL_NOWINDOWCHANGES)
      deviceCreationFlags9 |= D3DCREATE_NOWINDOWCHANGES;

    Logger::info(str::format("D3D5Interface::CreateDevice: Back buffer size: ", desc.dwWidth, "x", desc.dwHeight));

    const DWORD backBufferCount = DetermineBackBufferCount<IDirectDrawSurface>(rt->GetProxied());
    Logger::info(str::format("D3D5Interface::CreateDevice: Back buffer count: ", backBufferCount));

    // Determine the supported AA sample count by querying the D3D9 interface
    const d3d9::D3DMULTISAMPLE_TYPE multiSampleType = d3dOptions->emulateFSAA != FSAAEmulation::Disabled ?
                                                      m_commonD3DIntf->GetMultiSampleType(backBufferFormat) :
                                                      d3d9::D3DMULTISAMPLE_NONE;

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
    params.PresentationInterval       = D3DPRESENT_INTERVAL_DEFAULT; // A D3D5 device always uses VSync

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
      Logger::err("D3D5Interface::CreateDevice: Failed to create the D3D9 device");
      return hr;
    }

    try{
      Com<D3D5Device> device5 = new D3D5Device(nullptr, this, rclsidOverride, &params,
                                               std::move(device9), rt.ptr(), deviceCreationFlags9);

      // Set the common device on the common interface
      m_commonIntf->SetCommonD3DDevice(device5->GetCommonD3DDevice());
      // Now that we have a valid D3D9 device pointer, we can initialize the depth stencil (if any)
      device5->InitializeDS();

      *lplpD3DDevice = device5.ref();
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return DDERR_GENERIC;
    }

    return D3D_OK;
  }

}