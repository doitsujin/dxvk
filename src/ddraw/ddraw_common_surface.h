#pragma once

#include "ddraw_include.h"
#include "ddraw_format.h"

#include "ddraw_common_interface.h"
#include "d3d_common_device.h"

#include "ddraw_clipper.h"
#include "ddraw_palette.h"

namespace dxvk {

  enum class D3D9SurfaceType : uint8_t {
    None,
    BackBuffer,
    CubeTexture,
    Texture,
    DepthStencil,
    OffscreenPlainSurface,
    RenderTarget
  };

  class D3DCommonDevice;

  class DDraw7Surface;
  class DDraw4Surface;
  class DDraw3Surface;
  class DDraw2Surface;
  class DDrawSurface;

  class DDrawCommonSurface : public ComObjectClamp<IUnknown> {

  public:

    DDrawCommonSurface(DDrawCommonInterface* commonIntf);

    ~DDrawCommonSurface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) {
      *ppvObject = this;
      return S_OK;
    }

    IUnknown* GetShadowSurfaceProxied();

    DDrawCommonSurface* GetShadowCommonSurface();

    HRESULT RefreshSurfaceDescripton(const bool refreshFormat);

    void RefreshD3D9Device();

    d3d9::IDirect3DDevice9* GetRefreshedD3D9Device();

    HRESULT InitializeD3D9(const bool initRenderTarget);

    HRESULT InitializeOrUploadD3D9();

    bool IsInitialized() const {
      return m_surface9 != nullptr;
    }

    void SetCommonD3DDevice(D3DCommonDevice* commonD3DDevice) {
      m_commonD3DDevice = commonD3DDevice;
    }

    D3DCommonDevice* GetCommonD3DDevice() const {
      return m_commonD3DDevice;
    }

    DDrawCommonInterface* GetCommonInterface() const {
      return m_commonIntf.ptr();
    }

    void SetD3D9Surface(Com<d3d9::IDirect3DSurface9>&& surface9) {
      m_surface9 = surface9;
    }

    d3d9::IDirect3DSurface9* GetD3D9Surface() const {
      return m_surface9.ptr();
    }

    void SetD3D9Texture(Com<d3d9::IDirect3DTexture9>&& texture9) {
      m_texture9 = texture9;
    }

    d3d9::IDirect3DTexture9* GetD3D9Texture() const {
      return m_texture9.ptr();
    }

    void SetD3D9CubeTexture(Com<d3d9::IDirect3DCubeTexture9>&& cubeMap9) {
      m_cubeMap9 = cubeMap9;
    }

    d3d9::IDirect3DCubeTexture9* GetD3D9CubeTexture() const {
      return m_cubeMap9.ptr();
    }

    void ResetD3D9Objects() {
      m_cubeMap9 = nullptr;
      m_texture9 = nullptr;
      m_surface9 = nullptr;
      // Also reset all D3D9 related tracking flags
      m_d3d9SurfaceType = D3D9SurfaceType::None;
      m_dirtyD3D9 = false;
    }

    d3d9::D3DFORMAT GetD3D9Format() const {
      return m_format9;
    }

    D3D9SurfaceType GetD3D9SurfaceType() const {
      return m_d3d9SurfaceType;
    }

    bool IsDesc2Set() const {
      return m_isDesc2Set;
    }

    void SetDesc2(const DDSURFACEDESC2& desc2) {
      m_desc2 = desc2;
      m_isDesc2Set = true;
      RefreshStaticDescData(true);
    }

    const DDSURFACEDESC2* GetDesc2() const {
      return &m_desc2;
    }

    bool IsDescSet() const {
      return m_isDescSet;
    }

    void SetDesc(const DDSURFACEDESC& desc) {
      m_desc = desc;
      m_isDescSet = true;
      RefreshStaticDescData(true);
    }

    const DDSURFACEDESC* GetDesc() const {
      return &m_desc;
    }

    uint8_t GetColorBitCount() const {
      return (m_desc2.dwFlags & DDSD_PIXELFORMAT) ? m_desc2.ddpfPixelFormat.dwRGBBitCount :
             (m_desc.dwFlags & DDSD_PIXELFORMAT)  ? m_desc.ddpfPixelFormat.dwRGBBitCount : 0u;
    }

    bool IsAlphaFormat() const {
      return ((m_desc2.dwFlags & DDSD_PIXELFORMAT) && (m_desc2.ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS))
          || ((m_desc.dwFlags  & DDSD_PIXELFORMAT) && (m_desc.ddpfPixelFormat.dwFlags  & DDPF_ALPHAPIXELS));
    }

    bool HasValidColorKey() const {
      return (m_desc2.dwFlags & DDSD_CKSRCBLT) || (m_desc.dwFlags & DDSD_CKSRCBLT);
    }

    DDCOLORKEY GetColorKeyNormalized() const {
      const DDPIXELFORMAT* pixelFormat = (m_desc2.dwFlags & DDSD_PIXELFORMAT) ? &m_desc2.ddpfPixelFormat :
                                         (m_desc.dwFlags & DDSD_PIXELFORMAT)  ? &m_desc.ddpfPixelFormat : nullptr;
      const DDCOLORKEY*    colorKey    = (m_desc2.dwFlags & DDSD_CKSRCBLT) ? &m_desc2.ddckCKSrcBlt :
                                         (m_desc.dwFlags & DDSD_CKSRCBLT)  ? &m_desc.ddckCKSrcBlt : nullptr;

      // Empire of the Ants relies on us using the "Low" color space DWORD
      return ColorKeyToARGB(pixelFormat, colorKey != nullptr ? colorKey->dwColorSpaceLowValue : 0u);
    }

    bool IsFullSurfaceLock(const RECT* lockRect, const RECT* fullSurfaceRect) const {
      if (lockRect == nullptr) {
        if (fullSurfaceRect == nullptr)
          return true;

        lockRect = fullSurfaceRect;
      }

      return (m_rect.right  == lockRect->right  - lockRect->left) &&
             (m_rect.bottom == lockRect->bottom - lockRect->top);
    }

    const RECT* GetFullSurfaceRect() const {
      return &m_rect;
    }

    float GetNormalizedFloatDepth(DWORD input) const {
      DWORD max = m_format9 != d3d9::D3DFMT_D16 ? std::numeric_limits<uint32_t>::max()
                                                : std::numeric_limits<uint16_t>::max();
      return static_cast<float>(input) / static_cast<float>(max);
    }

    uint16_t GetMipCount() const {
      return m_mipCount;
    }

    uint32_t GetBackBufferIndex() const {
      return m_backBufferIndex;
    }

    void IncrementBackBufferIndex(uint32_t index) {
      m_backBufferIndex = index + 1;
    }

    bool IsDDrawSurfaceDirty() const {
      return m_dirtyDDraw;
    }

    void DirtyDDrawSurface() {
      m_dirtyDDraw = true;
    }

    void UnDirtyDDrawSurface() {
      m_dirtyDDraw = false;
    }

    bool IsD3D9SurfaceDirty() const {
      return m_dirtyD3D9;
    }

    void DirtyD3D9Surface() {
      m_dirtyD3D9 = true;
    }

    void UnDirtyD3D9Surface() {
      m_dirtyD3D9 = false;
    }

    void SetIsAttached(bool isAttached) {
      m_isAttached = isAttached;
    }

    bool IsAttached() const {
      return m_isAttached;
    }

    void MarkWithTextureHandle() {
      m_hasTextureHandle = true;
    }

    void SetClipper(DDrawClipper* clipper) {
      m_clipper = clipper;
    }

    DDrawClipper* GetClipper() const {
      return m_clipper.ptr();
    }

    void SetPalette(DDrawPalette* palette) {
      if (likely(m_palette != palette)) {
        if (unlikely(m_palette != nullptr))
          m_palette->SetCommonSurface(nullptr);

        m_palette = palette;

        if (likely(m_palette != nullptr))
          m_palette->SetCommonSurface(this);
      }
    }

    DDrawPalette* GetPalette() const {
      return m_palette.ptr();
    }

    void SetDD7Surface(DDraw7Surface* surf7) {
      m_surf7 = surf7;
    }

    DDraw7Surface* GetDD7Surface() const {
      return m_surf7;
    }

    void SetDD4Surface(DDraw4Surface* surf4) {
      m_surf4 = surf4;
    }

    DDraw4Surface* GetDD4Surface() const {
      return m_surf4;
    }

    void SetDD3Surface(DDraw3Surface* surf3) {
      m_surf3 = surf3;
    }

    DDraw3Surface* GetDD3Surface() const {
      return m_surf3;
    }

    void SetDD2Surface(DDraw2Surface* surf2) {
      m_surf2 = surf2;
    }

    DDraw2Surface* GetDD2Surface() const {
      return m_surf2;
    }

    void SetDDSurface(DDrawSurface* surf) {
      m_surf = surf;
    }

    DDrawSurface* GetDDSurface() const {
      return m_surf;
    }

    void SetOrigin(IUnknown* origin) {
      m_origin = origin;
    }

    IUnknown* GetOrigin() const {
      return m_origin;
    }

    bool IsComplex() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_COMPLEX
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_COMPLEX;
    }

    bool IsPrimarySurface() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_PRIMARYSURFACE;
    }

    bool IsFrontBuffer() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_FRONTBUFFER
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_FRONTBUFFER;
    }

    bool IsBackBuffer() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_BACKBUFFER;
    }

    bool IsFlippable() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_FLIP
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_FLIP;
    }

    bool IsDepthStencil() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_ZBUFFER
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_ZBUFFER;
    }

    bool IsOffScreenPlainSurface() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_OFFSCREENPLAIN;
    }

    bool IsTexture() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_TEXTURE
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_TEXTURE;
    }

    bool IsBindableAsTexture() const {
      // Surfaces which aren't explicitly marked as textures are only bindable on software devices
      const bool isBindableSurface = !m_commonD3DDevice->IsHALOrTNLHALDevice() && m_hasTextureHandle;

      return m_desc2.ddsCaps.dwCaps & DDSCAPS_TEXTURE
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_TEXTURE
          || isBindableSurface;
    }

    bool IsTextureMip() const {
      return m_desc2.ddsCaps.dwCaps  & DDSCAPS_MIPMAP
          || m_desc2.ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL
          || m_desc.ddsCaps.dwCaps   & DDSCAPS_MIPMAP;
    }

    bool IsCubeMap() const {
      return m_desc2.ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP;
    }

    bool IsOverlay() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_OVERLAY
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_OVERLAY;
    }

    bool Is3DSurface() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_3DDEVICE
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_3DDEVICE;
    }

    bool IsManaged() const {
      return m_desc2.ddsCaps.dwCaps2 & DDSCAPS2_TEXTUREMANAGE
          || m_desc2.ddsCaps.dwCaps2 & DDSCAPS2_D3DTEXTUREMANAGE;
    }

    bool IsInVideoMemory() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_VIDEOMEMORY;
    }

    bool IsInLocalVideoMemory() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_LOCALVIDMEM
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_LOCALVIDMEM;
    }

    bool IsInNonLocalVideoMemory() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_NONLOCALVIDMEM
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_NONLOCALVIDMEM;
    }

    bool IsInSystemMemory() const {
      return m_desc2.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY
          || m_desc.ddsCaps.dwCaps  & DDSCAPS_SYSTEMMEMORY;
    }

    bool IsRenderTarget() const {
      return m_isRenderTarget;
    }

    bool IsBackBufferOrFlippable() const {
      return m_isBackBufferOrFlippable;
    }

    bool IsDXTFormat() const {
      return m_format9 == d3d9::D3DFMT_DXT1
          || m_format9 == d3d9::D3DFMT_DXT2
          || m_format9 == d3d9::D3DFMT_DXT3
          || m_format9 == d3d9::D3DFMT_DXT4
          || m_format9 == d3d9::D3DFMT_DXT5;
    }

    // D3D7 is a bit more sane here, as always, so handle it separately
    HRESULT ValidateRTUsage7(bool isHALOrTNLHALDevice, bool isDeviceCreation) const {
      // Render targets require the DDSCAPS_3DDEVICE flag
      if (unlikely(!Is3DSurface())) {
        Logger::err("DDrawCommonSurface::ValidateRTUsage: Missing DDSCAPS_3DDEVICE");
        return DDERR_INVALIDCAPS;
      }
      // Depth stencil surfaces can't be set as render targets
      if (unlikely(IsDepthStencil())) {
        Logger::err("DDrawCommonSurface::ValidateRTUsage: Invalid DDSCAPS_ZBUFFER");
        return isDeviceCreation ? DDERR_INVALIDCAPS : DDERR_INVALIDPIXELFORMAT;
      }
      // DXVK doesn't support P8 render targets, so pretend these are always invalid
      if (unlikely(m_format9 == d3d9::D3DFMT_P8)) {
        Logger::err("DDrawCommonSurface::ValidateRTUsage: Invalid P8 render target");
        return isDeviceCreation ? DDERR_NOPALETTEATTACHED : DDERR_INVALIDPARAMS;
      }
      // Render targets can't be created in system memory on HAL/HAL T&L devices
      if (unlikely(IsInSystemMemory() && isHALOrTNLHALDevice)) {
        Logger::err("DDrawCommonSurface::ValidateRTUsage: Invalid DDSCAPS_SYSTEMMEMORY");
        return isDeviceCreation ? D3DERR_SURFACENOTINVIDMEM : DDERR_INVALIDPARAMS;
      }

      return DD_OK;
    }

    // D3D6 and earlier RT usage validations
    HRESULT ValidateRTUsage(bool isHALDevice, bool isDeviceCreation) const {
      // Render targets require the DDSCAPS_3DDEVICE flag
      if (unlikely(!Is3DSurface())) {
        Logger::err("DDrawCommonSurface::ValidateRTUsage: Missing DDSCAPS_3DDEVICE");
        return DDERR_INVALIDCAPS;
      }
      // Depth stencil surfaces can't be set as render targets
      if (unlikely(IsDepthStencil())) {
        Logger::err("DDrawCommonSurface::ValidateRTUsage: Invalid DDSCAPS_ZBUFFER");
        return isDeviceCreation ? DDERR_INVALIDCAPS : DDERR_INVALIDPIXELFORMAT;
      }
      // DXVK doesn't support P8 render targets, so pretend these are always invalid
      if (unlikely(m_format9 == d3d9::D3DFMT_P8)) {
        Logger::err("DDrawCommonSurface::ValidateRTUsage: Invalid P8 render target");
        return isDeviceCreation ? DDERR_NOPALETTEATTACHED : DDERR_INVALIDPARAMS;
      }
      // Render targets can't be created in system memory on HAL devices,
      // however system memory surfaces can later be set as render targets... yeah...
      if (unlikely(isDeviceCreation && IsInSystemMemory() && isHALDevice)) {
        Logger::err("DDrawCommonSurface::ValidateRTUsage: Invalid DDSCAPS_SYSTEMMEMORY");
        return D3DERR_SURFACENOTINVIDMEM;
      }

      return DD_OK;
    }

    bool SkipD3D9Operations() const {
      // Skip all D3D9 operations on P8 textures/surfaces, since DXVK doesn't support them
      if (unlikely(m_format9 == d3d9::D3DFMT_P8)) {
        static bool s_formatP8ErrorShown;

        if (!std::exchange(s_formatP8ErrorShown, true))
          Logger::warn("DDrawCommonSurface: Unsupported format D3DFMT_P8");

        return true;
      }

      return false;
    }

    void ListSurfaceDetails() const {
      const char* type = "generic surface";

      if (IsPrimarySurface())             type = "primary surface";
      else if (IsFrontBuffer())           type = "front buffer";
      else if (IsBackBufferOrFlippable()) type = "back buffer";
      else if (IsCubeMap())               type = "cube texture";
      else if (IsTextureMip())            type = "texture mipmap";
      else if (IsTexture())               type = "texture";
      else if (IsDepthStencil())          type = "depth stencil";
      else if (IsOverlay())               type = "overlay";
      else if (IsOffScreenPlainSurface()) type = "offscreen plain surface";
      else if (Is3DSurface())             type = "render target";

      const DWORD mipMapCount    = (m_desc2.dwFlags & DDSD_MIPMAPCOUNT) ? m_desc2.dwMipMapCount :
                                   (m_desc.dwFlags & DDSD_MIPMAPCOUNT) ? m_desc.dwMipMapCount : 0u;
      const DWORD backBuferCount = (m_desc2.dwFlags & DDSD_BACKBUFFERCOUNT) ? m_desc2.dwBackBufferCount :
                                   (m_desc.dwFlags & DDSD_BACKBUFFERCOUNT) ? m_desc.dwBackBufferCount : 0u;

      Logger::debug(str::format("   Type:        ", type,
                              "\n   Dimensions:  ", m_rect.right, "x", m_rect.bottom,
                              "\n   Format:      ", GetD3D9Format(),
                              "\n   IsComplex:   ", IsComplex() ? "yes" : "no",
                              "\n   IsAttached:  ", IsAttached() ? "yes" : "no"));
      if (mipMapCount)
        Logger::debug(str::format("   MipMaps:     ", mipMapCount));
      if (backBuferCount)
        Logger::debug(str::format("   BackBuffers: ", backBuferCount));
    }

  private:

    // Note: The flag check order IS important here, as some flags take
    // priority over others when considering D3D9 surface type mappings
    inline void DetermineD3D9SurfaceType(const bool initRenderTarget) {
      // Primary Surface
      if (IsPrimarySurface()) {
        m_d3d9SurfaceType = D3D9SurfaceType::BackBuffer;
      // Front Buffer
      } else if (IsFrontBuffer()) {
        m_d3d9SurfaceType = D3D9SurfaceType::BackBuffer;
      // Back Buffer
      } else if (IsBackBufferOrFlippable()) {
        m_d3d9SurfaceType = D3D9SurfaceType::BackBuffer;
      // Cube maps
      } else if (IsCubeMap()) {
        m_d3d9SurfaceType = D3D9SurfaceType::CubeTexture;
      // Textures
      } else if (IsBindableAsTexture()) {
        m_d3d9SurfaceType = D3D9SurfaceType::Texture;
      // Depth Stencil
      } else if (IsDepthStencil()) {
        m_d3d9SurfaceType = D3D9SurfaceType::DepthStencil;
      // Overlays
      } else if (unlikely(IsOverlay())) {
        m_d3d9SurfaceType = D3D9SurfaceType::OffscreenPlainSurface;
      // Offscreen Plain Surfaces
      } else if (IsOffScreenPlainSurface()) {
        if (unlikely(initRenderTarget)) {
          m_d3d9SurfaceType = D3D9SurfaceType::BackBuffer;
        } else {
          m_d3d9SurfaceType = D3D9SurfaceType::OffscreenPlainSurface;
        }
      // Generic render target
      } else if (Is3DSurface()) {
        m_d3d9SurfaceType = D3D9SurfaceType::RenderTarget;
      // We sometimes get generic surfaces, with only dimensions, format and placement info
      } else {
        m_d3d9SurfaceType = D3D9SurfaceType::OffscreenPlainSurface;
      }
    }

    inline void RefreshStaticDescData(const bool refreshFormat) {
      // determine and cache various frequently used flag combinations
      m_isRenderTarget          = IsFrontBuffer() || IsBackBuffer() || IsFlippable() || Is3DSurface();
      m_isBackBufferOrFlippable = !IsPrimarySurface() && !IsFrontBuffer() && (IsBackBuffer() || IsFlippable());
      // refresh static values such as the full surface rect and format
      m_rect.right  = (m_desc2.dwFlags & DDSD_WIDTH) ? m_desc2.dwWidth :
                      (m_desc.dwFlags & DDSD_WIDTH) ? m_desc.dwWidth  : 0;
      m_rect.bottom = (m_desc2.dwFlags & DDSD_HEIGHT) ? m_desc2.dwHeight :
                      (m_desc.dwFlags & DDSD_HEIGHT) ? m_desc.dwHeight : 0;
      if (refreshFormat) {
        const d3d9::D3DFORMAT format9 = ConvertFormat((m_desc2.dwFlags & DDSD_PIXELFORMAT) ? m_desc2.ddpfPixelFormat : m_desc.ddpfPixelFormat);
        // warn if a format change is detected on an already initialized surface
        if (unlikely(m_surface9 != nullptr && m_format9 != format9))
          Logger::warn("DDrawCommonSurface::RefreshStaticDescData: Surface format has changed post initialization");
        m_format9 = format9;
      }
    }

    bool                             m_dirtyDDraw         = false;
    bool                             m_dirtyD3D9          = false;

    bool                             m_isDesc2Set         = false;
    bool                             m_isDescSet          = false;
    bool                             m_hasTextureHandle   = false;

    bool                             m_isAttached         = false;
    bool                             m_isRenderTarget     = false;
    bool                             m_isBackBufferOrFlippable = false;

    Com<DDrawCommonInterface>        m_commonIntf;

    D3DCommonDevice*                 m_commonD3DDevice    = nullptr;

    uint16_t                         m_mipCount           = 1;
    uint32_t                         m_backBufferIndex    = 0;

    DDSURFACEDESC                    m_desc               = { };
    DDSURFACEDESC2                   m_desc2              = { };
    D3D9SurfaceType                  m_d3d9SurfaceType    = D3D9SurfaceType::None;
    RECT                             m_rect               = { };

    Com<DDrawClipper>                m_clipper;
    Com<DDrawPalette>                m_palette;

    Com<d3d9::IDirect3DSurface9>     m_surface9;
    Com<d3d9::IDirect3DTexture9>     m_texture9;
    Com<d3d9::IDirect3DCubeTexture9> m_cubeMap9;

    d3d9::D3DFORMAT                  m_format9            = d3d9::D3DFMT_UNKNOWN;

    DDraw7Surface*                   m_surf7              = nullptr;
    DDraw4Surface*                   m_surf4              = nullptr;
    DDraw3Surface*                   m_surf3              = nullptr;
    DDraw2Surface*                   m_surf2              = nullptr;
    DDrawSurface*                    m_surf               = nullptr;

    // Track the origin surface, as in the DDraw surface
    // that gets created through a CreateSurface call
    IUnknown*                        m_origin             = nullptr;

  };

}