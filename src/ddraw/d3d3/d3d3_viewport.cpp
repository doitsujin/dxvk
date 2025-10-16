#include "d3d3_viewport.h"

#include "d3d3_device.h"

#include "../ddraw_common_interface.h"
#include "../ddraw_common_surface.h"
#include "../ddraw_util.h"

#include "../d3d_light.h"
#include "../d3d_common_device.h"
#include "../d3d_common_material.h"

#include "../ddraw/ddraw_surface.h"

#include "../d3d5/d3d5_viewport.h"
#include "../d3d6/d3d6_viewport.h"

#include <vector>
#include <algorithm>

namespace dxvk {

  std::atomic<uint32_t> D3D3Viewport::s_viewportCount = 0;

  D3D3Viewport::D3D3Viewport(
        D3DCommonViewport* commonViewport,
        D3D3Interface* pParent)
    : DDrawChildObject<D3D3Interface, IDirect3DViewport>(pParent)
    , m_commonViewport ( commonViewport ) {

    if (m_commonViewport == nullptr)
      m_commonViewport = new D3DCommonViewport();

    m_commonViewport->SetD3D3Viewport(this);

    if (m_commonViewport->GetOrigin() == nullptr)
      m_commonViewport->SetOrigin(this);

    m_viewportCount = ++s_viewportCount;

    Logger::debug(str::format("D3D3Viewport: Created a new viewport nr. [[1-", m_viewportCount, "]]"));
  }

  D3D3Viewport::~D3D3Viewport() {
    std::vector<Com<D3DLight>>& lights = m_commonViewport->GetLights();

    // Dissasociate every bound light from this viewport
    for (auto light : lights) {
      light->SetViewport3(nullptr);
    }

    if (m_commonViewport->GetOrigin() == this)
      m_commonViewport->SetOrigin(nullptr);

    m_commonViewport->SetD3D3Viewport(nullptr);

    Logger::debug(str::format("D3D3Viewport: Viewport nr. [[1-", m_viewportCount, "]] bites the dust"));
  }

  // Interlocked refcount with the origin viewport
  ULONG STDMETHODCALLTYPE D3D3Viewport::AddRef() {
    IUnknown* origin = m_commonViewport->GetOrigin();
    if (unlikely(origin != nullptr && origin != this)) {
      return origin->AddRef();
    } else {
      return ComObjectClamp::AddRef();
    }
  }

  // Interlocked refcount with the origin viewport
  ULONG STDMETHODCALLTYPE D3D3Viewport::Release() {
    IUnknown* origin = m_commonViewport->GetOrigin();
    if (unlikely(origin != nullptr && origin != this)) {
      return origin->Release();
    } else {
      return ComObjectClamp::Release();
    }
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DViewport2))) {
      if (m_commonViewport->GetD3D5Viewport() != nullptr)
        return m_commonViewport->GetD3D5Viewport()->QueryInterface(riid, ppvObject);

      m_viewport5 = new D3D5Viewport(m_commonViewport.ptr(), nullptr);
      *ppvObject = m_viewport5.ref();

      return S_OK;
    }
    if (unlikely(riid == __uuidof(IDirect3DViewport3))) {
      if (m_commonViewport->GetD3D6Viewport() != nullptr)
        return m_commonViewport->GetD3D6Viewport()->QueryInterface(riid, ppvObject);

      m_viewport6 = new D3D6Viewport(m_commonViewport.ptr(), nullptr);
      *ppvObject = m_viewport6.ref();

      return S_OK;
    }

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirect3DViewport))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D3Viewport::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  // Docs state: "The IDirect3DViewport2::Initialize method is not implemented."
  HRESULT STDMETHODCALLTYPE D3D3Viewport::Initialize(LPDIRECT3D lpDirect3D) {
    return DDERR_ALREADYINITIALIZED;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::GetViewport(D3DVIEWPORT *data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonViewport->IsViewportSet()))
      return D3DERR_VIEWPORTDATANOTSET;

    d3d9::D3DVIEWPORT9* viewport9 = m_commonViewport->GetD3D9Viewport();

    data->dwX      = viewport9->X;
    data->dwY      = viewport9->Y;
    data->dwWidth  = viewport9->Width;
    data->dwHeight = viewport9->Height;
    data->dvMinZ   = viewport9->MinZ;
    data->dvMaxZ   = viewport9->MaxZ;

    D3DVECTOR* legacyScale = m_commonViewport->GetLegacyScale();
    data->dvScaleX = legacyScale->x * (float)data->dwWidth / 2.0f;
    data->dvScaleY = legacyScale->y * (float)data->dwHeight / 2.0f;
    D3DVECTOR* legacyClip = m_commonViewport->GetLegacyClip();
    // Don't compact these because precission issues can affect the outcome
    data->dvMaxX   = 2.0f / legacyScale->x * (1.0f + (legacyClip->x + 1.0f) / -2.0f); // dvClipX + dvClipWidth
    data->dvMaxY   = 2.0f / legacyScale->y * (legacyClip->y - 1.0f) / -2.0f;          // dvClipY

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::SetViewport(D3DVIEWPORT *data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(data->dwSize != sizeof(D3DVIEWPORT)))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!m_commonViewport->HasDevice()))
      return D3DERR_VIEWPORTHASNODEVICE;

    // These validations aren't performed at all on viewports bound to a D3D3 device
    if (unlikely(m_commonViewport->GetD3D3Device() == nullptr)) {
      // Use the full surface rect, since it is surface version agnostic
      const RECT* surfRect = m_commonViewport->GetCommonRenderTarget()->GetFullSurfaceRect();
      // D3D3 will fail when setting a viewport that's outside of the
      // current render target, though that works in D3D9
      HRESULT hr = ValidateViewportRT(data->dwX, data->dwY, data->dwWidth, data->dwHeight,
                                      surfRect->right, surfRect->bottom);
      if (unlikely(FAILED(hr)))
        return hr;
    }

    d3d9::D3DVIEWPORT9* viewport9 = m_commonViewport->GetD3D9Viewport();

    // The docs state: "The method ignores the values in the dvMaxX, dvMaxY,
    // dvMinZ, and dvMaxZ members.", which appears correct.
    viewport9->X      = data->dwX;
    viewport9->Y      = data->dwY;
    viewport9->Width  = data->dwWidth;
    viewport9->Height = data->dwHeight;
    viewport9->MinZ   = 0.0f;
    viewport9->MaxZ   = 1.0f;

    D3DVECTOR* legacyScale = m_commonViewport->GetLegacyScale();
    legacyScale->x = 2.0f * data->dvScaleX / (float)data->dwWidth;
    legacyScale->y = 2.0f * data->dvScaleY / (float)data->dwHeight;
    legacyScale->z = 1.0f;
    D3DVECTOR* legacyClip = m_commonViewport->GetLegacyClip();
    legacyClip->x = 0.0f;
    legacyClip->y = 0.0f;
    legacyClip->z = 0.0f;

    m_commonViewport->MarkViewportAsSet();

    if (m_commonViewport->IsCurrentViewport())
      ApplyViewport();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::TransformVertices(DWORD vertex_count, D3DTRANSFORMDATA *data, DWORD flags, DWORD *offscreen) {
    if (unlikely(!m_commonViewport->HasDevice()))
      return D3DERR_VIEWPORTHASNODEVICE;

    d3d9::IDirect3DDevice9* d3d9Device = m_commonViewport->GetCommonD3DDevice()->GetD3D9Device();

    // Temporarily activate this viewport, if not already active
    d3d9::D3DVIEWPORT9 currentViewport9;
    if (!m_commonViewport->IsCurrentViewport()) {
      D3D3Viewport* currentViewport = m_commonViewport->GetCurrentD3D3Viewport();
      if (currentViewport != nullptr) {
        currentViewport9 = *currentViewport->GetCommonViewport()->GetD3D9Viewport();
      } else {
        d3d9Device->GetViewport(&currentViewport9);
      }
      d3d9Device->SetViewport(m_commonViewport->GetD3D9Viewport());
    }

    HRESULT hr = m_commonViewport->TransformVertices(vertex_count, data, flags, offscreen);

    // Restore the previously active viewport
    if (!m_commonViewport->IsCurrentViewport()) {
      d3d9Device->SetViewport(&currentViewport9);
    }

    return hr;
  }

  // Docs state: "The IDirect3DViewport::LightElements method is not currently implemented."
  HRESULT STDMETHODCALLTYPE D3D3Viewport::LightElements(DWORD element_count, D3DLIGHTDATA *data) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::SetBackground(D3DMATERIALHANDLE hMat) {
    if (unlikely(m_commonViewport->GetMaterialHandle() == hMat))
      return D3D_OK;

    D3DCommonMaterial* commonMaterial = D3DCommonInterface::GetCommonMaterialFromHandle(hMat);

    if (unlikely(commonMaterial == nullptr))
      return DDERR_INVALIDPARAMS;

    // Cache only the set material handle, as its color can
    // change after it is set (get it on Clear directly)
    m_commonViewport->SetMaterialHandle(hMat);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::GetBackground(D3DMATERIALHANDLE *material, BOOL *valid) {
    if (unlikely(material == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    *material = m_commonViewport->GetMaterialHandle();
    *valid = m_commonViewport->IsMaterialSet();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::SetBackgroundDepth(IDirectDrawSurface *surface) {
    if (unlikely(surface != nullptr
             && !DDrawCommonInterface::IsWrappedSurface(surface))) {
      Logger::err("D3D3Viewport::SetBackgroundDepth: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    if (unlikely(surface == nullptr)) {
      m_backgroundDepth = nullptr;
      m_isBackgroundDepthSet = false;
    } else {
      m_backgroundDepth = reinterpret_cast<DDrawSurface*>(surface);
      m_isBackgroundDepthSet = true;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::GetBackgroundDepth(IDirectDrawSurface **surface, BOOL *valid) {
    if (unlikely(surface == nullptr || valid == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(surface);

    *surface = reinterpret_cast<IDirectDrawSurface*>(m_backgroundDepth.ptr());
    *valid = m_isBackgroundDepthSet;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::Clear(DWORD count, D3DRECT *rects, DWORD flags) {
    // Early D3D viewport fast skip
    if (unlikely(!count || !rects))
      return D3D_OK;

    if (unlikely(!m_commonViewport->HasDevice()))
      return D3DERR_VIEWPORTHASNODEVICE;

    const bool clearRenderTarget = flags & D3DCLEAR_TARGET;
    const bool clearDepthStencil = flags & D3DCLEAR_ZBUFFER;
    DDrawCommonSurface* rt = nullptr;
    DDrawCommonSurface* ds = nullptr;

    if (clearRenderTarget) {
      rt = m_commonViewport->GetCommonRenderTarget();
      if (likely(rt != nullptr)) {
        // If this isn't a full surface clear, we need to first upload the DDraw surface
        if (unlikely(count > 1 || !rt->IsFullSurfaceLock(reinterpret_cast<RECT*>(rects), nullptr))) {
          //Logger::debug("D3D3Viewport::Clear: Partial render target clear");
          // Use a common surface helper, because we want to handle all
          // possible surface interfaces that may be alive at this time
          rt->InitializeOrUploadD3D9();
        }
      }
    }
    if (clearDepthStencil) {
      ds = m_commonViewport->GetCommonDepthStencil();
      if (likely(ds != nullptr)) {
        // If this isn't a full surface clear, we need to first upload the DDraw surface
        if (unlikely(count > 1 || !ds->IsFullSurfaceLock(reinterpret_cast<RECT*>(rects), nullptr))) {
          //Logger::debug("D3D3Viewport::Clear: Partial depth stencil clear");
          // Use a common surface helper, because we want to handle all
          // possible surface interfaces that may be alive at this time
          ds->InitializeOrUploadD3D9();
        }
      }
    }

    d3d9::IDirect3DDevice9* d3d9Device = m_commonViewport->GetCommonD3DDevice()->GetD3D9Device();

    // Temporarily activate this viewport in order to clear it
    d3d9::D3DVIEWPORT9 currentViewport9;
    const bool isCurrentViewport = m_commonViewport->IsCurrentViewport();
    if (!isCurrentViewport) {
      D3D3Viewport* currentViewport = m_commonViewport->GetCurrentD3D3Viewport();
      if (currentViewport != nullptr) {
        currentViewport9 = *currentViewport->GetCommonViewport()->GetD3D9Viewport();
      } else {
        d3d9Device->GetViewport(&currentViewport9);
      }
      d3d9Device->SetViewport(m_commonViewport->GetD3D9Viewport());
    }

    static constexpr D3DCOLOR defaultColor = D3DCOLOR_ARGB(0, 0, 0, 0);
    D3DMATERIALHANDLE handle = m_commonViewport->GetMaterialHandle();
    D3DCommonMaterial* commonMaterial = D3DCommonInterface::GetCommonMaterialFromHandle(handle);
    D3DCOLOR clearColor = commonMaterial != nullptr ? commonMaterial->GetMaterialColor() : defaultColor;

    // TODO: Account for any set background depth surface, though in practice
    // it does not appear to matter even in the one game which uses it (Powerslide)
    HRESULT hr = d3d9Device->Clear(count, rects, flags, clearColor, 1.0f, 0u);

    // Restore the previously active viewport
    if (!isCurrentViewport) {
      d3d9Device->SetViewport(&currentViewport9);
    }

    // Can fail in D3D9 only in case of a missing depth stencil surface
    if (unlikely(FAILED(hr))) {
      // Fix up expected return codes
      return D3DERR_ZBUFFER_NOTPRESENT;
    }

    if (clearRenderTarget && rt != nullptr)
      rt->UnDirtyDDrawSurface();
    if (clearDepthStencil && ds != nullptr)
      ds->UnDirtyDDrawSurface();

    m_commonViewport->UpdateSurfaceDirtyTracking(clearRenderTarget, clearDepthStencil, false);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::AddLight(IDirect3DLight *light) {
    if (unlikely(light == nullptr))
      return DDERR_INVALIDPARAMS;

    D3DLight* d3dLight = reinterpret_cast<D3DLight*>(light);

    if (unlikely(d3dLight->HasViewport()))
      return D3DERR_LIGHTHASVIEWPORT;

    std::vector<Com<D3DLight>>& lights = m_commonViewport->GetLights();
    // No need to check if the light is already attached, since
    // if that's the case it will have a set viewport above
    lights.push_back(d3dLight);
    d3dLight->SetViewport3(this);

    if (m_commonViewport->HasDevice() && m_commonViewport->IsCurrentViewport())
      ApplyAndActivateLight(d3dLight->GetIndex(), d3dLight);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::DeleteLight(IDirect3DLight *light) {
    if (unlikely(light == nullptr))
      return DDERR_INVALIDPARAMS;

    D3DLight* d3dLight = reinterpret_cast<D3DLight*>(light);

    if (unlikely(!d3dLight->HasViewport()))
      return DDERR_INVALIDPARAMS;

    std::vector<Com<D3DLight>>& lights = m_commonViewport->GetLights();

    auto it = std::find(lights.begin(), lights.end(), d3dLight);
    if (likely(it != lights.end())) {
      const DWORD lightIndex = d3dLight->GetIndex();
      if (m_commonViewport->HasDevice() && m_commonViewport->IsCurrentViewport() && d3dLight->IsActive()) {
        //Logger::debug(str::format("D3D3Viewport: Disabling light nr. ", lightIndex));
        d3d9::IDirect3DDevice9* d3d9Device = m_commonViewport->GetCommonD3DDevice()->GetD3D9Device();
        d3d9Device->LightEnable(lightIndex, FALSE);
      }
      lights.erase(it);
      d3dLight->SetViewport3(nullptr);
    } else {
      Logger::warn("D3D3Viewport::DeleteLight: Light not found");
      return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D3Viewport::NextLight(IDirect3DLight *lpDirect3DLight, IDirect3DLight **lplpDirect3DLight, DWORD flags) {
    if (unlikely(lplpDirect3DLight == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpDirect3DLight);

    std::vector<Com<D3DLight>>& lights = m_commonViewport->GetLights();

    if (flags & D3DNEXT_HEAD) {
      if (likely(lights.size() > 0))
        *lplpDirect3DLight = lights.front().ref();
    } else if (flags & D3DNEXT_NEXT) {
      if (unlikely(lpDirect3DLight == nullptr))
        return DDERR_INVALIDPARAMS;

      if (likely(lights.size() > 0))
        Logger::warn("D3D3Viewport::NextLight: Unimplemented D3DNEXT_NEXT flag");
    } else if (flags & D3DNEXT_TAIL) {
      if (likely(lights.size() > 0))
        *lplpDirect3DLight = lights.back().ref();
    }

    return D3D_OK;
  }

  HRESULT D3D3Viewport::ApplyViewport() {
    if (!m_commonViewport->IsViewportSet())
      return D3D_OK;

    d3d9::IDirect3DDevice9* d3d9Device = m_commonViewport->GetCommonD3DDevice()->GetD3D9Device();

    HRESULT hr = d3d9Device->SetViewport(m_commonViewport->GetD3D9Viewport());
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D3Viewport: Failed to set the D3D9 viewport");
      return hr;
    }

    return D3D_OK;
  }

  HRESULT D3D3Viewport::ApplyAndActivateLights() {
    std::vector<Com<D3DLight>>& lights = m_commonViewport->GetLights();

    if (!lights.size())
      return D3D_OK;

    for (auto light: lights)
      ApplyAndActivateLight(light->GetIndex(), light.ptr());

    return D3D_OK;
  }

  HRESULT D3D3Viewport::DeactivateLights() {
    std::vector<Com<D3DLight>>& lights = m_commonViewport->GetLights();

    if (!lights.size())
      return D3D_OK;

    d3d9::IDirect3DDevice9* d3d9Device = m_commonViewport->GetCommonD3DDevice()->GetD3D9Device();

    for (auto light: lights) {
      const DWORD lightIndex = light->GetIndex();
      if (m_commonViewport->HasDevice() && m_commonViewport->IsCurrentViewport() && light->IsActive()) {
        //Logger::debug(str::format("D3D3Viewport: Disabling light nr. ", lightIndex));
        d3d9Device->LightEnable(lightIndex, FALSE);
      }
    }

    return D3D_OK;
  }

  HRESULT D3D3Viewport::ApplyAndActivateLight(DWORD index, D3DLight* light) {
    d3d9::IDirect3DDevice9* d3d9Device = m_commonViewport->GetCommonD3DDevice()->GetD3D9Device();

    HRESULT hr = d3d9Device->SetLight(index, light->GetD3D9Light());
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D3Viewport: Failed D3D9 SetLight call");
      return hr;
    }

    if (light->IsActive()) {
      //Logger::debug(str::format("D3D3Viewport: Enabling D3D9 light nr. ", index));
      hr = d3d9Device->LightEnable(index, TRUE);
      if (unlikely(FAILED(hr)))
        Logger::err("D3D3Viewport: Failed D3D9 LightEnable call (TRUE)");
    } else {
      //Logger::debug(str::format("D3D3Viewport: Disabling D3D9 light nr. ", index));
      hr = d3d9Device->LightEnable(index, FALSE);
      if (unlikely(FAILED(hr)))
        Logger::err("D3D3Viewport: Failed D3D9 LightEnable call (FALSE)");
    }

    return D3D_OK;
  }

}
