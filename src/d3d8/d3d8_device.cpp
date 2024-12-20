#include "d3d8_device.h"
#include "d3d8_interface.h"
#include "d3d8_shader.h"

#ifdef MSC_VER
#pragma fenv_access (on)
#endif

namespace dxvk {

  static constexpr DWORD isFVF(DWORD Handle) {
    return (Handle & D3DFVF_RESERVED0) == 0;
  }

  static constexpr DWORD getShaderHandle(DWORD Index) {
    return (Index << 1) | D3DFVF_RESERVED0;
  }

  static constexpr DWORD getShaderIndex(DWORD Handle) {
    if ((Handle & D3DFVF_RESERVED0) != 0) {
      return ((Handle & ~(D3DFVF_RESERVED0)) >> 1) - 1;
    } else {
      return Handle;
    }
  }

  struct D3D8VertexShaderInfo {
    Com<d3d9::IDirect3DVertexDeclaration9>  pVertexDecl;
    Com<d3d9::IDirect3DVertexShader9>       pVertexShader;
    std::vector<DWORD>                      declaration;
    std::vector<DWORD>                      function;
  };

  D3D8Device::D3D8Device(
    D3D8Interface*                pParent,
    Com<d3d9::IDirect3DDevice9>&& pDevice,
    D3DDEVTYPE                    DeviceType,
    HWND                          hFocusWindow,
    DWORD                         BehaviorFlags,
    D3DPRESENT_PARAMETERS*        pParams)
    : D3D8DeviceBase(std::move(pDevice))
    , m_d3d8Options(pParent->GetOptions())
    , m_parent(pParent)
    , m_presentParams(*pParams)
    , m_deviceType(DeviceType)
    , m_window(hFocusWindow)
    , m_behaviorFlags(BehaviorFlags)
    , m_multithread(BehaviorFlags & D3DCREATE_MULTITHREADED) {
    // Get the bridge interface to D3D9.
    if (FAILED(GetD3D9()->QueryInterface(__uuidof(IDxvkD3D8Bridge), (void**)&m_bridge))) {
      throw DxvkError("D3D8Device: ERROR! Failed to get D3D9 Bridge. d3d9.dll might not be DXVK!");
    }

    m_bridge->SetAPIName("D3D8");

    ResetState();
    RecreateBackBuffersAndAutoDepthStencil();
    
    if (m_d3d8Options.batching)
      m_batcher = new D3D8Batcher(this, GetD3D9());
  }

  D3D8Device::~D3D8Device() {
    if (m_batcher)
      delete m_batcher;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetInfo(DWORD DevInfoID, void* pDevInfoStruct, DWORD DevInfoStructSize) {
    Logger::debug(str::format("D3D8Device::GetInfo: ", DevInfoID));

    if (unlikely(pDevInfoStruct == nullptr || DevInfoStructSize == 0))
      return D3DERR_INVALIDCALL;

    HRESULT res;
    Com<d3d9::IDirect3DQuery9> pQuery;
    
    switch (DevInfoID) {
      // pre-D3D8 queries
      case 0:
      case D3DDEVINFOID_TEXTUREMANAGER:
      case D3DDEVINFOID_D3DTEXTUREMANAGER:
      case D3DDEVINFOID_TEXTURING:
        return E_FAIL;
      
      case D3DDEVINFOID_VCACHE:
        // The query will return D3D_OK on Nvidia and D3DERR_NOTAVAILABLE on AMD/Intel
        // in D3D9, however in the case of the latter we'll need to return a
        // zeroed out query result and S_FALSE. This behavior has been observed both
        // on modern native AMD drivers and D3D8-era native ATI drivers.
        res = GetD3D9()->CreateQuery(d3d9::D3DQUERYTYPE_VCACHE, &pQuery);

        struct D3DDEVINFO_VCACHE {
          DWORD         Pattern;
          DWORD         OptMethod;
          DWORD         CacheSize;
          DWORD         MagicNumber;
        };

        if (FAILED(res)) {
          // The struct size needs to be at least equal or larger
          if (DevInfoStructSize < sizeof(D3DDEVINFO_VCACHE))
            return D3DERR_INVALIDCALL;

          memset(pDevInfoStruct, 0, sizeof(D3DDEVINFO_VCACHE));
          return S_FALSE;
        }

        break;

      case D3DDEVINFOID_RESOURCEMANAGER: // Not yet implemented by D9VK.
        res = GetD3D9()->CreateQuery(d3d9::D3DQUERYTYPE_RESOURCEMANAGER, &pQuery);
        break;

      case D3DDEVINFOID_VERTEXSTATS: // Not yet implemented by D9VK.
        res = GetD3D9()->CreateQuery(d3d9::D3DQUERYTYPE_VERTEXSTATS, &pQuery);
        break;

      default:
        Logger::warn(str::format("D3D8Device::GetInfo: Unsupported device info ID: ", DevInfoID));
        return E_FAIL;
    }

    if (FAILED(res)) {
      if (res == D3DERR_NOTAVAILABLE) // unsupported
        return E_FAIL;
      else // any unknown error
        return S_FALSE;
    }

    if (pQuery != nullptr) {
      // Immediately issue the query. D3D9 will begin it automatically before ending.
      pQuery->Issue(D3DISSUE_END);
      // TODO: Will immediately issuing the query actually yield meaingful results?
      // Only relevant once RESOURCEMANAGER or VERTEXSTATS are implemented by D9VK,
      // since VCACHE queries will immediately return data during this call.
      res = pQuery->GetData(pDevInfoStruct, DevInfoStructSize, D3DGETDATA_FLUSH);
    }

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::TestCooperativeLevel() {
    // Equivalent of D3D11/DXGI present tests.
    return GetD3D9()->TestCooperativeLevel();
  }

  UINT STDMETHODCALLTYPE D3D8Device::GetAvailableTextureMem() {
    return GetD3D9()->GetAvailableTextureMem();
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::ResourceManagerDiscardBytes(DWORD bytes) { 
    return GetD3D9()->EvictManagedResources();
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetDirect3D(IDirect3D8** ppD3D8) {
    if (ppD3D8 == nullptr)
      return D3DERR_INVALIDCALL;

    *ppD3D8 = m_parent.ref();
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetDeviceCaps(D3DCAPS8* pCaps) {
    d3d9::D3DCAPS9 caps9;
    HRESULT res = GetD3D9()->GetDeviceCaps(&caps9);

    if (likely(SUCCEEDED(res)))
      dxvk::ConvertCaps8(caps9, pCaps);

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetDisplayMode(D3DDISPLAYMODE* pMode) {
    // swap chain 0
    return GetD3D9()->GetDisplayMode(0, (d3d9::D3DDISPLAYMODE*)pMode);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters) {
    return GetD3D9()->GetCreationParameters((d3d9::D3DDEVICE_CREATION_PARAMETERS*)pParameters);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetCursorProperties(
          UINT               XHotSpot,
          UINT               YHotSpot,
          IDirect3DSurface8* pCursorBitmap) {
    D3D8Surface* surf = static_cast<D3D8Surface*>(pCursorBitmap);
    return GetD3D9()->SetCursorProperties(XHotSpot, YHotSpot, D3D8Surface::GetD3D9Nullable(surf));
  }

  void    STDMETHODCALLTYPE D3D8Device::SetCursorPosition(UINT XScreenSpace, UINT YScreenSpace, DWORD Flags) {
    GetD3D9()->SetCursorPosition(XScreenSpace, YScreenSpace, Flags);
  }

  // Microsoft d3d8.h in the DirectX 9 SDK uses a different function signature...
  void    STDMETHODCALLTYPE D3D8Device::SetCursorPosition(int X, int Y, DWORD Flags) {
    GetD3D9()->SetCursorPosition(X, Y, Flags);
  }

  BOOL    STDMETHODCALLTYPE D3D8Device::ShowCursor(BOOL bShow) {
    return GetD3D9()->ShowCursor(bShow);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::CreateAdditionalSwapChain(
      D3DPRESENT_PARAMETERS* pPresentationParameters,
      IDirect3DSwapChain8** ppSwapChain) {
    InitReturnPtr(ppSwapChain);

    if (unlikely(pPresentationParameters == nullptr || ppSwapChain == nullptr))
      return D3DERR_INVALIDCALL;
    
    Com<d3d9::IDirect3DSwapChain9> pSwapChain9;
    d3d9::D3DPRESENT_PARAMETERS params = ConvertPresentParameters9(pPresentationParameters);
    HRESULT res = GetD3D9()->CreateAdditionalSwapChain(
      &params,
      &pSwapChain9
    );

    if (likely(SUCCEEDED(res)))
      *ppSwapChain = ref(new D3D8SwapChain(this, pPresentationParameters, std::move(pSwapChain9)));

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    D3D8DeviceLock lock = LockDevice();

    StateChange();

    if (unlikely(pPresentationParameters == nullptr))
      return D3DERR_INVALIDCALL;

    // D3DSWAPEFFECT_COPY can not be used with more than one back buffer.
    // This is also technically true for D3DSWAPEFFECT_COPY_VSYNC, however
    // RC Cars depends on it not being rejected.
    if (unlikely(pPresentationParameters->SwapEffect == D3DSWAPEFFECT_COPY
              && pPresentationParameters->BackBufferCount > 1))
      return D3DERR_INVALIDCALL;

    m_presentParams = *pPresentationParameters;
    ResetState();

    d3d9::D3DPRESENT_PARAMETERS params = ConvertPresentParameters9(pPresentationParameters);
    HRESULT res = GetD3D9()->Reset(&params);

    if (likely(SUCCEEDED(res)))
      RecreateBackBuffersAndAutoDepthStencil();

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::Present(
    const RECT* pSourceRect,
    const RECT* pDestRect,
          HWND hDestWindowOverride,
    const RGNDATA* pDirtyRegion) {
    D3D8DeviceLock lock = LockDevice();

    m_batcher->EndFrame();
    StateChange();
    return GetD3D9()->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetBackBuffer(
          UINT iBackBuffer,
          D3DBACKBUFFER_TYPE Type,
          IDirect3DSurface8** ppBackBuffer) {
    D3D8DeviceLock lock = LockDevice();

    InitReturnPtr(ppBackBuffer);

    if (unlikely(ppBackBuffer == nullptr))
      return D3DERR_INVALIDCALL;

    if (iBackBuffer >= m_backBuffers.size() || m_backBuffers[iBackBuffer] == nullptr) {
      Com<d3d9::IDirect3DSurface9> pSurface9;
      HRESULT res = GetD3D9()->GetBackBuffer(0, iBackBuffer, (d3d9::D3DBACKBUFFER_TYPE)Type, &pSurface9);

      if (likely(SUCCEEDED(res))) {
        m_backBuffers[iBackBuffer] = new D3D8Surface(this, std::move(pSurface9));
        *ppBackBuffer = m_backBuffers[iBackBuffer].ref();
      }

      return res;
    }

    *ppBackBuffer = m_backBuffers[iBackBuffer].ref();
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetRasterStatus(D3DRASTER_STATUS* pRasterStatus) {
    return GetD3D9()->GetRasterStatus(0, reinterpret_cast<d3d9::D3DRASTER_STATUS*>(pRasterStatus));
  }

  void STDMETHODCALLTYPE D3D8Device::SetGammaRamp(DWORD Flags, const D3DGAMMARAMP* pRamp) {
    StateChange();
    // For swap chain 0
    GetD3D9()->SetGammaRamp(0, Flags, reinterpret_cast<const d3d9::D3DGAMMARAMP*>(pRamp));
  }

  void STDMETHODCALLTYPE D3D8Device::GetGammaRamp(D3DGAMMARAMP* pRamp) {
    // For swap chain 0
    GetD3D9()->GetGammaRamp(0, reinterpret_cast<d3d9::D3DGAMMARAMP*>(pRamp));
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::CreateTexture(
          UINT                Width,
          UINT                Height,
          UINT                Levels,
          DWORD               Usage,
          D3DFORMAT           Format,
          D3DPOOL             Pool,
          IDirect3DTexture8** ppTexture) {
    // D3D8 returns D3DERR_INVALIDCALL for D3DFMT_UNKNOWN
    // before clearing the content of ppTexture.
    if (unlikely(Format == D3DFMT_UNKNOWN))
      return D3DERR_INVALIDCALL;

    InitReturnPtr(ppTexture);

    if (unlikely(ppTexture == nullptr))
      return D3DERR_INVALIDCALL;

    // Nvidia & Intel workaround for The Lord of the Rings: The Fellowship of the Ring
    if (m_d3d8Options.placeP8InScratch && Format == D3DFMT_P8)
      Pool = D3DPOOL_SCRATCH;

    Com<d3d9::IDirect3DTexture9> pTex9 = nullptr;
    HRESULT res = GetD3D9()->CreateTexture(
      Width,
      Height,
      Levels,
      Usage,
      d3d9::D3DFORMAT(Format),
      d3d9::D3DPOOL(Pool),
      &pTex9,
      NULL);

    if (likely(SUCCEEDED(res)))
      *ppTexture = ref(new D3D8Texture2D(this, std::move(pTex9)));

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::CreateVolumeTexture(
          UINT                      Width,
          UINT                      Height,
          UINT                      Depth,
          UINT                      Levels,
          DWORD                     Usage,
          D3DFORMAT                 Format,
          D3DPOOL                   Pool,
          IDirect3DVolumeTexture8** ppVolumeTexture) {
    // D3D8 returns D3DERR_INVALIDCALL for D3DFMT_UNKNOWN
    // before clearing the content of ppVolumeTexture.
    if (unlikely(Format == D3DFMT_UNKNOWN))
      return D3DERR_INVALIDCALL;

    InitReturnPtr(ppVolumeTexture);

    if (unlikely(ppVolumeTexture == nullptr))
      return D3DERR_INVALIDCALL;

    Com<d3d9::IDirect3DVolumeTexture9> pVolume9 = nullptr;
    HRESULT res = GetD3D9()->CreateVolumeTexture(
      Width, Height, Depth, Levels,
      Usage,
      d3d9::D3DFORMAT(Format),
      d3d9::D3DPOOL(Pool),
      &pVolume9,
      NULL);

    if (likely(SUCCEEDED(res)))
      *ppVolumeTexture = ref(new D3D8Texture3D(this, std::move(pVolume9)));

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::CreateCubeTexture(
        UINT                      EdgeLength,
          UINT                    Levels,
          DWORD                   Usage,
          D3DFORMAT               Format,
          D3DPOOL                 Pool,
          IDirect3DCubeTexture8** ppCubeTexture) {
    // D3D8 returns D3DERR_INVALIDCALL for D3DFMT_UNKNOWN
    // before clearing the content of ppCubeTexture.
    if (unlikely(Format == D3DFMT_UNKNOWN))
      return D3DERR_INVALIDCALL;

    InitReturnPtr(ppCubeTexture);

    if (unlikely(ppCubeTexture == nullptr))
      return D3DERR_INVALIDCALL;

    Com<d3d9::IDirect3DCubeTexture9> pCube9 = nullptr;
    HRESULT res = GetD3D9()->CreateCubeTexture(
      EdgeLength,
      Levels,
      Usage,
      d3d9::D3DFORMAT(Format),
      d3d9::D3DPOOL(Pool),
      &pCube9,
      NULL);

    if (likely(SUCCEEDED(res)))
      *ppCubeTexture = ref(new D3D8TextureCube(this, std::move(pCube9)));

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::CreateVertexBuffer(
          UINT                     Length,
          DWORD                    Usage,
          DWORD                    FVF,
          D3DPOOL                  Pool,
          IDirect3DVertexBuffer8** ppVertexBuffer) {
    InitReturnPtr(ppVertexBuffer);

    if (unlikely(ppVertexBuffer == nullptr))
      return D3DERR_INVALIDCALL;

    if (ShouldBatch()) {
      *ppVertexBuffer = m_batcher->CreateVertexBuffer(Length, Usage, FVF, Pool);
      return D3D_OK;
    }

    Com<d3d9::IDirect3DVertexBuffer9> pVertexBuffer9 = nullptr;
    HRESULT res = GetD3D9()->CreateVertexBuffer(Length, Usage, FVF, d3d9::D3DPOOL(Pool), &pVertexBuffer9, NULL);

    if (likely(SUCCEEDED(res)))
      *ppVertexBuffer = ref(new D3D8VertexBuffer(this, std::move(pVertexBuffer9), Pool, Usage));
  
    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::CreateIndexBuffer(
          UINT                    Length,
          DWORD                   Usage,
          D3DFORMAT               Format,
          D3DPOOL                 Pool,
          IDirect3DIndexBuffer8** ppIndexBuffer) {
    InitReturnPtr(ppIndexBuffer);

    if (unlikely(ppIndexBuffer == nullptr))
      return D3DERR_INVALIDCALL;

    Com<d3d9::IDirect3DIndexBuffer9> pIndexBuffer9 = nullptr;
    HRESULT res = GetD3D9()->CreateIndexBuffer(Length, Usage, d3d9::D3DFORMAT(Format), d3d9::D3DPOOL(Pool), &pIndexBuffer9, NULL);
    
    if (likely(SUCCEEDED(res)))
      *ppIndexBuffer = ref(new D3D8IndexBuffer(this, std::move(pIndexBuffer9), Pool, Usage));
    
    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::CreateRenderTarget(
          UINT                Width,
          UINT                Height,
          D3DFORMAT           Format,
          D3DMULTISAMPLE_TYPE MultiSample,
          BOOL                Lockable,
          IDirect3DSurface8** ppSurface) {
    // D3D8 returns D3DERR_INVALIDCALL for D3DFMT_UNKNOWN
    // before clearing the content of ppSurface.
    if (unlikely(Format == D3DFMT_UNKNOWN))
      return D3DERR_INVALIDCALL;

    InitReturnPtr(ppSurface);

    if (unlikely(ppSurface == nullptr))
      return D3DERR_INVALIDCALL;

    Com<d3d9::IDirect3DSurface9> pSurf9 = nullptr;
    HRESULT res = GetD3D9()->CreateRenderTarget(
      Width,
      Height,
      d3d9::D3DFORMAT(Format),
      d3d9::D3DMULTISAMPLE_TYPE(MultiSample),
      0,    // TODO: CreateRenderTarget MultisampleQuality
      Lockable,
      &pSurf9,
      NULL);

    if (likely(SUCCEEDED(res)))
      *ppSurface = ref(new D3D8Surface(this, std::move(pSurf9)));

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::CreateDepthStencilSurface(
          UINT                Width,
          UINT                Height,
          D3DFORMAT           Format,
          D3DMULTISAMPLE_TYPE MultiSample,
          IDirect3DSurface8** ppSurface) {
    // D3D8 returns D3DERR_INVALIDCALL for D3DFMT_UNKNOWN
    // before clearing the content of ppSurface.
    if (unlikely(Format == D3DFMT_UNKNOWN))
      return D3DERR_INVALIDCALL;

    InitReturnPtr(ppSurface);

    if (unlikely(ppSurface == nullptr))
      return D3DERR_INVALIDCALL;

    Com<d3d9::IDirect3DSurface9> pSurf9 = nullptr;
    HRESULT res = GetD3D9()->CreateDepthStencilSurface(
      Width,
      Height,
      d3d9::D3DFORMAT(Format),
      d3d9::D3DMULTISAMPLE_TYPE(MultiSample),
      0,    // TODO: CreateDepthStencilSurface MultisampleQuality
      true, // TODO: CreateDepthStencilSurface Discard
      &pSurf9,
      NULL);

    if (likely(SUCCEEDED(res)))
      *ppSurface = ref(new D3D8Surface(this, std::move(pSurf9)));

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::CreateImageSurface(
          UINT                Width,
          UINT                Height,
          D3DFORMAT           Format,
          IDirect3DSurface8** ppSurface) {
    // Only D3D8 CreateImageSurface clears the content of ppSurface
    // before checking if Format is equal to D3DFMT_UNKNOWN.
    InitReturnPtr(ppSurface);

    if (unlikely(Format == D3DFMT_UNKNOWN))
      return D3DERR_INVALIDCALL;

    if (unlikely(ppSurface == nullptr))
      return D3DERR_INVALIDCALL;

    // FIXME: Handle D3DPOOL_SCRATCH in CopyRects
    D3DPOOL pool = isUnsupportedSurfaceFormat(Format) ? D3DPOOL_SCRATCH : D3DPOOL_SYSTEMMEM;

    Com<d3d9::IDirect3DSurface9> pSurf = nullptr;
    HRESULT res = GetD3D9()->CreateOffscreenPlainSurface(
      Width,
      Height,
      d3d9::D3DFORMAT(Format),
      d3d9::D3DPOOL(pool),
      &pSurf,
      NULL);

    if (likely(SUCCEEDED(res)))
      *ppSurface = ref(new D3D8Surface(this, std::move(pSurf)));

    return res;
  }

  // Copies texture rect in system mem using memcpy.
  // Rects must be congruent, but need not be aligned.
  HRESULT copyTextureBuffers(
      D3D8Surface*                  src,
      D3D8Surface*                  dst,
      const d3d9::D3DSURFACE_DESC&  srcDesc,
      const d3d9::D3DSURFACE_DESC&  dstDesc,
      const RECT&                   srcRect,
      const RECT&                   dstRect) {
    HRESULT res = D3D_OK;
    D3DLOCKED_RECT srcLocked, dstLocked;

    bool compressed = isDXT(srcDesc.Format);

    res = src->LockRect(&srcLocked, &srcRect, D3DLOCK_READONLY);
    if (FAILED(res))
      return res;
    
    res = dst->LockRect(&dstLocked, &dstRect, 0);
    if (FAILED(res)) {
      src->UnlockRect();
      return res;
    }

    auto rows = srcRect.bottom  - srcRect.top;
    auto cols = srcRect.right   - srcRect.left;
    auto bpp  = srcLocked.Pitch / srcDesc.Width;

    if (!compressed
     && srcRect.left    == 0
     && srcRect.right   == LONG(srcDesc.Width)
     && srcDesc.Width   == dstDesc.Width
     && srcLocked.Pitch == dstLocked.Pitch) {

      // If copying the entire texture into a congruent destination,
      // we can do this in one continuous copy.
      std::memcpy(dstLocked.pBits, srcLocked.pBits, srcLocked.Pitch * rows);

    } else {
      // Bytes per row of the rect
      auto amplitude = cols * bpp;

      // Handle DXT compressed textures.
      // TODO: Are rects always 4x4 aligned?
      if (compressed) {
        // Assume that DXT blocks are 4x4 pixels.
        constexpr UINT blockWidth  = 4;
        constexpr UINT blockHeight = 4;

        // Compute rect dimensions in 4x4 blocks
        UINT rectWidthBlocks  = cols / blockWidth;
        UINT rectHeightBlocks = rows / blockHeight;

        // Compute total texture width in blocks
        // to derive block size in bytes using the pitch.
        UINT texWidthBlocks = std::max(srcDesc.Width / blockWidth, 1u);
        UINT bytesPerBlock  = srcLocked.Pitch / texWidthBlocks;

        // Copy H/4 rows of W/4 blocks
        amplitude = rectWidthBlocks * bytesPerBlock;
        rows      = rectHeightBlocks;
      }

      // Copy one row at a time
      size_t srcOffset = 0, dstOffset = 0;
      for (auto i = 0; i < rows; i++) {
        std::memcpy(
          (uint8_t*)dstLocked.pBits + dstOffset,
          (uint8_t*)srcLocked.pBits + srcOffset,
          amplitude);
        srcOffset += srcLocked.Pitch;
        dstOffset += dstLocked.Pitch;
      }
    }

    res = dst->UnlockRect();
    res = src->UnlockRect();
    return res;
  }

  /**
   * \brief D3D8 CopyRects implementation
   *
   * \details
   * The following table shows the possible combinations of source
   * and destination surface pools, and how we handle each of them.
   *
   *     ┌────────────┬───────────────────────────┬───────────────────────┬───────────────────────┬──────────┐
   *     │ Src/Dst    │ DEFAULT                   │ MANAGED               │ SYSTEMMEM             │ SCRATCH  │
   *     ├────────────┼───────────────────────────┼───────────────────────┼───────────────────────┼──────────┤
   *     │ DEFAULT    │  StretchRect              │  GetRenderTargetData  │  GetRenderTargetData  │ -        │
   *     │ MANAGED    │  UpdateTextureFromBuffer  │  memcpy               │  memcpy               │ -        │
   *     │ SYSTEMMEM  │  UpdateSurface            │  memcpy               │  memcpy               │ -        │
   *     │ SCRATCH    │  -                        │  -                    │  -                    │ -        │
   *     └────────────┴───────────────────────────┴───────────────────────┴───────────────────────┴──────────┘
   */
  HRESULT STDMETHODCALLTYPE D3D8Device::CopyRects(
          IDirect3DSurface8*  pSourceSurface,
    const RECT*               pSourceRectsArray,
          UINT                cRects,
          IDirect3DSurface8*  pDestinationSurface,
    const POINT*              pDestPointsArray) {
    D3D8DeviceLock lock = LockDevice();

    // The source and destination surfaces can not be identical.
    if (unlikely(pSourceSurface == nullptr ||
                 pDestinationSurface == nullptr ||
                 pSourceSurface == pDestinationSurface)) {
      return D3DERR_INVALIDCALL;
    }

    // TODO: No stretching or clipping of either source or destination rectangles.
    // All src/dest rectangles must fit within the dest surface.

    Com<D3D8Surface> src = static_cast<D3D8Surface*>(pSourceSurface);
    Com<D3D8Surface> dst = static_cast<D3D8Surface*>(pDestinationSurface);

    d3d9::D3DSURFACE_DESC srcDesc, dstDesc;
    src->GetD3D9()->GetDesc(&srcDesc);
    dst->GetD3D9()->GetDesc(&dstDesc);

    // This method does not support format conversion.
    if (unlikely(srcDesc.Format != dstDesc.Format))
      return D3DERR_INVALIDCALL;

    // This method cannot be applied to surfaces whose formats
    // are classified as depth stencil formats.
    if (unlikely(isDepthStencilFormat(D3DFORMAT(srcDesc.Format))))
      return D3DERR_INVALIDCALL;

    StateChange();

    // If pSourceRectsArray is NULL, then the entire surface is copied
    RECT rect;
    POINT point = { 0, 0 };
    if (pSourceRectsArray == NULL) {
      cRects = 1;
      rect.top    = rect.left = 0;
      rect.right  = srcDesc.Width;
      rect.bottom = srcDesc.Height;
      pSourceRectsArray = &rect;

      pDestPointsArray = &point;
    }

    for (UINT i = 0; i < cRects; i++) {

      RECT srcRect, dstRect;
      srcRect = pSourceRectsArray[i];

      // True if the copy is asymmetric
      bool asymmetric = true;
      // True if the copy requires stretching (not technically supported)
      bool stretch = true;
      // True if the copy is not perfectly aligned (supported)
      bool offset = true;

      if (pDestPointsArray != NULL) {
        dstRect.left    = pDestPointsArray[i].x;
        dstRect.right   = dstRect.left + (srcRect.right - srcRect.left);
        dstRect.top     = pDestPointsArray[i].y;
        dstRect.bottom  = dstRect.top + (srcRect.bottom - srcRect.top);
        asymmetric  = dstRect.left  != srcRect.left  || dstRect.top    != srcRect.top
                   || dstRect.right != srcRect.right || dstRect.bottom != srcRect.bottom;

        stretch     = (dstRect.right-dstRect.left) != (srcRect.right-srcRect.left)
                   || (dstRect.bottom-dstRect.top) != (srcRect.bottom-srcRect.top);

        offset      = !stretch && asymmetric;
      } else {
        dstRect     = srcRect;
        asymmetric  = stretch = offset = false;
      }

      POINT dstPt = { dstRect.left, dstRect.top };

      auto unhandled = [&] {
        Logger::warn(str::format("D3D8Device::CopyRects: Unhandled case from src pool ", srcDesc.Pool, " to dst pool ", dstDesc.Pool));
        return D3DERR_INVALIDCALL;
      };

      auto logError = [&] (HRESULT res) {
        if (FAILED(res)) {
          // Only a debug message because some games mess up CopyRects every frame in a way
          // that fails on native too but are perfectly fine with it.
          Logger::debug(str::format("D3D8Device::CopyRects: Failed to copy from src pool ", srcDesc.Pool, " to dst pool ", dstDesc.Pool));
        }
        return res;
      };

      switch (dstDesc.Pool) {

        // Dest: DEFAULT
        case d3d9::D3DPOOL_DEFAULT:
          switch (srcDesc.Pool) {
            case d3d9::D3DPOOL_DEFAULT: {
              // DEFAULT -> DEFAULT: use StretchRect
              return logError(GetD3D9()->StretchRect(
                src->GetD3D9(),
                &srcRect,
                dst->GetD3D9(),
                &dstRect,
                d3d9::D3DTEXF_NONE
              ));
            }
            case d3d9::D3DPOOL_MANAGED: {
              // MANAGED -> DEFAULT: UpdateTextureFromBuffer
              return logError(m_bridge->UpdateTextureFromBuffer(
                src->GetD3D9(),
                dst->GetD3D9(),
                &srcRect,
                &dstPt
              ));
            }
            case d3d9::D3DPOOL_SYSTEMMEM: {
              // SYSTEMMEM -> DEFAULT: use UpdateSurface
              return logError(GetD3D9()->UpdateSurface(
                src->GetD3D9(),
                &srcRect,
                dst->GetD3D9(),
                &dstPt
              ));
            }
            case d3d9::D3DPOOL_SCRATCH:
            default: {
              // TODO: Unhandled case.
              return unhandled();
            }
          } break;

        // Dest: MANAGED
        case d3d9::D3DPOOL_MANAGED:
          switch (srcDesc.Pool) {
            case d3d9::D3DPOOL_DEFAULT: {
              // TODO: Copy on GPU (handle MANAGED similarly to SYSTEMMEM for now)

              // Get temporary off-screen surface for stretching.
              Com<d3d9::IDirect3DSurface9> pBlitImage = dst->GetBlitImage();

              // Stretch the source RT to the temporary surface.
              HRESULT res = GetD3D9()->StretchRect(
                src->GetD3D9(),
                &srcRect,
                pBlitImage.ptr(),
                &dstRect,
                d3d9::D3DTEXF_NONE);

              if (FAILED(res)) {
                return logError(res);
              }

              // Now sync the rendertarget data into main memory.
              return logError(GetD3D9()->GetRenderTargetData(pBlitImage.ptr(), dst->GetD3D9()));
            }
            case d3d9::D3DPOOL_MANAGED:
            case d3d9::D3DPOOL_SYSTEMMEM: {
              // SYSTEMMEM -> MANAGED: LockRect / memcpy

              if (stretch) {
                return logError(D3DERR_INVALIDCALL);
              }

              return logError(copyTextureBuffers(src.ptr(), dst.ptr(), srcDesc, dstDesc, srcRect, dstRect));
            }
            case d3d9::D3DPOOL_SCRATCH:
            default: {
              // TODO: Unhandled case.
              return unhandled();
            }
          } break;

        // DEST: SYSTEMMEM
        case d3d9::D3DPOOL_SYSTEMMEM: {

          // RT (DEFAULT) -> SYSTEMMEM: Use GetRenderTargetData as fast path if possible
          if ((srcDesc.Usage & D3DUSAGE_RENDERTARGET || m_renderTarget == src.ptr())) {

            // GetRenderTargetData works if the formats and sizes match
            if (srcDesc.MultiSampleType == d3d9::D3DMULTISAMPLE_NONE
                && srcDesc.Width  == dstDesc.Width
                && srcDesc.Height == dstDesc.Height
                && srcDesc.Format == dstDesc.Format
                && !asymmetric) {
              return logError(GetD3D9()->GetRenderTargetData(src->GetD3D9(), dst->GetD3D9()));
            }
          }

          switch (srcDesc.Pool) {
            case d3d9::D3DPOOL_DEFAULT: {
              // Get temporary off-screen surface for stretching.
              Com<d3d9::IDirect3DSurface9> pBlitImage = dst->GetBlitImage();

              // Stretch the source RT to the temporary surface.
              HRESULT res = GetD3D9()->StretchRect(
                src->GetD3D9(),
                &srcRect,
                pBlitImage.ptr(),
                &dstRect,
                d3d9::D3DTEXF_NONE);
              if (FAILED(res)) {
                return logError(res);
              }

              // Now sync the rendertarget data into main memory.
              return logError(GetD3D9()->GetRenderTargetData(pBlitImage.ptr(), dst->GetD3D9()));
            }

            // SYSMEM/MANAGED -> SYSMEM: LockRect / memcpy
            case d3d9::D3DPOOL_MANAGED:
            case d3d9::D3DPOOL_SYSTEMMEM: {
              if (stretch) {
                return logError(D3DERR_INVALIDCALL);
              }

              return logError(copyTextureBuffers(src.ptr(), dst.ptr(), srcDesc, dstDesc, srcRect, dstRect));
            }
            case d3d9::D3DPOOL_SCRATCH:
            default: {
              // TODO: Unhandled case.
              return unhandled();
            }
          } break;
        }

        // DEST: SCRATCH
        case d3d9::D3DPOOL_SCRATCH:
        default: {
          // TODO: Unhandled case.
          return unhandled();
        }
      }
    }

    return D3DERR_INVALIDCALL;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::UpdateTexture(
          IDirect3DBaseTexture8* pSourceTexture,
          IDirect3DBaseTexture8* pDestinationTexture) {
    if (unlikely(pSourceTexture == nullptr || pDestinationTexture == nullptr))
      return D3DERR_INVALIDCALL;

    D3D8Texture2D* src = static_cast<D3D8Texture2D*>(pSourceTexture);
    D3D8Texture2D* dst = static_cast<D3D8Texture2D*>(pDestinationTexture);

    StateChange();
    return GetD3D9()->UpdateTexture(D3D8Texture2D::GetD3D9Nullable(src), D3D8Texture2D::GetD3D9Nullable(dst));
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetFrontBuffer(IDirect3DSurface8* pDestSurface) {
    if (unlikely(pDestSurface == nullptr))
      return D3DERR_INVALIDCALL;

    Com<D3D8Surface> surf = static_cast<D3D8Surface*>(pDestSurface);

    StateChange();
    // This actually gets a copy of the front buffer and writes it to pDestSurface
    return GetD3D9()->GetFrontBufferData(0, D3D8Surface::GetD3D9Nullable(surf));
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetRenderTarget(IDirect3DSurface8* pRenderTarget, IDirect3DSurface8* pNewZStencil) {
    D3D8DeviceLock lock = LockDevice();

    HRESULT res;

    if (pRenderTarget != nullptr) {
      D3D8Surface* surf = static_cast<D3D8Surface*>(pRenderTarget);

      // This will always be a state change and needs to be forwarded to
      // D3D9, even when the same render target is set, as the viewport
      // needs to be readjusted and reset.
      StateChange();
      res = GetD3D9()->SetRenderTarget(0, D3D8Surface::GetD3D9Nullable(surf));

      if (unlikely(FAILED(res))) return res;

      m_renderTarget = surf;
    }

    // SetDepthStencilSurface is a separate call
    D3D8Surface* zStencil = static_cast<D3D8Surface*>(pNewZStencil);

    // Depth stencil dimensions can not be lower than
    // those of the currently set render target.
    if (m_renderTarget != nullptr && zStencil != nullptr) {
      D3DSURFACE_DESC rtDesc;
      res = m_renderTarget->GetDesc(&rtDesc);

      if (unlikely(FAILED(res))) return res;

      D3DSURFACE_DESC dsDesc;
      res = zStencil->GetDesc(&dsDesc);

      if (unlikely(FAILED(res))) return res;

      if (unlikely(dsDesc.Width  < rtDesc.Width
                || dsDesc.Height < rtDesc.Height))
        return D3DERR_INVALIDCALL;
    }

    StateChange();
    res = GetD3D9()->SetDepthStencilSurface(D3D8Surface::GetD3D9Nullable(zStencil));

    if (unlikely(FAILED(res))) return res;

    m_depthStencil = zStencil;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetRenderTarget(IDirect3DSurface8** ppRenderTarget) {
    D3D8DeviceLock lock = LockDevice();

    InitReturnPtr(ppRenderTarget);

    if (unlikely(ppRenderTarget == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(m_renderTarget == nullptr)) {
      Com<d3d9::IDirect3DSurface9> pRT9 = nullptr;
      HRESULT res = GetD3D9()->GetRenderTarget(0, &pRT9); // use RT index 0

      if (likely(SUCCEEDED(res))) {
        m_renderTarget = new D3D8Surface(this, std::move(pRT9));
        *ppRenderTarget = m_renderTarget.ref();
      }

      return res;
    }

    *ppRenderTarget = m_renderTarget.ref();
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetDepthStencilSurface(IDirect3DSurface8** ppZStencilSurface) {
    D3D8DeviceLock lock = LockDevice();

    InitReturnPtr(ppZStencilSurface);

    if (unlikely(ppZStencilSurface == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(m_depthStencil == nullptr)) {
      Com<d3d9::IDirect3DSurface9> pStencil9 = nullptr;
      HRESULT res = GetD3D9()->GetDepthStencilSurface(&pStencil9);

      if (likely(SUCCEEDED(res))) {
        m_depthStencil = new D3D8Surface(this, std::move(pStencil9));
        *ppZStencilSurface = m_depthStencil.ref();
      }

      return res;
    }

    *ppZStencilSurface = m_depthStencil.ref();
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::BeginScene() { return GetD3D9()->BeginScene(); }
  
  HRESULT STDMETHODCALLTYPE D3D8Device::EndScene() { StateChange(); return GetD3D9()->EndScene(); }

  HRESULT STDMETHODCALLTYPE D3D8Device::Clear(
          DWORD    Count,
    const D3DRECT* pRects,
          DWORD    Flags,
          D3DCOLOR Color,
          float    Z,
          DWORD    Stencil) {
    StateChange();
    return GetD3D9()->Clear(Count, pRects, Flags, Color, Z, Stencil);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) {
    StateChange();
    return GetD3D9()->SetTransform(d3d9::D3DTRANSFORMSTATETYPE(State), pMatrix);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) {
    return GetD3D9()->GetTransform(d3d9::D3DTRANSFORMSTATETYPE(State), pMatrix);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::MultiplyTransform(D3DTRANSFORMSTATETYPE TransformState, const D3DMATRIX* pMatrix) {
    StateChange();
    return GetD3D9()->MultiplyTransform(d3d9::D3DTRANSFORMSTATETYPE(TransformState), pMatrix);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetViewport(const D3DVIEWPORT8* pViewport) {
    D3D8DeviceLock lock = LockDevice();

    if (likely(pViewport != nullptr)) {
      // We need a valid render target to validate the viewport
      if (unlikely(m_renderTarget == nullptr))
        return D3DERR_INVALIDCALL;

      D3DSURFACE_DESC rtDesc;
      HRESULT res = m_renderTarget->GetDesc(&rtDesc);

      // D3D8 will fail when setting a viewport that's outside of the
      // current render target, although this apparently works in D3D9
      if (likely(SUCCEEDED(res)) &&
          unlikely(pViewport->X + pViewport->Width  > rtDesc.Width ||
                   pViewport->Y + pViewport->Height > rtDesc.Height)) {
        // On Linux/Wine and in windowed mode, we can get in situations
        // where the actual render target dimensions are off by one
        // pixel to what the game sets them to. Allow this corner case
        // to skip the validation, in order to prevent issues.
        bool isOnePixelWider  = pViewport->X + pViewport->Width  == rtDesc.Width  + 1;
        bool isOnePixelTaller = pViewport->Y + pViewport->Height == rtDesc.Height + 1;

        if (m_presentParams.Windowed && (isOnePixelWider || isOnePixelTaller)) {
          Logger::debug("D3D8Device::SetViewport: Viewport exceeds render target dimensions by one pixel");
        } else {
          return D3DERR_INVALIDCALL;
        }
      }
    }

    StateChange();
    return GetD3D9()->SetViewport(reinterpret_cast<const d3d9::D3DVIEWPORT9*>(pViewport));
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetViewport(D3DVIEWPORT8* pViewport) {
    D3D8DeviceLock lock = LockDevice();
    return GetD3D9()->GetViewport(reinterpret_cast<d3d9::D3DVIEWPORT9*>(pViewport));
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetMaterial(const D3DMATERIAL8* pMaterial) {
    StateChange();
    return GetD3D9()->SetMaterial((const d3d9::D3DMATERIAL9*)pMaterial);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetMaterial(D3DMATERIAL8* pMaterial) {
    return GetD3D9()->GetMaterial((d3d9::D3DMATERIAL9*)pMaterial);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetLight(DWORD Index, const D3DLIGHT8* pLight) {
    StateChange();
    return GetD3D9()->SetLight(Index, (const d3d9::D3DLIGHT9*)pLight);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetLight(DWORD Index, D3DLIGHT8* pLight) {
    return GetD3D9()->GetLight(Index, (d3d9::D3DLIGHT9*)pLight);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::LightEnable(DWORD Index, BOOL Enable) {
    StateChange();
    return GetD3D9()->LightEnable(Index, Enable);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetLightEnable(DWORD Index, BOOL* pEnable) {
    return GetD3D9()->GetLightEnable(Index, pEnable);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetClipPlane(DWORD Index, const float* pPlane) {
    StateChange();
    return GetD3D9()->SetClipPlane(Index, pPlane);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetClipPlane(DWORD Index, float* pPlane) {
    return GetD3D9()->GetClipPlane(Index, pPlane);
  }
  
  HRESULT STDMETHODCALLTYPE D3D8Device::CreateStateBlock(
          D3DSTATEBLOCKTYPE     Type,
          DWORD*                pToken) {
    D3D8DeviceLock lock = LockDevice();

    if (unlikely(pToken == nullptr))
      return D3DERR_INVALIDCALL;

    // Applications cannot create a state block while another is being recorded
    if (unlikely(ShouldRecord()))
      return D3DERR_INVALIDCALL;

    Com<d3d9::IDirect3DStateBlock9> pStateBlock9;
    HRESULT res = GetD3D9()->CreateStateBlock(d3d9::D3DSTATEBLOCKTYPE(Type), &pStateBlock9);

    if (likely(SUCCEEDED(res))) {
      m_token++;
      auto stateBlockIterPair = m_stateBlocks.emplace(std::piecewise_construct,
                                                      std::forward_as_tuple(m_token),
                                                      std::forward_as_tuple(this, Type, pStateBlock9.ref()));
      *pToken = m_token;

      // D3D8 state blocks automatically capture state on creation.
      stateBlockIterPair.first->second.Capture();
    }

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::CaptureStateBlock(DWORD Token) {
    D3D8DeviceLock lock = LockDevice();

    // Applications cannot capture a state block while another is being recorded
    if (unlikely(ShouldRecord()))
      return D3DERR_INVALIDCALL;

    auto stateBlockIter = m_stateBlocks.find(Token);

    if (unlikely(stateBlockIter == m_stateBlocks.end())) {
      Logger::warn(str::format("D3D8Device::CaptureStateBlock: Invalid token: ", std::hex, Token));
      return D3D_OK;
    }

    return stateBlockIter->second.Capture();
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::ApplyStateBlock(DWORD Token) {
    D3D8DeviceLock lock = LockDevice();

    // Applications cannot apply a state block while another is being recorded
    if (unlikely(ShouldRecord()))
      return D3DERR_INVALIDCALL;

    StateChange();

    auto stateBlockIter = m_stateBlocks.find(Token);

    if (unlikely(stateBlockIter == m_stateBlocks.end())) {
      Logger::warn(str::format("D3D8Device::ApplyStateBlock: Invalid token: ", std::hex, Token));
      return D3D_OK;
    }

    return stateBlockIter->second.Apply();
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::DeleteStateBlock(DWORD Token) {
    D3D8DeviceLock lock = LockDevice();

    // Applications cannot delete a state block while another is being recorded
    if (unlikely(ShouldRecord()))
      return D3DERR_INVALIDCALL;

    auto stateBlockIter = m_stateBlocks.find(Token);

    if (unlikely(stateBlockIter == m_stateBlocks.end())) {
      Logger::warn(str::format("D3D8Device::DeleteStateBlock: Invalid token: ", std::hex, Token));
      return D3D_OK;
    }

    m_stateBlocks.erase(stateBlockIter);

    // native apparently does drop the token counter in
    // situations where the token being removed is the
    // last allocated token, which allows some reuse
    if (m_token == Token)
      m_token--;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::BeginStateBlock() {
    D3D8DeviceLock lock = LockDevice();

    if (unlikely(m_recorder != nullptr))
      return D3DERR_INVALIDCALL;

    HRESULT res = GetD3D9()->BeginStateBlock();

    if (likely(SUCCEEDED(res))) {
      m_token++;
      auto stateBlockIterPair = m_stateBlocks.emplace(std::piecewise_construct,
                                                      std::forward_as_tuple(m_token),
                                                      std::forward_as_tuple(this));
      m_recorder = &stateBlockIterPair.first->second;
      m_recorderToken = m_token;
    }

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::EndStateBlock(DWORD* pToken) {
    D3D8DeviceLock lock = LockDevice();

    if (unlikely(pToken == nullptr || m_recorder == nullptr))
      return D3DERR_INVALIDCALL;

    Com<d3d9::IDirect3DStateBlock9> pStateBlock;
    HRESULT res = GetD3D9()->EndStateBlock(&pStateBlock);

    if (likely(SUCCEEDED(res))) {
      m_recorder->SetD3D9(std::move(pStateBlock));

      *pToken = m_recorderToken;

      m_recorder = nullptr;
      m_recorderToken = 0;
    }

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetClipStatus(const D3DCLIPSTATUS8* pClipStatus) {
    StateChange();
    return GetD3D9()->SetClipStatus(reinterpret_cast<const d3d9::D3DCLIPSTATUS9*>(pClipStatus));
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetClipStatus(D3DCLIPSTATUS8* pClipStatus) {
    return GetD3D9()->GetClipStatus(reinterpret_cast<d3d9::D3DCLIPSTATUS9*>(pClipStatus));
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetTexture(DWORD Stage, IDirect3DBaseTexture8** ppTexture) {
    D3D8DeviceLock lock = LockDevice();

    InitReturnPtr(ppTexture);

    if (unlikely(ppTexture == nullptr))
      return D3DERR_INVALIDCALL;

    *ppTexture = m_textures[Stage].ref();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetTexture(DWORD Stage, IDirect3DBaseTexture8* pTexture) {
    D3D8DeviceLock lock = LockDevice();

    if (unlikely(Stage >= d8caps::MAX_TEXTURE_STAGES))
      return D3DERR_INVALIDCALL;

    if (unlikely(ShouldRecord()))
      return m_recorder->SetTexture(Stage, pTexture);

    D3D8Texture2D* tex = static_cast<D3D8Texture2D*>(pTexture);

    if (unlikely(m_textures[Stage] == tex))
      return D3D_OK;

    StateChange();
    HRESULT res = GetD3D9()->SetTexture(Stage, D3D8Texture2D::GetD3D9Nullable(tex));

    if (likely(SUCCEEDED(res)))
      m_textures[Stage] = tex;

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetTextureStageState(
          DWORD                    Stage,
          D3DTEXTURESTAGESTATETYPE Type,
          DWORD*                   pValue) {
    d3d9::D3DSAMPLERSTATETYPE stateType = GetSamplerStateType9(Type);

    if (stateType != -1u) {
      // if the type has been remapped to a sampler state type:
      return GetD3D9()->GetSamplerState(Stage, stateType, pValue);
    }
    else {
      return GetD3D9()->GetTextureStageState(Stage, d3d9::D3DTEXTURESTAGESTATETYPE(Type), pValue);
    }
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetTextureStageState(
          DWORD                    Stage,
          D3DTEXTURESTAGESTATETYPE Type,
          DWORD                    Value) {
    d3d9::D3DSAMPLERSTATETYPE stateType = GetSamplerStateType9(Type);

    StateChange();
    if (stateType != -1u) {
      // if the type has been remapped to a sampler state type:
      return GetD3D9()->SetSamplerState(Stage, stateType, Value);
    } else {
      return GetD3D9()->SetTextureStageState(Stage, d3d9::D3DTEXTURESTAGESTATETYPE(Type), Value);
    }
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::ValidateDevice(DWORD* pNumPasses) {
    return GetD3D9()->ValidateDevice(pNumPasses);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY* pEntries) {
    StateChange();
    return GetD3D9()->SetPaletteEntries(PaletteNumber, pEntries);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries) {
    return GetD3D9()->GetPaletteEntries(PaletteNumber, pEntries);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetCurrentTexturePalette(UINT PaletteNumber) {
    StateChange();
    return GetD3D9()->SetCurrentTexturePalette(PaletteNumber);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetCurrentTexturePalette(UINT* PaletteNumber) {
    return GetD3D9()->GetCurrentTexturePalette(PaletteNumber);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::DrawPrimitive(
          D3DPRIMITIVETYPE PrimitiveType,
          UINT             StartVertex,
          UINT             PrimitiveCount) {
    D3D8DeviceLock lock = LockDevice();

    if (ShouldBatch())
      return m_batcher->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
    return GetD3D9()->DrawPrimitive(d3d9::D3DPRIMITIVETYPE(PrimitiveType), StartVertex, PrimitiveCount);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::DrawIndexedPrimitive(
          D3DPRIMITIVETYPE PrimitiveType,
          UINT             MinVertexIndex,
          UINT             NumVertices,
          UINT             StartIndex,
          UINT             PrimitiveCount) {
    D3D8DeviceLock lock = LockDevice();

    return GetD3D9()->DrawIndexedPrimitive(
      d3d9::D3DPRIMITIVETYPE(PrimitiveType),
      static_cast<INT>(std::min(m_baseVertexIndex, static_cast<UINT>(INT_MAX))), // set by SetIndices
      MinVertexIndex,
      NumVertices,
      StartIndex,
      PrimitiveCount);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::DrawPrimitiveUP(
          D3DPRIMITIVETYPE PrimitiveType,
          UINT             PrimitiveCount,
    const void*            pVertexStreamZeroData,
          UINT             VertexStreamZeroStride) {
    D3D8DeviceLock lock = LockDevice();

    StateChange();

    // Stream 0 is set to null by this call
    m_streams[0] = D3D8VBO {nullptr, 0};

    return GetD3D9()->DrawPrimitiveUP(d3d9::D3DPRIMITIVETYPE(PrimitiveType), PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::DrawIndexedPrimitiveUP(
          D3DPRIMITIVETYPE PrimitiveType,
          UINT             MinVertexIndex,
          UINT             NumVertices,
          UINT             PrimitiveCount,
    const void*            pIndexData,
          D3DFORMAT        IndexDataFormat,
    const void*            pVertexStreamZeroData,
          UINT             VertexStreamZeroStride) {
    D3D8DeviceLock lock = LockDevice();

    StateChange();

    // Stream 0 and the index buffer are set to null by this call
    m_streams[0] = D3D8VBO {nullptr, 0};
    m_indices = nullptr;
    m_baseVertexIndex = 0;
    
    return GetD3D9()->DrawIndexedPrimitiveUP(
      d3d9::D3DPRIMITIVETYPE(PrimitiveType),
      MinVertexIndex,
      NumVertices,
      PrimitiveCount,
      pIndexData,
      d3d9::D3DFORMAT(IndexDataFormat),
      pVertexStreamZeroData,
      VertexStreamZeroStride);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::ProcessVertices(
      UINT                         SrcStartIndex,
      UINT                         DestIndex,
      UINT                         VertexCount,
      IDirect3DVertexBuffer8*      pDestBuffer,
      DWORD                        Flags) {
    D3D8VertexBuffer* buffer = static_cast<D3D8VertexBuffer*>(pDestBuffer);
    return GetD3D9()->ProcessVertices(
      SrcStartIndex,
      DestIndex,
      VertexCount,
      D3D8VertexBuffer::GetD3D9Nullable(buffer),
      nullptr,
      Flags
    );
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetVertexShaderConstant(
          DWORD StartRegister,
    const void* pConstantData,
          DWORD ConstantCount) {
    StateChange();
    // ConstantCount is actually the same as Vector4fCount
    return GetD3D9()->SetVertexShaderConstantF(StartRegister, reinterpret_cast<const float*>(pConstantData), ConstantCount);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetVertexShaderConstant(DWORD Register, void* pConstantData, DWORD ConstantCount) {
    return GetD3D9()->GetVertexShaderConstantF(Register, (float*)pConstantData, ConstantCount);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetStreamSource(
          UINT                    StreamNumber,
          IDirect3DVertexBuffer8* pStreamData,
          UINT                    Stride) {
    D3D8DeviceLock lock = LockDevice();

    if (unlikely(StreamNumber >= d8caps::MAX_STREAMS))
      return D3DERR_INVALIDCALL;

    if (unlikely(ShouldRecord()))
      return m_recorder->SetStreamSource(StreamNumber, pStreamData, Stride);

    D3D8VertexBuffer* buffer = static_cast<D3D8VertexBuffer*>(pStreamData);
    HRESULT res = GetD3D9()->SetStreamSource(StreamNumber, D3D8VertexBuffer::GetD3D9Nullable(buffer), 0, Stride);

    if (likely(SUCCEEDED(res))) {
      if (ShouldBatch())
        m_batcher->SetStream(StreamNumber, buffer, Stride);

      m_streams[StreamNumber].buffer = buffer;
      // The previous stride is preserved if pStreamData is NULL
      if (likely(buffer != nullptr))
        m_streams[StreamNumber].stride = Stride;
    }

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetStreamSource(
          UINT                     StreamNumber,
          IDirect3DVertexBuffer8** ppStreamData,
          UINT*                    pStride) {
    D3D8DeviceLock lock = LockDevice();

    InitReturnPtr(ppStreamData);

    if (likely(pStride != nullptr))
      *pStride = 0;

    if (unlikely(ppStreamData == nullptr || pStride == nullptr))
      return D3DERR_INVALIDCALL;

    if (unlikely(StreamNumber >= d8caps::MAX_STREAMS))
      return D3DERR_INVALIDCALL;
    
    const D3D8VBO& vbo = m_streams[StreamNumber];

    *ppStreamData = vbo.buffer.ref();
    *pStride      = vbo.stride;
    
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetIndices(IDirect3DIndexBuffer8* pIndexData, UINT BaseVertexIndex) {
    D3D8DeviceLock lock = LockDevice();

    if (unlikely(ShouldRecord()))
      return m_recorder->SetIndices(pIndexData, BaseVertexIndex);

    if (unlikely(BaseVertexIndex > INT_MAX))
      Logger::warn("D3D8Device::SetIndices: BaseVertexIndex exceeds INT_MAX");

    // used by DrawIndexedPrimitive
    m_baseVertexIndex = BaseVertexIndex;

    D3D8IndexBuffer* buffer = static_cast<D3D8IndexBuffer*>(pIndexData);
    HRESULT res = GetD3D9()->SetIndices(D3D8IndexBuffer::GetD3D9Nullable(buffer));

    if (likely(SUCCEEDED(res))) {
      if (ShouldBatch())
        m_batcher->SetIndices(buffer, m_baseVertexIndex);

      m_indices = buffer;
    }

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetIndices(
          IDirect3DIndexBuffer8** ppIndexData,
          UINT* pBaseVertexIndex) {
    D3D8DeviceLock lock = LockDevice();

    InitReturnPtr(ppIndexData);

    if (unlikely(ppIndexData == nullptr || pBaseVertexIndex == nullptr))
      return D3DERR_INVALIDCALL;

    *ppIndexData      = m_indices.ref();
    *pBaseVertexIndex = m_baseVertexIndex;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetPixelShaderConstant(DWORD Register, void* pConstantData, DWORD ConstantCount) {
    return GetD3D9()->GetPixelShaderConstantF(Register, (float*)pConstantData, ConstantCount);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetPixelShaderConstant(
          DWORD StartRegister,
    const void* pConstantData,
          DWORD ConstantCount) {
    StateChange();
    // ConstantCount is actually the same as Vector4fCount
    return GetD3D9()->SetPixelShaderConstantF(StartRegister, reinterpret_cast<const float*>(pConstantData), ConstantCount);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::DrawRectPatch(
          UINT               Handle,
    const float*             pNumSegs,
    const D3DRECTPATCH_INFO* pRectPatchInfo) {
    return GetD3D9()->DrawRectPatch(Handle, pNumSegs, reinterpret_cast<const d3d9::D3DRECTPATCH_INFO*>(pRectPatchInfo));
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::DrawTriPatch(
          UINT              Handle,
    const float*            pNumSegs,
    const D3DTRIPATCH_INFO* pTriPatchInfo) {
    return GetD3D9()->DrawTriPatch(Handle, pNumSegs, reinterpret_cast<const d3d9::D3DTRIPATCH_INFO*>(pTriPatchInfo));
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::DeletePatch(UINT Handle) {
    return GetD3D9()->DeletePatch(Handle);
  }

  // Render States //

  // ZBIAS can be an integer from 0 to 1 and needs to be remapped to float
  static constexpr float ZBIAS_SCALE     = -0.000005f;
  static constexpr float ZBIAS_SCALE_INV = 1 / ZBIAS_SCALE;

  HRESULT STDMETHODCALLTYPE D3D8Device::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
    D3D8DeviceLock lock = LockDevice();

    d3d9::D3DRENDERSTATETYPE State9 = (d3d9::D3DRENDERSTATETYPE)State;
    bool stateChange = true;

    switch (State) {
      // Most render states translate 1:1 to D3D9
      default:
        break;

      // TODO: D3DRS_LINEPATTERN - vkCmdSetLineRasterizationModeEXT
      case D3DRS_LINEPATTERN: {
        [[maybe_unused]]
        D3DLINEPATTERN pattern = bit::cast<D3DLINEPATTERN>(Value);
        stateChange = false;
      } break;

      // Not supported by D3D8.
      case D3DRS_ZVISIBLE:
        stateChange = false;
        break;

      // TODO: Not implemented by D9VK. Try anyway.
      case D3DRS_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      case D3DRS_ZBIAS:
        State9 = d3d9::D3DRS_DEPTHBIAS;
        Value  = bit::cast<DWORD>(float(Value) * ZBIAS_SCALE);
        break;

      case D3DRS_SOFTWAREVERTEXPROCESSING:
        // D3D9 can return D3DERR_INVALIDCALL, but we don't care.
        if (!(m_behaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING))
          return D3D_OK;

        // This was a very easy footgun for D3D8 applications.
        if (unlikely(ShouldRecord()))
          return m_recorder->SetSoftwareVertexProcessing(Value);

        return GetD3D9()->SetSoftwareVertexProcessing(Value);

      // TODO: D3DRS_PATCHSEGMENTS
      case D3DRS_PATCHSEGMENTS:
        stateChange = false;
        break;
    }

    if (stateChange) {
      DWORD value;
      // Value at this point is converted for use with D3D9,
      // so we need to compare it against D3D9 directly
      HRESULT res = GetD3D9()->GetRenderState(State9, &value);
      if (likely(SUCCEEDED(res)) && value != Value)
        StateChange();
    }

    // This call will never fail
    return GetD3D9()->SetRenderState(State9, Value);
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) {
    D3D8DeviceLock lock = LockDevice();

    if (unlikely(pValue == nullptr))
      return D3DERR_INVALIDCALL;

    d3d9::D3DRENDERSTATETYPE State9 = (d3d9::D3DRENDERSTATETYPE)State;

    switch (State) {
      // Most render states translate 1:1 to D3D9
      default:
        break;

      // TODO: D3DRS_LINEPATTERN
      case D3DRS_LINEPATTERN:
        break;

      // Not supported by D3D8.
      case D3DRS_ZVISIBLE:
        break;

      case D3DRS_EDGEANTIALIAS:
        State9 = d3d9::D3DRS_ANTIALIASEDLINEENABLE;
        break;

      case D3DRS_ZBIAS: {
        float bias  = 0;
        HRESULT res = GetD3D9()->GetRenderState(d3d9::D3DRS_DEPTHBIAS, (DWORD*)&bias);
        *pValue     = bit::cast<DWORD>(bias * ZBIAS_SCALE_INV);
        return res;
      } break;

      case D3DRS_SOFTWAREVERTEXPROCESSING:
        return GetD3D9()->GetSoftwareVertexProcessing();

      // TODO: D3DRS_PATCHSEGMENTS
      case D3DRS_PATCHSEGMENTS:
        break;
    }

    // This call will never fail
    return GetD3D9()->GetRenderState(State9, pValue);
  }

  // Vertex Shaders //

  HRESULT STDMETHODCALLTYPE D3D8Device::CreateVertexShader(
        const DWORD* pDeclaration,
        const DWORD* pFunction,
              DWORD* pHandle,
              DWORD Usage ) {
    D3D8DeviceLock lock = LockDevice();

    if (unlikely(pDeclaration == nullptr || pHandle == nullptr))
      return D3DERR_INVALIDCALL;

    // Validate VS version for non-FF shaders
    if (pFunction != nullptr) {
      uint32_t majorVersion = (pFunction[0] >> 8) & 0xff;
      uint32_t minorVersion = pFunction[0] & 0xff;

      if (unlikely(majorVersion != 1 || minorVersion > 1)) {
        Logger::err(str::format("D3D8Device::CreateVertexShader: Unsupported VS version ", majorVersion, ".", minorVersion));
        return D3DERR_INVALIDCALL;
      }
    }

    D3D8VertexShaderInfo& info = m_vertexShaders.emplace_back();

    // Store D3D8 bytecodes in the shader info
    for (UINT i = 0; pDeclaration[i] != D3DVSD_END(); i++)
      info.declaration.push_back(pDeclaration[i]);
    info.declaration.push_back(D3DVSD_END());

    if (pFunction != nullptr) {
      for (UINT i = 0; pFunction[i] != D3DVS_END(); i++)
        info.function.push_back(pFunction[i]);
      info.function.push_back(D3DVS_END());
    }
    
    D3D9VertexShaderCode result = TranslateVertexShader8(pDeclaration, pFunction, m_d3d8Options);

    // Create vertex declaration
    HRESULT res = GetD3D9()->CreateVertexDeclaration(result.declaration, &(info.pVertexDecl));
    if (unlikely(FAILED(res)))
      return res;

    if (pFunction != nullptr) {
      res = GetD3D9()->CreateVertexShader(result.function.data(), &(info.pVertexShader));
    } else {
      // pFunction is NULL: fixed function pipeline
      info.pVertexShader = nullptr;
    }

    if (likely(SUCCEEDED(res))) {
      // Set bit to indicate this is not an FVF
      *pHandle = getShaderHandle(m_vertexShaders.size());
    }

    return res;
  }

  inline D3D8VertexShaderInfo* getVertexShaderInfo(D3D8Device* device, DWORD Handle) {

    Handle = getShaderIndex(Handle);

    if (unlikely(Handle >= device->m_vertexShaders.size())) {
      Logger::debug(str::format("D3D8: Invalid vertex shader index ", std::hex, Handle));
      return nullptr;
    }

    D3D8VertexShaderInfo& info = device->m_vertexShaders[Handle];

    if (unlikely(info.pVertexDecl == nullptr && info.pVertexShader == nullptr)) {
      Logger::debug(str::format("D3D8: Application provided deleted vertex shader ", std::hex, Handle));
      return nullptr;
    }

    return &info;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetVertexShader(DWORD Handle) {
    D3D8DeviceLock lock = LockDevice();

    HRESULT res;

    if (unlikely(ShouldRecord())) {
      return m_recorder->SetVertexShader(Handle);
    }

    // Check for extra bit that indicates this is not an FVF
    if (!isFVF(Handle)) {

      D3D8VertexShaderInfo* info = getVertexShaderInfo(this, Handle);

      if (!info)
        return D3DERR_INVALIDCALL;

      StateChange();

      GetD3D9()->SetVertexDeclaration(info->pVertexDecl.ptr());
      res = GetD3D9()->SetVertexShader(info->pVertexShader.ptr());

      if (likely(SUCCEEDED(res))) {
        // Cache current shader
        m_currentVertexShader = Handle;
      }

      return res;

    } else if (m_currentVertexShader != Handle) {
      StateChange();

      //GetD3D9()->SetVertexDeclaration(nullptr);
      GetD3D9()->SetVertexShader(nullptr);
      res = GetD3D9()->SetFVF(Handle);

      if (likely(SUCCEEDED(res))) {
        // Cache current FVF
        m_currentVertexShader = Handle;
      }

      return res;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetVertexShader(DWORD* pHandle) {
    D3D8DeviceLock lock = LockDevice();

    if (unlikely(pHandle == nullptr))
      return D3DERR_INVALIDCALL;

    // Return cached shader
    *pHandle = m_currentVertexShader;

    return D3D_OK;

    /*
    // Slow path. Use to debug cached shader validation. //
    
    d3d9::IDirect3DVertexShader9* pVertexShader;
    HRESULT res = GetD3D9()->GetVertexShader(&pVertexShader);

    if (FAILED(res) || pVertexShader == nullptr) {
      return GetD3D9()->GetFVF(pHandle);
    }

    for (unsigned int i = 0; i < m_vertexShaders.size(); i++) {
      D3D8VertexShaderInfo& info = m_vertexShaders[i];

      if (info.pVertexShader == pVertexShader) {
        *pHandle = getShaderHandle(DWORD(i));
        return res;
      }
    }

    return res;
    */
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::DeleteVertexShader(DWORD Handle) {
    D3D8DeviceLock lock = LockDevice();

    if (!isFVF(Handle)) {
      D3D8VertexShaderInfo* info = getVertexShaderInfo(this, Handle);

      if (!info)
        return D3DERR_INVALIDCALL;

      if (info->pVertexDecl != nullptr)
        info->pVertexDecl = nullptr;
      if (info->pVertexShader != nullptr)
        info->pVertexShader = nullptr;

      info->declaration.clear();
      info->function.clear();

      if (m_currentVertexShader == Handle)
        m_currentVertexShader = 0;
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetVertexShaderDeclaration(DWORD Handle, void* pData, DWORD* pSizeOfData) {
    D3D8DeviceLock lock = LockDevice();

    D3D8VertexShaderInfo* pInfo = getVertexShaderInfo(this, Handle);

    if (unlikely(!pInfo))
      return D3DERR_INVALIDCALL;
    
    UINT SizeOfData = *pSizeOfData;
    
    // Get actual size
    UINT ActualSize = pInfo->declaration.size() * sizeof(DWORD);
    
    if (pData == nullptr) {
      *pSizeOfData = ActualSize;
      return D3D_OK;
    }

    // D3D8-specific behavior
    if (SizeOfData < ActualSize) {
      // D3DERR_MOREDATA should be returned according to the D3D8 documentation,
      // along with a correction to the ActualSize, however tests have shown that
      // D3DERR_INVALIDCALL is returned and no size correction is performed.
      return D3DERR_INVALIDCALL;
    }

    memcpy(pData, pInfo->declaration.data(), ActualSize);
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetVertexShaderFunction(DWORD Handle, void* pData, DWORD* pSizeOfData) {
    D3D8DeviceLock lock = LockDevice();

    D3D8VertexShaderInfo* pInfo = getVertexShaderInfo(this, Handle);

    if (unlikely(!pInfo))
      return D3DERR_INVALIDCALL;
    
    UINT SizeOfData = *pSizeOfData;
    
    // Get actual size
    UINT ActualSize = pInfo->function.size() * sizeof(DWORD);
    
    if (pData == nullptr) {
      *pSizeOfData = ActualSize;
      return D3D_OK;
    }

    // D3D8-specific behavior
    if (SizeOfData < ActualSize) {
      // D3DERR_MOREDATA should be returned according to the D3D8 documentation,
      // along with a correction to the ActualSize, however tests have shown that
      // D3DERR_INVALIDCALL is returned and no size correction is performed.
      return D3DERR_INVALIDCALL;
    }

    memcpy(pData, pInfo->function.data(), ActualSize);
    return D3D_OK;

  }

  // Pixel Shaders //

  HRESULT STDMETHODCALLTYPE D3D8Device::CreatePixelShader(
        const DWORD* pFunction,
              DWORD* pHandle) {
    D3D8DeviceLock lock = LockDevice();

    if (unlikely(pFunction == nullptr || pHandle == nullptr))
      return D3DERR_INVALIDCALL;

    uint32_t majorVersion = (pFunction[0] >> 8) & 0xff;
    uint32_t minorVersion = pFunction[0] & 0xff;

    if (unlikely(majorVersion != 1 || minorVersion > 4)) {
      Logger::err(str::format("D3D8Device::CreatePixelShader: Unsupported PS version ", majorVersion, ".", minorVersion));
      return D3DERR_INVALIDCALL;
    }

    d3d9::IDirect3DPixelShader9* pPixelShader;
    
    HRESULT res = GetD3D9()->CreatePixelShader(pFunction, &pPixelShader);

    if (likely(SUCCEEDED(res))) {
      m_pixelShaders.push_back(pPixelShader);
      // Still set the shader bit, to prevent conflicts with NULL.
      *pHandle = getShaderHandle(m_pixelShaders.size());
    }

    return res;
  }

  inline d3d9::IDirect3DPixelShader9* getPixelShaderPtr(D3D8Device* device, DWORD Handle) {
    
    Handle = getShaderIndex(Handle);

    if (unlikely(Handle >= device->m_pixelShaders.size())) {
      Logger::debug(str::format("D3D8: Invalid pixel shader index ", std::hex, Handle));
      return nullptr;
    }

    d3d9::IDirect3DPixelShader9* pPixelShader = device->m_pixelShaders[Handle].ptr();

    if (unlikely(pPixelShader == nullptr)) {
      Logger::debug(str::format("D3D8: Application provided deleted pixel shader ", std::hex, Handle));
      return nullptr;
    }

    return pPixelShader;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::SetPixelShader(DWORD Handle) {
    D3D8DeviceLock lock = LockDevice();

    if (unlikely(ShouldRecord())) {
      return m_recorder->SetPixelShader(Handle);
    }

    if (Handle == DWORD(NULL)) {
      StateChange();
      m_currentPixelShader = DWORD(NULL);
      return GetD3D9()->SetPixelShader(nullptr);
    }

    d3d9::IDirect3DPixelShader9* pPixelShader = getPixelShaderPtr(this, Handle);

    if (unlikely(!pPixelShader)) {
      return D3DERR_INVALIDCALL;
    }

    StateChange();
    HRESULT res = GetD3D9()->SetPixelShader(pPixelShader);

    if (likely(SUCCEEDED(res))) {
      // Cache current pixel shader
      m_currentPixelShader = Handle;
    }

    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetPixelShader(DWORD* pHandle) {
    D3D8DeviceLock lock = LockDevice();

    if (unlikely(pHandle == nullptr))
      return D3DERR_INVALIDCALL;

    // Return cached shader
    *pHandle = m_currentPixelShader;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::DeletePixelShader(DWORD Handle) {
    D3D8DeviceLock lock = LockDevice();

    d3d9::IDirect3DPixelShader9* pPixelShader = getPixelShaderPtr(this, Handle);

    if (unlikely(!pPixelShader)) {
      return D3DERR_INVALIDCALL;
    }

    m_pixelShaders[getShaderIndex(Handle)] = nullptr;

    if (m_currentPixelShader == Handle)
      m_currentPixelShader = 0;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE D3D8Device::GetPixelShaderFunction(DWORD Handle, void* pData, DWORD* pSizeOfData) {
    D3D8DeviceLock lock = LockDevice();

    d3d9::IDirect3DPixelShader9* pPixelShader = getPixelShaderPtr(this, Handle);

    if (unlikely(!pPixelShader))
      return D3DERR_INVALIDCALL;

    UINT SizeOfData = *pSizeOfData;
    
    // Get actual size
    UINT ActualSize = 0;
    pPixelShader->GetFunction(nullptr, &ActualSize);
    
    if (pData == nullptr) {
      *pSizeOfData = ActualSize;
      return D3D_OK;
    }

    // D3D8-specific behavior
    if (SizeOfData < ActualSize) {
      // D3DERR_MOREDATA should be returned according to the D3D8 documentation,
      // along with a correction to the ActualSize, however tests have shown that
      // D3DERR_INVALIDCALL is returned and no size correction is performed.
      return D3DERR_INVALIDCALL;
    }

    return pPixelShader->GetFunction(pData, &SizeOfData);
  }

}
