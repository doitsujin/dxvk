#include "d3d7_device.h"

#include "../ddraw_common_interface.h"

#include "d3d7_buffer.h"
#include "d3d7_state_block.h"

#include "../ddraw7/ddraw7_surface.h"

namespace dxvk {

  D3D7Device::D3D7Device(
        D3DCommonDevice* commonD3DDevice,
        Com<IDirect3DDevice7>&& d3d7DeviceProxy,
        D3D7Interface* pParent,
        GUID deviceGUID,
        const d3d9::D3DPRESENT_PARAMETERS* pParams9,
        Com<d3d9::IDirect3DDevice9>&& pDevice9,
        DDraw7Surface* pSurface,
        DWORD CreationFlags9)
    : DDrawWrappedObject<D3D7Interface, IDirect3DDevice7>(pParent, std::move(d3d7DeviceProxy))
    , m_commonD3DDevice ( commonD3DDevice )
    , m_multithread ( CreationFlags9 & D3DCREATE_MULTITHREADED )
    , m_rt ( pSurface ) {
    if (m_parent != nullptr) {
      m_commonIntf = m_parent->GetCommonInterface();
    } else if (m_commonD3DDevice != nullptr) {
      m_commonIntf = m_commonD3DDevice->GetCommonInterface();
    } else {
      throw DxvkError("D3D7Device: ERROR! Failed to retrieve the common interface!");
    }

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();
    // Retrieve and cache the device capabilities
    m_desc = GetD3D7BaseCaps(d3dOptions);
    ApplyD3D7DeviceCaps(&m_desc, deviceGUID);

    d3d9::IDirect3DDevice9* device9;

    if (likely(m_commonD3DDevice == nullptr)) {
      m_commonD3DDevice = new D3DCommonDevice(m_commonIntf, deviceGUID, pParams9, CreationFlags9);

      m_commonD3DDevice->SetD3D9Device(std::move(pDevice9));
      device9 = m_commonD3DDevice->GetD3D9Device();

      if (unlikely(d3dOptions->emulateFSAA == FSAAEmulation::Forced)) {
        Logger::warn("D3D7Device: Force enabling AA");
        device9->SetRenderState(d3d9::D3DRS_MULTISAMPLEANTIALIAS, TRUE);
      }
    } else {
      device9 = m_commonD3DDevice->GetD3D9Device();
      // Very important, otherwise the depth stencil isn't dirtied on draws
      m_ds = m_rt->GetAttachedDepthStencil();
    }

    // Common D3D9 index buffers
    static constexpr DWORD Usage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;

    for (uint8_t ibIndex = 0; ibIndex < ddrawCaps::IndexBufferCount ; ibIndex++) {
      const UINT ibSize = ddrawCaps::IndexCount[ibIndex] * sizeof(WORD);

      HRESULT hr = device9->CreateIndexBuffer(ibSize, Usage, d3d9::D3DFMT_INDEX16,
                                              d3d9::D3DPOOL_DEFAULT, &m_ib9[ibIndex], nullptr);
      if (unlikely(FAILED(hr)))
        throw DxvkError("D3D7Device: ERROR! Failed to initialize D3D9 index buffers.");
    }

    // Get the bridge interface to D3D9
    if (unlikely(FAILED(device9->QueryInterface(__uuidof(IDxvkLegacyD3DDeviceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D7Device: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    if (unlikely(!m_commonD3DDevice->GetTotalTextureMemory()))
      m_commonD3DDevice->SetTotalTextureMemory(m_bridge->DetermineInitialTextureMemory());

    // Update D3D9 legacy light state
    m_bridge->SetLegacyLightsState(false);

    // Update D3D9 alternate pixel center
    m_bridge->SetAlternatePixelCenter(d3dOptions->alternatePixelCenter == AlternatePixelCenter::Enabled);

    if (m_commonD3DDevice->GetOrigin() == nullptr)
      m_commonD3DDevice->SetOrigin(this);

    m_commonD3DDevice->SetD3D7Device(this);

    m_textures.fill(nullptr);
  }

  D3D7Device::~D3D7Device() {
    if (m_commonD3DDevice->GetD3D7Device() == this)
      m_commonD3DDevice->SetD3D7Device(nullptr);

    if (m_commonD3DDevice->GetOrigin() == this)
      m_commonD3DDevice->SetOrigin(nullptr);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DDevice))) {
      return E_NOINTERFACE;
    }
    if (unlikely(riid == __uuidof(IDirect3DDevice2))) {
      return E_NOINTERFACE;
    }
    // Some games, like Conquest: Frontier Wars, query for
    // IDirect3DDevice3, although that's not supported
    if (unlikely(riid == __uuidof(IDirect3DDevice3))) {
      return E_NOINTERFACE;
    }

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirect3DDevice7))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D7Device::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetCaps(D3DDEVICEDESC7 *desc) {
    if (unlikely(desc == nullptr))
      return DDERR_INVALIDPARAMS;

    *desc = m_desc;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK cb, void *ctx) {
    if (unlikely(cb == nullptr))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    // Note: The list of formats exposed in D3D7 is restricted to the below

    DDPIXELFORMAT textureFormat = GetTextureFormat(d3d9::D3DFMT_X1R5G5B5);
    HRESULT hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_A1R5G5B5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    // D3DFMT_X4R4G4B4 is not supported by D3D7
    textureFormat = GetTextureFormat(d3d9::D3DFMT_A4R4G4B4);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_R5G6B5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_X8R8G8B8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_A8R8G8B8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    if (d3dOptions->supportR3G3B2) {
      textureFormat = GetTextureFormat(d3d9::D3DFMT_R3G3B2);
      hr = cb(&textureFormat, ctx);
      if (unlikely(hr != D3DENUMRET_OK))
        return D3D_OK;
    }

    // Not supported in D3D9, but some games may use it
    // Note: Advertizing P8 support breaks Sacrifice
    /*textureFormat = GetTextureFormat(d3d9::D3DFMT_P8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;*/

    textureFormat = GetTextureFormat(d3d9::D3DFMT_V8U8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_L6V5U5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_X8L8V8U8);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT1);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT2);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT3);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT4);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_DXT5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::BeginScene() {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(m_commonD3DDevice->IsInScene()))
      return D3DERR_SCENE_IN_SCENE;

    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->BeginScene();
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonD3DDevice->SetInScene(true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::EndScene() {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(!m_commonD3DDevice->IsInScene()))
      return D3DERR_SCENE_NOT_IN_SCENE;

    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->EndScene();
    if (unlikely(FAILED(hr)))
      return hr;

    m_commonD3DDevice->SetInScene(false);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetDirect3D(IDirect3D7 **d3d) {
    if (unlikely(d3d == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(d3d);

    if (unlikely(m_parent == nullptr)) {
      Logger::err("D3D7Device::GetDirect3D: Found no valid parent D3D interface");
      return DDERR_NOTFOUND;
    }

    *d3d = ref(m_parent);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetRenderTarget(IDirectDrawSurface7 *surface, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(surface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(surface))) {
      Logger::err("D3D7Device::SetRenderTarget: Received an unwrapped RT");
      return DDERR_UNSUPPORTED;
    }

    DDraw7Surface* rt7 = static_cast<DDraw7Surface*>(surface);

    HRESULT hr = rt7->GetCommonSurface()->ValidateRTUsage7(m_commonD3DDevice->IsHALOrTNLHALDevice(), false);
    if (unlikely(FAILED(hr)))
      return hr;

    hr = rt7->InitializeD3D9RenderTarget();
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::SetRenderTarget: Failed to initialize D3D9 RT");
      return hr;
    }

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    hr = device9->SetRenderTarget(0, rt7->GetCommonSurface()->GetD3D9Surface());
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::SetRenderTarget: Failed to set D3D9 RT");
      return hr;
    }

    m_rt = rt7;
    m_ds = m_rt->GetAttachedDepthStencil();

    if (m_ds != nullptr) {
      hr = m_ds->InitializeD3D9DepthStencil();
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7Device::SetRenderTarget: Failed to initialize/upload D3D9 DS");
        return hr;
      }

      hr = device9->SetDepthStencilSurface(m_ds->GetCommonSurface()->GetD3D9Surface());
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7Device::SetRenderTarget: Failed to set D3D9 DS");
        return hr;
      }
    } else {
      hr = device9->SetDepthStencilSurface(nullptr);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7Device::SetRenderTarget: Failed to clear the D3D9 DS");
        return hr;
      }
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetRenderTarget(IDirectDrawSurface7 **surface) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(surface == nullptr))
      return DDERR_INVALIDPARAMS;

    *surface = m_rt.ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::Clear(DWORD count, D3DRECT *rects, DWORD flags, D3DCOLOR color, D3DVALUE z, DWORD stencil) {
    D3DDeviceLock lock = LockDevice();

    // D3D7 and later fast skip
    if (unlikely(!count && rects))
      return D3D_OK;

    const bool clearRenderTarget = flags & D3DCLEAR_TARGET;
    const bool clearDepthStencil = (flags & D3DCLEAR_ZBUFFER) || (flags & D3DCLEAR_STENCIL);

    if (unlikely(clearRenderTarget && count)) {
      // If this isn't a full surface clear, we need to first upload the DDraw surface
      if (count > 1 || !m_rt->GetCommonSurface()->IsFullSurfaceLock(reinterpret_cast<RECT*>(rects), nullptr)) {
        //Logger::debug("D3D7Device::Clear: Partial render target clear");
        m_rt->InitializeOrUploadD3D9();
      }
    }
    if (unlikely(clearDepthStencil && count && m_ds != nullptr)) {
      // If this isn't a full surface clear, we need to first upload the DDraw surface
      if (count > 1 || !m_ds->GetCommonSurface()->IsFullSurfaceLock(reinterpret_cast<RECT*>(rects), nullptr)) {
        //Logger::debug("D3D7Device::Clear: Partial depth stencil clear");
        m_ds->InitializeOrUploadD3D9();
      }
    }

    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->Clear(count, rects, flags, color, z, stencil);
    // Can fail in D3D9 only in case of a missing depth stencil surface
    if (unlikely(FAILED(hr))) {
      // Fix up expected return codes
      if (flags & D3DCLEAR_ZBUFFER) {
        return D3DERR_ZBUFFER_NOTPRESENT;
      } else {
        return D3DERR_STENCILBUFFER_NOTPRESENT;
      }
    }

    if (clearRenderTarget)
      m_rt->GetCommonSurface()->UnDirtyDDrawSurface();
    if (clearDepthStencil && m_ds != nullptr)
      m_ds->GetCommonSurface()->UnDirtyDDrawSurface();

    UpdateSurfaceDirtyTracking(clearRenderTarget, clearDepthStencil, false);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    return m_commonD3DDevice->GetD3D9Device()->SetTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    return m_commonD3DDevice->GetD3D9Device()->GetTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::MultiplyTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    return m_commonD3DDevice->GetD3D9Device()->MultiplyTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetViewport(D3DVIEWPORT7 *data) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    // Use the full surface rect, since it is surface version agnostic
    const RECT* surfRect = m_rt->GetCommonSurface()->GetFullSurfaceRect();
    // D3D7 will fail when setting a viewport that's outside of the
    // current render target, though that works in D3D9
    HRESULT hr = ValidateViewportRT(data->dwX, data->dwY, data->dwWidth, data->dwHeight,
                                    surfRect->right, surfRect->bottom);
    if (unlikely(FAILED(hr)))
      return hr;

    // (The) Summoner sets both to 0.0f and expects to get
    // the behavioral equivalent of setting 0.0f/1.0f, although
    // the actual D3D7 behavior will result in 0.0f/0.001f.
    //
    // It is somewhat unclear why this works properly on native,
    // however it is possible some corrections were performed at
    // driver level or in the runtime, but without affecting
    // reported viewport dvMinZ/dvMaxZ values.
    if (unlikely(m_commonIntf->GetOptions()->viewportZCorrection)) {
      data->dvMinZ = 0.0f;
      data->dvMaxZ = 1.0f;
    }

    return m_commonD3DDevice->GetD3D9Device()->SetViewport(reinterpret_cast<d3d9::D3DVIEWPORT9*>(data));
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetViewport(D3DVIEWPORT7 *data) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    return m_commonD3DDevice->GetD3D9Device()->GetViewport(reinterpret_cast<d3d9::D3DVIEWPORT9*>(data));
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetMaterial(D3DMATERIAL7 *data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    return m_commonD3DDevice->GetD3D9Device()->SetMaterial(reinterpret_cast<d3d9::D3DMATERIAL9*>(data));
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetMaterial(D3DMATERIAL7 *data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    return m_commonD3DDevice->GetD3D9Device()->GetMaterial(reinterpret_cast<d3d9::D3DMATERIAL9*>(data));
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetLight(DWORD idx, D3DLIGHT7 *data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    // D3DLIGHT_PARALLELPOINT can not be used in D3D7
    if (unlikely(!data->dltType || data->dltType > D3DLIGHT_DIRECTIONAL))
      return DDERR_INVALIDPARAMS;

    // For POINT/SPOT lights, attenuation should be positive, as per docs:
    // "Valid values for these members range from 0.0 to infinity."
    if (unlikely((data->dltType == D3DLIGHT_POINT
               || data->dltType == D3DLIGHT_SPOT) &&
                 (data->dvAttenuation0 < 0.0f
               || data->dvAttenuation1 < 0.0f
               || data->dvAttenuation2 < 0.0f)))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DLIGHT9* light9 = reinterpret_cast<d3d9::D3DLIGHT9*>(data);

    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->SetLight(idx, light9);
    if (unlikely(FAILED(hr)))
      return DDERR_INVALIDPARAMS;

    m_lights[idx] = *light9;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetLight(DWORD idx, D3DLIGHT7 *data) {
    if (unlikely(data == nullptr))
      return DDERR_INVALIDPARAMS;

    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->GetLight(idx, reinterpret_cast<d3d9::D3DLIGHT9*>(data));
    if (unlikely(FAILED(hr)))
      return DDERR_INVALIDPARAMS;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetRenderState(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState) {
    D3DDeviceLock lock = LockDevice();

    d3d9::D3DRENDERSTATETYPE State9 = d3d9::D3DRENDERSTATETYPE(dwRenderStateType);

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      //case D3DRENDERSTATE_ANTIALIAS:
      //case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
      case D3DRENDERSTATE_ZENABLE:
      case D3DRENDERSTATE_FILLMODE:
      case D3DRENDERSTATE_SHADEMODE:
      //case D3DRENDERSTATE_LINEPATTERN:
      case D3DRENDERSTATE_ZWRITEENABLE:
      case D3DRENDERSTATE_ALPHATESTENABLE:
      case D3DRENDERSTATE_LASTPIXEL:
      case D3DRENDERSTATE_SRCBLEND:
      case D3DRENDERSTATE_DESTBLEND:
      case D3DRENDERSTATE_CULLMODE:
      case D3DRENDERSTATE_ZFUNC:
      case D3DRENDERSTATE_ALPHAREF:
      case D3DRENDERSTATE_ALPHAFUNC:
      case D3DRENDERSTATE_DITHERENABLE:
      case D3DRENDERSTATE_ALPHABLENDENABLE:
      case D3DRENDERSTATE_FOGENABLE:
      case D3DRENDERSTATE_SPECULARENABLE:
      //case D3DRENDERSTATE_ZVISIBLE:
      //case D3DRENDERSTATE_STIPPLEDALPHA:
      case D3DRENDERSTATE_FOGCOLOR:
      case D3DRENDERSTATE_FOGTABLEMODE:
      case D3DRENDERSTATE_FOGSTART:   // same as D3DRENDERSTATE_FOGTABLESTART
      case D3DRENDERSTATE_FOGEND:     // same as D3DRENDERSTATE_FOGTABLEEND
      case D3DRENDERSTATE_FOGDENSITY: // same as D3DRENDERSTATE_FOGTABLEDENSITY
      //case D3DRENDERSTATE_EDGEANTIALIAS:
      //case D3DRENDERSTATE_COLORKEYENABLE:
      //case D3DRENDERSTATE_ZBIAS:
      case D3DRENDERSTATE_RANGEFOGENABLE:
      case D3DRENDERSTATE_STENCILENABLE:
      case D3DRENDERSTATE_STENCILFAIL:
      case D3DRENDERSTATE_STENCILZFAIL:
      case D3DRENDERSTATE_STENCILPASS:
      case D3DRENDERSTATE_STENCILFUNC:
      case D3DRENDERSTATE_STENCILREF:
      case D3DRENDERSTATE_STENCILMASK:
      case D3DRENDERSTATE_STENCILWRITEMASK:
      case D3DRENDERSTATE_TEXTUREFACTOR:
      case D3DRENDERSTATE_WRAP0:
      case D3DRENDERSTATE_WRAP1:
      case D3DRENDERSTATE_WRAP2:
      case D3DRENDERSTATE_WRAP3:
      case D3DRENDERSTATE_WRAP4:
      case D3DRENDERSTATE_WRAP5:
      case D3DRENDERSTATE_WRAP6:
      case D3DRENDERSTATE_WRAP7:
      case D3DRENDERSTATE_CLIPPING:
      case D3DRENDERSTATE_LIGHTING:
      //case D3DRENDERSTATE_EXTENTS:
      case D3DRENDERSTATE_AMBIENT:
      case D3DRENDERSTATE_FOGVERTEXMODE:
      case D3DRENDERSTATE_COLORVERTEX:
      case D3DRENDERSTATE_LOCALVIEWER:
      case D3DRENDERSTATE_NORMALIZENORMALS:
      //case D3DRENDERSTATE_COLORKEYBLENDENABLE:
      case D3DRENDERSTATE_DIFFUSEMATERIALSOURCE:
      case D3DRENDERSTATE_SPECULARMATERIALSOURCE:
      case D3DRENDERSTATE_AMBIENTMATERIALSOURCE:
      case D3DRENDERSTATE_EMISSIVEMATERIALSOURCE:
      case D3DRENDERSTATE_VERTEXBLEND:
      case D3DRENDERSTATE_CLIPPLANEENABLE:
        break;

      case D3DRENDERSTATE_ANTIALIAS: {
        const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

        if (likely(d3dOptions->emulateFSAA == FSAAEmulation::Disabled)) {
          if (unlikely(dwRenderState == D3DANTIALIAS_SORTDEPENDENT
                    || dwRenderState == D3DANTIALIAS_SORTINDEPENDENT))
            Logger::warn("D3D7Device::SetRenderState: Device does not expose FSAA emulation");
          return D3D_OK;
        }

        State9        = d3d9::D3DRS_MULTISAMPLEANTIALIAS;
        m_commonD3DDevice->SetAntialias(dwRenderState);
        dwRenderState = dwRenderState == D3DANTIALIAS_SORTDEPENDENT
                     || dwRenderState == D3DANTIALIAS_SORTINDEPENDENT
                     || d3dOptions->emulateFSAA == FSAAEmulation::Forced ? TRUE : FALSE;
        break;
      }

      // Always enabled on later APIs, so it can't really be turned off
      // Even the D3D7 docs state: "Note that many 3-D adapters apply
      // texture perspective correction unconditionally."
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        return D3D_OK;

      // TODO: Implement D3DRS_LINEPATTERN - vkCmdSetLineRasterizationModeEXT
      // and advertise support with D3DPRASTERCAPS_PAT once that is done
      case D3DRENDERSTATE_LINEPATTERN:
        static bool s_linePatternErrorShown;

        if (!std::exchange(s_linePatternErrorShown, true))
          Logger::warn("D3D7Device::SetRenderState: Unimplemented render state D3DRS_LINEPATTERN");

        m_commonD3DDevice->SetLinePattern(bit::cast<D3DLINEPATTERN>(dwRenderState));
        return D3D_OK;

      // Not supported by D3D7
      case D3DRENDERSTATE_ZVISIBLE:
        return D3D_OK;

      // Tests have shown age accurate GPUs didn't offer support for stippling at all
      case D3DRENDERSTATE_STIPPLEDALPHA:
        return D3D_OK;

      case D3DRENDERSTATE_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      case D3DRENDERSTATE_COLORKEYENABLE: {
        m_commonD3DDevice->SetColorKeyEnable(dwRenderState);

        const bool validColorKey = m_textures[0] != nullptr ? m_textures[0]->GetCommonSurface()->HasValidColorKey() : false;
        m_bridge->SetColorKeyState(dwRenderState && validColorKey);
        if (dwRenderState && validColorKey) {
          DDCOLORKEY normalizedColorKey = m_textures[0]->GetCommonSurface()->GetColorKeyNormalized();
          m_bridge->SetColorKey(normalizedColorKey.dwColorSpaceLowValue,
                                normalizedColorKey.dwColorSpaceHighValue);
        }

        return D3D_OK;
      }

      case D3DRENDERSTATE_ZBIAS:
        State9         = d3d9::D3DRS_DEPTHBIAS;
        dwRenderState  = bit::cast<DWORD>(static_cast<float>(dwRenderState) * ddrawCaps::ZBIAS_SCALE);
        break;

      // TODO:
      case D3DRENDERSTATE_EXTENTS:
        static bool s_extentsErrorShown;

        if (dwRenderState && !std::exchange(s_extentsErrorShown, true))
          Logger::warn("D3D7Device::SetRenderState: Unimplemented render state D3DRENDERSTATE_EXTENTS");

        return D3D_OK;

      // D3DPTEXTURECAPS_COLORKEYBLEND isn't advertised by any D3D7 capable
      // or later GPUs, so this render state serves no practical purpose
      case D3DRENDERSTATE_COLORKEYBLENDENABLE:
        m_commonD3DDevice->SetColorKeyBlendEnable(dwRenderState);
        return D3D_OK;

      // As opposed to D3D8/9, D3D7 actually validates and
      // errors out in case of unknown/invalid render states
      default:
        return DDERR_INVALIDPARAMS;
    }

    // This call will never fail
    return m_commonD3DDevice->GetD3D9Device()->SetRenderState(State9, dwRenderState);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetRenderState(D3DRENDERSTATETYPE dwRenderStateType, LPDWORD lpdwRenderState) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(lpdwRenderState == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DRENDERSTATETYPE State9 = d3d9::D3DRENDERSTATETYPE(dwRenderStateType);

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      //case D3DRENDERSTATE_ANTIALIAS:
      //case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
      case D3DRENDERSTATE_ZENABLE:
      case D3DRENDERSTATE_FILLMODE:
      case D3DRENDERSTATE_SHADEMODE:
      //case D3DRENDERSTATE_LINEPATTERN:
      case D3DRENDERSTATE_ZWRITEENABLE:
      case D3DRENDERSTATE_ALPHATESTENABLE:
      case D3DRENDERSTATE_LASTPIXEL:
      case D3DRENDERSTATE_SRCBLEND:
      case D3DRENDERSTATE_DESTBLEND:
      case D3DRENDERSTATE_CULLMODE:
      case D3DRENDERSTATE_ZFUNC:
      case D3DRENDERSTATE_ALPHAREF:
      case D3DRENDERSTATE_ALPHAFUNC:
      case D3DRENDERSTATE_DITHERENABLE:
      case D3DRENDERSTATE_ALPHABLENDENABLE:
      case D3DRENDERSTATE_FOGENABLE:
      case D3DRENDERSTATE_SPECULARENABLE:
      //case D3DRENDERSTATE_ZVISIBLE:
      //case D3DRENDERSTATE_STIPPLEDALPHA:
      case D3DRENDERSTATE_FOGCOLOR:
      case D3DRENDERSTATE_FOGTABLEMODE:
      case D3DRENDERSTATE_FOGSTART:   // same as D3DRENDERSTATE_FOGTABLESTART
      case D3DRENDERSTATE_FOGEND:     // same as D3DRENDERSTATE_FOGTABLEEND
      case D3DRENDERSTATE_FOGDENSITY: // same as D3DRENDERSTATE_FOGTABLEDENSITY
      //case D3DRENDERSTATE_EDGEANTIALIAS:
      //case D3DRENDERSTATE_COLORKEYENABLE:
      //case D3DRENDERSTATE_ZBIAS:
      case D3DRENDERSTATE_RANGEFOGENABLE:
      case D3DRENDERSTATE_STENCILENABLE:
      case D3DRENDERSTATE_STENCILFAIL:
      case D3DRENDERSTATE_STENCILZFAIL:
      case D3DRENDERSTATE_STENCILPASS:
      case D3DRENDERSTATE_STENCILFUNC:
      case D3DRENDERSTATE_STENCILREF:
      case D3DRENDERSTATE_STENCILMASK:
      case D3DRENDERSTATE_STENCILWRITEMASK:
      case D3DRENDERSTATE_TEXTUREFACTOR:
      case D3DRENDERSTATE_WRAP0:
      case D3DRENDERSTATE_WRAP1:
      case D3DRENDERSTATE_WRAP2:
      case D3DRENDERSTATE_WRAP3:
      case D3DRENDERSTATE_WRAP4:
      case D3DRENDERSTATE_WRAP5:
      case D3DRENDERSTATE_WRAP6:
      case D3DRENDERSTATE_WRAP7:
      case D3DRENDERSTATE_CLIPPING:
      case D3DRENDERSTATE_LIGHTING:
      //case D3DRENDERSTATE_EXTENTS:
      case D3DRENDERSTATE_AMBIENT:
      case D3DRENDERSTATE_FOGVERTEXMODE:
      case D3DRENDERSTATE_COLORVERTEX:
      case D3DRENDERSTATE_LOCALVIEWER:
      case D3DRENDERSTATE_NORMALIZENORMALS:
      //case D3DRENDERSTATE_COLORKEYBLENDENABLE:
      case D3DRENDERSTATE_DIFFUSEMATERIALSOURCE:
      case D3DRENDERSTATE_SPECULARMATERIALSOURCE:
      case D3DRENDERSTATE_AMBIENTMATERIALSOURCE:
      case D3DRENDERSTATE_EMISSIVEMATERIALSOURCE:
      case D3DRENDERSTATE_VERTEXBLEND:
      case D3DRENDERSTATE_CLIPPLANEENABLE:
        break;

      case D3DRENDERSTATE_ANTIALIAS:
        *lpdwRenderState = m_commonD3DDevice->GetAntialias();
        return D3D_OK;

      // Always enabled on later APIs, so it can't really be turned off
      // Even the D3D7 docs state: "Note that many 3-D adapters apply
      // texture perspective correction unconditionally."
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        *lpdwRenderState = TRUE;
        return D3D_OK;

      case D3DRENDERSTATE_LINEPATTERN:
        *lpdwRenderState = bit::cast<DWORD>(m_commonD3DDevice->GetLinePattern());
        return D3D_OK;

      // Not supported by D3D7
      case D3DRENDERSTATE_ZVISIBLE:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_STIPPLEDALPHA:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      case D3DRENDERSTATE_COLORKEYENABLE:
        *lpdwRenderState = m_commonD3DDevice->GetColorKeyEnable();
        return D3D_OK;

      case D3DRENDERSTATE_ZBIAS: {
        DWORD bias = 0;
        m_commonD3DDevice->GetD3D9Device()->GetRenderState(d3d9::D3DRS_DEPTHBIAS, &bias);
        *lpdwRenderState = static_cast<DWORD>(bit::cast<float>(bias) * ddrawCaps::ZBIAS_SCALE_INV);
        return D3D_OK;
      }

      case D3DRENDERSTATE_EXTENTS:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_COLORKEYBLENDENABLE:
        *lpdwRenderState = m_commonD3DDevice->GetColorKeyBlendEnable();
        return D3D_OK;

      // As opposed to D3D8/9, D3D7 actually validates and
      // errors out in case of unknown/invalid render states
      default:
        if (likely(!m_commonIntf->GetOptions()->apitraceMode))
          return DDERR_INVALIDPARAMS;
        break;
    }

    // This call will never fail
    return m_commonD3DDevice->GetD3D9Device()->GetRenderState(State9, lpdwRenderState);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::BeginStateBlock() {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(m_recorder != nullptr))
      return D3DERR_INBEGINSTATEBLOCK;

    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->BeginStateBlock();
    if (unlikely(FAILED(hr)))
      return hr;

    m_handle++;
    auto stateBlockIterPair = m_stateBlocks.emplace(std::piecewise_construct,
                                                    std::forward_as_tuple(m_handle),
                                                    std::forward_as_tuple(this));
    m_recorder = &stateBlockIterPair.first->second;
    m_recorderHandle = m_handle;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::EndStateBlock(LPDWORD lpdwBlockHandle) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(lpdwBlockHandle == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(m_recorder == nullptr))
      return D3DERR_NOTINBEGINSTATEBLOCK;

    Com<d3d9::IDirect3DStateBlock9> pStateBlock;
    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->EndStateBlock(&pStateBlock);
    if (unlikely(FAILED(hr)))
      return hr;

    m_recorder->SetD3D9(std::move(pStateBlock));

    *lpdwBlockHandle = m_recorderHandle;

    m_recorder = nullptr;
    m_recorderHandle = 0;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::ApplyStateBlock(DWORD dwBlockHandle) {
    D3DDeviceLock lock = LockDevice();

    // Applications cannot apply a state block while another is being recorded
    if (unlikely(ShouldRecord()))
      return D3DERR_INBEGINSTATEBLOCK;

    auto stateBlockIter = m_stateBlocks.find(dwBlockHandle);

    if (unlikely(stateBlockIter == m_stateBlocks.end())) {
      Logger::err(str::format("D3D7Device::ApplyStateBlock: Invalid dwBlockHandle: ", std::hex, dwBlockHandle));
      return D3DERR_INVALIDSTATEBLOCK;
    }

    return stateBlockIter->second.Apply();
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::CaptureStateBlock(DWORD dwBlockHandle) {
    D3DDeviceLock lock = LockDevice();

    // Applications cannot capture a state block while another is being recorded
    if (unlikely(ShouldRecord()))
      return D3DERR_INBEGINSTATEBLOCK;

    auto stateBlockIter = m_stateBlocks.find(dwBlockHandle);

    if (unlikely(stateBlockIter == m_stateBlocks.end())) {
      Logger::err(str::format("D3D7Device::CaptureStateBlock: Invalid dwBlockHandle: ", std::hex, dwBlockHandle));
      return D3DERR_INVALIDSTATEBLOCK;
    }

    return stateBlockIter->second.Capture();
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DeleteStateBlock(DWORD dwBlockHandle) {
    D3DDeviceLock lock = LockDevice();

    // Applications cannot delete a state block while another is being recorded
    if (unlikely(ShouldRecord()))
      return D3DERR_INBEGINSTATEBLOCK;

    auto stateBlockIter = m_stateBlocks.find(dwBlockHandle);

    if (unlikely(stateBlockIter == m_stateBlocks.end())) {
      Logger::err(str::format("D3D7Device::DeleteStateBlock: Invalid dwBlockHandle: ", std::hex, dwBlockHandle));
      return D3DERR_INVALIDSTATEBLOCK;
    }

    m_stateBlocks.erase(stateBlockIter);

    // Native apparently does drop the handle counter in
    // situations where the handle being removed is the
    // last allocated handle, which allows some reuse
    if (m_handle == dwBlockHandle)
      m_handle--;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::CreateStateBlock(D3DSTATEBLOCKTYPE d3dsbType, LPDWORD lpdwBlockHandle) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(lpdwBlockHandle == nullptr))
      return DDERR_INVALIDPARAMS;

    // Applications cannot create a state block while another is being recorded
    if (unlikely(ShouldRecord()))
      return D3DERR_INBEGINSTATEBLOCK;

    D3D7StateBlockType stateBlockType = ConvertStateBlockType(d3dsbType);

    if (unlikely(stateBlockType == D3D7StateBlockType::Unknown)) {
      Logger::warn(str::format("D3D7Device::CreateStateBlock: Invalid state block type: ", d3dsbType));
      return DDERR_INVALIDPARAMS;
    }

    Com<d3d9::IDirect3DStateBlock9> pStateBlock9;
    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->CreateStateBlock(d3d9::D3DSTATEBLOCKTYPE(d3dsbType), &pStateBlock9);
    if (unlikely(FAILED(hr)))
      return hr;

    m_handle++;
    m_stateBlocks.emplace(std::piecewise_construct,
                          std::forward_as_tuple(m_handle),
                          std::forward_as_tuple(this, stateBlockType, pStateBlock9.ptr()));
    *lpdwBlockHandle = m_handle;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::PreLoad(IDirectDrawSurface7 *surface) {
    if (unlikely(surface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(surface))) {
      Logger::err("D3D7Device::PreLoad: Received an unwrapped surface");
      return DDERR_UNSUPPORTED;
    }

    DDraw7Surface* surface7 = static_cast<DDraw7Surface*>(surface);

    if (unlikely(!surface7->GetCommonSurface()->IsManaged()))
      return DDERR_INVALIDPARAMS;

    // Make sure the texture or surface is initialized and updated
    HRESULT hr = surface7->InitializeOrUploadD3D9();
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::PreLoad: Failed to initialize/upload D3D9 surface");
      return hr;
    }

    // Does not return an HRESULT
    surface7->GetCommonSurface()->GetD3D9Surface()->PreLoad();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawPrimitive(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, LPVOID lpvVertices, DWORD dwVertexCount, DWORD dwFlags) {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(!dwVertexCount))
      return D3D_OK;

    if (unlikely(lpvVertices == nullptr))
      return DDERR_INVALIDPARAMS;

    DDrawDirtySurfaceUpload();

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    device9->SetFVF(dwVertexTypeDesc);
    HRESULT hr = device9->DrawPrimitiveUP(
                     d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                     GetPrimitiveCount(d3dptPrimitiveType, dwVertexCount),
                     lpvVertices,
                     GetFVFSize(dwVertexTypeDesc));

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::DrawPrimitive: Failed D3D9 call to DrawPrimitiveUP");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawIndexedPrimitive(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, LPVOID lpvVertices, DWORD dwVertexCount, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags) {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(!dwVertexCount || !dwIndexCount))
      return D3D_OK;

    if (unlikely(lpvVertices == nullptr || lpwIndices == nullptr))
      return DDERR_INVALIDPARAMS;

    DDrawDirtySurfaceUpload();

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    device9->SetFVF(dwVertexTypeDesc);
    HRESULT hr = device9->DrawIndexedPrimitiveUP(
                      d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                      0,
                      dwVertexCount,
                      GetPrimitiveCount(d3dptPrimitiveType, dwIndexCount),
                      lpwIndices,
                      d3d9::D3DFMT_INDEX16,
                      lpvVertices,
                      GetFVFSize(dwVertexTypeDesc));

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::DrawIndexedPrimitive: Failed D3D9 call to DrawIndexedPrimitiveUP");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetClipStatus(D3DCLIPSTATUS *clip_status) {
    if (unlikely(clip_status == nullptr))
      return DDERR_INVALIDPARAMS;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetClipStatus(D3DCLIPSTATUS *clip_status) {
    if (unlikely(clip_status == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::D3DVIEWPORT9 viewport9;
    if (SUCCEEDED(m_commonD3DDevice->GetD3D9Device()->GetViewport(&viewport9))) {
      clip_status->dwFlags = D3DCLIPSTATUS_EXTENTS2;
      clip_status->dwStatus = 0;
      clip_status->minx = viewport9.X;
      clip_status->maxx = viewport9.X + viewport9.Width;
      clip_status->miny = viewport9.Y;
      clip_status->maxy = viewport9.Y + viewport9.Height;
      clip_status->minz = 0;
      clip_status->maxz = 0;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawPrimitiveStrided(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwVertexCount, DWORD dwFlags) {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(!dwVertexCount))
      return D3D_OK;

    if (unlikely(lpVertexArray == nullptr))
      return DDERR_INVALIDPARAMS;

    DDrawDirtySurfaceUpload();

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    // Transform strided vertex data to a standard vertex buffer stream
    PackedVertexBuffer pvb = TransformStridedtoUP(dwVertexTypeDesc, lpVertexArray, dwVertexCount);

    device9->SetFVF(dwVertexTypeDesc);
    HRESULT hr = device9->DrawPrimitiveUP(
                     d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                     GetPrimitiveCount(d3dptPrimitiveType, dwVertexCount),
                     pvb.vertexData.data(),
                     pvb.stride);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::DrawPrimitiveStrided: Failed D3D9 call to DrawPrimitiveUP");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawIndexedPrimitiveStrided(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwVertexCount, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags) {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(!dwVertexCount || !dwIndexCount))
      return D3D_OK;

    if (unlikely(lpVertexArray == nullptr || lpwIndices == nullptr))
      return DDERR_INVALIDPARAMS;

    DDrawDirtySurfaceUpload();

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    // Transform strided vertex data to a standard vertex buffer stream
    PackedVertexBuffer pvb = TransformStridedtoUP(dwVertexTypeDesc, lpVertexArray, dwVertexCount);

    device9->SetFVF(dwVertexTypeDesc);
    HRESULT hr = device9->DrawIndexedPrimitiveUP(
                      d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                      0,
                      dwVertexCount,
                      GetPrimitiveCount(d3dptPrimitiveType, dwIndexCount),
                      lpwIndices,
                      d3d9::D3DFMT_INDEX16,
                      pvb.vertexData.data(),
                      pvb.stride);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::DrawIndexedPrimitiveStrided: Failed D3D9 call to DrawIndexedPrimitiveUP");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawPrimitiveVB(D3DPRIMITIVETYPE d3dptPrimitiveType, LPDIRECT3DVERTEXBUFFER7 lpd3dVertexBuffer, DWORD dwStartVertex, DWORD dwNumVertices, DWORD dwFlags) {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(!dwNumVertices))
      return D3D_OK;

    if (unlikely(lpd3dVertexBuffer == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<D3D7VertexBuffer> vb7 = static_cast<D3D7VertexBuffer*>(lpd3dVertexBuffer);

    if (unlikely(vb7->GetDevice() != this)) {
      Logger::err("D3D7Device::DrawPrimitiveVB: Invalid vertex buffer parent device");
      return DDERR_GENERIC;
    }

    if (unlikely(vb7->IsLocked())) {
      Logger::err("D3D7Device::DrawPrimitiveVB: Buffer is locked");
      return D3DERR_VERTEXBUFFERLOCKED;
    }

    DDrawDirtySurfaceUpload();

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    device9->SetFVF(vb7->GetFVF());
    device9->SetStreamSource(0, vb7->GetD3D9VertexBuffer(), 0, vb7->GetStride());
    HRESULT hr = device9->DrawPrimitive(
                      d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                      dwStartVertex,
                      GetPrimitiveCount(d3dptPrimitiveType, dwNumVertices));

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::DrawPrimitiveVB: Failed D3D9 call to DrawPrimitive");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE d3dptPrimitiveType, LPDIRECT3DVERTEXBUFFER7 lpd3dVertexBuffer, DWORD dwStartVertex, DWORD dwNumVertices, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags) {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(!dwNumVertices || !dwIndexCount))
      return D3D_OK;

    if (unlikely(lpd3dVertexBuffer == nullptr || lpwIndices == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<D3D7VertexBuffer> vb7 = static_cast<D3D7VertexBuffer*>(lpd3dVertexBuffer);

    if (unlikely(vb7->GetDevice() != this)) {
      Logger::err("D3D7Device::DrawIndexedPrimitiveVB: Invalid vertex buffer parent device");
      return DDERR_GENERIC;
    }

    if (unlikely(vb7->IsLocked())) {
      Logger::err("D3D7Device::DrawIndexedPrimitiveVB: Buffer is locked");
      return D3DERR_VERTEXBUFFERLOCKED;
    }

    if (unlikely(dwIndexCount > ddrawCaps::MaxIndexCount)) {
      Logger::err("D3D7Device::DrawIndexedPrimitiveVB: Exceeded size of largest index buffer");
      return DDERR_UNSUPPORTED;
    }

    DDrawDirtySurfaceUpload();

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    uint8_t ibIndex = 0;
    // Fit index buffer uploads into the smallest buffer size possible
    while (dwIndexCount > ddrawCaps::IndexCount[ibIndex])
      ibIndex++;

    d3d9::IDirect3DIndexBuffer9* ib9 = m_ib9[ibIndex].ptr();

    const size_t ibSize = dwIndexCount * sizeof(WORD);
    void* pData = nullptr;

    // Locking and unlocking are generally expected to work here
    ib9->Lock(0, ibSize, &pData, D3DLOCK_DISCARD);
    memcpy(pData, static_cast<void*>(lpwIndices), ibSize);
    ib9->Unlock();

    device9->SetIndices(ib9);
    device9->SetFVF(vb7->GetFVF());
    device9->SetStreamSource(0, vb7->GetD3D9VertexBuffer(), 0, vb7->GetStride());
    HRESULT hr = device9->DrawIndexedPrimitive(
                      d3d9::D3DPRIMITIVETYPE(d3dptPrimitiveType),
                      dwStartVertex,
                      0,
                      dwNumVertices,
                      0,
                      GetPrimitiveCount(d3dptPrimitiveType, dwIndexCount));

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::DrawIndexedPrimitiveVB: Failed D3D9 call to DrawIndexedPrimitive");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::ComputeSphereVisibility(D3DVECTOR *lpCenters, D3DVALUE *lpRadii, DWORD dwNumSpheres, DWORD dwFlags, DWORD *lpdwReturnValues) {
    if (unlikely(lpCenters == nullptr || lpRadii == nullptr || lpdwReturnValues == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(dwNumSpheres == 0))
      return D3D_OK;

    // Docs state: "The array need not be initialized, but it must be large enough to contain a DWORD for
    // each sphere being tested. When the method returns, each element in the array contains a combination
    // of flags that describe the visibility of that sphere within the current viewport for this device.
    // If a sphere is completely visible, the corresponding entry in lpdwReturnValues is 0."
    // Consider everything to be visible as a minimal implementation, which makes Space Empires V happy.
    for (DWORD i = 0; i < dwNumSpheres; i++)
      lpdwReturnValues[i] = 0;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetTexture(DWORD stage, IDirectDrawSurface7 **surface) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(surface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(stage >= ddrawCaps::TextureStageCount))
      return DDERR_INVALIDPARAMS;

    *surface = m_textures[stage].ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetTexture(DWORD stage, IDirectDrawSurface7 *surface) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(stage >= ddrawCaps::TextureStageCount))
      return DDERR_INVALIDPARAMS;

    if (unlikely(ShouldRecord()))
      return m_recorder->SetTexture(stage, surface);

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    HRESULT hr;

    // Unbinding texture stages
    if (surface == nullptr) {
      hr = device9->SetTexture(stage, nullptr);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D7Device::SetTexture: Failed to unbind D3D9 texture");
        return hr;
      }

      if (likely(m_textures[stage] != nullptr)) {
        m_textures[stage] = nullptr;

        if (likely(stage == 0))
          m_bridge->SetColorKeyState(false);
      }

      return D3D_OK;
    }

    // Binding texture stages
    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(surface))) {
      Logger::err("D3D7Device::SetTexture: Received an unwrapped texture");
      return DDERR_UNSUPPORTED;
    }

    DDraw7Surface* surface7 = static_cast<DDraw7Surface*>(surface);

    DDrawCommonSurface* commonSurface = surface7->GetCommonSurface();

    // If textures have been used on a different device, they
    // will get their D3D9 object reinitialized at this point
    if (unlikely(commonSurface->GetCommonD3DDevice() != m_commonD3DDevice.ptr()))
      commonSurface->DirtyDDrawSurface();

    hr = surface7->InitializeOrUploadD3D9();
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::SetTexture: Failed to initialize/upload D3D9 texture");
      return hr;
    }

    // Don't fast skip, since color key might change
    //if (unlikely(m_textures[stage] == surface7))
      //return D3D_OK;

    d3d9::IDirect3DTexture9*     tex9  = commonSurface->GetD3D9Texture();
    d3d9::IDirect3DCubeTexture9* cube9 = commonSurface->GetD3D9CubeTexture();

    if (likely(tex9 != nullptr)) {
      hr = device9->SetTexture(stage, tex9);
      if (unlikely(FAILED(hr))) {
        Logger::warn("D3D7Device::SetTexture: Failed to bind D3D9 texture");
        return hr;
      }

      if (likely(stage == 0)) {
        const bool colorKeyEnable = m_commonD3DDevice->GetColorKeyEnable();
        const bool validColorKey = commonSurface->HasValidColorKey();
        m_bridge->SetColorKeyState(colorKeyEnable && validColorKey);
        if (colorKeyEnable && validColorKey) {
          DDCOLORKEY normalizedColorKey = commonSurface->GetColorKeyNormalized();
          m_bridge->SetColorKey(normalizedColorKey.dwColorSpaceLowValue,
                                normalizedColorKey.dwColorSpaceHighValue);
        }
      }
    } else if (likely(cube9 != nullptr)) {
      hr = device9->SetTexture(stage, cube9);
      if (unlikely(FAILED(hr))) {
        Logger::warn("D3D7Device::SetTexture: Failed to bind D3D9 cube texture");
        return hr;
      }

      if (likely(stage == 0)) {
        const bool colorKeyEnable = m_commonD3DDevice->GetColorKeyEnable();
        const bool validColorKey = commonSurface->HasValidColorKey();
        if (unlikely(colorKeyEnable && validColorKey))
          Logger::warn("D3D7Device::SetTexture: Unsupported use of cube texture color key");
      }
    } else {
      Logger::err("D3D7Device::SetTexture: Found no valid D3D9 texture");
    }

    m_textures[stage] = surface7;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, LPDWORD lpdwState) {
    if (unlikely(lpdwState == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    // In the case of D3DTSS_ADDRESS, which is exclusive to D3D7
    // and D3D6, simply return based on D3DTSS_ADDRESSU
    if (d3dTexStageStateType == D3DTSS_ADDRESS) {
      return device9->GetSamplerState(dwStage, d3d9::D3DSAMP_ADDRESSU, lpdwState);
    }

    d3d9::D3DSAMPLERSTATETYPE stateType = ConvertSamplerStateType(d3dTexStageStateType);

    // If the type has been remapped to a sampler state type
    if (stateType != -1u) {
      // MAG/MIN/MIP filter enums are each different than the unified D3D9 D3DTEXTUREFILTERTYPE
      if (stateType == d3d9::D3DSAMP_MAGFILTER || stateType == d3d9::D3DSAMP_MINFILTER || stateType == d3d9::D3DSAMP_MIPFILTER) {
        DWORD dwStateProxy9;

        HRESULT hr = device9->GetSamplerState(dwStage, stateType, &dwStateProxy9);
        if (unlikely(FAILED(hr)))
          return hr;

        *lpdwState = DecodeD3D9TexFilterValues(d3dTexStageStateType, dwStateProxy9);

        return D3D_OK;
      } else {
        return device9->GetSamplerState(dwStage, stateType, lpdwState);
      }
    } else {
      return device9->GetTextureStageState(dwStage, d3d9::D3DTEXTURESTAGESTATETYPE(d3dTexStageStateType), lpdwState);
    }
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, DWORD dwState) {
    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    // In the case of D3DTSS_ADDRESS, which is exclusive to D3D7
    // and D3D6, we need to set up both D3DTSS_ADDRESSU and D3DTSS_ADDRESSV
    if (d3dTexStageStateType == D3DTSS_ADDRESS) {
      device9->SetSamplerState(dwStage, d3d9::D3DSAMP_ADDRESSU, dwState);
      return device9->SetSamplerState(dwStage, d3d9::D3DSAMP_ADDRESSV, dwState);
    }

    d3d9::D3DSAMPLERSTATETYPE stateType = ConvertSamplerStateType(d3dTexStageStateType);

    // If the type has been remapped to a sampler state type
    if (stateType != -1u) {
      // MAG/MIN/MIP filter enums are each different than the unified D3D9 D3DTEXTUREFILTERTYPE
      if (stateType == d3d9::D3DSAMP_MAGFILTER || stateType == d3d9::D3DSAMP_MINFILTER || stateType == d3d9::D3DSAMP_MIPFILTER) {
        const DWORD dwState9 = DecodeD3D7TexFilterValues(d3dTexStageStateType, dwState);
        return device9->SetSamplerState(dwStage, stateType, dwState9);
      } else {
        return device9->SetSamplerState(dwStage, stateType, dwState);
      }
    } else {
      return device9->SetTextureStageState(dwStage, d3d9::D3DTEXTURESTAGESTATETYPE(d3dTexStageStateType), dwState);
    }
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::ValidateDevice(LPDWORD lpdwPasses) {
    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->ValidateDevice(lpdwPasses);
    if (unlikely(FAILED(hr)))
      return DDERR_INVALIDPARAMS;

    return D3D_OK;
  }

  // This is a precursor of our ol' D3D8 pal CopyRects
  HRESULT STDMETHODCALLTYPE D3D7Device::Load(IDirectDrawSurface7 *dst_surface, POINT *dst_point, IDirectDrawSurface7 *src_surface, RECT *src_rect, DWORD flags) {
    if (dst_surface == nullptr || src_surface == nullptr)
      return DDERR_INVALIDPARAMS;

    DDraw7Surface* ddraw7SurfaceSrc = nullptr;
    DDraw7Surface* ddraw7SurfaceDst = nullptr;

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(src_surface))) {
      Logger::err("D3D7Device::Load: Unwrapped surface source");
      return DDERR_UNSUPPORTED;
    }

    const RECT* sourceFullSurfaceRect = nullptr;
    ddraw7SurfaceSrc = static_cast<DDraw7Surface*>(src_surface);
    ddraw7SurfaceSrc->DownloadSurfaceData();
    sourceFullSurfaceRect = ddraw7SurfaceSrc->GetCommonSurface()->GetFullSurfaceRect();

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(dst_surface))) {
      Logger::err("D3D7Device::Load: Unwrapped surface destination");
      return DDERR_UNSUPPORTED;
    }

    ddraw7SurfaceDst = static_cast<DDraw7Surface*>(dst_surface);
    if ((dst_point == nullptr || (dst_point->x == 0 && dst_point->y == 0)) &&
        ddraw7SurfaceDst->GetCommonSurface()->IsFullSurfaceLock(src_rect, sourceFullSurfaceRect)) {
      ddraw7SurfaceDst->GetCommonSurface()->UnDirtyD3D9Surface();
    } else {
      ddraw7SurfaceDst->DownloadSurfaceData();
    }

    HRESULT hr = m_proxy->Load(ddraw7SurfaceDst->GetProxied(), dst_point,
                               ddraw7SurfaceSrc->GetProxied(), src_rect, flags);
    if (unlikely(FAILED(hr)))
      return hr;

    DDrawCommonSurface* dstCommonSurf = ddraw7SurfaceDst->GetCommonSurface();
    hr = dstCommonSurf->RefreshSurfaceDescripton(true);
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::Load: Failed to refresh surface description");
      return hr;
    }

    dstCommonSurf->DirtyDDrawSurface();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::LightEnable(DWORD dwLightIndex, BOOL bEnable) {
    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->LightEnable(dwLightIndex, bEnable);
    if (unlikely(FAILED(hr)))
      return DDERR_INVALIDPARAMS;

    static constexpr d3d9::D3DLIGHT9 DefaultLight = {
      d3d9::D3DLIGHT_DIRECTIONAL, // Type
      {1.0f, 1.0f, 1.0f, 0.0f},   // Diffuse
      {0.0f, 0.0f, 0.0f, 0.0f},   // Specular
      {0.0f, 0.0f, 0.0f, 0.0f},   // Ambient
      {0.0f, 0.0f, 0.0f},         // Position
      {0.0f, 0.0f, 1.0f},         // Direction
      0.0f,                       // Range
      0.0f,                       // Falloff
      0.0f, 0.0f, 0.0f,           // Attenuations [constant, linear, quadratic]
      0.0f,                       // Theta
      0.0f                        // Phi
    };

    m_lightsStates[dwLightIndex] = bEnable;
    // Store a default light if the light cache doesn't contain one
    m_lights.try_emplace(dwLightIndex, DefaultLight);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetLightEnable(DWORD dwLightIndex, BOOL *pbEnable) {
    if (unlikely(pbEnable == nullptr))
      return DDERR_INVALIDPARAMS;

    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->GetLightEnable(dwLightIndex, pbEnable);
    if (unlikely(FAILED(hr)))
      return DDERR_INVALIDPARAMS;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::SetClipPlane(DWORD dwIndex, D3DVALUE *pPlaneEquation) {
    return m_commonD3DDevice->GetD3D9Device()->SetClipPlane(dwIndex, pPlaneEquation);
  }

  HRESULT STDMETHODCALLTYPE D3D7Device::GetClipPlane(DWORD dwIndex, D3DVALUE *pPlaneEquation) {
    return m_commonD3DDevice->GetD3D9Device()->GetClipPlane(dwIndex, pPlaneEquation);
  }

  // Docs state: "This method returns S_FALSE on retail builds of DirectX."
  HRESULT STDMETHODCALLTYPE D3D7Device::GetInfo(DWORD info_id, void *info, DWORD info_size) {
    return S_FALSE;
  }

  void D3D7Device::InitializeDS() {
    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    m_rt->InitializeD3D9RenderTarget();

    m_ds = m_rt->GetAttachedDepthStencil();

    if (m_ds != nullptr) {
      HRESULT hrDS = m_ds->InitializeD3D9DepthStencil();
      if (unlikely(FAILED(hrDS))) {
        Logger::err("D3D7Device::InitializeDS: Failed to initialize D3D9 DS");
      } else {
        const RECT* dsRect = m_ds->GetCommonSurface()->GetFullSurfaceRect();
        Logger::info(str::format("D3D7Device::InitializeDS: Depth stencil: ", dsRect->right, "x", dsRect->bottom));

        HRESULT hrDS9 = device9->SetDepthStencilSurface(m_ds->GetCommonSurface()->GetD3D9Surface());
        if (unlikely(FAILED(hrDS9))) {
          Logger::err("D3D7Device::InitializeDS: Failed to set D3D9 depth stencil");
        } else {
          // This needs to act like an auto depth stencil of sorts, so manually enable z-buffering
          device9->SetRenderState(d3d9::D3DRS_ZENABLE, d3d9::D3DZB_TRUE);
        }
      }
    } else {
      device9->SetDepthStencilSurface(nullptr);
      // Should be superfluous, but play it safe
      device9->SetRenderState(d3d9::D3DRS_ZENABLE, d3d9::D3DZB_FALSE);
    }
  }

  void D3D7Device::UpdateSurfaceDirtyTracking(bool dirtyRenderTarget, bool dirtyDepthStencil, bool dirtyPrimarySurface) {
    if (likely(dirtyRenderTarget))
      m_rt->GetCommonSurface()->DirtyD3D9Surface();

    if (likely(dirtyPrimarySurface)) {
      DDrawCommonSurface* primarySurface = m_commonIntf->GetPrimarySurface();
      // The primary surface can be bound as RT, in which case it will
      // get dirtied twice, but we have no guarantees that will happen
      if (likely(primarySurface != nullptr))
        primarySurface->DirtyD3D9Surface();
    }

    if (likely(dirtyDepthStencil && m_ds != nullptr))
      m_ds->GetCommonSurface()->DirtyD3D9Surface();
  }

  HRESULT D3D7Device::ResetD3D9Swapchain(d3d9::D3DPRESENT_PARAMETERS* params) {
    Logger::info("D3D7Device::ResetD3D9Swapchain: Resetting the D3D9 swapchain");

    HRESULT hr = m_bridge->ResetSwapChain(params);
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D7Device::ResetD3D9Swapchain: Failed to reset the D3D9 swapchain");
      return hr;
    }

    DDrawCommonSurface* commonSurface = m_rt->GetCommonSurface();
    commonSurface->ResetD3D9Objects();
    // Ensure the DDraw surface content gets re-uploaded if needed
    commonSurface->DirtyDDrawSurface();

    // Reset the D3D9 objects for all the following surfaces in the swapchain
    DDraw7Surface* nextFlippable = m_rt->GetNextFlippable();

    while (nextFlippable != nullptr) {
      commonSurface = nextFlippable->GetCommonSurface();
      commonSurface->ResetD3D9Objects();
      // Ensure the DDraw surface content gets re-uploaded if needed
      commonSurface->DirtyDDrawSurface();

      nextFlippable = nextFlippable->GetNextFlippable();
    }

    // Reset the D3D9 objects for all the previous surfaces in the swapchain
    DDraw7Surface* parentSurf = m_rt->GetParentSurface();

    while (parentSurf != nullptr) {
      commonSurface = parentSurf->GetCommonSurface();
      commonSurface->ResetD3D9Objects();
      // Ensure the DDraw surface content gets re-uploaded if needed
      commonSurface->DirtyDDrawSurface();

      parentSurf = parentSurf->GetParentSurface();
    }

    // Note that the D3D9 depth stencil survives a swapchain reset,
    // so there's no need to worry about it in this case

    return D3D_OK;
  }

  inline void D3D7Device::DDrawDirtySurfaceUpload() {
    // Render target
    m_rt->InitializeOrUploadD3D9();
    // Depth stencil (if present)
    if (likely(m_ds != nullptr))
      m_ds->InitializeOrUploadD3D9();
    // Bound texture(s)
    for (auto& tex : m_textures) {
      if (tex.ptr() != nullptr)
        tex->InitializeOrUploadD3D9();
    }
  }

}