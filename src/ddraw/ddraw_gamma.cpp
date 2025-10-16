#include "ddraw_gamma.h"

#include "d3d_common_device.h"

namespace dxvk {

  DDrawGammaControl::DDrawGammaControl(
        DDrawCommonSurface* commonSurf,
        Com<IDirectDrawGammaControl>&& proxyGamma,
        IUnknown* pParent)
    : DDrawWrappedObject<IUnknown, IDirectDrawGammaControl>(pParent, std::move(proxyGamma))
    , m_commonSurf ( commonSurf ) {
    Logger::debug("DDrawGammaControl: Created a new gamma control interface");
  }

  DDrawGammaControl::~DDrawGammaControl() {
    Logger::debug("DDrawGammaControl: A gamma control interface bites the dust");
  }

  HRESULT STDMETHODCALLTYPE DDrawGammaControl::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IUnknown)
              || riid == __uuidof(IDirectDrawSurface))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface2))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface3))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface4))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirectDrawSurface7))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }
    if (unlikely(riid == __uuidof(IDirect3DTexture2))) {
      return m_parent->QueryInterface(riid, ppvObject);
    }

    if (likely(riid == __uuidof(IDirectDrawGammaControl))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("DDrawGammaControl::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE DDrawGammaControl::GetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData) {
    if (unlikely(lpRampData == nullptr))
      return DDERR_INVALIDPARAMS;

    DDrawCommonInterface* commonIntf = m_commonSurf->GetCommonInterface();

    D3DCommonDevice* commonDevice = commonIntf->GetCommonD3DDevice();

    if (likely(commonDevice != nullptr)) {
      d3d9::IDirect3DDevice9* d3d9Device = commonDevice->GetD3D9Device();

      d3d9::D3DGAMMARAMP rampData = { };
      d3d9Device->GetGammaRamp(0, &rampData);

      // Both gamma structs are identical in content/size
      memcpy(static_cast<void*>(lpRampData), static_cast<const void*>(&rampData), sizeof(DDGAMMARAMP));
    } else {
      return m_proxy->GetGammaRamp(dwFlags, lpRampData);
    }

    return DD_OK;
  }

  HRESULT STDMETHODCALLTYPE DDrawGammaControl::SetGammaRamp(DWORD dwFlags, LPDDGAMMARAMP lpRampData) {
    if (unlikely(lpRampData == nullptr))
      return DDERR_INVALIDPARAMS;

    DDrawCommonInterface* commonIntf = m_commonSurf->GetCommonInterface();

    if (likely(!commonIntf->GetOptions()->ignoreGammaRamp)) {
      D3DCommonDevice* commonDevice = commonIntf->GetCommonD3DDevice();

      if (likely(commonDevice != nullptr)) {
        d3d9::IDirect3DDevice9* d3d9Device = commonDevice->GetD3D9Device();

        d3d9Device->SetGammaRamp(0, D3DSGR_NO_CALIBRATION,
                                 reinterpret_cast<const d3d9::D3DGAMMARAMP*>(lpRampData));
      } else {
        return m_proxy->SetGammaRamp(dwFlags, lpRampData);
      }
    } else {
      Logger::info("DDrawGammaControl::SetGammaRamp: Ignoring application set gamma ramp");
    }

    return DD_OK;
  }

}
