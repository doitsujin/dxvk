#include "d3d6_device.h"

#include "../d3d_common_material.h"
#include "../ddraw_common_interface.h"

#include "d3d6_buffer.h"

#include "../d3d5/d3d5_device.h"
#include "../d3d3/d3d3_device.h"

#include "../ddraw/ddraw_surface.h"
#include "../ddraw4/ddraw4_surface.h"

#include <algorithm>

namespace dxvk {

  D3D6Device::D3D6Device(
        D3DCommonDevice* commonD3DDevice,
        D3D6Interface* pParent,
        GUID deviceGUID,
        const d3d9::D3DPRESENT_PARAMETERS* pParams9,
        Com<d3d9::IDirect3DDevice9>&& pDevice9,
        DDraw4Surface* pSurface,
        DWORD CreationFlags9)
    : DDrawChildObject<D3D6Interface, IDirect3DDevice3>(pParent)
    , m_commonD3DDevice ( commonD3DDevice )
    , m_multithread ( CreationFlags9 & D3DCREATE_MULTITHREADED )
    , m_rt ( pSurface ) {
    if (m_parent != nullptr) {
      m_commonIntf = m_parent->GetCommonInterface();
    } else if (m_commonD3DDevice != nullptr) {
      m_commonIntf = m_commonD3DDevice->GetCommonInterface();
    } else {
      throw DxvkError("D3D6Device: ERROR! Failed to retrieve the common interface!");
    }

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();
    // Retrieve and cache the device capabilities
    m_desc = GetD3D6Caps(deviceGUID, d3dOptions);

    d3d9::IDirect3DDevice9* device9;

    if (likely(m_commonD3DDevice == nullptr)) {
      m_commonD3DDevice = new D3DCommonDevice(m_commonIntf, deviceGUID, pParams9, CreationFlags9);

      m_commonD3DDevice->SetD3D9Device(std::move(pDevice9));
      device9 = m_commonD3DDevice->GetD3D9Device();

      if (unlikely(d3dOptions->emulateFSAA == FSAAEmulation::Forced)) {
        Logger::warn("D3D6Device: Force enabling AA");
        device9->SetRenderState(d3d9::D3DRS_MULTISAMPLEANTIALIAS, TRUE);
      }
    } else {
      device9 = m_commonD3DDevice->GetD3D9Device();
      // Very important, otherwise the depth stencil isn't dirtied on draws
      m_ds = m_rt->GetAttachedDepthStencil();
    }

    // Common D3D9 index buffers
    if (unlikely(FAILED(InitializeIndexBuffers()))) {
      throw DxvkError("D3D6Device: ERROR! Failed to initialize D3D9 index buffers.");
    }

    // Get the bridge interface to D3D9
    if (unlikely(FAILED(device9->QueryInterface(__uuidof(IDxvkLegacyD3DDeviceBridge), reinterpret_cast<void**>(&m_bridge))))) {
      throw DxvkError("D3D6Device: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    if (unlikely(!m_commonD3DDevice->GetTotalTextureMemory()))
      m_commonD3DDevice->SetTotalTextureMemory(m_bridge->DetermineInitialTextureMemory());

    // Update D3D9 legacy light state
    m_bridge->SetLegacyLightsState(true);

    // Update D3D9 alternate pixel center
    m_bridge->SetAlternatePixelCenter(d3dOptions->alternatePixelCenter == AlternatePixelCenter::Enabled);

    if (m_commonD3DDevice->GetOrigin() == nullptr)
      m_commonD3DDevice->SetOrigin(this);

    m_commonD3DDevice->SetD3D6Device(this);

    m_textures.fill(nullptr);
  }

  D3D6Device::~D3D6Device() {
    // Dissasociate every bound viewport from this device
    for (auto viewport : m_viewports) {
      viewport->GetCommonViewport()->SetD3D6Device(nullptr);
    }

    if (m_commonD3DDevice->GetD3D6Device() == this)
      m_commonD3DDevice->SetD3D6Device(nullptr);

    if (m_commonD3DDevice->GetOrigin() == this)
      m_commonD3DDevice->SetOrigin(nullptr);
  }

  // Interlocked refcount with the origin device
  ULONG STDMETHODCALLTYPE D3D6Device::AddRef() {
    IUnknown* origin = m_commonD3DDevice->GetOrigin();
    if (unlikely(origin != nullptr && origin != this)) {
      return origin->AddRef();
    } else {
      return ComObjectClamp::AddRef();
    }
  }

  // Interlocked refcount with the origin device
  ULONG STDMETHODCALLTYPE D3D6Device::Release() {
    IUnknown* origin = m_commonD3DDevice->GetOrigin();
    if (unlikely(origin != nullptr && origin != this)) {
      return origin->Release();
    } else {
      return ComObjectClamp::Release();
    }
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::QueryInterface(REFIID riid, void** ppvObject) {
    if (unlikely(ppvObject == nullptr))
      return E_POINTER;

    InitReturnPtr(ppvObject);

    if (unlikely(riid == __uuidof(IDirect3DDevice))) {
      if (m_commonD3DDevice->GetD3D3Device() != nullptr)
        return m_commonD3DDevice->GetD3D3Device()->QueryInterface(riid, ppvObject);

      Com<DDrawSurface> rt = m_rt->GetCommonSurface()->GetDDSurface();
      // Manually retrieve a DDrawSurface object if it doesn't otherwise exist
      if (unlikely(rt == nullptr)) {
        Com<IDirectDrawSurface> ppvProxyObject;
        m_rt->QueryInterface(__uuidof(IDirectDrawSurface), reinterpret_cast<void**>(&ppvProxyObject));
        rt = reinterpret_cast<DDrawSurface*>(ppvProxyObject.ptr());
      }

      // Reuse the existing D3D9 device in situations where games want
      // to get access only to D3D3 execute buffers on a D3D6 device
      m_device3 = new D3D3Device(m_commonD3DDevice.ptr(), rt.ptr(), m_commonD3DDevice->GetDeviceGUID(),
                                 m_commonD3DDevice->GetPresentParameters(),
                                 nullptr, m_commonD3DDevice->GetD3D9CreationFlags());
      m_commonD3DDevice->SetD3D3Device(m_device3.ptr());
      *ppvObject = m_device3.ref();

      return S_OK;
    }
    // Some games, such as Hype: The Time Quest, apparently query this
    // as well, albeit without actually using the returned object...
    if (unlikely(riid == __uuidof(IDirect3DDevice2))) {
      if (m_commonD3DDevice->GetD3D5Device() != nullptr)
        return m_commonD3DDevice->GetD3D5Device()->QueryInterface(riid, ppvObject);

      Com<DDrawSurface> rt = m_rt->GetCommonSurface()->GetDDSurface();
      // Manually retrieve a DDrawSurface object if it doesn't otherwise exist
      if (unlikely(rt == nullptr)) {
        Com<IDirectDrawSurface> ppvProxyObject;
        m_rt->QueryInterface(__uuidof(IDirectDrawSurface), reinterpret_cast<void**>(&ppvProxyObject));
        rt = reinterpret_cast<DDrawSurface*>(ppvProxyObject.ptr());
      }

      m_device5 = new D3D5Device(m_commonD3DDevice.ptr(), m_commonD3DDevice->GetCommonD3DInterface()->GetD3D5Interface(),
                                 m_commonD3DDevice->GetDeviceGUID(), m_commonD3DDevice->GetPresentParameters(),
                                 nullptr, rt.ptr(), m_commonD3DDevice->GetD3D9CreationFlags());
      m_commonD3DDevice->SetD3D5Device(m_device5.ptr());
      *ppvObject = m_device5.ref();

      return S_OK;
    }

    if (likely(riid == __uuidof(IUnknown) ||
               riid == __uuidof(IDirect3DDevice3))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D6Device::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetCaps(D3DDEVICEDESC *hal_desc, D3DDEVICEDESC *hel_desc) {
    if (unlikely(hal_desc == nullptr || hel_desc == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!IsValidD3DDeviceDescSize(hal_desc->dwSize)
              || !IsValidD3DDeviceDescSize(hel_desc->dwSize)))
      return DDERR_INVALIDPARAMS;

    D3DDEVICEDESC desc_HAL = m_desc;
    D3DDEVICEDESC desc_HEL = m_desc;

    const GUID deviceGUID = m_commonD3DDevice->GetDeviceGUID();

    if (deviceGUID == IID_IDirect3DRGBDevice) {
      desc_HAL.dwFlags = 0;
      desc_HAL.dcmColorModel = 0;
      // Some applications apparently care about HAL texture caps
      desc_HAL.dpcLineCaps.dwTextureCaps &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                          & ~D3DPTEXTURECAPS_POW2
                                          & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
      desc_HAL.dpcTriCaps.dwTextureCaps  &= ~D3DPTEXTURECAPS_PERSPECTIVE
                                          & ~D3DPTEXTURECAPS_POW2
                                          & ~D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    } else if (deviceGUID == IID_IDirect3DHALDevice) {
      desc_HEL.dcmColorModel = 0;
      desc_HEL.dwDevCaps &= ~D3DDEVCAPS_HWTRANSFORMANDLIGHT
                          & ~D3DDEVCAPS_DRAWPRIMITIVES2
                          & ~D3DDEVCAPS_DRAWPRIMITIVES2EX;
    } else {
      Logger::warn("D3D6Device::GetCaps: Unhandled device type");
    }

    memcpy(hal_desc, &desc_HAL, hal_desc->dwSize);
    memcpy(hel_desc, &desc_HEL, hel_desc->dwSize);

    return D3D_OK;
  }

  // Docs state: "The IDirect3DDevice3::GetStats method is obsolete,
  // and not implemented in the IDirect3DDevice3 interface."
  HRESULT STDMETHODCALLTYPE D3D6Device::GetStats(D3DSTATS *stats) {
    return DDERR_UNSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::AddViewport(IDirect3DViewport3 *viewport) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    AddViewportInternal(viewport);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::DeleteViewport(IDirect3DViewport3 *viewport) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    DeleteViewportInternal(viewport);

    // Clear the current viewport if it is deleted from the device
    D3D6Viewport* d3d6Viewport = static_cast<D3D6Viewport*>(viewport);
    if (m_currentViewport.ptr() == d3d6Viewport)
      m_currentViewport = nullptr;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::NextViewport(IDirect3DViewport3 *lpDirect3DViewport, IDirect3DViewport3 **lplpAnotherViewport, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(lplpAnotherViewport == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(lplpAnotherViewport);

    if (flags & D3DNEXT_HEAD) {
      if (likely(m_viewports.size() > 0))
        *lplpAnotherViewport = m_viewports.front().ref();
    } else if (flags & D3DNEXT_NEXT) {
      if (unlikely(lpDirect3DViewport == nullptr))
        return DDERR_INVALIDPARAMS;

      if (likely(m_viewports.size() > 0))
        Logger::warn("D3D6Device::NextViewport: Unimplemented D3DNEXT_NEXT flag");
    } else if (flags & D3DNEXT_TAIL) {
      if (likely(m_viewports.size() > 0))
        *lplpAnotherViewport = m_viewports.back().ref();
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK cb, void *ctx) {
    if (unlikely(cb == nullptr))
      return DDERR_INVALIDPARAMS;

    const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

    // Note: The list of formats exposed in D3D6 is restricted to the below

    DDPIXELFORMAT textureFormat = GetTextureFormat(d3d9::D3DFMT_X1R5G5B5);
    HRESULT hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    textureFormat = GetTextureFormat(d3d9::D3DFMT_A1R5G5B5);
    hr = cb(&textureFormat, ctx);
    if (unlikely(hr != D3DENUMRET_OK))
      return D3D_OK;

    // D3DFMT_X4R4G4B4 is not supported by D3D6
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

  HRESULT STDMETHODCALLTYPE D3D6Device::BeginScene() {
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

  HRESULT STDMETHODCALLTYPE D3D6Device::EndScene() {
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

  HRESULT STDMETHODCALLTYPE D3D6Device::GetDirect3D(IDirect3D3 **d3d) {
    if (unlikely(d3d == nullptr))
      return DDERR_INVALIDPARAMS;

    InitReturnPtr(d3d);

    if (unlikely(m_parent == nullptr)) {
      Logger::err("D3D6Device::GetDirect3D: Found no valid parent D3D interface");
      return DDERR_NOTFOUND;
    }

    *d3d = ref(m_parent);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::SetCurrentViewport(IDirect3DViewport3 *viewport) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(viewport == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<D3D6Viewport> d3d6Viewport = static_cast<D3D6Viewport*>(viewport);

    if (unlikely(m_currentViewport == d3d6Viewport))
      return D3D_OK;

    // Validate that the viewport is attached to this (common) device
    if (unlikely(m_commonD3DDevice != d3d6Viewport->GetCommonViewport()->GetCommonD3DDevice()))
      return DDERR_INVALIDPARAMS;

    if (likely(m_currentViewport != nullptr)) {
      m_currentViewport->DeactivateLights();
      m_currentViewport->GetCommonViewport()->SetIsCurrentViewport(false);
    }

    m_currentViewport = d3d6Viewport.ptr();

    m_currentViewport->GetCommonViewport()->SetIsCurrentViewport(true);
    m_currentViewport->ApplyViewport();
    m_currentViewport->ApplyAndActivateLights();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetCurrentViewport(IDirect3DViewport3 **viewport) {
    D3DDeviceLock lock = LockDevice();

    // This does indeed return D3DERR_NOCURRENTVIEWPORT...
    if (unlikely(viewport == nullptr))
      return D3DERR_NOCURRENTVIEWPORT;

    // Current viewport is checked before initializing the return pointer
    if (unlikely(m_currentViewport == nullptr))
      return D3DERR_NOCURRENTVIEWPORT;

    InitReturnPtr(viewport);

    *viewport = m_currentViewport.ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::SetRenderTarget(IDirectDrawSurface4 *surface, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(surface == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(!DDrawCommonInterface::IsWrappedSurface(surface))) {
      Logger::err("D3D6Device::SetRenderTarget: Received an unwrapped RT");
      return DDERR_UNSUPPORTED;
    }

    DDraw4Surface* rt6 = static_cast<DDraw4Surface*>(surface);

    HRESULT hr = rt6->GetCommonSurface()->ValidateRTUsage(m_commonD3DDevice->IsHALOrTNLHALDevice(), false);
    if (unlikely(FAILED(hr)))
      return hr;

    hr = rt6->InitializeD3D9RenderTarget();
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::SetRenderTarget: Failed to initialize D3D9 RT");
      return hr;
    }

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    hr = device9->SetRenderTarget(0, rt6->GetCommonSurface()->GetD3D9Surface());
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::SetRenderTarget: Failed to set D3D9 RT");
      return hr;
    }

    m_rt = rt6;
    m_ds = m_rt->GetAttachedDepthStencil();

    if (m_ds != nullptr) {
      hr = m_ds->InitializeD3D9DepthStencil();
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D6Device::SetRenderTarget: Failed to initialize/upload D3D9 DS");
        return hr;
      }

      hr = device9->SetDepthStencilSurface(m_ds->GetCommonSurface()->GetD3D9Surface());
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D6Device::SetRenderTarget: Failed to set D3D9 DS");
        return hr;
      }
    } else {
      hr = device9->SetDepthStencilSurface(nullptr);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D6Device::SetRenderTarget: Failed to clear the D3D9 DS");
        return hr;
      }
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetRenderTarget(IDirectDrawSurface4 **surface) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(surface == nullptr))
      return DDERR_INVALIDPARAMS;

    *surface = m_rt.ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::Begin(D3DPRIMITIVETYPE d3dptPrimitiveType, DWORD dwVertexTypeDesc, DWORD dwFlags) {
    D3DDeviceLock lock = LockDevice();

    // All FVF combinations are technically supported,
    // but I doubt that is the case in practice
    if (dwVertexTypeDesc != D3DFVF_VERTEX &&
        dwVertexTypeDesc != D3DFVF_LVERTEX &&
        dwVertexTypeDesc != D3DFVF_TLVERTEX) {
      Logger::warn("D3D6Device::Begin: Unsupported FVF format");
      return DDERR_INVALIDPARAMS;
    }

    m_vertexStreamInfo.d3dpt = d3dptPrimitiveType;
    m_vertexStreamInfo.d3dvt = ConvertFVFType(dwVertexTypeDesc);
    m_vertexStreamInfo.dwFlags = dwFlags;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::BeginIndexed(D3DPRIMITIVETYPE primitive_type, DWORD fvf, void *vertices, DWORD vertex_count, DWORD flags) {
    Logger::warn("!!! D3D6Device::BeginIndexed: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::Vertex(void *vertex) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(vertex == nullptr))
      return DDERR_INVALIDPARAMS;

    switch (m_vertexStreamInfo.d3dvt) {
      case D3DVT_VERTEX:
        m_vertexStream.push_back(*reinterpret_cast<D3DVERTEX*>(vertex));
        break;
      case D3DVT_LVERTEX:
        m_lvertexStream.push_back(*reinterpret_cast<D3DLVERTEX*>(vertex));
        break;
      case D3DVT_TLVERTEX:
        m_tlvertexStream.push_back(*reinterpret_cast<D3DTLVERTEX*>(vertex));
        break;
      default:
        Logger::warn(">>> D3D6Device::Vertex: Invalid vertex type");
        return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::Index(WORD wVertexIndex) {
    Logger::warn("!!! D3D6Device::Index: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::End(DWORD dwFlags) {
    D3DDeviceLock lock = LockDevice();

    HRESULT hr;

    switch (m_vertexStreamInfo.d3dvt) {
      case D3DVT_VERTEX:
        hr = DrawPrimitive(m_vertexStreamInfo.d3dpt, m_vertexStreamInfo.d3dvt, m_vertexStream.data(),
                           m_vertexStream.size(), m_vertexStreamInfo.dwFlags);
        m_vertexStream.clear();
        break;
      case D3DVT_LVERTEX:
        hr = DrawPrimitive(m_vertexStreamInfo.d3dpt, m_vertexStreamInfo.d3dvt, m_lvertexStream.data(),
                           m_lvertexStream.size(), m_vertexStreamInfo.dwFlags);
        m_lvertexStream.clear();
        break;
      case D3DVT_TLVERTEX:
        hr = DrawPrimitive(m_vertexStreamInfo.d3dpt, m_vertexStreamInfo.d3dvt, m_tlvertexStream.data(),
                           m_tlvertexStream.size(), m_vertexStreamInfo.dwFlags);
        m_tlvertexStream.clear();
        break;
      default:
        Logger::warn(">>> D3D6Device::End: Invalid vertex type");
        return DDERR_INVALIDPARAMS;
    }

    if (unlikely(FAILED(hr)))
      Logger::err(">>> D3D6Device::End: Failed call to DrawPrimitive");

    m_vertexStreamInfo = { };

    return hr;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetRenderState(D3DRENDERSTATETYPE dwRenderStateType, LPDWORD lpdwRenderState) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(lpdwRenderState == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();
    d3d9::D3DRENDERSTATETYPE State9 = d3d9::D3DRENDERSTATETYPE(dwRenderStateType);

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      //case D3DRENDERSTATE_TEXTUREHANDLE:
      //case D3DRENDERSTATE_ANTIALIAS:
      //case D3DRENDERSTATE_TEXTUREADDRESS:
      //case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
      //case D3DRENDERSTATE_WRAPU:
      //case D3DRENDERSTATE_WRAPV:
      case D3DRENDERSTATE_ZENABLE:
      case D3DRENDERSTATE_FILLMODE:
      case D3DRENDERSTATE_SHADEMODE:
      //case D3DRENDERSTATE_LINEPATTERN:
      //case D3DRENDERSTATE_MONOENABLE:
      //case D3DRENDERSTATE_ROP2:
      //case D3DRENDERSTATE_PLANEMASK:
      case D3DRENDERSTATE_ZWRITEENABLE:
      case D3DRENDERSTATE_ALPHATESTENABLE:
      case D3DRENDERSTATE_LASTPIXEL:
      //case D3DRENDERSTATE_TEXTUREMAG:
      //case D3DRENDERSTATE_TEXTUREMIN:
      case D3DRENDERSTATE_SRCBLEND:
      case D3DRENDERSTATE_DESTBLEND:
      //case D3DRENDERSTATE_TEXTUREMAPBLEND:
      case D3DRENDERSTATE_CULLMODE:
      case D3DRENDERSTATE_ZFUNC:
      case D3DRENDERSTATE_ALPHAREF:
      case D3DRENDERSTATE_ALPHAFUNC:
      case D3DRENDERSTATE_DITHERENABLE:
      case D3DRENDERSTATE_ALPHABLENDENABLE:
      case D3DRENDERSTATE_FOGENABLE:
      case D3DRENDERSTATE_SPECULARENABLE:
      //case D3DRENDERSTATE_ZVISIBLE:
      //case D3DRENDERSTATE_SUBPIXEL:
      //case D3DRENDERSTATE_SUBPIXELX:
      //case D3DRENDERSTATE_STIPPLEDALPHA:
      case D3DRENDERSTATE_FOGCOLOR:
      case D3DRENDERSTATE_FOGTABLEMODE:
      case D3DRENDERSTATE_FOGTABLESTART:
      case D3DRENDERSTATE_FOGTABLEEND:
      case D3DRENDERSTATE_FOGTABLEDENSITY:
      //case D3DRENDERSTATE_STIPPLEENABLE:
      //case D3DRENDERSTATE_EDGEANTIALIAS:
      //case D3DRENDERSTATE_COLORKEYENABLE:
      //case D3DRENDERSTATE_BORDERCOLOR:
      //case D3DRENDERSTATE_TEXTUREADDRESSU:
      //case D3DRENDERSTATE_TEXTUREADDRESSV:
      //case D3DRENDERSTATE_MIPMAPLODBIAS:
      //case D3DRENDERSTATE_ZBIAS:
      case D3DRENDERSTATE_RANGEFOGENABLE:
      //case D3DRENDERSTATE_ANISOTROPY:
      //case D3DRENDERSTATE_FLUSHBATCH:
      //case D3DRENDERSTATE_TRANSLUCENTSORTINDEPENDENT:
      case D3DRENDERSTATE_STENCILENABLE:
      case D3DRENDERSTATE_STENCILFAIL:
      case D3DRENDERSTATE_STENCILZFAIL:
      case D3DRENDERSTATE_STENCILPASS:
      case D3DRENDERSTATE_STENCILFUNC:
      case D3DRENDERSTATE_STENCILREF:
      case D3DRENDERSTATE_STENCILMASK:
      case D3DRENDERSTATE_STENCILWRITEMASK:
      case D3DRENDERSTATE_TEXTUREFACTOR:
      //case D3DRENDERSTATE_STIPPLEPATTERN00:
      //case D3DRENDERSTATE_STIPPLEPATTERN01:
      //case D3DRENDERSTATE_STIPPLEPATTERN02:
      //case D3DRENDERSTATE_STIPPLEPATTERN03:
      //case D3DRENDERSTATE_STIPPLEPATTERN04:
      //case D3DRENDERSTATE_STIPPLEPATTERN05:
      //case D3DRENDERSTATE_STIPPLEPATTERN06:
      //case D3DRENDERSTATE_STIPPLEPATTERN07:
      //case D3DRENDERSTATE_STIPPLEPATTERN08:
      //case D3DRENDERSTATE_STIPPLEPATTERN09:
      //case D3DRENDERSTATE_STIPPLEPATTERN10:
      //case D3DRENDERSTATE_STIPPLEPATTERN11:
      //case D3DRENDERSTATE_STIPPLEPATTERN12:
      //case D3DRENDERSTATE_STIPPLEPATTERN13:
      //case D3DRENDERSTATE_STIPPLEPATTERN14:
      //case D3DRENDERSTATE_STIPPLEPATTERN15:
      //case D3DRENDERSTATE_STIPPLEPATTERN16:
      //case D3DRENDERSTATE_STIPPLEPATTERN17:
      //case D3DRENDERSTATE_STIPPLEPATTERN18:
      //case D3DRENDERSTATE_STIPPLEPATTERN19:
      //case D3DRENDERSTATE_STIPPLEPATTERN20:
      //case D3DRENDERSTATE_STIPPLEPATTERN21:
      //case D3DRENDERSTATE_STIPPLEPATTERN22:
      //case D3DRENDERSTATE_STIPPLEPATTERN23:
      //case D3DRENDERSTATE_STIPPLEPATTERN24:
      //case D3DRENDERSTATE_STIPPLEPATTERN25:
      //case D3DRENDERSTATE_STIPPLEPATTERN26:
      //case D3DRENDERSTATE_STIPPLEPATTERN27:
      //case D3DRENDERSTATE_STIPPLEPATTERN28:
      //case D3DRENDERSTATE_STIPPLEPATTERN29:
      //case D3DRENDERSTATE_STIPPLEPATTERN30:
      //case D3DRENDERSTATE_STIPPLEPATTERN31:
      case D3DRENDERSTATE_WRAP0:
      case D3DRENDERSTATE_WRAP1:
      case D3DRENDERSTATE_WRAP2:
      case D3DRENDERSTATE_WRAP3:
      case D3DRENDERSTATE_WRAP4:
      case D3DRENDERSTATE_WRAP5:
      case D3DRENDERSTATE_WRAP6:
      case D3DRENDERSTATE_WRAP7:
        break;

      // "Texture handle for use when rendering with the IDirect3DDevice2 or earlier interfaces."
      // Note: This is actually used by Grandia II, but with IDirectDrawSurface4 objects...
      case D3DRENDERSTATE_TEXTUREHANDLE:
        *lpdwRenderState = m_commonD3DDevice->GetCurrentTextureHandle();
        return D3D_OK;

      case D3DRENDERSTATE_ANTIALIAS:
        *lpdwRenderState = m_commonD3DDevice->GetAntialias();
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESS:
        device9->GetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, lpdwRenderState);
        return D3D_OK;

      // Always enabled on later APIs, default TRUE in D3D6
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        *lpdwRenderState = TRUE;
        return D3D_OK;

      // Not implemented in DXVK, but retrieve it as it were
      case D3DRENDERSTATE_WRAPU: {
        DWORD value9 = 0;
        device9->GetRenderState(d3d9::D3DRS_WRAP0, &value9);
        *lpdwRenderState = (value9 & D3DWRAP_U) ? TRUE : FALSE;
        return D3D_OK;
      }

      // Not implemented in DXVK, but retrieve it as it were
      case D3DRENDERSTATE_WRAPV: {
        DWORD value9 = 0;
        device9->GetRenderState(d3d9::D3DRS_WRAP0, &value9);
        *lpdwRenderState = (value9 & D3DWRAP_V) ? TRUE : FALSE;
        return D3D_OK;
      }

      case D3DRENDERSTATE_LINEPATTERN:
        *lpdwRenderState = bit::cast<DWORD>(m_commonD3DDevice->GetLinePattern());
        return D3D_OK;

      case D3DRENDERSTATE_MONOENABLE:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_ROP2:
        *lpdwRenderState = R2_COPYPEN;
        return D3D_OK;

      case D3DRENDERSTATE_PLANEMASK:
        *lpdwRenderState = 0;
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREMAG:
        device9->GetSamplerState(0, d3d9::D3DSAMP_MAGFILTER, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREMIN: {
        DWORD minFilter = 0;
        DWORD mipFilter = 0;
        device9->GetSamplerState(0, d3d9::D3DSAMP_MINFILTER, &minFilter);
        device9->GetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, &mipFilter);
        *lpdwRenderState = DecodeTextureMinValues(minFilter, mipFilter);
        return D3D_OK;
      }

      case D3DRENDERSTATE_TEXTUREMAPBLEND:
        *lpdwRenderState = m_commonD3DDevice->GetTextureMapBlend();
        return D3D_OK;

      // Not supported by D3D6
      case D3DRENDERSTATE_ZVISIBLE:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_SUBPIXEL:
      case D3DRENDERSTATE_SUBPIXELX:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_STIPPLEDALPHA:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_STIPPLEENABLE:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      case D3DRENDERSTATE_COLORKEYENABLE:
        *lpdwRenderState = m_commonD3DDevice->GetColorKeyEnable();
        return D3D_OK;

      case D3DRENDERSTATE_BORDERCOLOR:
        device9->GetSamplerState(0, d3d9::D3DSAMP_BORDERCOLOR, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESSU:
        device9->GetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESSV:
        device9->GetSamplerState(0, d3d9::D3DSAMP_ADDRESSV, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_MIPMAPLODBIAS:
        device9->GetSamplerState(0, d3d9::D3DSAMP_MIPMAPLODBIAS, lpdwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_ZBIAS: {
        DWORD bias = 0;
        device9->GetRenderState(d3d9::D3DRS_DEPTHBIAS, &bias);
        *lpdwRenderState = static_cast<DWORD>(bit::cast<float>(bias) * ddrawCaps::ZBIAS_SCALE_INV);
        return D3D_OK;
      }

      case D3DRENDERSTATE_ANISOTROPY:
        device9->GetSamplerState(0, d3d9::D3DSAMP_MAXANISOTROPY, lpdwRenderState);
        return D3D_OK;

      // "Batched primitives are implicitly flushed when rendering with the
      // IDirect3DDevice3 interface, as well as when rendering with execute buffers."
      case D3DRENDERSTATE_FLUSHBATCH:
        *lpdwRenderState = TRUE;
        return D3D_OK;

      case D3DRENDERSTATE_TRANSLUCENTSORTINDEPENDENT:
        *lpdwRenderState = FALSE;
        return D3D_OK;

      case D3DRENDERSTATE_STIPPLEPATTERN00:
      case D3DRENDERSTATE_STIPPLEPATTERN01:
      case D3DRENDERSTATE_STIPPLEPATTERN02:
      case D3DRENDERSTATE_STIPPLEPATTERN03:
      case D3DRENDERSTATE_STIPPLEPATTERN04:
      case D3DRENDERSTATE_STIPPLEPATTERN05:
      case D3DRENDERSTATE_STIPPLEPATTERN06:
      case D3DRENDERSTATE_STIPPLEPATTERN07:
      case D3DRENDERSTATE_STIPPLEPATTERN08:
      case D3DRENDERSTATE_STIPPLEPATTERN09:
      case D3DRENDERSTATE_STIPPLEPATTERN10:
      case D3DRENDERSTATE_STIPPLEPATTERN11:
      case D3DRENDERSTATE_STIPPLEPATTERN12:
      case D3DRENDERSTATE_STIPPLEPATTERN13:
      case D3DRENDERSTATE_STIPPLEPATTERN14:
      case D3DRENDERSTATE_STIPPLEPATTERN15:
      case D3DRENDERSTATE_STIPPLEPATTERN16:
      case D3DRENDERSTATE_STIPPLEPATTERN17:
      case D3DRENDERSTATE_STIPPLEPATTERN18:
      case D3DRENDERSTATE_STIPPLEPATTERN19:
      case D3DRENDERSTATE_STIPPLEPATTERN20:
      case D3DRENDERSTATE_STIPPLEPATTERN21:
      case D3DRENDERSTATE_STIPPLEPATTERN22:
      case D3DRENDERSTATE_STIPPLEPATTERN23:
      case D3DRENDERSTATE_STIPPLEPATTERN24:
      case D3DRENDERSTATE_STIPPLEPATTERN25:
      case D3DRENDERSTATE_STIPPLEPATTERN26:
      case D3DRENDERSTATE_STIPPLEPATTERN27:
      case D3DRENDERSTATE_STIPPLEPATTERN28:
      case D3DRENDERSTATE_STIPPLEPATTERN29:
      case D3DRENDERSTATE_STIPPLEPATTERN30:
      case D3DRENDERSTATE_STIPPLEPATTERN31:
        *lpdwRenderState = 0;
        return D3D_OK;

      // As opposed to D3D7, D3D6 does not error out on
      // unknown or invalid render states.
      default:
        if (likely(!m_commonIntf->GetOptions()->apitraceMode)) {
          *lpdwRenderState = 0;
          return D3D_OK;
        }
        break;
    }

    // This call will never fail
    return device9->GetRenderState(State9, lpdwRenderState);
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::SetRenderState(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState) {
    D3DDeviceLock lock = LockDevice();

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();
    d3d9::D3DRENDERSTATETYPE State9 = d3d9::D3DRENDERSTATETYPE(dwRenderStateType);

    switch (dwRenderStateType) {
      // Most render states translate 1:1 to D3D9
      // Most render states translate 1:1 to D3D9
      //case D3DRENDERSTATE_TEXTUREHANDLE:
      //case D3DRENDERSTATE_ANTIALIAS:
      //case D3DRENDERSTATE_TEXTUREADDRESS:
      //case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
      //case D3DRENDERSTATE_WRAPU:
      //case D3DRENDERSTATE_WRAPV:
      case D3DRENDERSTATE_ZENABLE:
      case D3DRENDERSTATE_FILLMODE:
      case D3DRENDERSTATE_SHADEMODE:
      //case D3DRENDERSTATE_LINEPATTERN:
      //case D3DRENDERSTATE_MONOENABLE:
      //case D3DRENDERSTATE_ROP2:
      //case D3DRENDERSTATE_PLANEMASK:
      case D3DRENDERSTATE_ZWRITEENABLE:
      case D3DRENDERSTATE_ALPHATESTENABLE:
      case D3DRENDERSTATE_LASTPIXEL:
      //case D3DRENDERSTATE_TEXTUREMAG:
      //case D3DRENDERSTATE_TEXTUREMIN:
      case D3DRENDERSTATE_SRCBLEND:
      case D3DRENDERSTATE_DESTBLEND:
      //case D3DRENDERSTATE_TEXTUREMAPBLEND:
      case D3DRENDERSTATE_CULLMODE:
      case D3DRENDERSTATE_ZFUNC:
      case D3DRENDERSTATE_ALPHAREF:
      case D3DRENDERSTATE_ALPHAFUNC:
      case D3DRENDERSTATE_DITHERENABLE:
      case D3DRENDERSTATE_ALPHABLENDENABLE:
      case D3DRENDERSTATE_FOGENABLE:
      case D3DRENDERSTATE_SPECULARENABLE:
      //case D3DRENDERSTATE_ZVISIBLE:
      //case D3DRENDERSTATE_SUBPIXEL:
      //case D3DRENDERSTATE_SUBPIXELX:
      //case D3DRENDERSTATE_STIPPLEDALPHA:
      case D3DRENDERSTATE_FOGCOLOR:
      case D3DRENDERSTATE_FOGTABLEMODE:
      case D3DRENDERSTATE_FOGTABLESTART:
      case D3DRENDERSTATE_FOGTABLEEND:
      case D3DRENDERSTATE_FOGTABLEDENSITY:
      //case D3DRENDERSTATE_STIPPLEENABLE:
      //case D3DRENDERSTATE_EDGEANTIALIAS:
      //case D3DRENDERSTATE_COLORKEYENABLE:
      //case D3DRENDERSTATE_BORDERCOLOR:
      //case D3DRENDERSTATE_TEXTUREADDRESSU:
      //case D3DRENDERSTATE_TEXTUREADDRESSV:
      //case D3DRENDERSTATE_MIPMAPLODBIAS:
      //case D3DRENDERSTATE_ZBIAS:
      case D3DRENDERSTATE_RANGEFOGENABLE:
      //case D3DRENDERSTATE_ANISOTROPY:
      //case D3DRENDERSTATE_FLUSHBATCH:
      //case D3DRENDERSTATE_TRANSLUCENTSORTINDEPENDENT:
      case D3DRENDERSTATE_STENCILENABLE:
      case D3DRENDERSTATE_STENCILFAIL:
      case D3DRENDERSTATE_STENCILZFAIL:
      case D3DRENDERSTATE_STENCILPASS:
      case D3DRENDERSTATE_STENCILFUNC:
      case D3DRENDERSTATE_STENCILREF:
      case D3DRENDERSTATE_STENCILMASK:
      case D3DRENDERSTATE_STENCILWRITEMASK:
      case D3DRENDERSTATE_TEXTUREFACTOR:
      //case D3DRENDERSTATE_STIPPLEPATTERN00:
      //case D3DRENDERSTATE_STIPPLEPATTERN01:
      //case D3DRENDERSTATE_STIPPLEPATTERN02:
      //case D3DRENDERSTATE_STIPPLEPATTERN03:
      //case D3DRENDERSTATE_STIPPLEPATTERN04:
      //case D3DRENDERSTATE_STIPPLEPATTERN05:
      //case D3DRENDERSTATE_STIPPLEPATTERN06:
      //case D3DRENDERSTATE_STIPPLEPATTERN07:
      //case D3DRENDERSTATE_STIPPLEPATTERN08:
      //case D3DRENDERSTATE_STIPPLEPATTERN09:
      //case D3DRENDERSTATE_STIPPLEPATTERN10:
      //case D3DRENDERSTATE_STIPPLEPATTERN11:
      //case D3DRENDERSTATE_STIPPLEPATTERN12:
      //case D3DRENDERSTATE_STIPPLEPATTERN13:
      //case D3DRENDERSTATE_STIPPLEPATTERN14:
      //case D3DRENDERSTATE_STIPPLEPATTERN15:
      //case D3DRENDERSTATE_STIPPLEPATTERN16:
      //case D3DRENDERSTATE_STIPPLEPATTERN17:
      //case D3DRENDERSTATE_STIPPLEPATTERN18:
      //case D3DRENDERSTATE_STIPPLEPATTERN19:
      //case D3DRENDERSTATE_STIPPLEPATTERN20:
      //case D3DRENDERSTATE_STIPPLEPATTERN21:
      //case D3DRENDERSTATE_STIPPLEPATTERN22:
      //case D3DRENDERSTATE_STIPPLEPATTERN23:
      //case D3DRENDERSTATE_STIPPLEPATTERN24:
      //case D3DRENDERSTATE_STIPPLEPATTERN25:
      //case D3DRENDERSTATE_STIPPLEPATTERN26:
      //case D3DRENDERSTATE_STIPPLEPATTERN27:
      //case D3DRENDERSTATE_STIPPLEPATTERN28:
      //case D3DRENDERSTATE_STIPPLEPATTERN29:
      //case D3DRENDERSTATE_STIPPLEPATTERN30:
      //case D3DRENDERSTATE_STIPPLEPATTERN31:
      case D3DRENDERSTATE_WRAP0:
      case D3DRENDERSTATE_WRAP1:
      case D3DRENDERSTATE_WRAP2:
      case D3DRENDERSTATE_WRAP3:
      case D3DRENDERSTATE_WRAP4:
      case D3DRENDERSTATE_WRAP5:
      case D3DRENDERSTATE_WRAP6:
      case D3DRENDERSTATE_WRAP7:
        break;

      // "Texture handle for use when rendering with the IDirect3DDevice2 or earlier interfaces."
      // Note: This is actually used by Grandia II, but with IDirectDrawSurface4 objects...
      case D3DRENDERSTATE_TEXTUREHANDLE: {
        DDraw4Surface* surface4 = nullptr;

        if (likely(dwRenderState != 0)) {
          surface4 = DDrawCommonInterface::GetSurface4FromTextureHandle(dwRenderState);
          if (unlikely(surface4 == nullptr))
            return DDERR_INVALIDPARAMS;
        }

        HRESULT hr = SetTextureWithHandle(surface4, dwRenderState);
        if (unlikely(FAILED(hr)))
          return hr;

        if (unlikely(surface4 == nullptr))
          m_bridge->SetColorKeyState(false);

        return D3D_OK;
      }

      case D3DRENDERSTATE_ANTIALIAS: {
        const D3DOptions* d3dOptions = m_commonIntf->GetOptions();

        if (likely(d3dOptions->emulateFSAA == FSAAEmulation::Disabled)) {
          if (unlikely(dwRenderState == D3DANTIALIAS_SORTDEPENDENT
                    || dwRenderState == D3DANTIALIAS_SORTINDEPENDENT))
            Logger::warn("D3D6Device::SetRenderState: Device does not expose FSAA emulation");
          return D3D_OK;
        }

        State9        = d3d9::D3DRS_MULTISAMPLEANTIALIAS;
        m_commonD3DDevice->SetAntialias(dwRenderState);
        dwRenderState = dwRenderState == D3DANTIALIAS_SORTDEPENDENT
                     || dwRenderState == D3DANTIALIAS_SORTINDEPENDENT
                     || d3dOptions->emulateFSAA == FSAAEmulation::Forced ? TRUE : FALSE;
        break;
      }

      case D3DRENDERSTATE_TEXTUREADDRESS:
        device9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, dwRenderState);
        device9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSV, dwRenderState);
        return D3D_OK;

      // Always enabled on later APIs, default TRUE in D3D6
      case D3DRENDERSTATE_TEXTUREPERSPECTIVE:
        return D3D_OK;

      // Not implemented in DXVK, but forward it anyway
      case D3DRENDERSTATE_WRAPU: {
        DWORD value9 = 0;
        device9->GetRenderState(d3d9::D3DRS_WRAP0, &value9);
        if (dwRenderState == TRUE) {
          device9->SetRenderState(d3d9::D3DRS_WRAP0, value9 | D3DWRAP_U);
        } else {
          device9->SetRenderState(d3d9::D3DRS_WRAP0, value9 & ~D3DWRAP_U);
        }
        return D3D_OK;
      }

      // Not implemented in DXVK, but forward it anyway
      case D3DRENDERSTATE_WRAPV: {
        DWORD value9 = 0;
        device9->GetRenderState(d3d9::D3DRS_WRAP0, &value9);
        if (dwRenderState == TRUE) {
          device9->SetRenderState(d3d9::D3DRS_WRAP0, value9 | D3DWRAP_V);
        } else {
          device9->SetRenderState(d3d9::D3DRS_WRAP0, value9 & ~D3DWRAP_V);
        }
        return D3D_OK;
      }

      // TODO: Implement D3DRS_LINEPATTERN - vkCmdSetLineRasterizationModeEXT
      // and advertise support with D3DPRASTERCAPS_PAT once that is done
      case D3DRENDERSTATE_LINEPATTERN:
        static bool s_linePatternErrorShown;

        if (!std::exchange(s_linePatternErrorShown, true))
          Logger::warn("D3D6Device::SetRenderState: Unimplemented render state D3DRS_LINEPATTERN");

        m_commonD3DDevice->SetLinePattern(bit::cast<D3DLINEPATTERN>(dwRenderState));
        return D3D_OK;

      case D3DRENDERSTATE_MONOENABLE:
        return D3D_OK;

      case D3DRENDERSTATE_ROP2:
        return D3D_OK;

      // Docs state: "This render state is not supported by the software
      // rasterizers, and is often ignored by hardware drivers."
      case D3DRENDERSTATE_PLANEMASK:
        return D3D_OK;

      // Docs: "[...]  only the first two (D3DFILTER_NEAREST and
      // D3DFILTER_LINEAR) are valid with D3DRENDERSTATE_TEXTUREMAG."
      case D3DRENDERSTATE_TEXTUREMAG: {
        switch (dwRenderState) {
          case D3DFILTER_NEAREST:
          case D3DFILTER_LINEAR:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MAGFILTER, dwRenderState);
            break;
          default:
            break;
        }
        return D3D_OK;
      }

      case D3DRENDERSTATE_TEXTUREMIN: {
        switch (dwRenderState) {
          case D3DFILTER_NEAREST:
          case D3DFILTER_LINEAR:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, dwRenderState);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_NONE);
            break;
          // "The closest mipmap level is chosen and a point filter is applied."
          case D3DFILTER_MIPNEAREST:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_POINT);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_POINT);
            break;
          // "The closest mipmap level is chosen and a bilinear filter is applied within it."
          case D3DFILTER_MIPLINEAR:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_LINEAR);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_POINT);
            break;
          // "The two closest mipmap levels are chosen and then a linear
          //  blend is used between point filtered samples of each level."
          case D3DFILTER_LINEARMIPNEAREST:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_POINT);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_LINEAR);
            break;
          // "The two closest mipmap levels are chosen and then combined using a bilinear filter."
          case D3DFILTER_LINEARMIPLINEAR:
            device9->SetSamplerState(0, d3d9::D3DSAMP_MINFILTER, d3d9::D3DTEXF_LINEAR);
            device9->SetSamplerState(0, d3d9::D3DSAMP_MIPFILTER, d3d9::D3DTEXF_LINEAR);
            break;
          default:
            break;
        }
        return D3D_OK;
      }

      case D3DRENDERSTATE_TEXTUREMAPBLEND:
        // Setting the same blend state won't reset the texture state
        if (m_commonD3DDevice->GetTextureMapBlend() == dwRenderState)
          return D3D_OK;

        m_commonD3DDevice->SetTextureMapBlend(dwRenderState);
        // Any explicitly set D3DTSS_ALPHAOP value will get overwritten at this point
        m_alphaOpSet = false;

        switch (dwRenderState) {
          // "In this mode, the RGB and alpha values of the texture replace
          //  the colors that would have been used with no texturing."
          case D3DTBLEND_DECAL:
          case D3DTBLEND_COPY:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_CURRENT);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            break;
          // "In this mode, the RGB values of the texture are multiplied with the RGB values
          //  that would have been used with no texturing. Any alpha values in the texture
          //  replace the alpha values in the colors that would have been used with no texturing;
          //  if the texture does not contain an alpha component, alpha values at the vertices
          //  in the source are interpolated between vertices."
          case D3DTBLEND_MODULATE:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_MODULATE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_CURRENT);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            break;
          // "In this mode, the RGB and alpha values of the texture are blended with the colors
          //  that would have been used with no texturing, according to the following formulas [...]"
          case D3DTBLEND_DECALALPHA:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_BLENDTEXTUREALPHA);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_CURRENT);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            break;
          // "In this mode, the RGB values of the texture are multiplied with the RGB values that
          //  would have been used with no texturing, and the alpha values of the texture
          //  are multiplied with the alpha values that would have been used with no texturing."
          case D3DTBLEND_MODULATEALPHA:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_MODULATE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_CURRENT);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            break;
          // "Add the Gouraud interpolants to the texture lookup with saturation semantics
          //  (that is, if the color value overflows it is set to the maximum possible value)."
          case D3DTBLEND_ADD:
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLOROP,   D3DTOP_ADD);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
            device9->SetTextureStageState(0, d3d9::D3DTSS_COLORARG2, D3DTA_CURRENT);
            device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAARG2, D3DTA_CURRENT);
            break;
          // Unsupported
          default:
          case D3DTBLEND_DECALMASK:
          case D3DTBLEND_MODULATEMASK:
            break;
        }

        return D3D_OK;

      // Not supported by D3D6
      case D3DRENDERSTATE_ZVISIBLE:
        return D3D_OK;

      // Docs state: "Most hardware either doesn't support it (always off) or
      // always supports it (always on).", and "All hardware should be subpixel correct.
      // Some software rasterizers are not subpixel correct because of the performance loss."
      case D3DRENDERSTATE_SUBPIXEL:
      case D3DRENDERSTATE_SUBPIXELX:
        return D3D_OK;

      // Tests have shown age accurate GPUs didn't offer support for stippling at all
      case D3DRENDERSTATE_STIPPLEDALPHA:
        return D3D_OK;

      // Tests have shown age accurate GPUs didn't offer support for stippling at all
      case D3DRENDERSTATE_STIPPLEENABLE:
        return D3D_OK;

      case D3DRENDERSTATE_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      case D3DRENDERSTATE_COLORKEYENABLE: {
        m_commonD3DDevice->SetColorKeyEnable(dwRenderState);

        DDrawCommonSurface* commonSurf = m_textures[0] != nullptr ?
                                         m_textures[0]->GetCommonTexture()->GetCommonSurface() : nullptr;
        const bool validColorKey = commonSurf != nullptr ? commonSurf->HasValidColorKey() : false;
        m_bridge->SetColorKeyState(dwRenderState && validColorKey);
        if (dwRenderState && validColorKey) {
          DDCOLORKEY normalizedColorKey = commonSurf->GetColorKeyNormalized();
          m_bridge->SetColorKey(normalizedColorKey.dwColorSpaceLowValue,
                                normalizedColorKey.dwColorSpaceHighValue);
        }

        return D3D_OK;
      }

      case D3DRENDERSTATE_BORDERCOLOR:
        device9->SetSamplerState(0, d3d9::D3DSAMP_BORDERCOLOR, dwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESSU:
        device9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSU, dwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_TEXTUREADDRESSV:
        device9->SetSamplerState(0, d3d9::D3DSAMP_ADDRESSV, dwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_MIPMAPLODBIAS:
        device9->SetSamplerState(0, d3d9::D3DSAMP_MIPMAPLODBIAS, dwRenderState);
        return D3D_OK;

      case D3DRENDERSTATE_ZBIAS:
        State9         = d3d9::D3DRS_DEPTHBIAS;
        dwRenderState  = bit::cast<DWORD>(static_cast<float>(dwRenderState) * ddrawCaps::ZBIAS_SCALE);
        break;

      case D3DRENDERSTATE_ANISOTROPY:
        device9->SetSamplerState(0, d3d9::D3DSAMP_MAXANISOTROPY, dwRenderState);
        return D3D_OK;

      // "Batched primitives are implicitly flushed when rendering with the
      // IDirect3DDevice3 interface, as well as when rendering with execute buffers."
      case D3DRENDERSTATE_FLUSHBATCH:
        return D3D_OK;

      // Unclear if this ever did much as far as the runtime was concered.
      // Most likely it was passed to the driver, though I don't expect
      // it was ever implemented, as it's a D3D6-only render state.
      case D3DRENDERSTATE_TRANSLUCENTSORTINDEPENDENT:
        return D3D_OK;

      // Tests have shown age accurate GPUs didn't offer support for stippling at all
      case D3DRENDERSTATE_STIPPLEPATTERN00:
      case D3DRENDERSTATE_STIPPLEPATTERN01:
      case D3DRENDERSTATE_STIPPLEPATTERN02:
      case D3DRENDERSTATE_STIPPLEPATTERN03:
      case D3DRENDERSTATE_STIPPLEPATTERN04:
      case D3DRENDERSTATE_STIPPLEPATTERN05:
      case D3DRENDERSTATE_STIPPLEPATTERN06:
      case D3DRENDERSTATE_STIPPLEPATTERN07:
      case D3DRENDERSTATE_STIPPLEPATTERN08:
      case D3DRENDERSTATE_STIPPLEPATTERN09:
      case D3DRENDERSTATE_STIPPLEPATTERN10:
      case D3DRENDERSTATE_STIPPLEPATTERN11:
      case D3DRENDERSTATE_STIPPLEPATTERN12:
      case D3DRENDERSTATE_STIPPLEPATTERN13:
      case D3DRENDERSTATE_STIPPLEPATTERN14:
      case D3DRENDERSTATE_STIPPLEPATTERN15:
      case D3DRENDERSTATE_STIPPLEPATTERN16:
      case D3DRENDERSTATE_STIPPLEPATTERN17:
      case D3DRENDERSTATE_STIPPLEPATTERN18:
      case D3DRENDERSTATE_STIPPLEPATTERN19:
      case D3DRENDERSTATE_STIPPLEPATTERN20:
      case D3DRENDERSTATE_STIPPLEPATTERN21:
      case D3DRENDERSTATE_STIPPLEPATTERN22:
      case D3DRENDERSTATE_STIPPLEPATTERN23:
      case D3DRENDERSTATE_STIPPLEPATTERN24:
      case D3DRENDERSTATE_STIPPLEPATTERN25:
      case D3DRENDERSTATE_STIPPLEPATTERN26:
      case D3DRENDERSTATE_STIPPLEPATTERN27:
      case D3DRENDERSTATE_STIPPLEPATTERN28:
      case D3DRENDERSTATE_STIPPLEPATTERN29:
      case D3DRENDERSTATE_STIPPLEPATTERN30:
      case D3DRENDERSTATE_STIPPLEPATTERN31:
        return D3D_OK;

      // As opposed to D3D7, D3D6 does not error out on
      // unknown or invalid render states.
      default:
        return D3D_OK;
    }

    // This call will never fail
    return device9->SetRenderState(State9, dwRenderState);
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetLightState(D3DLIGHTSTATETYPE dwLightStateType, LPDWORD lpdwLightState) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(lpdwLightState == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    switch (dwLightStateType) {
      case D3DLIGHTSTATE_MATERIAL:
        *lpdwLightState = m_commonD3DDevice->GetCurrentMaterialHandle();
        break;
      case D3DLIGHTSTATE_AMBIENT:
        device9->GetRenderState(d3d9::D3DRS_AMBIENT, lpdwLightState);
        break;
      case D3DLIGHTSTATE_COLORMODEL:
        *lpdwLightState = D3DCOLOR_RGB;
        break;
      case D3DLIGHTSTATE_FOGMODE:
        device9->GetRenderState(d3d9::D3DRS_FOGVERTEXMODE, lpdwLightState);
        break;
      case D3DLIGHTSTATE_FOGSTART:
        device9->GetRenderState(d3d9::D3DRS_FOGSTART, lpdwLightState);
        break;
      case D3DLIGHTSTATE_FOGEND:
        device9->GetRenderState(d3d9::D3DRS_FOGEND, lpdwLightState);
        break;
      case D3DLIGHTSTATE_FOGDENSITY:
        device9->GetRenderState(d3d9::D3DRS_FOGDENSITY, lpdwLightState);
        break;
      case D3DLIGHTSTATE_COLORVERTEX:
        device9->GetRenderState(d3d9::D3DRS_COLORVERTEX, lpdwLightState);
        break;
      default:
        return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::SetLightState(D3DLIGHTSTATETYPE dwLightStateType, DWORD dwLightState) {
    D3DDeviceLock lock = LockDevice();

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    switch (dwLightStateType) {
      case D3DLIGHTSTATE_MATERIAL: {
        if (unlikely(!dwLightState)) {
          m_commonD3DDevice->SetCurrentMaterialHandle(dwLightState);
          static constexpr d3d9::D3DMATERIAL9 DefaultMaterial9 = { };
          device9->SetMaterial(&DefaultMaterial9);
          return D3D_OK;
        }

        d3d9::D3DMATERIAL9* material9 = D3DCommonInterface::GetCommonMaterialFromHandle(dwLightState)->GetD3D9Material();
        if (unlikely(material9 == nullptr))
          return DDERR_INVALIDPARAMS;

        m_commonD3DDevice->SetCurrentMaterialHandle(dwLightState);
        //Logger::debug(str::format("D3D6Device::SetLightState: Applying material nr. ", dwLightState, " to D3D9"));
        device9->SetMaterial(material9);

        break;
      }
      case D3DLIGHTSTATE_AMBIENT:
        device9->SetRenderState(d3d9::D3DRS_AMBIENT, dwLightState);
        break;
      case D3DLIGHTSTATE_COLORMODEL:
        if (unlikely(dwLightState != D3DCOLOR_RGB))
          Logger::warn("D3D6Device::SetLightState: Unsupported D3DLIGHTSTATE_COLORMODEL");
        break;
      case D3DLIGHTSTATE_FOGMODE:
        device9->SetRenderState(d3d9::D3DRS_FOGVERTEXMODE, dwLightState);
        break;
      case D3DLIGHTSTATE_FOGSTART:
        device9->SetRenderState(d3d9::D3DRS_FOGSTART, dwLightState);
        break;
      case D3DLIGHTSTATE_FOGEND:
        device9->SetRenderState(d3d9::D3DRS_FOGEND, dwLightState);
        break;
      case D3DLIGHTSTATE_FOGDENSITY:
        device9->SetRenderState(d3d9::D3DRS_FOGDENSITY, dwLightState);
        break;
      case D3DLIGHTSTATE_COLORVERTEX:
        device9->SetRenderState(d3d9::D3DRS_COLORVERTEX, dwLightState);
        break;
      default:
        return DDERR_INVALIDPARAMS;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::SetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    return m_commonD3DDevice->GetD3D9Device()->SetTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    return m_commonD3DDevice->GetD3D9Device()->GetTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::MultiplyTransform(D3DTRANSFORMSTATETYPE state, D3DMATRIX *matrix) {
    return m_commonD3DDevice->GetD3D9Device()->MultiplyTransform(ConvertTransformState(state), matrix);
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::DrawPrimitive(D3DPRIMITIVETYPE primitive_type, DWORD vertex_type, void *vertices, DWORD vertex_count, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(!vertex_count))
      return D3D_OK;

    if (unlikely(vertices == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    DDrawDirtySurfaceUpload();

    const bool useLighting = !(flags & D3DDP_DONOTLIGHT) &&
                              (vertex_type & D3DFVF_NORMAL) &&
                              m_commonD3DDevice->GetCurrentMaterialHandle() != 0;

    if (!useLighting)
      device9->SetRenderState(d3d9::D3DRS_LIGHTING, FALSE);
    HandlePreDrawLegacyProjection(device9, flags);

    device9->SetFVF(vertex_type);
    HRESULT hr = device9->DrawPrimitiveUP(
                      d3d9::D3DPRIMITIVETYPE(primitive_type),
                      GetPrimitiveCount(primitive_type, vertex_count),
                      vertices,
                      GetFVFSize(vertex_type));

    if (!useLighting)
      device9->SetRenderState(d3d9::D3DRS_LIGHTING, TRUE);
    HandlePostDrawLegacyProjection(device9);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::DrawPrimitive: Failed D3D9 call to DrawPrimitiveUP");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::DrawIndexedPrimitive(D3DPRIMITIVETYPE primitive_type, DWORD fvf, void *vertices, DWORD vertex_count, WORD *indices, DWORD index_count, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(!vertex_count || !index_count))
      return D3D_OK;

    if (unlikely(vertices == nullptr || indices == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    DDrawDirtySurfaceUpload();

    const bool useLighting = !(flags & D3DDP_DONOTLIGHT) &&
                              (fvf & D3DFVF_NORMAL) &&
                              m_commonD3DDevice->GetCurrentMaterialHandle() != 0;

    if (!useLighting)
      device9->SetRenderState(d3d9::D3DRS_LIGHTING, FALSE);
    HandlePreDrawLegacyProjection(device9, flags);

    device9->SetFVF(fvf);
    HRESULT hr = device9->DrawIndexedPrimitiveUP(
                      d3d9::D3DPRIMITIVETYPE(primitive_type),
                      0,
                      vertex_count,
                      GetPrimitiveCount(primitive_type, index_count),
                      indices,
                      d3d9::D3DFMT_INDEX16,
                      vertices,
                      GetFVFSize(fvf));

    if (!useLighting)
      device9->SetRenderState(d3d9::D3DRS_LIGHTING, TRUE);
    HandlePostDrawLegacyProjection(device9);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::DrawIndexedPrimitive: Failed D3D9 call to DrawIndexedPrimitiveUP");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::SetClipStatus(D3DCLIPSTATUS *clip_status) {
    if (unlikely(clip_status == nullptr))
      return DDERR_INVALIDPARAMS;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetClipStatus(D3DCLIPSTATUS *clip_status) {
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

  HRESULT STDMETHODCALLTYPE D3D6Device::DrawPrimitiveStrided(D3DPRIMITIVETYPE primitive_type, DWORD fvf, D3DDRAWPRIMITIVESTRIDEDDATA *strided_data, DWORD vertex_count, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(!vertex_count))
      return D3D_OK;

    if (unlikely(strided_data == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    DDrawDirtySurfaceUpload();

    // Transform strided vertex data to a standard vertex buffer stream
    PackedVertexBuffer pvb = TransformStridedtoUP(fvf, strided_data, vertex_count);

    const bool useLighting = !(flags & D3DDP_DONOTLIGHT) &&
                              (fvf & D3DFVF_NORMAL) &&
                              m_commonD3DDevice->GetCurrentMaterialHandle() != 0;

    if (!useLighting)
      device9->SetRenderState(d3d9::D3DRS_LIGHTING, FALSE);
    HandlePreDrawLegacyProjection(device9, flags);

    device9->SetFVF(fvf);
    HRESULT hr = device9->DrawPrimitiveUP(
                      d3d9::D3DPRIMITIVETYPE(primitive_type),
                      GetPrimitiveCount(primitive_type, vertex_count),
                      pvb.vertexData.data(),
                      pvb.stride);

    if (!useLighting)
      device9->SetRenderState(d3d9::D3DRS_LIGHTING, TRUE);
    HandlePostDrawLegacyProjection(device9);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::DrawPrimitiveStrided: Failed D3D9 call to DrawPrimitiveUP");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::DrawIndexedPrimitiveStrided(D3DPRIMITIVETYPE primitive_type, DWORD fvf, D3DDRAWPRIMITIVESTRIDEDDATA *strided_data, DWORD vertex_count, WORD *indices, DWORD index_count, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(!vertex_count || !index_count))
      return D3D_OK;

    if (unlikely(strided_data == nullptr || indices == nullptr))
      return DDERR_INVALIDPARAMS;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    DDrawDirtySurfaceUpload();

    // Transform strided vertex data to a standard vertex buffer stream
    PackedVertexBuffer pvb = TransformStridedtoUP(fvf, strided_data, vertex_count);

    const bool useLighting = !(flags & D3DDP_DONOTLIGHT) &&
                              (fvf & D3DFVF_NORMAL) &&
                              m_commonD3DDevice->GetCurrentMaterialHandle() != 0;

    if (!useLighting)
      device9->SetRenderState(d3d9::D3DRS_LIGHTING, FALSE);
    HandlePreDrawLegacyProjection(device9, flags);

    device9->SetFVF(fvf);
    HRESULT hr = device9->DrawIndexedPrimitiveUP(
                      d3d9::D3DPRIMITIVETYPE(primitive_type),
                      0,
                      vertex_count,
                      GetPrimitiveCount(primitive_type, index_count),
                      indices,
                      d3d9::D3DFMT_INDEX16,
                      pvb.vertexData.data(),
                      pvb.stride);

    if (!useLighting)
      device9->SetRenderState(d3d9::D3DRS_LIGHTING, TRUE);
    HandlePostDrawLegacyProjection(device9);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::DrawIndexedPrimitiveStrided: Failed D3D9 call to DrawIndexedPrimitiveUP");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::DrawPrimitiveVB(D3DPRIMITIVETYPE primitive_type, IDirect3DVertexBuffer *vb, DWORD start_vertex, DWORD vertex_count, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(!vertex_count))
      return D3D_OK;

    if (unlikely(vb == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<D3D6VertexBuffer> vb6 = static_cast<D3D6VertexBuffer*>(vb);

    if (unlikely(vb6->GetDevice() != this)) {
      Logger::err("D3D6Device::DrawIndexedPrimitiveVB: Invalid vertex buffer parent device");
      return DDERR_GENERIC;
    }

    if (unlikely(vb6->IsLocked())) {
      Logger::err("D3D6Device::DrawPrimitiveVB: Buffer is locked");
      return D3DERR_VERTEXBUFFERLOCKED;
    }

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    DDrawDirtySurfaceUpload();

    const bool useLighting = !(flags & D3DDP_DONOTLIGHT) &&
                              (vb6->GetFVF() & D3DFVF_NORMAL) &&
                              m_commonD3DDevice->GetCurrentMaterialHandle() != 0;

    if (!useLighting)
      device9->SetRenderState(d3d9::D3DRS_LIGHTING, FALSE);
    HandlePreDrawLegacyProjection(device9, flags);

    device9->SetFVF(vb6->GetFVF());
    device9->SetStreamSource(0, vb6->GetD3D9VertexBuffer(), 0, vb6->GetStride());
    HRESULT hr = device9->DrawPrimitive(
                      d3d9::D3DPRIMITIVETYPE(primitive_type),
                      start_vertex,
                      GetPrimitiveCount(primitive_type, vertex_count));

    if (!useLighting)
      device9->SetRenderState(d3d9::D3DRS_LIGHTING, TRUE);
    HandlePostDrawLegacyProjection(device9);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::DrawPrimitiveVB: Failed D3D9 call to DrawPrimitive");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE primitive_type, IDirect3DVertexBuffer *vb, WORD *indices, DWORD index_count, DWORD flags) {
    D3DDeviceLock lock = LockDevice();

    RefreshLastUsedDevice();

    if (unlikely(!index_count))
      return D3D_OK;

    if (unlikely(vb == nullptr || indices == nullptr))
      return DDERR_INVALIDPARAMS;

    Com<D3D6VertexBuffer> vb6 = static_cast<D3D6VertexBuffer*>(vb);

    if (unlikely(vb6->GetDevice() != this)) {
      Logger::err("D3D6Device::DrawIndexedPrimitiveVB: Invalid vertex buffer parent device");
      return DDERR_GENERIC;
    }

    if (unlikely(vb6->IsLocked())) {
      Logger::err("D3D6Device::DrawIndexedPrimitiveVB: Buffer is locked");
      return D3DERR_VERTEXBUFFERLOCKED;
    }

    uint8_t ibIndex = 0;
    // Try to fit index buffer uploads into the smallest buffer size possible,
    // out of the five available: XS, S, M, L and XL (XL being the theoretical max)
    while (index_count > ddrawCaps::IndexCount[ibIndex]) {
      ibIndex++;
      if (unlikely(ibIndex > ddrawCaps::IndexBufferCount - 1)) {
        Logger::err("D3D6Device::DrawIndexedPrimitiveVB: Exceeded size of largest index buffer");
        return DDERR_UNSUPPORTED;
      }
    }

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    DDrawDirtySurfaceUpload();

    const bool useLighting = !(flags & D3DDP_DONOTLIGHT) &&
                              (vb6->GetFVF() & D3DFVF_NORMAL) &&
                              m_commonD3DDevice->GetCurrentMaterialHandle() != 0;

    if (!useLighting)
      device9->SetRenderState(d3d9::D3DRS_LIGHTING, FALSE);
    HandlePreDrawLegacyProjection(device9, flags);

    d3d9::IDirect3DIndexBuffer9* ib9 = m_ib9[ibIndex].ptr();

    UploadIndices(ib9, indices, index_count);
    device9->SetIndices(ib9);
    device9->SetFVF(vb6->GetFVF());
    device9->SetStreamSource(0, vb6->GetD3D9VertexBuffer(), 0, vb6->GetStride());
    HRESULT hr = device9->DrawIndexedPrimitive(
                      d3d9::D3DPRIMITIVETYPE(primitive_type),
                      0,
                      0,
                      vb6->GetNumVertices(),
                      0,
                      GetPrimitiveCount(primitive_type, index_count));

    if (!useLighting)
      device9->SetRenderState(d3d9::D3DRS_LIGHTING, TRUE);
    HandlePostDrawLegacyProjection(device9);

    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::DrawIndexedPrimitiveVB: Failed D3D9 call to DrawIndexedPrimitive");
      return hr;
    }

    UpdateSurfaceDirtyTracking(true, true, true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::ComputeSphereVisibility(D3DVECTOR *lpCenters, D3DVALUE *lpRadii, DWORD dwNumSpheres, DWORD dwFlags, DWORD *lpdwReturnValues) {
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

  HRESULT STDMETHODCALLTYPE D3D6Device::GetTexture(DWORD stage, IDirect3DTexture2 **texture) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(texture == nullptr))
      return DDERR_INVALIDPARAMS;

    if (unlikely(stage >= ddrawCaps::TextureStageCount))
      return DDERR_INVALIDPARAMS;

    *texture = m_textures[stage].ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::SetTexture(DWORD stage, IDirect3DTexture2 *texture) {
    D3DDeviceLock lock = LockDevice();

    if (unlikely(stage >= ddrawCaps::TextureStageCount))
      return DDERR_INVALIDPARAMS;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    HRESULT hr;

    // Unbinding texture stages
    if (texture == nullptr) {
      hr = device9->SetTexture(stage, nullptr);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D6Device::SetTexture: Failed to unbind D3D9 texture");
        return hr;
      }

      if (likely(m_textures[stage] != nullptr)) {
        m_textures[stage] = nullptr;

        if (likely(stage == 0))
          m_bridge->SetColorKeyState(false);
      }

      return D3D_OK;
    }

    // D3D5Texture (aka IDirect3DTexture2) is shared between D3D5 and D3D6
    D3D5Texture* texture6 = static_cast<D3D5Texture*>(texture);
    DDraw4Surface* surface4 = texture6->GetCommonTexture()->GetDD4Surface();

    // Shouldn't ever happen, but play it safe
    if (unlikely(surface4 == nullptr)) {
      Logger::err("D3D6Device::SetTexture: Failed to retrieve parent surface");
      return DDERR_UNSUPPORTED;
    }

    DDrawCommonSurface* commonSurface = surface4->GetCommonSurface();

    // If textures have been used on a different device, they
    // will get their D3D9 object reinitialized at this point
    if (unlikely(commonSurface->GetCommonD3DDevice() != m_commonD3DDevice.ptr()))
      commonSurface->DirtyDDrawSurface();

    hr = surface4->InitializeOrUploadD3D9();
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::SetTexture: Failed to initialize/upload D3D9 texture");
      return hr;
    }

    // Don't fast skip, since color key might change
    //if (unlikely(m_textures[stage] == texture6))
      //return D3D_OK;

    d3d9::IDirect3DTexture9* tex9 = commonSurface->GetD3D9Texture();

    if (likely(tex9 != nullptr)) {
      hr = device9->SetTexture(stage, tex9);
      if (unlikely(FAILED(hr))) {
        Logger::warn("D3D6Device::SetTexture: Failed to bind D3D9 texture");
        return hr;
      }

      if (likely(stage == 0)) {
        // "Any alpha values in the texture replace the alpha values in the colors that would
        //  have been used with no texturing; if the texture does not contain an alpha component,
        //  alpha values at the vertices in the source are interpolated between vertices."
        if (m_commonD3DDevice->GetTextureMapBlend() == D3DTBLEND_MODULATE && !m_alphaOpSet) {
          const DWORD textureOp = commonSurface->IsAlphaFormat() ? D3DTOP_SELECTARG1 : D3DTOP_SELECTARG2;
          device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP, textureOp);
        }

        const bool colorKeyEnable = m_commonD3DDevice->GetColorKeyEnable();
        const bool validColorKey = commonSurface->HasValidColorKey();
        m_bridge->SetColorKeyState(colorKeyEnable && validColorKey);
        if (colorKeyEnable && validColorKey) {
          DDCOLORKEY normalizedColorKey = commonSurface->GetColorKeyNormalized();
          m_bridge->SetColorKey(normalizedColorKey.dwColorSpaceLowValue,
                                normalizedColorKey.dwColorSpaceHighValue);
        }
      }
    } else {
      Logger::err("D3D6Device::SetTexture: Found no valid D3D9 texture");
    }

    m_textures[stage] = texture6;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D6Device::GetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, LPDWORD lpdwState) {
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

  HRESULT STDMETHODCALLTYPE D3D6Device::SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE d3dTexStageStateType, DWORD dwState) {
    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    // In the case of D3DTSS_ADDRESS, which is exclusive to D3D7
    // and D3D6, we need to set up both D3DTSS_ADDRESSU and D3DTSS_ADDRESSV
    if (d3dTexStageStateType == D3DTSS_ADDRESS) {
      device9->SetSamplerState(dwStage, d3d9::D3DSAMP_ADDRESSU, dwState);
      return device9->SetSamplerState(dwStage, d3d9::D3DSAMP_ADDRESSV, dwState);
    }

    // Prioritize what the application sets over texture map blend modes
    if (d3dTexStageStateType == D3DTSS_ALPHAOP)
      m_alphaOpSet = true;

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

  HRESULT STDMETHODCALLTYPE D3D6Device::ValidateDevice(LPDWORD lpdwPasses) {
    HRESULT hr = m_commonD3DDevice->GetD3D9Device()->ValidateDevice(lpdwPasses);
    if (unlikely(FAILED(hr)))
      return DDERR_INVALIDPARAMS;

    return D3D_OK;
  }

  void D3D6Device::InitializeDS() {
    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    m_rt->InitializeD3D9RenderTarget();

    m_ds = m_rt->GetAttachedDepthStencil();

    if (m_ds != nullptr) {
      HRESULT hrDS = m_ds->InitializeD3D9DepthStencil();
      if (unlikely(FAILED(hrDS))) {
        Logger::err("D3D6Device::InitializeDS: Failed to initialize D3D9 DS");
      } else {
        const RECT* dsRect = m_ds->GetCommonSurface()->GetFullSurfaceRect();
        Logger::info(str::format("D3D6Device::InitializeDS: Depth stencil: ", dsRect->right, "x", dsRect->bottom));

        HRESULT hrDS9 = device9->SetDepthStencilSurface(m_ds->GetCommonSurface()->GetD3D9Surface());
        if (unlikely(FAILED(hrDS9))) {
          Logger::err("D3D6Device::InitializeDS: Failed to set D3D9 depth stencil");
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

  void D3D6Device::UpdateSurfaceDirtyTracking(bool dirtyRenderTarget, bool dirtyDepthStencil, bool dirtyPrimarySurface) {
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

  HRESULT D3D6Device::ResetD3D9Swapchain(d3d9::D3DPRESENT_PARAMETERS* params) {
    Logger::info("D3D6Device::ResetD3D9Swapchain: Resetting the D3D9 swapchain");

    HRESULT hr = m_bridge->ResetSwapChain(params);
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::ResetD3D9Swapchain: Failed to reset the D3D9 swapchain");
      return hr;
    }

    DDrawCommonSurface* commonSurface = m_rt->GetCommonSurface();
    commonSurface->SetD3D9Surface(nullptr);
    commonSurface->UnDirtyD3D9Surface();
    // Ensure the DDraw surface content gets re-uploaded if needed
    commonSurface->DirtyDDrawSurface();

    // Reset the D3D9 objects for all the following surfaces in the swapchain
    DDraw4Surface* nextFlippable = m_rt->GetNextFlippable();

    while (nextFlippable != nullptr) {
      commonSurface = nextFlippable->GetCommonSurface();
      commonSurface->SetD3D9Surface(nullptr);
      commonSurface->UnDirtyD3D9Surface();
      // Ensure the DDraw surface content gets re-uploaded if needed
      commonSurface->DirtyDDrawSurface();

      nextFlippable = nextFlippable->GetNextFlippable();
    }

    // Reset the D3D9 objects for all the previous surfaces in the swapchain
    DDraw4Surface* parentSurf = m_rt->GetParentSurface();

    while (parentSurf != nullptr) {
      commonSurface = parentSurf->GetCommonSurface();
      commonSurface->SetD3D9Surface(nullptr);
      commonSurface->UnDirtyD3D9Surface();
      // Ensure the DDraw surface content gets re-uploaded if needed
      commonSurface->DirtyDDrawSurface();

      parentSurf = parentSurf->GetParentSurface();
    }

    // Note that the D3D9 depth stencil survives a swapchain reset,
    // so there's no need to worry about it in this case

    return D3D_OK;
  }

  inline HRESULT D3D6Device::InitializeIndexBuffers() {
    static constexpr DWORD Usage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    for (uint8_t ibIndex = 0; ibIndex < ddrawCaps::IndexBufferCount ; ibIndex++) {
      const UINT ibSize = ddrawCaps::IndexCount[ibIndex] * sizeof(WORD);

      HRESULT hr = device9->CreateIndexBuffer(ibSize, Usage, d3d9::D3DFMT_INDEX16,
                                              d3d9::D3DPOOL_DEFAULT, &m_ib9[ibIndex], nullptr);
      if (unlikely(FAILED(hr)))
        return hr;
    }

    return D3D_OK;
  }

  inline void D3D6Device::UploadIndices(d3d9::IDirect3DIndexBuffer9* ib9, WORD* indices, DWORD indexCount) {
    const size_t size = indexCount * sizeof(WORD);
    void* pData = nullptr;

    // Locking and unlocking are generally expected to work here
    ib9->Lock(0, size, &pData, D3DLOCK_DISCARD);
    memcpy(pData, static_cast<void*>(indices), size);
    ib9->Unlock();
  }

  inline void D3D6Device::DDrawDirtySurfaceUpload() {
    // Render target
    m_rt->InitializeOrUploadD3D9();
    // Depth stencil (if present)
    if (likely(m_ds != nullptr))
      m_ds->InitializeOrUploadD3D9();
    // Bound texture(s)
    DDraw4Surface* surf4 = nullptr;
    for (auto& tex : m_textures) {
      if (tex.ptr() != nullptr) {
        surf4 = tex->GetCommonTexture()->GetDD4Surface();
        if (likely(surf4 != nullptr))
          surf4->InitializeOrUploadD3D9();
      }
    }
  }

  inline void D3D6Device::AddViewportInternal(IDirect3DViewport3* viewport) {
    D3D6Viewport* d3d6Viewport = static_cast<D3D6Viewport*>(viewport);

    auto it = std::find(m_viewports.begin(), m_viewports.end(), d3d6Viewport);
    if (unlikely(it != m_viewports.end())) {
      Logger::warn("D3D6Device::AddViewportInternal: Pre-existing viewport found");
    } else {
      m_viewports.push_back(d3d6Viewport);
      d3d6Viewport->GetCommonViewport()->SetD3D6Device(this);
    }
  }

  inline void D3D6Device::DeleteViewportInternal(IDirect3DViewport3* viewport) {
    D3D6Viewport* d3d6Viewport = static_cast<D3D6Viewport*>(viewport);

    auto it = std::find(m_viewports.begin(), m_viewports.end(), d3d6Viewport);
    if (likely(it != m_viewports.end())) {
      m_viewports.erase(it);
       d3d6Viewport->GetCommonViewport()->SetD3D6Device(nullptr);
    } else {
      Logger::warn("D3D6Device::DeleteViewportInternal: Viewport not found");
    }
  }

  inline HRESULT D3D6Device::SetTextureWithHandle(DDraw4Surface* surface, DWORD textureHandle) {
    HRESULT hr;

    d3d9::IDirect3DDevice9* device9 = m_commonD3DDevice->GetD3D9Device();

    // Unbinding texture stages
    if (surface == nullptr) {
      hr = device9->SetTexture(0, nullptr);
      if (unlikely(FAILED(hr))) {
        Logger::err("D3D6Device::SetTextureWithHandle: Failed to unbind D3D9 texture");
        return hr;
      }

      if (likely(m_commonD3DDevice->GetCurrentTextureHandle() != 0))
        m_commonD3DDevice->SetCurrentTextureHandle(0);

      return D3D_OK;
    }

    DDrawCommonSurface* commonSurface = surface->GetCommonSurface();

    // If textures have been used on a different device, they
    // will get their D3D9 object reinitialized at this point
    if (unlikely(commonSurface->GetCommonD3DDevice() != m_commonD3DDevice.ptr()))
      commonSurface->DirtyDDrawSurface();

    hr = surface->InitializeOrUploadD3D9();
    if (unlikely(FAILED(hr))) {
      Logger::err("D3D6Device::SetTextureWithHandle: Failed to initialize/upload D3D9 texture");
      return hr;
    }

    // Don't fast skip, since color key might change
    //if (unlikely(m_commonD3DDevice->GetCurrentTextureHandle() == textureHandle))
      //return D3D_OK;

    d3d9::IDirect3DTexture9* tex9 = commonSurface->GetD3D9Texture();

    if (likely(tex9 != nullptr)) {
      hr = device9->SetTexture(0, tex9);
      if (unlikely(FAILED(hr))) {
        Logger::warn("D3D6Device::SetTextureWithHandle: Failed to bind D3D9 texture");
        return hr;
      }

      // "Any alpha values in the texture replace the alpha values in the colors that would
      //  have been used with no texturing; if the texture does not contain an alpha component,
      //  alpha values at the vertices in the source are interpolated between vertices."
      if (m_commonD3DDevice->GetTextureMapBlend() == D3DTBLEND_MODULATE && !m_alphaOpSet) {
        const DWORD textureOp = commonSurface->IsAlphaFormat() ? D3DTOP_SELECTARG1 : D3DTOP_SELECTARG2;
        device9->SetTextureStageState(0, d3d9::D3DTSS_ALPHAOP, textureOp);
      }

      const bool colorKeyEnable = m_commonD3DDevice->GetColorKeyEnable();
      const bool validColorKey = commonSurface->HasValidColorKey();
      m_bridge->SetColorKeyState(colorKeyEnable && validColorKey);
      if (colorKeyEnable && validColorKey) {
        DDCOLORKEY normalizedColorKey = commonSurface->GetColorKeyNormalized();
        m_bridge->SetColorKey(normalizedColorKey.dwColorSpaceLowValue,
                              normalizedColorKey.dwColorSpaceHighValue);
      }
    } else {
      Logger::err("D3D6Device::SetTextureWithHandle: Found no valid D3D9 texture");
    }

    m_commonD3DDevice->SetCurrentTextureHandle(textureHandle);

    return D3D_OK;
  }

}