#include "d3d3_interface.h"

#include "d3d3_material.h"
#include "d3d3_viewport.h"

#include "../d3d_light.h"

#include "../d3d5/d3d5_interface.h"
#include "../d3d6/d3d6_interface.h"

#include "../ddraw/ddraw_interface.h"

namespace dxvk {

  std::atomic<uint32_t> D3D3Interface::s_intfCount = 0;

  D3D3Interface::D3D3Interface(
        D3DCommonInterface* commonD3DIntf,
        DDrawCommonInterface* commonIntf,
        IUnknown* pParent)
    : DDrawChildObject<IUnknown, IDirect3D>(pParent)
    , m_commonD3DIntf ( commonD3DIntf )
    , m_commonIntf ( commonIntf ) {
    if (m_commonD3DIntf == nullptr)
      m_commonD3DIntf = new D3DCommonInterface();

    d3d9::IDirect3D9* d3d9Intf = m_commonD3DIntf->GetD3D9Interface();

    // Get the bridge interface to D3D9
    if (unlikely(FAILED(d3d9Intf->QueryInterface(__uuidof(IDxvkLegacyD3DInterfaceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D3Interface: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    m_commonD3DIntf->SetD3D3Interface(this);

    // Don't enable D3D3 compatibility mode when coming from a higher interface
    if (likely(m_commonD3DIntf->GetD3D5Interface() == nullptr
            && m_commonD3DIntf->GetD3D6Interface() == nullptr)) {
      m_bridge->SetD3DCompatibility(D3DCompatibility::D3D3);
    }

    m_intfCount = ++s_intfCount;

    Logger::debug(str::format("D3D3Interface: Created a new interface nr. ((1-", m_intfCount, "))"));
  }

  D3D3Interface::~D3D3Interface() {
    if (m_commonD3DIntf->GetD3D3Interface() == this)
      m_commonD3DIntf->SetD3D3Interface(nullptr);

    // Needed for D3D3 device creation from an IDirectDrawSurface object
    if (m_commonIntf->GetD3D3Interface() == this)
      m_commonIntf->SetD3D3Interface(nullptr);

    Logger::debug(str::format("D3D3Interface: Interface nr. ((1-", m_intfCount, ")) bites the dust"));
  }

  // Interlocked refcount with the parent IDirectDraw
  ULONG STDMETHODCALLTYPE D3D3Interface::AddRef() {
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
  ULONG STDMETHODCALLTYPE D3D3Interface::Release() {
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

  HRESULT STDMETHODCALLTYPE D3D3Interface::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (riid == __uuidof(IDirectDraw)) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    // Deathtrap Dungeon queries for IDirect3D2...
    if (unlikely(riid == __uuidof(IDirect3D2))) {
      if (likely(m_commonD3DIntf->GetD3D5Interface() != nullptr))
        return m_commonD3DIntf->GetD3D5Interface()->QueryInterface(riid, ppvObject);

      m_d3d5Intf = new D3D5Interface(m_commonD3DIntf.ptr(), m_commonIntf, m_parent);
      *ppvObject = m_d3d5Intf.ref();
      return S_OK;
    }
    // ... and Final Fantasy VIII queries for IDirect3D3, because why not...
    if (unlikely(riid == __uuidof(IDirect3D3))) {
      if (likely(m_commonD3DIntf->GetD3D6Interface() != nullptr))
        return m_commonD3DIntf->GetD3D6Interface()->QueryInterface(riid, ppvObject);

      // We don't have a proxied object on D3D3Interface, so use the parent
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
               riid == __uuidof(IDirect3D))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D3Interface::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  // Docs state: "This method is provided for compliance with the COM protocol.
  // Returns DDERR_ALREADYINITIALIZED because the Direct3D object is initialized when it is created."
  HRESULT STDMETHODCALLTYPE D3D3Interface::Initialize(REFIID riid) {
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg) {
    if (unlikely(lpEnumDevicesCallback == nullptr))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    // D3D3 reports both HAL and HEL caps for any type of device,
    // with minor differences between the two. Note that the
    // device listing order matters, so list RAMP first, RGB second,
    // and HAL last. A RAMP device also needs to be advertised in D3D3,
    // since some games like Resident Evil expect it to be present.

    HRESULT hr;

    // RAMP device (monochrome), this is expected to be exposed
    GUID guidRAMP = IID_IDirect3DRampDevice;
    // The caps of a RAMP device are mostly identical to an RGB device
    D3DDEVICEDESC3 desc3RAMP_HAL = GetD3D3Caps(IID_IDirect3DRGBDevice, d3dOptions);
    D3DDEVICEDESC3 desc3RAMP_HEL = desc3RAMP_HAL;
    D3DDEVICEDESC descRAMP_HAL = { };
    D3DDEVICEDESC descRAMP_HEL = { };
    desc3RAMP_HAL.dwFlags = 0;
    desc3RAMP_HAL.dcmColorModel = 0;
    // RAMP devices use a monochrome color model
    desc3RAMP_HEL.dcmColorModel = D3DCOLOR_MONO;
    // Some applications apparently care about HAL texture caps
    desc3RAMP_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                             & ~D3DPTEXTURECAPS_POW2
                                             & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    desc3RAMP_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                             & ~D3DPTEXTURECAPS_POW2
                                             & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    memcpy(&descRAMP_HAL, &desc3RAMP_HAL, sizeof(D3DDEVICEDESC3));
    memcpy(&descRAMP_HEL, &desc3RAMP_HEL, sizeof(D3DDEVICEDESC3));
    if (likely(!d3dOptions->legacyDeviceNames)) {
      static char deviceDescRAMP[100] = "D3VK Ramp";
      static char deviceNameRAMP[100] = "D3VK Ramp";
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
    D3DDEVICEDESC3 desc3RGB_HAL = GetD3D3Caps(IID_IDirect3DRGBDevice, d3dOptions);
    D3DDEVICEDESC3 desc3RGB_HEL = desc3RGB_HAL;
    D3DDEVICEDESC descRGB_HAL = { };
    D3DDEVICEDESC descRGB_HEL = { };
    desc3RGB_HAL.dwFlags = 0;
    desc3RGB_HAL.dcmColorModel = 0;
    // Some applications apparently care about HAL texture caps
    desc3RGB_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                            & ~D3DPTEXTURECAPS_POW2
                                            & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    desc3RGB_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                            & ~D3DPTEXTURECAPS_POW2
                                            & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    memcpy(&descRGB_HAL, &desc3RGB_HAL, sizeof(D3DDEVICEDESC3));
    memcpy(&descRGB_HEL, &desc3RGB_HEL, sizeof(D3DDEVICEDESC3));

    if (likely(!d3dOptions->legacyDeviceNames)) {
      static char deviceDescRGB[100] = "D3VK RGB";
      static char deviceNameRGB[100] = "D3VK RGB";
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
    D3DDEVICEDESC3 desc3HAL_HAL = GetD3D3Caps(IID_IDirect3DHALDevice, d3dOptions);
    D3DDEVICEDESC3 desc3HAL_HEL = desc3HAL_HAL;
    D3DDEVICEDESC descHAL_HAL = { };
    D3DDEVICEDESC descHAL_HEL = { };
    desc3HAL_HEL.dcmColorModel = 0;
    // Some applications apparently care about HEL texture caps
    desc3HAL_HEL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                            & ~D3DPTEXTURECAPS_POW2
                                            & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    desc3HAL_HEL.dpcTriCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                           & ~D3DPTEXTURECAPS_POW2
                                           & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    desc3HAL_HEL.dwDevCaps &= ~D3DDEVCAPS_HWTRANSFORMANDLIGHT
                            & ~D3DDEVCAPS_DRAWPRIMITIVES2
                            & ~D3DDEVCAPS_DRAWPRIMITIVES2EX;
    memcpy(&descHAL_HAL, &desc3HAL_HAL, sizeof(D3DDEVICEDESC3));
    memcpy(&descHAL_HEL, &desc3HAL_HEL, sizeof(D3DDEVICEDESC3));

    if (likely(!d3dOptions->legacyDeviceNames)) {
      static char deviceDescHAL[100] = "D3VK HAL";
      static char deviceNameHAL[100] = "D3VK HAL";
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

  HRESULT STDMETHODCALLTYPE D3D3Interface::CreateLight(LPDIRECT3DLIGHT *lplpDirect3DLight, IUnknown *pUnkOuter) {
    if (unlikely(lplpDirect3DLight == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDirect3DLight);

    *lplpDirect3DLight = ref(new D3DLight(this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::CreateMaterial(LPDIRECT3DMATERIAL *lplpDirect3DMaterial, IUnknown *pUnkOuter) {
    if (unlikely(lplpDirect3DMaterial == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDirect3DMaterial);

    *lplpDirect3DMaterial = ref(new D3D3Material(this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::CreateViewport(LPDIRECT3DVIEWPORT *lplpD3DViewport, IUnknown *pUnkOuter) {
    InitReturnPtr(lplpD3DViewport);

    *lplpD3DViewport = ref(new D3D3Viewport(nullptr, this));

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Interface::FindDevice(D3DFINDDEVICESEARCH *lpD3DFDS, D3DFINDDEVICERESULT *lpD3DFDR) {
    if (unlikely(lpD3DFDS == nullptr || lpD3DFDR == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(lpD3DFDS->dwSize != sizeof(D3DFINDDEVICESEARCH)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!IsValidFindDeviceResultSize(lpD3DFDR->dwSize)))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    // Software emulation, this is expected to be exposed
    D3DDEVICEDESC3 descRGB_HAL = GetD3D3Caps(IID_IDirect3DRGBDevice, d3dOptions);
    D3DDEVICEDESC3 descRGB_HEL = descRGB_HAL;
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
    D3DDEVICEDESC3 descHAL_HAL = GetD3D3Caps(IID_IDirect3DHALDevice, d3dOptions);
    D3DDEVICEDESC3 descHAL_HEL = descHAL_HAL;
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

    D3DFINDDEVICERESULT3 lpD3DFRD3 = { };
    lpD3DFRD3.dwSize = sizeof(D3DFINDDEVICERESULT3);

    if (lpD3DFDS->dwFlags & D3DFDS_GUID) {
      if (lpD3DFDS->guid == IID_IDirect3DRGBDevice ||
          lpD3DFDS->guid == IID_IDirect3DMMXDevice ||
          lpD3DFDS->guid == IID_IDirect3DRampDevice) {
        lpD3DFRD3.guid = IID_IDirect3DRGBDevice;
        lpD3DFRD3.ddHwDesc = descRGB_HAL;
        lpD3DFRD3.ddSwDesc = descRGB_HEL;
      } else if (lpD3DFDS->guid == IID_IDirect3DHALDevice) {
        lpD3DFRD3.guid = IID_IDirect3DHALDevice;
        lpD3DFRD3.ddHwDesc = descHAL_HAL;
        lpD3DFRD3.ddSwDesc = descHAL_HEL;
      } else {
        Logger::err(str::format("D3D3Interface::FindDevice: Unknown device type: ", lpD3DFDS->guid));
        return DDERR_NOTFOUND;
      }

      memcpy(lpD3DFDR, &lpD3DFRD3, sizeof(D3DFINDDEVICERESULT3));
    } else if (lpD3DFDS->dwFlags & D3DFDS_HARDWARE) {
      if (likely(lpD3DFDS->bHardware == TRUE)) {
        lpD3DFRD3.guid = IID_IDirect3DHALDevice;
        lpD3DFRD3.ddHwDesc = descHAL_HAL;
        lpD3DFRD3.ddSwDesc = descHAL_HEL;
      } else {
        lpD3DFRD3.guid = IID_IDirect3DRGBDevice;
        lpD3DFRD3.ddHwDesc = descRGB_HAL;
        lpD3DFRD3.ddSwDesc = descRGB_HEL;
      }

      memcpy(lpD3DFDR, &lpD3DFRD3, sizeof(D3DFINDDEVICERESULT3));
    } else if (lpD3DFDS->dwFlags & D3DFDS_COLORMODEL) {
      lpD3DFRD3.guid = IID_IDirect3DHALDevice;
      lpD3DFRD3.ddHwDesc = descHAL_HAL;
      lpD3DFRD3.ddSwDesc = descHAL_HEL;

      memcpy(lpD3DFDR, &lpD3DFRD3, sizeof(D3DFINDDEVICERESULT3));
    } else if (lpD3DFDS->dwFlags == 0) {
      lpD3DFRD3.guid = IID_IDirect3DHALDevice;
      lpD3DFRD3.ddHwDesc = descHAL_HAL;
      lpD3DFRD3.ddSwDesc = descHAL_HEL;

      memcpy(lpD3DFDR, &lpD3DFRD3, sizeof(D3DFINDDEVICERESULT3));
    } else {
      Logger::err("D3D3Interface::FindDevice: Unhandled matching type");
      return DDERR_NOTFOUND;
    }

    return D3D_OK;
  }

}