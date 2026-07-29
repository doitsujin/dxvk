#include "ddraw_common_surface.h"

#include "d3d_common_device.h"

#include "ddraw7/ddraw7_surface.h"
#include "ddraw4/ddraw4_surface.h"
#include "ddraw2/ddraw3_surface.h"
#include "ddraw2/ddraw2_surface.h"
#include "ddraw/ddraw_surface.h"

namespace dxvk {

  DDrawCommonSurface::DDrawCommonSurface(DDrawCommonInterface* commonIntf)
    : m_commonIntf ( commonIntf ) {
  }

  DDrawCommonSurface::~DDrawCommonSurface() {
    if (unlikely(IsPrimarySurface() && m_commonIntf->GetPrimarySurface() == this))
      m_commonIntf->SetPrimarySurface(nullptr);

    if (unlikely(m_palette != nullptr))
      m_palette->SetCommonSurface(nullptr);
  }

  IUnknown* DDrawCommonSurface::GetShadowSurfaceProxied() {
    if (m_surf7 != nullptr) {
      DDraw7Surface* shadow7 = m_surf7->GetShadowSurface();
      if (shadow7 != nullptr)
        return shadow7->GetProxied();
    }
    if (m_surf4 != nullptr) {
      DDraw4Surface* shadow4 = m_surf4->GetShadowSurface();
      if (shadow4 != nullptr)
        return shadow4->GetProxied();
    }
    if (m_surf3 != nullptr) {
      DDraw3Surface* shadow3 = m_surf3->GetShadowSurface();
      if (shadow3 != nullptr)
        return shadow3->GetProxied();
    }
    if (m_surf2 != nullptr) {
      DDraw2Surface* shadow2 = m_surf2->GetShadowSurface();
      if (shadow2 != nullptr)
        return shadow2->GetProxied();
    }
    if (m_surf != nullptr) {
      DDrawSurface* shadow = m_surf->GetShadowSurface();
      if (shadow != nullptr)
        return shadow->GetProxied();
    }

    return nullptr;
  }

  DDrawCommonSurface* DDrawCommonSurface::GetShadowCommonSurface() {
    if (m_surf7 != nullptr) {
      DDraw7Surface* shadow7 = m_surf7->GetShadowSurface();
      if (shadow7 != nullptr)
        return shadow7->GetCommonSurface();
    }
    if (m_surf4 != nullptr) {
      DDraw4Surface* shadow4 = m_surf4->GetShadowSurface();
      if (shadow4 != nullptr)
        return shadow4->GetCommonSurface();
    }
    if (m_surf3 != nullptr) {
      DDraw3Surface* shadow3 = m_surf3->GetShadowSurface();
      if (shadow3 != nullptr)
        return shadow3->GetCommonSurface();
    }
    if (m_surf2 != nullptr) {
      DDraw2Surface* shadow2 = m_surf2->GetShadowSurface();
      if (shadow2 != nullptr)
        return shadow2->GetCommonSurface();
    }
    if (m_surf != nullptr) {
      DDrawSurface* shadow = m_surf->GetShadowSurface();
      if (shadow != nullptr)
        return shadow->GetCommonSurface();
    }

    return nullptr;
  }

  HRESULT DDrawCommonSurface::RefreshSurfaceDescripton(const bool refreshFormat) {
    HRESULT hr;

    DDSURFACEDESC2 desc2;
    desc2.dwSize = sizeof(DDSURFACEDESC2);

    if (m_surf7 != nullptr) {
      hr = m_surf7->GetProxied()->GetSurfaceDesc(&desc2);
      if (unlikely(FAILED(hr)))
        return hr;
      m_desc2 = desc2;
      RefreshStaticDescData(refreshFormat);
    } else if (m_surf4 != nullptr) {
      hr = m_surf4->GetProxied()->GetSurfaceDesc(&desc2);
      if (unlikely(FAILED(hr)))
        return hr;
      m_desc2 = desc2;
      RefreshStaticDescData(refreshFormat);
    }

    DDSURFACEDESC desc;
    desc.dwSize = sizeof(DDSURFACEDESC);

    if (m_surf != nullptr) {
      hr = m_surf->GetProxied()->GetSurfaceDesc(&desc);
      if (unlikely(FAILED(hr)))
        return hr;
      m_desc = desc;
      RefreshStaticDescData(refreshFormat);
    } else if (m_surf2 != nullptr) {
      hr = m_surf2->GetProxied()->GetSurfaceDesc(&desc);
      if (unlikely(FAILED(hr)))
        return hr;
      m_desc = desc;
      RefreshStaticDescData(refreshFormat);
    } else if (m_surf3 != nullptr) {
      hr = m_surf3->GetProxied()->GetSurfaceDesc(&desc);
      if (unlikely(FAILED(hr)))
        return hr;
      m_desc = desc;
      RefreshStaticDescData(refreshFormat);
    }

    return DD_OK;
  }

  void DDrawCommonSurface::RefreshD3D9Device() {
    D3DCommonDevice* commonD3DDevice = m_commonIntf->GetCommonD3DDevice();

    if (unlikely(m_commonD3DDevice != commonD3DDevice)) {
      // Check if the device has been recreated and reset all D3D9 resources
      if (m_commonD3DDevice != nullptr) {
        Logger::debug("DDrawCommonSurface: Device has changed, clearing all D3D9 resources");
        m_cubeMap9 = nullptr;
        m_texture9 = nullptr;
        m_surface9 = nullptr;
        // Also reset all D3D9 related tracking flags
        m_isD3D9BackBuffer = false;
        m_isD3D9DepthStencil = false;
        m_dirtyD3D9 = false;
      }

      m_commonD3DDevice = commonD3DDevice;
    }
  }

  d3d9::IDirect3DDevice9* DDrawCommonSurface::GetRefreshedD3D9Device() {
    RefreshD3D9Device();

    if (likely(m_commonD3DDevice != nullptr))
      return m_commonD3DDevice->GetD3D9Device();

    return nullptr;
  }

  HRESULT DDrawCommonSurface::InitializeD3D9(const bool initRenderTarget) {
    const DWORD dwWidth  = static_cast<DWORD>(m_rect.right);
    const DWORD dwHeight = static_cast<DWORD>(m_rect.bottom);

    if (unlikely(dwWidth == 0 || dwHeight == 0)) {
      Logger::err("DDrawCommonSurface::InitializeD3D9: Surface has 0 height or width");
      return DDERR_UNSUPPORTED;
    }

    if (unlikely(m_format9 == d3d9::D3DFMT_UNKNOWN)) {
      Logger::err("DDrawCommonSurface::InitializeD3D9: Surface has an unknown format");
      return DDERR_UNSUPPORTED;
    }

    // Don't initialize P8 textures/surfaces since we don't support them.
    // Some applications do require them to be created by ddraw, otherwise
    // they will simply fail to start, so just ignore them for now.
    if (unlikely(m_format9 == d3d9::D3DFMT_P8)) {
      static bool s_formatP8ErrorShown;

      if (!std::exchange(s_formatP8ErrorShown, true))
        Logger::warn("DDrawCommonSurface::InitializeD3D9: Unsupported format D3DFMT_P8");

      return DD_OK;
    // Similarly, D3DFMT_R3G3B2 isn't supported by D3D9 dxvk, however some
    // applications require it to be supported by ddraw, even if they do not
    // use it. Simply ignore any D3DFMT_R3G3B2 textures/surfaces for now.
    } else if (unlikely(m_format9 == d3d9::D3DFMT_R3G3B2)) {
      static bool s_formatR3G3B2ErrorShown;

      if (!std::exchange(s_formatR3G3B2ErrorShown, true))
        Logger::warn("DDrawCommonSurface::InitializeD3D9: Unsupported format D3DFMT_R3G3B2");

      return DD_OK;
    }

    d3d9::D3DPOOL pool;
    DWORD         usage = 0u;

    // General surface/texture pool placement
    //
    // Early DDraw/D3D didn't make the distinction between local and
    // non-local video memory, so also cater to sole DDSCAPS_VIDEOMEMORY surfaces
    if (IsInLocalVideoMemory() || (IsInVideoMemory() && !IsInNonLocalVideoMemory()))
      pool = d3d9::D3DPOOL_DEFAULT;
    // There's no explicit non-local video memory placement
    // per se in D3D9, but D3DPOOL_MANAGED is close enough
    else if (IsManaged() || IsInNonLocalVideoMemory())
      pool = d3d9::D3DPOOL_MANAGED;
    else if (IsInSystemMemory())
      // We can't know beforehand if a texture is or isn't going to be
      // used in SetTexture() calls, and textures placed in D3DPOOL_SYSTEMMEM
      // will not work in that context, so revert to D3DPOOL_MANAGED
      pool = IsTextureOrCubeMap() ? d3d9::D3DPOOL_MANAGED : d3d9::D3DPOOL_SYSTEMMEM;
    else
      pool = d3d9::D3DPOOL_DEFAULT;

    // Place all possible render targets in DEFAULT
    //
    // Note: This is somewhat problematic for textures and cube maps
    // which will have D3DUSAGE_RENDERTARGET, but also need to have
    // D3DUSAGE_DYNAMIC for locking/uploads to work. The flag combination
    // isn't supported in D3D9, but we have a D3D7 exception in place.
    //
    if (IsRenderTarget() || initRenderTarget) {
      //Logger::debug("DDrawCommonSurface::InitializeD3D9: Usage: D3DUSAGE_RENDERTARGET");
      pool  = d3d9::D3DPOOL_DEFAULT;
      usage |= D3DUSAGE_RENDERTARGET;
    }
    // All depth stencils will be created in DEFAULT
    if (IsDepthStencil()) {
      //Logger::debug("DDrawCommonSurface::InitializeD3D9: Usage: D3DUSAGE_DEPTHSTENCIL");
      pool  = d3d9::D3DPOOL_DEFAULT;
      usage |= D3DUSAGE_DEPTHSTENCIL;
    }

    // General usage flags
    if (IsTextureOrCubeMap()) {
      // Needed to ensure D3DPOOL_DEFAULT textures/cubemaps are lockable
      if (pool == d3d9::D3DPOOL_DEFAULT) {
        //Logger::debug("DDrawCommonSurface::InitializeD3D9: Usage: D3DUSAGE_DYNAMIC");
        usage |= D3DUSAGE_DYNAMIC;
      }
      if (unlikely(m_commonIntf->GetOptions()->autoGenMipMaps)) {
        //Logger::debug("DDrawCommonSurface::InitializeD3D9: Usage: D3DUSAGE_AUTOGENMIPMAP");
        usage |= D3DUSAGE_AUTOGENMIPMAP;
      }
    }

    // Use the MSAA type that was determined to be supported during device creation
    const d3d9::D3DMULTISAMPLE_TYPE multiSampleType = m_commonD3DDevice->GetMultiSampleType();
    d3d9::IDirect3DDevice9* d3d9Device = m_commonD3DDevice->GetD3D9Device();

    HRESULT hr = DDERR_GENERIC;

    // Primary Surface
    if (IsPrimarySurface()) {
      hr = d3d9Device->GetBackBuffer(0, m_backBufferIndex, d3d9::D3DBACKBUFFER_TYPE_MONO, &m_surface9);

      if (unlikely(unlikely(FAILED(hr)))) {
        Logger::err("DDrawCommonSurface::InitializeD3D9: Failed to retrieve primary surface");
        return hr;
      }

      MarkAsD3D9BackBuffer();
    // Front Buffer
    } else if (IsFrontBuffer()) {
      hr = d3d9Device->GetBackBuffer(0, m_backBufferIndex, d3d9::D3DBACKBUFFER_TYPE_MONO, &m_surface9);

      if (unlikely(unlikely(FAILED(hr)))) {
        Logger::err("DDrawCommonSurface::InitializeD3D9: Failed to retrieve front buffer");
        return hr;
      }

      MarkAsD3D9BackBuffer();
    // Back Buffer
    } else if (IsBackBufferOrFlippable()) {
      hr = d3d9Device->GetBackBuffer(0, m_backBufferIndex, d3d9::D3DBACKBUFFER_TYPE_MONO, &m_surface9);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawCommonSurface::InitializeD3D9: Failed to retrieve back buffer");
        return hr;
      }

      MarkAsD3D9BackBuffer();
    // Cube maps
    } else if (IsCubeMap()) {
      hr = d3d9Device->CreateCubeTexture(
        dwWidth, m_mipCount, usage,
        m_format9, pool, &m_cubeMap9, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawCommonSurface::InitializeD3D9: Failed to create cube map");
        return hr;
      }

      // Always attach the positive X face to this surface
      m_cubeMap9->GetCubeMapSurface(d3d9::D3DCUBEMAP_FACE_POSITIVE_X, 0, &m_surface9);
    // Textures
    } else if (IsTexture()) {
      hr = d3d9Device->CreateTexture(
        dwWidth, dwHeight, m_mipCount, usage,
        m_format9, pool, &m_texture9, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawCommonSurface::InitializeD3D9: Failed to create texture");
        return hr;
      }

      // Attach level 0 to this surface
      m_texture9->GetSurfaceLevel(0, &m_surface9);
    // Depth Stencil
    } else if (IsDepthStencil()) {
      hr = d3d9Device->CreateDepthStencilSurface(
        dwWidth, dwHeight, m_format9,
        multiSampleType, 0, FALSE, &m_surface9, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawCommonSurface::InitializeD3D9: Failed to create depth stencil");
        return hr;
      }
    // Overlays
    } else if (unlikely(IsOverlay())) {
      hr = d3d9Device->CreateOffscreenPlainSurface(
        dwWidth, dwHeight, m_format9,
        pool, &m_surface9, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawCommonSurface::InitializeD3D9: Failed to create offscreen plain surface");
        return hr;
      }
    // Offscreen Plain Surfaces
    } else if (IsOffScreenPlainSurface()) {
      if (unlikely(initRenderTarget)) {
        hr = d3d9Device->GetBackBuffer(0, m_backBufferIndex, d3d9::D3DBACKBUFFER_TYPE_MONO, &m_surface9);

        if (unlikely(unlikely(FAILED(hr)))) {
          Logger::err("DDrawCommonSurface::InitializeD3D9: Failed to retrieve offscreen plain surface");
          return hr;
        }

        MarkAsD3D9BackBuffer();

      } else {
        hr = d3d9Device->CreateOffscreenPlainSurface(
          dwWidth, dwHeight, m_format9,
          pool, &m_surface9, nullptr);

        if (unlikely(FAILED(hr))) {
          Logger::err("DDrawCommonSurface::InitializeD3D9: Failed to create offscreen plain surface");
          return hr;
        }
      }
    // Generic render target
    } else if (Is3DSurface()) {
      // Must be lockable for blitting to work. Note that
      // D3D9 does not allow the creation of lockable RTs when
      // using MSAA, but we have a D3D7 exception in place.
      hr = d3d9Device->CreateRenderTarget(
        dwWidth, dwHeight, m_format9,
        multiSampleType, usage, TRUE, &m_surface9, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawCommonSurface::InitializeD3D9: Failed to create render target");
        return hr;
      }
    // We sometimes get generic surfaces, with only dimensions, format and placement info
    } else {
      hr = d3d9Device->CreateOffscreenPlainSurface(
          dwWidth, dwHeight, m_format9,
          pool, &m_surface9, nullptr);

      if (unlikely(FAILED(hr))) {
        Logger::err("DDrawCommonSurface::InitializeD3D9: Failed to create offscreen plain surface");
        return hr;
      }
    }

    return DD_OK;
  }

  HRESULT DDrawCommonSurface::InitializeOrUploadD3D9() {
    if (m_surf7 != nullptr) {
      return m_surf7->InitializeOrUploadD3D9();
    }
    if (m_surf4 != nullptr) {
      return m_surf4->InitializeOrUploadD3D9();
    }
    if (m_surf3 != nullptr) {
      return m_surf3->InitializeOrUploadD3D9();
    }
    if (m_surf2 != nullptr) {
      return m_surf2->InitializeOrUploadD3D9();
    }
    if (m_surf != nullptr) {
      return m_surf->InitializeOrUploadD3D9();
    }

    return DDERR_GENERIC;
  }

}