#include "d3d9_device.h"

#include "d3d9_swapchain.h"
#include "d3d9_caps.h"
#include "d3d9_util.h"
#include "d3d9_texture.h"
#include "d3d9_buffer.h"

#include "../util/util_bit.h"
#include "../util/util_math.h"

#include <algorithm>

namespace dxvk {

  Direct3DDevice9Ex::Direct3DDevice9Ex(
    bool extended,
    IDirect3D9Ex* parent,
    UINT adapter,
    Rc<DxvkAdapter> dxvkAdapter,
    Rc<DxvkDevice> dxvkDevice,
    D3DDEVTYPE deviceType,
    HWND window,
    DWORD flags,
    D3DPRESENT_PARAMETERS* presentParams,
    D3DDISPLAYMODEEX* displayMode)
    : m_extended{ extended }
    , m_parent{ parent }
    , m_adapter{ adapter }
    , m_dxvkAdapter{ dxvkAdapter }
    , m_device{ dxvkDevice }
    , m_deviceType{ deviceType }
    , m_window{ window }
    , m_flags{ flags }
    , m_d3d9Formats{ dxvkAdapter }
    , m_csChunk{ AllocCsChunk() }
    , m_csThread{ dxvkDevice->createContext() }
    , m_multithread{ flags & D3DCREATE_MULTITHREADED }
    , m_frameLatency{ DefaultFrameLatency }
    , m_frameId{ 0 }
    , m_deferViewportBinding{ false } {
    for (uint32_t i = 0; i < m_frameEvents.size(); i++)
      m_frameEvents[i] = new DxvkEvent();

    EmitCs([
      cDevice = m_device
    ] (DxvkContext * ctx) {
        ctx->beginRecording(cDevice->createCommandList());
    });


    HRESULT hr = this->Reset(presentParams);

    if (FAILED(hr))
      throw DxvkError("Direct3DDevice9Ex: device initial reset failed.");
  }

  Direct3DDevice9Ex::~Direct3DDevice9Ex() {
    Flush();
    SynchronizeCsThread();
    m_device->waitForIdle(); // Sync Device
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDirect3DDevice9)
     || (m_extended && riid == __uuidof(IDirect3DDevice9Ex))) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("Direct3DDevice9Ex::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::TestCooperativeLevel() {
    // Equivelant of D3D11/DXGI present tests. We can always present.
    return D3D_OK;
  }

  UINT    STDMETHODCALLTYPE Direct3DDevice9Ex::GetAvailableTextureMem() {
    auto memoryProp = m_dxvkAdapter->memoryProperties();

    VkDeviceSize availableTextureMemory = 0;

    for (uint32_t i = 0; i < memoryProp.memoryHeapCount; i++)
      availableTextureMemory += memoryProp.memoryHeaps[i].size;

    // The value returned is a 32-bit value, so we need to clamp it.
    VkDeviceSize maxMemory = UINT32_MAX;
    availableTextureMemory = std::min(availableTextureMemory, maxMemory);

    return UINT(availableTextureMemory);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::EvictManagedResources() {
    Logger::warn("Direct3DDevice9Ex::EvictManagedResources: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDirect3D(IDirect3D9** ppD3D9) {
    if (ppD3D9 == nullptr)
      return D3DERR_INVALIDCALL;

    *ppD3D9 = m_parent.ref();
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDeviceCaps(D3DCAPS9* pCaps) {
    return caps::getDeviceCaps(m_adapter, m_deviceType, pCaps);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode) {
    auto lock = LockDevice();

    auto* swapchain = getInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->GetDisplayMode(pMode);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) {
    if (pParameters == nullptr)
      return D3DERR_INVALIDCALL;

    pParameters->AdapterOrdinal = m_adapter;
    pParameters->BehaviorFlags = m_flags;
    pParameters->DeviceType = m_deviceType;
    pParameters->hFocusWindow = m_window;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetCursorProperties(
    UINT XHotSpot,
    UINT YHotSpot,
    IDirect3DSurface9* pCursorBitmap) {
    Logger::warn("Direct3DDevice9Ex::SetCursorProperties: Stub");
    return D3D_OK;
  }

  void    STDMETHODCALLTYPE Direct3DDevice9Ex::SetCursorPosition(int X, int Y, DWORD Flags) {
    auto lock = LockDevice();

    m_cursor.updateCursor(X, Y, Flags & D3DCURSOR_IMMEDIATE_UPDATE);
  }

  BOOL    STDMETHODCALLTYPE Direct3DDevice9Ex::ShowCursor(BOOL bShow) {
    // Ok so if they call FALSE here it means they want to use the regular windows cursor.
    // if they call TRUE here it means they want to use some weird bitmap cursor that I currently dont care about.
    // Therefore we always want to show the regular cursor no matter what!
    ::ShowCursor(true);

    return TRUE;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** ppSwapChain) {
    auto lock = LockDevice();

    InitReturnPtr(ppSwapChain);

    if (ppSwapChain == nullptr || pPresentationParameters == nullptr)
      return D3DERR_INVALIDCALL;

    auto* swapchain = new Direct3DSwapChain9Ex(this, pPresentationParameters);
    swapchain->AddRefPrivate();
    m_swapchains.push_back(swapchain);
    *ppSwapChain = ref(swapchain);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) {
    auto lock = LockDevice();

    InitReturnPtr(pSwapChain);

    auto* swapchain = getInternalSwapchain(iSwapChain);

    if (swapchain == nullptr || pSwapChain == nullptr)
      return D3DERR_INVALIDCALL;

    *pSwapChain = static_cast<IDirect3DSwapChain9*>(ref(swapchain));

    return D3D_OK;
  }

  UINT    STDMETHODCALLTYPE Direct3DDevice9Ex::GetNumberOfSwapChains() {
    auto lock = LockDevice();

    return UINT(m_swapchains.size());
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    return ResetEx(pPresentationParameters, nullptr);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::Present(
    const RECT* pSourceRect,
    const RECT* pDestRect,
          HWND hDestWindowOverride,
    const RGNDATA* pDirtyRegion) {
    return PresentEx(
      pSourceRect,
      pDestRect,
      hDestWindowOverride,
      pDirtyRegion,
      0);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetBackBuffer(
    UINT iSwapChain,
    UINT iBackBuffer,
    D3DBACKBUFFER_TYPE Type,
    IDirect3DSurface9** ppBackBuffer) {
    auto lock = LockDevice();

    InitReturnPtr(ppBackBuffer);

    auto* swapchain = getInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) {
    auto lock = LockDevice();

    auto* swapchain = getInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->GetRasterStatus(pRasterStatus);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetDialogBoxMode(BOOL bEnableDialogs) {
    Logger::warn("Direct3DDevice9Ex::SetDialogBoxMode: Stub");
    return D3D_OK;
  }

  void    STDMETHODCALLTYPE Direct3DDevice9Ex::SetGammaRamp(
    UINT iSwapChain,
    DWORD Flags,
    const D3DGAMMARAMP* pRamp) {
    auto lock = LockDevice();

    auto* swapchain = getInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return;

    swapchain->SetGammaRamp(Flags, pRamp);
  }

  void    STDMETHODCALLTYPE Direct3DDevice9Ex::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp) {
    auto lock = LockDevice();

    auto* swapchain = getInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return;

    swapchain->GetGammaRamp(pRamp);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateTexture(UINT Width,
    UINT Height,
    UINT Levels,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DTexture9** ppTexture,
    HANDLE* pSharedHandle) {
    InitReturnPtr(ppTexture);
    InitReturnPtr(pSharedHandle);

    if (ppTexture == nullptr)
      return D3DERR_INVALIDCALL;

    D3D9TextureDesc desc;
    desc.Type = D3DRTYPE_TEXTURE;
    desc.Width = Width;
    desc.Height = Height;
    desc.Depth = 1;
    desc.MipLevels = Levels;
    desc.Usage = Usage;
    desc.Format = fixupFormat(Format);
    desc.Pool = Pool;
    desc.Discard = FALSE;
    desc.MultiSample = D3DMULTISAMPLE_NONE;
    desc.MultisampleQuality = 0;
    desc.Lockable = FALSE;

    try {
      const Com<Direct3DTexture9> texture = new Direct3DTexture9{ this, &desc };
      *ppTexture = texture.ref();
      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVolumeTexture(
    UINT Width,
    UINT Height,
    UINT Depth,
    UINT Levels,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DVolumeTexture9** ppVolumeTexture,
    HANDLE* pSharedHandle) {
    Logger::warn("Direct3DDevice9Ex::CreateVolumeTexture: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateCubeTexture(
    UINT EdgeLength,
    UINT Levels,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DCubeTexture9** ppCubeTexture,
    HANDLE* pSharedHandle) {
    Logger::warn("Direct3DDevice9Ex::CreateCubeTexture: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVertexBuffer(
    UINT Length,
    DWORD Usage,
    DWORD FVF,
    D3DPOOL Pool,
    IDirect3DVertexBuffer9** ppVertexBuffer,
    HANDLE* pSharedHandle) {
    auto lock = LockDevice();

    if (ppVertexBuffer == nullptr)
      return D3DERR_INVALIDCALL;

    D3D9_BUFFER_DESC desc;
    desc.Format = D3D9Format::VERTEXDATA;
    desc.FVF = FVF;
    desc.Pool = Pool;
    desc.Size = Length;
    desc.Type = D3DRTYPE_VERTEXBUFFER;
    desc.Usage = Usage;

    try {
      const Com<Direct3DVertexBuffer9> buffer = new Direct3DVertexBuffer9{ this, &desc };
      *ppVertexBuffer = buffer.ref();
      return D3D_OK;
    }
    catch (const DxvkError & e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateIndexBuffer(
    UINT Length,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DIndexBuffer9** ppIndexBuffer,
    HANDLE* pSharedHandle) {
    auto lock = LockDevice();

    if (ppIndexBuffer == nullptr)
      return D3DERR_INVALIDCALL;

    D3D9_BUFFER_DESC desc;
    desc.Format = fixupFormat(Format);
    desc.Pool = Pool;
    desc.Size = Length;
    desc.Type = D3DRTYPE_INDEXBUFFER;
    desc.Usage = Usage;

    try {
      const Com<Direct3DIndexBuffer9> buffer = new Direct3DIndexBuffer9{ this, &desc };
      *ppIndexBuffer = buffer.ref();
      return D3D_OK;
    }
    catch (const DxvkError & e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateRenderTarget(
    UINT Width,
    UINT Height,
    D3DFORMAT Format,
    D3DMULTISAMPLE_TYPE MultiSample,
    DWORD MultisampleQuality,
    BOOL Lockable,
    IDirect3DSurface9** ppSurface,
    HANDLE* pSharedHandle) {
    return CreateRenderTargetEx(
      Width,
      Height,
      Format,
      MultiSample,
      MultisampleQuality,
      Lockable,
      ppSurface,
      pSharedHandle,
      0);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateDepthStencilSurface(
    UINT Width,
    UINT Height,
    D3DFORMAT Format,
    D3DMULTISAMPLE_TYPE MultiSample,
    DWORD MultisampleQuality,
    BOOL Discard,
    IDirect3DSurface9** ppSurface,
    HANDLE* pSharedHandle) {
    return CreateDepthStencilSurfaceEx(
      Width,
      Height,
      Format,
      MultiSample,
      MultisampleQuality,
      Discard,
      ppSurface,
      pSharedHandle,
      0);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::UpdateSurface(
    IDirect3DSurface9* pSourceSurface,
    const RECT* pSourceRect,
    IDirect3DSurface9* pDestinationSurface,
    const POINT* pDestPoint) {
    Logger::warn("Direct3DDevice9Ex::UpdateSurface: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) {
    Logger::warn("Direct3DDevice9Ex::UpdateTexture: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) {
    Logger::warn("Direct3DDevice9Ex::GetRenderTargetData: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) {
    auto lock = LockDevice();

    auto* swapchain = getInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->GetFrontBufferData(pDestSurface);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::StretchRect(
    IDirect3DSurface9* pSourceSurface,
    const RECT* pSourceRect,
    IDirect3DSurface9* pDestSurface,
    const RECT* pDestRect,
    D3DTEXTUREFILTERTYPE Filter) {
    Logger::warn("Direct3DDevice9Ex::StretchRect: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ColorFill(
    IDirect3DSurface9* pSurface,
    const RECT* pRect,
    D3DCOLOR color) {
    Logger::warn("Direct3DDevice9Ex::ColorFill: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateOffscreenPlainSurface(
    UINT Width,
    UINT Height,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DSurface9** ppSurface,
    HANDLE* pSharedHandle) {
    return CreateOffscreenPlainSurfaceEx(
      Width,
      Height,
      Format,
      Pool,
      ppSurface,
      pSharedHandle,
      0);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) {
    auto lock = LockDevice();

    if (RenderTargetIndex >= caps::MaxSimultaneousRenderTargets
     || (pRenderTarget == nullptr && RenderTargetIndex == 0) )
      return D3DERR_INVALIDCALL;

    Direct3DSurface9* rt = static_cast<Direct3DSurface9*>(pRenderTarget);

    if (m_state.renderTargets[RenderTargetIndex] == rt)
      return D3D_OK;

    FlushImplicit(FALSE);

    changePrivate(m_state.renderTargets[RenderTargetIndex], rt);
    
    BindFramebuffer();

    if (RenderTargetIndex == 0) {
      m_deferViewportBinding = true;

      SetViewport(nullptr);

      auto rtv = rt->GetRenderTargetView(false);

      RECT scissorRect;
      scissorRect.left = 0;
      scissorRect.top = 0;
      scissorRect.right = rtv->image()->info().extent.width;
      scissorRect.bottom = rtv->image()->info().extent.height;

      SetScissorRect(&scissorRect);
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) {
    auto lock = LockDevice();

    InitReturnPtr(ppRenderTarget);

    if (ppRenderTarget == nullptr || RenderTargetIndex > caps::MaxSimultaneousRenderTargets)
      return D3DERR_INVALIDCALL;

    if (m_state.renderTargets[RenderTargetIndex] == nullptr)
      return D3DERR_NOTFOUND;

    *ppRenderTarget = ref(m_state.renderTargets[RenderTargetIndex]);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) {
    auto lock = LockDevice();

    Direct3DSurface9* ds = static_cast<Direct3DSurface9*>(pNewZStencil);

    if (m_state.depthStencil == ds)
      return D3D_OK;

    FlushImplicit(FALSE);

    changePrivate(m_state.depthStencil, ds);

    BindFramebuffer();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) {
    auto lock = LockDevice();

    InitReturnPtr(ppZStencilSurface);

    if (ppZStencilSurface == nullptr)
      return D3DERR_INVALIDCALL;

    if (m_state.depthStencil == nullptr)
      return D3DERR_NOTFOUND;

    *ppZStencilSurface = ref(m_state.depthStencil);

    return D3D_OK;
  }

  // The Begin/EndScene functions actually do nothing.
  // Some games don't even call them.

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::BeginScene() {
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::EndScene() {
    FlushImplicit(true);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::Clear(
    DWORD Count,
    const D3DRECT* pRects,
    DWORD Flags,
    D3DCOLOR Color,
    float Z,
    DWORD Stencil) {
    auto lock = LockDevice();

    if (Flags & D3DCLEAR_STENCIL || Flags & D3DCLEAR_ZBUFFER) {
      auto dsv = m_state.depthStencil != nullptr ? m_state.depthStencil->GetDepthStencilView() : nullptr;

      if (dsv != nullptr) {
        VkImageAspectFlags aspectMask = 0;

        if (Flags & D3DCLEAR_ZBUFFER)
          aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;

        if (Flags & D3DCLEAR_STENCIL)
          aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

        aspectMask &= imageFormatInfo(dsv->info().format)->aspectMask;

        if (aspectMask != 0) {
          VkClearValue clearValue;
          clearValue.depthStencil.depth = Z;
          clearValue.depthStencil.stencil = Stencil;

          EmitCs([
            cClearValue = clearValue,
            cAspectMask = aspectMask,
            cImageView  = dsv
          ] (DxvkContext * ctx) {
              ctx->clearRenderTarget(
                cImageView,
                cAspectMask,
                cClearValue);
            });
        }
      }
    }

    if (Flags & D3DCLEAR_TARGET) {
      VkClearValue clearValue;
      DecodeD3DCOLOR(Color, clearValue.color.float32);

      for (auto rt : m_state.renderTargets) {
        auto rtv = rt != nullptr ? rt->GetRenderTargetView(false) : nullptr; // TODO: handle srgb-ness

        if (rtv != nullptr) {
          EmitCs([
            cClearValue = clearValue,
            cImageView  = rtv
          ] (DxvkContext* ctx) {
            ctx->clearRenderTarget(
              cImageView,
              VK_IMAGE_ASPECT_COLOR_BIT,
              cClearValue);
          });
        }
      }
    }

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) {
    Logger::warn("Direct3DDevice9Ex::SetTransform: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) {
    Logger::warn("Direct3DDevice9Ex::GetTransform: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::MultiplyTransform(D3DTRANSFORMSTATETYPE TransformState, const D3DMATRIX* pMatrix) {
    Logger::warn("Direct3DDevice9Ex::MultiplyTransform: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetViewport(const D3DVIEWPORT9* pViewport) {
    auto lock = LockDevice();

    D3DVIEWPORT9 viewport;
    if (pViewport == nullptr) {
      auto rtv = m_state.renderTargets[0]->GetRenderTargetView(false);

      viewport.X = 0;
      viewport.Y = 0;
      viewport.Width = rtv->image()->info().extent.width;
      viewport.Height = rtv->image()->info().extent.height;
      viewport.MinZ = 0.0f;
      viewport.MaxZ = 1.0f;
    }
    else
      viewport = *pViewport;

    m_state.viewport = viewport;

    if (m_deferViewportBinding)
      m_deferViewportBinding = false;
    else
      BindViewportAndScissor();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetViewport(D3DVIEWPORT9* pViewport) {
    auto lock = LockDevice();

    if (pViewport == nullptr)
      return D3DERR_INVALIDCALL;

    *pViewport = m_state.viewport;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetMaterial(const D3DMATERIAL9* pMaterial) {
    Logger::warn("Direct3DDevice9Ex::SetMaterial: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetMaterial(D3DMATERIAL9* pMaterial) {
    Logger::warn("Direct3DDevice9Ex::GetMaterial: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetLight(DWORD Index, const D3DLIGHT9* pLight) {
    Logger::warn("Direct3DDevice9Ex::SetLight: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetLight(DWORD Index, D3DLIGHT9* pLight) {
    Logger::warn("Direct3DDevice9Ex::GetLight: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::LightEnable(DWORD Index, BOOL Enable) {
    Logger::warn("Direct3DDevice9Ex::LightEnable: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetLightEnable(DWORD Index, BOOL* pEnable) {
    Logger::warn("Direct3DDevice9Ex::GetLightEnable: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetClipPlane(DWORD Index, const float* pPlane) {
    Logger::warn("Direct3DDevice9Ex::SetClipPlane: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetClipPlane(DWORD Index, float* pPlane) {
    Logger::warn("Direct3DDevice9Ex::GetClipPlane: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
    Logger::warn("Direct3DDevice9Ex::SetRenderState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) {
    Logger::warn("Direct3DDevice9Ex::GetRenderState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) {
    Logger::warn("Direct3DDevice9Ex::CreateStateBlock: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::BeginStateBlock() {
    Logger::warn("Direct3DDevice9Ex::BeginStateBlock: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::EndStateBlock(IDirect3DStateBlock9** ppSB) {
    Logger::warn("Direct3DDevice9Ex::EndStateBlock: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetClipStatus(const D3DCLIPSTATUS9* pClipStatus) {
    Logger::warn("Direct3DDevice9Ex::SetClipStatus: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetClipStatus(D3DCLIPSTATUS9* pClipStatus) {
    Logger::warn("Direct3DDevice9Ex::GetClipStatus: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture) {
    Logger::warn("Direct3DDevice9Ex::GetTexture: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture) {
    Logger::warn("Direct3DDevice9Ex::SetTexture: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetTextureStageState(
    DWORD Stage,
    D3DTEXTURESTAGESTATETYPE Type,
    DWORD* pValue) {
    Logger::warn("Direct3DDevice9Ex::GetTextureStageState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetTextureStageState(
    DWORD Stage,
    D3DTEXTURESTAGESTATETYPE Type,
    DWORD Value) {
    Logger::warn("Direct3DDevice9Ex::SetTextureStageState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetSamplerState(
    DWORD Sampler,
    D3DSAMPLERSTATETYPE Type,
    DWORD* pValue) {
    Logger::warn("Direct3DDevice9Ex::GetSamplerState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetSamplerState(
    DWORD Sampler,
    D3DSAMPLERSTATETYPE Type,
    DWORD Value) {
    Logger::warn("Direct3DDevice9Ex::SetSamplerState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ValidateDevice(DWORD* pNumPasses) {
    if (pNumPasses != nullptr)
      *pNumPasses = 1;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY* pEntries) {
    Logger::warn("Direct3DDevice9Ex::SetPaletteEntries: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries) {
    Logger::warn("Direct3DDevice9Ex::GetPaletteEntries: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetCurrentTexturePalette(UINT PaletteNumber) {
    Logger::warn("Direct3DDevice9Ex::SetCurrentTexturePalette: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetCurrentTexturePalette(UINT *PaletteNumber) {
    Logger::warn("Direct3DDevice9Ex::GetCurrentTexturePalette: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetScissorRect(const RECT* pRect) {
    auto lock = LockDevice();

    if (pRect == nullptr)
      return D3DERR_INVALIDCALL;

    m_state.scissorRect = *pRect;

    BindViewportAndScissor();

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetScissorRect(RECT* pRect) {
    auto lock = LockDevice();

    if (pRect == nullptr)
      return D3DERR_INVALIDCALL;

    *pRect = m_state.scissorRect;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetSoftwareVertexProcessing(BOOL bSoftware) {
    Logger::warn("Direct3DDevice9Ex::SetSoftwareVertexProcessing: Stub");
    return D3D_OK;
  }

  BOOL    STDMETHODCALLTYPE Direct3DDevice9Ex::GetSoftwareVertexProcessing() {
    Logger::warn("Direct3DDevice9Ex::GetSoftwareVertexProcessing: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetNPatchMode(float nSegments) {
    Logger::warn("Direct3DDevice9Ex::SetNPatchMode: Stub");
    return D3D_OK;
  }

  float   STDMETHODCALLTYPE Direct3DDevice9Ex::GetNPatchMode() {
    Logger::warn("Direct3DDevice9Ex::GetNPatchMode: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawPrimitive(
    D3DPRIMITIVETYPE PrimitiveType,
    UINT StartVertex,
    UINT PrimitiveCount) {
    Logger::warn("Direct3DDevice9Ex::DrawPrimitive: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawIndexedPrimitive(
    D3DPRIMITIVETYPE PrimitiveType,
    INT BaseVertexIndex,
    UINT MinVertexIndex,
    UINT NumVertices,
    UINT startIndex,
    UINT primCount) {
    Logger::warn("Direct3DDevice9Ex::DrawIndexedPrimitive: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawPrimitiveUP(
    D3DPRIMITIVETYPE PrimitiveType,
    UINT PrimitiveCount,
    const void* pVertexStreamZeroData,
    UINT VertexStreamZeroStride) {
    Logger::warn("Direct3DDevice9Ex::DrawPrimitiveUP: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawIndexedPrimitiveUP(
    D3DPRIMITIVETYPE PrimitiveType,
    UINT MinVertexIndex,
    UINT NumVertices,
    UINT PrimitiveCount,
    const void* pIndexData,
    D3DFORMAT IndexDataFormat,
    const void* pVertexStreamZeroData,
    UINT VertexStreamZeroStride) {
    Logger::warn("Direct3DDevice9Ex::DrawIndexedPrimitiveUP: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ProcessVertices(
    UINT SrcStartIndex,
    UINT DestIndex,
    UINT VertexCount,
    IDirect3DVertexBuffer9* pDestBuffer,
    IDirect3DVertexDeclaration9* pVertexDecl,
    DWORD Flags) {
    Logger::warn("Direct3DDevice9Ex::ProcessVertices: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVertexDeclaration(const D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl) {
    Logger::warn("Direct3DDevice9Ex::CreateVertexDeclaration: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) {
    Logger::warn("Direct3DDevice9Ex::SetVertexDeclaration: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) {
    Logger::warn("Direct3DDevice9Ex::GetVertexDeclaration: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetFVF(DWORD FVF) {
    Logger::warn("Direct3DDevice9Ex::SetFVF: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetFVF(DWORD* pFVF) {
    Logger::warn("Direct3DDevice9Ex::GetFVF: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateVertexShader(const DWORD* pFunction, IDirect3DVertexShader9** ppShader) {
    Logger::warn("Direct3DDevice9Ex::CreateVertexShader: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShader(IDirect3DVertexShader9* pShader) {
    Logger::warn("Direct3DDevice9Ex::SetVertexShader: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShader(IDirect3DVertexShader9** ppShader) {
    Logger::warn("Direct3DDevice9Ex::GetVertexShader: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShaderConstantF(
    UINT StartRegister,
    const float* pConstantData,
    UINT Vector4fCount) {
    Logger::warn("Direct3DDevice9Ex::SetVertexShaderConstantF: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShaderConstantF(
    UINT StartRegister,
    float* pConstantData,
    UINT Vector4fCount) {
    Logger::warn("Direct3DDevice9Ex::GetVertexShaderConstantF: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShaderConstantI(
    UINT StartRegister,
    const int* pConstantData,
    UINT Vector4iCount) {
    Logger::warn("Direct3DDevice9Ex::SetVertexShaderConstantI: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShaderConstantI(
    UINT StartRegister,
    int* pConstantData,
    UINT Vector4iCount) {
    Logger::warn("Direct3DDevice9Ex::GetVertexShaderConstantI: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetVertexShaderConstantB(
    UINT StartRegister,
    const BOOL* pConstantData,
    UINT BoolCount) {
    Logger::warn("Direct3DDevice9Ex::SetVertexShaderConstantB: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetVertexShaderConstantB(
    UINT StartRegister,
    BOOL* pConstantData,
    UINT BoolCount) {
    Logger::warn("Direct3DDevice9Ex::GetVertexShaderConstantB: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetStreamSource(
    UINT StreamNumber,
    IDirect3DVertexBuffer9* pStreamData,
    UINT OffsetInBytes,
    UINT Stride) {
    Logger::warn("Direct3DDevice9Ex::SetStreamSource: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetStreamSource(
    UINT StreamNumber,
    IDirect3DVertexBuffer9** ppStreamData,
    UINT* pOffsetInBytes,
    UINT* pStride) {
    Logger::warn("Direct3DDevice9Ex::GetStreamSource: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetStreamSourceFreq(UINT StreamNumber, UINT Setting) {
    Logger::warn("Direct3DDevice9Ex::SetStreamSourceFreq: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetStreamSourceFreq(UINT StreamNumber, UINT* pSetting) {
    Logger::warn("Direct3DDevice9Ex::GetStreamSourceFreq: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetIndices(IDirect3DIndexBuffer9* pIndexData) {
    Logger::warn("Direct3DDevice9Ex::SetIndices: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetIndices(IDirect3DIndexBuffer9** ppIndexData) {
    Logger::warn("Direct3DDevice9Ex::GetIndices: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreatePixelShader(const DWORD* pFunction, IDirect3DPixelShader9** ppShader) {
    Logger::warn("Direct3DDevice9Ex::CreatePixelShader: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShader(IDirect3DPixelShader9* pShader) {
    Logger::warn("Direct3DDevice9Ex::SetPixelShader: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShader(IDirect3DPixelShader9** ppShader) {
    Logger::warn("Direct3DDevice9Ex::GetPixelShader: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShaderConstantF(
    UINT StartRegister,
    const float* pConstantData,
    UINT Vector4fCount) {
    Logger::warn("Direct3DDevice9Ex::SetPixelShaderConstantF: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShaderConstantF(
    UINT StartRegister,
    float* pConstantData,
    UINT Vector4fCount) {
    Logger::warn("Direct3DDevice9Ex::GetPixelShaderConstantF: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShaderConstantI(
    UINT StartRegister,
    const int* pConstantData,
    UINT Vector4iCount) {
    Logger::warn("Direct3DDevice9Ex::SetPixelShaderConstantI: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShaderConstantI(
    UINT StartRegister,
    int* pConstantData,
    UINT Vector4iCount) {
    Logger::warn("Direct3DDevice9Ex::GetPixelShaderConstantI: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetPixelShaderConstantB(
    UINT StartRegister,
    const BOOL* pConstantData,
    UINT BoolCount) {
    Logger::warn("Direct3DDevice9Ex::SetPixelShaderConstantB: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetPixelShaderConstantB(
    UINT StartRegister,
    BOOL* pConstantData,
    UINT BoolCount) {
    Logger::warn("Direct3DDevice9Ex::GetPixelShaderConstantB: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawRectPatch(
    UINT Handle,
    const float* pNumSegs,
    const D3DRECTPATCH_INFO* pRectPatchInfo) {
    Logger::warn("Direct3DDevice9Ex::DrawRectPatch: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DrawTriPatch(
    UINT Handle,
    const float* pNumSegs,
    const D3DTRIPATCH_INFO* pTriPatchInfo) {
    Logger::warn("Direct3DDevice9Ex::DrawTriPatch: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::DeletePatch(UINT Handle) {
    Logger::warn("Direct3DDevice9Ex::DeletePatch: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) {
    Logger::warn("Direct3DDevice9Ex::CreateQuery: Stub");
    return D3D_OK;
  }

  // Ex Methods

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetConvolutionMonoKernel(
    UINT width,
    UINT height,
    float* rows,
    float* columns) {
    Logger::warn("Direct3DDevice9Ex::SetConvolutionMonoKernel: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ComposeRects(
    IDirect3DSurface9* pSrc,
    IDirect3DSurface9* pDst,
    IDirect3DVertexBuffer9* pSrcRectDescs,
    UINT NumRects,
    IDirect3DVertexBuffer9* pDstRectDescs,
    D3DCOMPOSERECTSOP Operation,
    int Xoffset,
    int Yoffset) {
    Logger::warn("Direct3DDevice9Ex::ComposeRects: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetGPUThreadPriority(INT* pPriority) {
    Logger::warn("Direct3DDevice9Ex::GetGPUThreadPriority: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetGPUThreadPriority(INT Priority) {
    Logger::warn("Direct3DDevice9Ex::SetGPUThreadPriority: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::WaitForVBlank(UINT iSwapChain) {
    auto lock = LockDevice();

    auto* swapchain = getInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->WaitForVBlank();
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CheckResourceResidency(IDirect3DResource9** pResourceArray, UINT32 NumResources) {
    Logger::warn("Direct3DDevice9Ex::CheckResourceResidency: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::SetMaximumFrameLatency(UINT MaxLatency) {
    auto lock = LockDevice();

    if (MaxLatency == 0)
      MaxLatency = DefaultFrameLatency;

    if (MaxLatency > m_frameEvents.size())
      MaxLatency = m_frameEvents.size();

    m_frameLatency = MaxLatency;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetMaximumFrameLatency(UINT* pMaxLatency) {
    auto lock = LockDevice();

    if (pMaxLatency == nullptr)
      return D3DERR_INVALIDCALL;

    *pMaxLatency = m_frameLatency;

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CheckDeviceState(HWND hDestinationWindow) {
    Logger::warn("Direct3DDevice9Ex::CheckDeviceState: Stub");
    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::PresentEx(
    const RECT* pSourceRect,
    const RECT* pDestRect,
          HWND hDestWindowOverride,
    const RGNDATA* pDirtyRegion,
          DWORD dwFlags) {
    auto lock = LockDevice();

    auto* swapchain = getInternalSwapchain(0);
    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;
    
    return swapchain->Present(
      pSourceRect,
      pDestRect,
      hDestWindowOverride,
      pDirtyRegion,
      dwFlags);
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateRenderTargetEx(
    UINT Width,
    UINT Height,
    D3DFORMAT Format,
    D3DMULTISAMPLE_TYPE MultiSample,
    DWORD MultisampleQuality,
    BOOL Lockable,
    IDirect3DSurface9** ppSurface,
    HANDLE* pSharedHandle,
    DWORD Usage) {
    InitReturnPtr(ppSurface);
    InitReturnPtr(pSharedHandle);

    if (ppSurface == nullptr)
      return D3DERR_INVALIDCALL;

    D3D9TextureDesc desc;
    desc.Type = D3DRTYPE_SURFACE;
    desc.Width = Width;
    desc.Height = Height;
    desc.Depth = 1;
    desc.MipLevels = 1;
    desc.Usage = Usage | D3DUSAGE_RENDERTARGET;
    desc.Format = fixupFormat(Format);
    desc.Pool = D3DPOOL_DEFAULT;
    desc.Discard = FALSE;
    desc.MultiSample = MultiSample;
    desc.MultisampleQuality = MultisampleQuality;
    desc.Lockable = Lockable;

    try {
      const Com<Direct3DSurface9> surface = new Direct3DSurface9{ this, &desc };
      *ppSurface = surface.ref();
      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateOffscreenPlainSurfaceEx(
    UINT Width,
    UINT Height,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DSurface9** ppSurface,
    HANDLE* pSharedHandle,
    DWORD Usage) {
    InitReturnPtr(ppSurface);
    InitReturnPtr(pSharedHandle);

    if (ppSurface == nullptr)
      return D3DERR_INVALIDCALL;

    D3D9TextureDesc desc;
    desc.Type = D3DRTYPE_SURFACE;
    desc.Width = Width;
    desc.Height = Height;
    desc.Depth = 1;
    desc.MipLevels = 1;
    desc.Usage = Usage;
    desc.Format = fixupFormat(Format);
    desc.Pool = D3DPOOL_DEFAULT;
    desc.Discard = FALSE;
    desc.MultiSample = D3DMULTISAMPLE_NONE;
    desc.MultisampleQuality = 0;
    desc.Lockable = TRUE;

    try {
      const Com<Direct3DSurface9> surface = new Direct3DSurface9{ this, &desc };
      *ppSurface = surface.ref();
      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::CreateDepthStencilSurfaceEx(
    UINT Width,
    UINT Height,
    D3DFORMAT Format,
    D3DMULTISAMPLE_TYPE MultiSample,
    DWORD MultisampleQuality,
    BOOL Discard,
    IDirect3DSurface9** ppSurface,
    HANDLE* pSharedHandle,
    DWORD Usage) {
    InitReturnPtr(ppSurface);
    InitReturnPtr(pSharedHandle);

    if (ppSurface == nullptr)
      return D3DERR_INVALIDCALL;

    auto format = fixupFormat(Format);
    bool lockable = format == D3D9Format::D32_LOCKABLE
                 || format == D3D9Format::D32F_LOCKABLE
                 || format == D3D9Format::D16_LOCKABLE
                 || format == D3D9Format::S8_LOCKABLE;

    D3D9TextureDesc desc;
    desc.Type = D3DRTYPE_SURFACE;
    desc.Width = Width;
    desc.Height = Height;
    desc.Depth = 1;
    desc.MipLevels = 1;
    desc.Usage = Usage | D3DUSAGE_DEPTHSTENCIL;
    desc.Format = format;
    desc.Pool = D3DPOOL_DEFAULT;
    desc.Discard = Discard;
    desc.MultiSample = MultiSample;
    desc.MultisampleQuality = MultisampleQuality;
    desc.Lockable = lockable;

    try {
      const Com<Direct3DSurface9> surface = new Direct3DSurface9{ this, &desc };
      *ppSurface = surface.ref();
      return D3D_OK;
    }
    catch (const DxvkError& e) {
      Logger::err(e.message());
      return D3DERR_INVALIDCALL;
    }
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::ResetEx(D3DPRESENT_PARAMETERS* pPresentationParameters, D3DDISPLAYMODEEX *pFullscreenDisplayMode) {
    auto lock = LockDevice();

    if (pPresentationParameters == nullptr)
      return D3DERR_INVALIDCALL;

    SetDepthStencilSurface(nullptr);

    for (uint32_t i = 0; i < caps::MaxSimultaneousRenderTargets; i++)
      SetRenderTarget(0, nullptr);

    SetRenderState(D3DRS_ZENABLE, pPresentationParameters->EnableAutoDepthStencil != FALSE ? D3DZB_TRUE : D3DZB_FALSE);
    SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    SetRenderState(D3DRS_LASTPIXEL, TRUE);
    SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
    SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
    SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    SetRenderState(D3DRS_ALPHAREF, 0);
    SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_ALWAYS);
    SetRenderState(D3DRS_DITHERENABLE, FALSE);
    SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    SetRenderState(D3DRS_FOGENABLE, FALSE);
    SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    //	SetRenderState(D3DRS_ZVISIBLE, 0);
    SetRenderState(D3DRS_FOGCOLOR, 0);
    SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
    SetRenderState(D3DRS_FOGSTART, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_FOGEND, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_FOGDENSITY, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_RANGEFOGENABLE, FALSE);
    SetRenderState(D3DRS_STENCILENABLE, FALSE);
    SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
    SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
    SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
    SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
    SetRenderState(D3DRS_STENCILREF, 0);
    SetRenderState(D3DRS_STENCILMASK, 0xFFFFFFFF);
    SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
    SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
    SetRenderState(D3DRS_WRAP0, 0);
    SetRenderState(D3DRS_WRAP1, 0);
    SetRenderState(D3DRS_WRAP2, 0);
    SetRenderState(D3DRS_WRAP3, 0);
    SetRenderState(D3DRS_WRAP4, 0);
    SetRenderState(D3DRS_WRAP5, 0);
    SetRenderState(D3DRS_WRAP6, 0);
    SetRenderState(D3DRS_WRAP7, 0);
    SetRenderState(D3DRS_CLIPPING, TRUE);
    SetRenderState(D3DRS_LIGHTING, TRUE);
    SetRenderState(D3DRS_AMBIENT, 0);
    SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
    SetRenderState(D3DRS_COLORVERTEX, TRUE);
    SetRenderState(D3DRS_LOCALVIEWER, TRUE);
    SetRenderState(D3DRS_NORMALIZENORMALS, FALSE);
    SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
    SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2);
    SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
    SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
    SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
    SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    SetRenderState(D3DRS_POINTSIZE, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_POINTSIZE_MIN, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_POINTSPRITEENABLE, FALSE);
    SetRenderState(D3DRS_POINTSCALEENABLE, FALSE);
    SetRenderState(D3DRS_POINTSCALE_A, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_POINTSCALE_B, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_POINTSCALE_C, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);
    SetRenderState(D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF);
    SetRenderState(D3DRS_PATCHEDGESTYLE, D3DPATCHEDGE_DISCRETE);
    SetRenderState(D3DRS_DEBUGMONITORTOKEN, D3DDMT_ENABLE);
    SetRenderState(D3DRS_POINTSIZE_MAX, bit::cast<DWORD>(64.0f));
    SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
    SetRenderState(D3DRS_COLORWRITEENABLE, 0x0000000F);
    SetRenderState(D3DRS_TWEENFACTOR, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    SetRenderState(D3DRS_POSITIONDEGREE, D3DDEGREE_CUBIC);
    SetRenderState(D3DRS_NORMALDEGREE, D3DDEGREE_LINEAR);
    SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, FALSE);
    SetRenderState(D3DRS_MINTESSELLATIONLEVEL, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_MAXTESSELLATIONLEVEL, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_ADAPTIVETESS_X, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_ADAPTIVETESS_Y, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_ADAPTIVETESS_Z, bit::cast<DWORD>(1.0f));
    SetRenderState(D3DRS_ADAPTIVETESS_W, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_ENABLEADAPTIVETESSELLATION, FALSE);
    SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
    SetRenderState(D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP);
    SetRenderState(D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_KEEP);
    SetRenderState(D3DRS_CCW_STENCILPASS, D3DSTENCILOP_KEEP);
    SetRenderState(D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS);
    SetRenderState(D3DRS_COLORWRITEENABLE1, 0x0000000F);
    SetRenderState(D3DRS_COLORWRITEENABLE2, 0x0000000F);
    SetRenderState(D3DRS_COLORWRITEENABLE3, 0x0000000F);
    SetRenderState(D3DRS_BLENDFACTOR, 0xFFFFFFFF);
    SetRenderState(D3DRS_SRGBWRITEENABLE, 0);
    SetRenderState(D3DRS_DEPTHBIAS, bit::cast<DWORD>(0.0f));
    SetRenderState(D3DRS_WRAP8, 0);
    SetRenderState(D3DRS_WRAP9, 0);
    SetRenderState(D3DRS_WRAP10, 0);
    SetRenderState(D3DRS_WRAP11, 0);
    SetRenderState(D3DRS_WRAP12, 0);
    SetRenderState(D3DRS_WRAP13, 0);
    SetRenderState(D3DRS_WRAP14, 0);
    SetRenderState(D3DRS_WRAP15, 0);
    SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
    SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_ZERO);
    SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);

    for (uint32_t i = 0; i < caps::MaxTextureBlendStages; i++)
    {
      SetTextureStageState(i, D3DTSS_COLOROP, i == 0 ? D3DTOP_MODULATE : D3DTOP_DISABLE);
      SetTextureStageState(i, D3DTSS_COLORARG1, D3DTA_TEXTURE);
      SetTextureStageState(i, D3DTSS_COLORARG2, D3DTA_CURRENT);
      SetTextureStageState(i, D3DTSS_ALPHAOP, i == 0 ? D3DTOP_SELECTARG1 : D3DTOP_DISABLE);
      SetTextureStageState(i, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
      SetTextureStageState(i, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
      SetTextureStageState(i, D3DTSS_BUMPENVMAT00, bit::cast<DWORD>(0.0f));
      SetTextureStageState(i, D3DTSS_BUMPENVMAT01, bit::cast<DWORD>(0.0f));
      SetTextureStageState(i, D3DTSS_BUMPENVMAT10, bit::cast<DWORD>(0.0f));
      SetTextureStageState(i, D3DTSS_BUMPENVMAT11, bit::cast<DWORD>(0.0f));
      SetTextureStageState(i, D3DTSS_TEXCOORDINDEX, i);
      SetTextureStageState(i, D3DTSS_BUMPENVLSCALE, bit::cast<DWORD>(0.0f));
      SetTextureStageState(i, D3DTSS_BUMPENVLOFFSET, bit::cast<DWORD>(0.0f));
      SetTextureStageState(i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
      SetTextureStageState(i, D3DTSS_COLORARG0, D3DTA_CURRENT);
      SetTextureStageState(i, D3DTSS_ALPHAARG0, D3DTA_CURRENT);
      SetTextureStageState(i, D3DTSS_RESULTARG, D3DTA_CURRENT);
      SetTextureStageState(i, D3DTSS_CONSTANT, 0x00000000);
    }

    forEachSampler([&](uint32_t i)
    {
      SetTexture(i, 0);
      SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
      SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
      SetSamplerState(i, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
      SetSamplerState(i, D3DSAMP_BORDERCOLOR, 0x00000000);
      SetSamplerState(i, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
      SetSamplerState(i, D3DSAMP_MINFILTER, D3DTEXF_POINT);
      SetSamplerState(i, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
      SetSamplerState(i, D3DSAMP_MIPMAPLODBIAS, 0);
      SetSamplerState(i, D3DSAMP_MAXMIPLEVEL, 0);
      SetSamplerState(i, D3DSAMP_MAXANISOTROPY, 1);
      SetSamplerState(i, D3DSAMP_SRGBTEXTURE, 0);
      SetSamplerState(i, D3DSAMP_ELEMENTINDEX, 0);
      SetSamplerState(i, D3DSAMP_DMAPOFFSET, 0);
    });

    for (uint32_t i = 0; i < caps::MaxClipPlanes; i++) {
      float plane[4] = { 0, 0, 0, 0 };
      SetClipPlane(i, plane);
    }

    Flush();
    SynchronizeCsThread();

    HRESULT hr;
    auto* implicitSwapchain = getInternalSwapchain(0);
    if (implicitSwapchain == nullptr) {
      Com<IDirect3DSwapChain9> swapchain;
      hr = CreateAdditionalSwapChain(pPresentationParameters, &swapchain);
      if (FAILED(hr))
        throw DxvkError("Reset: failed to create implicit swapchain");
    }
    else {
      hr = implicitSwapchain->Reset(pPresentationParameters);
      if (FAILED(hr))
        throw DxvkError("Reset: failed to reset swapchain");
    }

    Com<IDirect3DSurface9> backbuffer;
    hr = m_swapchains[0]->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
    if (FAILED(hr))
      throw DxvkError("Reset: failed to get implicit swapchain backbuffers");

    SetRenderTarget(0, backbuffer.ptr());

    if (pPresentationParameters->EnableAutoDepthStencil) {
      Com<IDirect3DSurface9> autoDepthStencil;
      CreateDepthStencilSurface(
        pPresentationParameters->BackBufferWidth,
        pPresentationParameters->BackBufferHeight,
        pPresentationParameters->AutoDepthStencilFormat,
        pPresentationParameters->MultiSampleType,
        pPresentationParameters->MultiSampleQuality,
        FALSE,
        &autoDepthStencil,
        nullptr);

      SetDepthStencilSurface(autoDepthStencil.ptr());
    }

    ShowCursor(FALSE);

    return D3D_OK;
  }

  HRESULT STDMETHODCALLTYPE Direct3DDevice9Ex::GetDisplayModeEx(
    UINT iSwapChain,
    D3DDISPLAYMODEEX* pMode,
    D3DDISPLAYROTATION* pRotation) {
    auto lock = LockDevice();

    auto* swapchain = getInternalSwapchain(iSwapChain);

    if (swapchain == nullptr)
      return D3DERR_INVALIDCALL;

    return swapchain->GetDisplayModeEx(pMode, pRotation);
  }

  bool Direct3DDevice9Ex::IsExtended() {
    return m_extended;
  }

  HWND Direct3DDevice9Ex::GetWindow() {
    return m_window;
  }

  Rc<DxvkEvent> Direct3DDevice9Ex::GetFrameSyncEvent() {
    uint32_t frameLatency = m_frameLatency;

    uint32_t frameId = m_frameId++ % frameLatency;
    return m_frameEvents[frameId];
  }

  DxvkDeviceFeatures Direct3DDevice9Ex::GetDeviceFeatures(const Rc<DxvkAdapter>& adapter) {
    DxvkDeviceFeatures supported = adapter->features();
    DxvkDeviceFeatures enabled = {};

    // Geometry shaders are used for some meta ops
    enabled.core.features.geometryShader = VK_TRUE;
    enabled.core.features.robustBufferAccess = VK_TRUE;

    enabled.extMemoryPriority.memoryPriority = supported.extMemoryPriority.memoryPriority;

    enabled.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor = supported.extVertexAttributeDivisor.vertexAttributeInstanceRateDivisor;
    enabled.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor = supported.extVertexAttributeDivisor.vertexAttributeInstanceRateZeroDivisor;

    // SM1 level hardware
    enabled.core.features.depthClamp = VK_TRUE;
    enabled.core.features.depthBiasClamp = VK_TRUE;
    enabled.core.features.fillModeNonSolid = VK_TRUE;
    enabled.core.features.pipelineStatisticsQuery = supported.core.features.pipelineStatisticsQuery;
    enabled.core.features.sampleRateShading = VK_TRUE;
    enabled.core.features.samplerAnisotropy = VK_TRUE;
    enabled.core.features.shaderClipDistance = VK_TRUE;
    enabled.core.features.shaderCullDistance = VK_TRUE;

    // Ensure we support real BC formats and unofficial vendor ones.
    enabled.core.features.textureCompressionBC = VK_TRUE;

    // SM2 level hardware
    enabled.core.features.occlusionQueryPrecise = VK_TRUE;

    // SM3 level hardware
    enabled.core.features.multiViewport = VK_TRUE;
    enabled.core.features.independentBlend = VK_TRUE;

    // D3D10 level hardware supports this in D3D9 native.
    enabled.core.features.fullDrawIndexUint32 = VK_TRUE;

    return enabled;
  }

  Direct3DSwapChain9Ex* Direct3DDevice9Ex::getInternalSwapchain(UINT index) {
    if (index >= m_swapchains.size())
      return nullptr;

    return static_cast<Direct3DSwapChain9Ex*>(m_swapchains[index]);
  }

  D3D9_VK_FORMAT_MAPPING Direct3DDevice9Ex::LookupFormat(
    D3D9Format            Format) const {
    return m_d3d9Formats.GetFormatMapping(Format);
  }

  bool Direct3DDevice9Ex::WaitForResource(
  const Rc<DxvkResource>&                 Resource,
        DWORD                             MapFlags) {
    // Wait for the any pending D3D9 command to be executed
    // on the CS thread so that we can determine whether the
    // resource is currently in use or not.

    SynchronizeCsThread();

    if (Resource->isInUse()) {
      if (MapFlags & D3DLOCK_DONOTWAIT) {
        // We don't have to wait, but misbehaving games may
        // still try to spin on `Map` until the resource is
        // idle, so we should flush pending commands
        FlushImplicit(FALSE);
        return false;
      }
      else {
        // Make sure pending commands using the resource get
        // executed on the the GPU if we have to wait for it
        Flush();
        SynchronizeCsThread();

        while (Resource->isInUse())
          dxvk::this_thread::yield();
      }
    }

    return true;
  }

  HRESULT Direct3DDevice9Ex::LockImage(
            Direct3DCommonTexture9* pResource,
            UINT                    Subresource,
            D3DLOCKED_BOX*          pLockedBox,
      const D3DBOX*                 pBox,
            DWORD                   Flags) {
    auto lock = LockDevice();

    const Rc<DxvkImage>  mappedImage  = pResource->GetImage();
    const Rc<DxvkBuffer> mappedBuffer = pResource->GetMappedBuffer();
    
    auto formatInfo = imageFormatInfo(mappedImage->info().format);
    auto subresource = pResource->GetSubresourceFromIndex(
        formatInfo->aspectMask, Subresource);
    
    pResource->SetMappedSubresource(subresource, Flags);

    uint32_t dimOffsets[3] = { 0, 0, 0 };
    if (pBox != nullptr) {
      dimOffsets[0] = formatInfo->elementSize * align(pBox->Left, formatInfo->blockSize.width);
      dimOffsets[1] = formatInfo->elementSize * align(pBox->Top, formatInfo->blockSize.height);
      dimOffsets[2] = formatInfo->elementSize * align(pBox->Front, formatInfo->blockSize.depth);
    }
    
    if (pResource->GetMapMode() == D3D9_COMMON_TEXTURE_MAP_MODE_DIRECT) {
      const VkImageType imageType = mappedImage->info().type;
      
      // Wait for the resource to become available
      if (!WaitForResource(mappedImage, Flags))
        return D3DERR_WASSTILLDRAWING;
      
      // Query the subresource's memory layout and hope that
      // the application respects the returned pitch values.
      VkSubresourceLayout layout  = mappedImage->querySubresourceLayout(subresource);
      pLockedBox->RowPitch   = imageType >= VK_IMAGE_TYPE_2D ? layout.rowPitch   : layout.size;
      pLockedBox->SlicePitch = imageType >= VK_IMAGE_TYPE_3D ? layout.depthPitch : layout.size;

      uint8_t* data = reinterpret_cast<uint8_t*>(mappedImage->mapPtr(layout.offset));
      data += dimOffsets[2] * pLockedBox->SlicePitch
           +  dimOffsets[1] * pLockedBox->RowPitch
           +  dimOffsets[0];
      pLockedBox->pBits = data;
      return S_OK;
    } else if (formatInfo->aspectMask == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
      VkExtent3D levelExtent = mappedImage->mipLevelExtent(subresource.mipLevel);

      // The actual Vulkan image format may differ
      // from the format requested by the application
      VkFormat packFormat = GetPackedDepthStencilFormat(pResource->Desc()->Format);
      auto packFormatInfo = imageFormatInfo(packFormat);

      // This is slow, but we have to dispatch a pack
      // operation and then immediately synchronize.
      EmitCs([
        cImageBuffer = mappedBuffer,
        cImage       = mappedImage,
        cSubresource = subresource,
        cFormat      = packFormat
      ] (DxvkContext* ctx) {
        auto layers = vk::makeSubresourceLayers(cSubresource);
        auto x = cImage->mipLevelExtent(cSubresource.mipLevel);

        VkOffset2D offset = { 0, 0 };
        VkExtent2D extent = { x.width, x.height };

        ctx->copyDepthStencilImageToPackedBuffer(
          cImageBuffer, 0, cImage, layers, offset, extent, cFormat);
      });

      WaitForResource(mappedBuffer, 0);

      DxvkBufferSliceHandle physSlice = mappedBuffer->getSliceHandle();
      pLockedBox->RowPitch   = packFormatInfo->elementSize * levelExtent.width;
      pLockedBox->SlicePitch = packFormatInfo->elementSize * levelExtent.width * levelExtent.height;

      uint8_t* data = reinterpret_cast<uint8_t*>(physSlice.mapPtr);
      data += dimOffsets[2] * pLockedBox->SlicePitch
           +  dimOffsets[1] * pLockedBox->RowPitch
           +  dimOffsets[0];
      pLockedBox->pBits = data;
      return S_OK;
    } else {
      VkExtent3D levelExtent = mappedImage->mipLevelExtent(subresource.mipLevel);
      VkExtent3D blockCount = util::computeBlockCount(levelExtent, formatInfo->blockSize);
      
      DxvkBufferSliceHandle physSlice;
      
      if (Flags & D3DLOCK_DISCARD) {
        // We do not have to preserve the contents of the
        // buffer if the entire image gets discarded.
        physSlice = mappedBuffer->allocSlice();
        
        EmitCs([
          cImageBuffer = mappedBuffer,
          cBufferSlice = physSlice
        ] (DxvkContext* ctx) {
          ctx->invalidateBuffer(cImageBuffer, cBufferSlice);
        });
      } else {
        // When using any map mode which requires the image contents
        // to be preserved, and if the GPU has write access to the
        // image, copy the current image contents into the buffer.
        const bool copyExistingData = pResource->Desc()->Pool == D3DPOOL_SYSTEMMEM || pResource->Desc()->Pool == D3DPOOL_SCRATCH;
        
        if (copyExistingData) {
          auto subresourceLayers = vk::makeSubresourceLayers(subresource);
          
          EmitCs([
            cImageBuffer  = mappedBuffer,
            cImage        = mappedImage,
            cSubresources = subresourceLayers,
            cLevelExtent  = levelExtent
          ] (DxvkContext* ctx) {
            ctx->copyImageToBuffer(
              cImageBuffer, 0, VkExtent2D { 0u, 0u },
              cImage, cSubresources, VkOffset3D { 0, 0, 0 },
              cLevelExtent);
          });
        }
        
        WaitForResource(mappedBuffer, 0);
        physSlice = mappedBuffer->getSliceHandle();
      }
      
      // Set up map pointer. Data is tightly packed within the mapped buffer.
      pLockedBox->RowPitch   = formatInfo->elementSize * blockCount.width;
      pLockedBox->SlicePitch = formatInfo->elementSize * blockCount.width * blockCount.height;

      uint8_t* data = reinterpret_cast<uint8_t*>(physSlice.mapPtr);
      data += dimOffsets[2] * pLockedBox->SlicePitch
           +  dimOffsets[1] * pLockedBox->RowPitch
           +  dimOffsets[0];
      pLockedBox->pBits = data;
      return S_OK;
    }
  }

  HRESULT Direct3DDevice9Ex::UnlockImage(
        Direct3DCommonTexture9* pResource,
        UINT                    Subresource) {
    auto lock = LockDevice();

    if (pResource->GetMapFlags() & D3DLOCK_READONLY)
      return D3D_OK;

    if (pResource->GetMapMode() == D3D9_COMMON_TEXTURE_MAP_MODE_BUFFER) {
      // Now that data has been written into the buffer,
      // we need to copy its contents into the image
      const Rc<DxvkImage>  mappedImage = pResource->GetImage();
      const Rc<DxvkBuffer> mappedBuffer = pResource->GetMappedBuffer();

      VkImageSubresource subresource = pResource->GetMappedSubresource();

      VkExtent3D levelExtent = mappedImage
        ->mipLevelExtent(subresource.mipLevel);

      VkImageSubresourceLayers subresourceLayers = {
        subresource.aspectMask,
        subresource.mipLevel,
        subresource.arrayLayer, 1 };

      EmitCs([
        cSrcBuffer = mappedBuffer,
          cDstImage = mappedImage,
          cDstLayers = subresourceLayers,
          cDstLevelExtent = levelExtent
      ] (DxvkContext* ctx) {
          ctx->copyBufferToImage(cDstImage, cDstLayers,
            VkOffset3D{ 0, 0, 0 }, cDstLevelExtent,
            cSrcBuffer, 0, { 0u, 0u });
        });
    }

    pResource->ClearMappedSubresource();

    return D3D_OK;
  }

  HRESULT Direct3DDevice9Ex::LockBuffer(
          Direct3DCommonBuffer9*  pResource,
          UINT                    OffsetToLock,
          UINT                    SizeToLock,
          void**                  ppbData,
          DWORD                   Flags) {
    auto lock = LockDevice();

    if (ppbData == nullptr)
      return D3DERR_INVALIDCALL;

    pResource->SetMapFlags(Flags);

    if (Flags & D3DLOCK_DISCARD) {
      // Allocate a new backing slice for the buffer and set
      // it as the 'new' mapped slice. This assumes that the
      // only way to invalidate a buffer is by mapping it.

      // TODO: Investigate locking regions rather than the whole resource.
      auto physSlice = pResource->DiscardMapSlice();
      uint8_t* data =  reinterpret_cast<uint8_t*>(physSlice.mapPtr);
               data += OffsetToLock;

      *ppbData = reinterpret_cast<void*>(data);

      EmitCs([
        cBuffer      = pResource->GetBuffer(D3D9_COMMON_BUFFER_TYPE_MAPPING),
        cBufferSlice = physSlice
      ] (DxvkContext * ctx) {
          ctx->invalidateBuffer(cBuffer, cBufferSlice);
      });

      return D3D_OK;
    }
    else {
      // Wait until the resource is no longer in use
      if (!(Flags & D3DLOCK_NOOVERWRITE) && pResource->GetMapMode() == D3D9_COMMON_BUFFER_MAP_MODE_DIRECT) {
        if (!WaitForResource(pResource->GetBuffer(D3D9_COMMON_BUFFER_TYPE_REAL), Flags))
          return D3DERR_WASSTILLDRAWING;
      }

      // Use map pointer from previous map operation. This
      // way we don't have to synchronize with the CS thread
      // if the map mode is D3DLOCK_NOOVERWRITE.
      DxvkBufferSliceHandle physSlice = pResource->GetMappedSlice();

      uint8_t* data =  reinterpret_cast<uint8_t*>(physSlice.mapPtr);
               data += OffsetToLock;

      *ppbData = reinterpret_cast<void*>(data);

      return D3D_OK;
    }
  }

  HRESULT Direct3DDevice9Ex::UnlockBuffer(
        Direct3DCommonBuffer9* pResource) {
    auto lock = LockDevice();

    if (pResource->GetMapMode() != D3D9_COMMON_BUFFER_MAP_MODE_BUFFER)
      return D3D_OK;

    if (pResource->SetMapFlags(0) & D3DLOCK_READONLY)
      return D3D_OK;

    FlushImplicit(FALSE);

    auto dstBuffer = pResource->GetBufferSlice(D3D9_COMMON_BUFFER_TYPE_REAL);
    auto srcBuffer = pResource->GetBufferSlice(D3D9_COMMON_BUFFER_TYPE_STAGING);

    EmitCs([
      cDstSlice = dstBuffer,
      cSrcSlice = srcBuffer
    ] (DxvkContext * ctx) {
      ctx->copyBuffer(
        cDstSlice.buffer(),
        cDstSlice.offset(),
        cSrcSlice.buffer(),
        cSrcSlice.offset(),
        cSrcSlice.length());
    });

    return D3D_OK;
  }

  void Direct3DDevice9Ex::EmitCsChunk(DxvkCsChunkRef&& chunk) {
    m_csThread.dispatchChunk(std::move(chunk));
    m_csIsBusy = true;
  }


  void Direct3DDevice9Ex::FlushImplicit(BOOL StrongHint) {
    // Flush only if the GPU is about to go idle, in
    // order to keep the number of submissions low.
    if (StrongHint || m_device->pendingSubmissions() <= MaxPendingSubmits) {
      auto now = std::chrono::high_resolution_clock::now();

      // Prevent flushing too often in short intervals.
      if (now - m_lastFlush >= std::chrono::microseconds(MinFlushIntervalUs))
        Flush();
    }
  }

  void Direct3DDevice9Ex::SynchronizeCsThread() {
    auto lock = LockDevice();

    // Dispatch current chunk so that all commands
    // recorded prior to this function will be run
    FlushCsChunk();

    m_csThread.synchronize();
  }

  void Direct3DDevice9Ex::Flush() {
    auto lock = LockDevice();

    if (m_csIsBusy || m_csChunk->commandCount() != 0) {
      // Add commands to flush the threaded
      // context, then flush the command list
      EmitCs([](DxvkContext* ctx) {
        ctx->flushCommandList();
      });

      FlushCsChunk();

      // Reset flush timer used for implicit flushes
      m_lastFlush = std::chrono::high_resolution_clock::now();
      m_csIsBusy = false;
    }
  }

  void Direct3DDevice9Ex::BindFramebuffer() {
    DxvkRenderTargets attachments;

    // D3D9 doesn't have the concept of a framebuffer object,
    // so we'll just create a new one every time the render
    // target bindings are updated. Set up the attachments.
    for (UINT i = 0; i < m_state.renderTargets.size(); i++) {
      if (m_state.renderTargets.at(i) != nullptr) {
        attachments.color[i] = {
          m_state.renderTargets.at(i)->GetRenderTargetView(false), // TODO: SRGB-ness here. Use state when that is hooked up.
          m_state.renderTargets.at(i)->GetRenderTargetLayout(false) };
      }
    }

    if (m_state.depthStencil != nullptr) {
      attachments.depth = {
        m_state.depthStencil->GetDepthStencilView(),
        m_state.depthStencil->GetDepthLayout() };
    }

    // Create and bind the framebuffer object to the context
    EmitCs([
      cAttachments = std::move(attachments)
    ] (DxvkContext * ctx) {
        ctx->bindRenderTargets(cAttachments, false);
      });
  }

  void Direct3DDevice9Ex::BindViewportAndScissor() {
    VkViewport viewport;
    VkRect2D scissor;

    // D3D9's coordinate system has its origin in the bottom left,
    // but the viewport coordinates are aligned to the top-left
    // corner so we can get away with flipping the viewport.
    const D3DVIEWPORT9& vp = m_state.viewport;

    viewport = VkViewport{
      float(vp.X),       float(vp.Height + vp.Y),
      float(vp.Width),   -float(vp.Height),
      vp.MinZ,           vp.MaxZ,
    };

    // Scissor rectangles. Vulkan does not provide an easy way
    // to disable the scissor test, so we'll have to set scissor
    // rects that are at least as large as the framebuffer.
    bool enableScissorTest = false;

    // TODO: Hook up scissor rect enabled when we get state in.

    if (enableScissorTest) {
      RECT sr = m_state.scissorRect;

      VkOffset2D srPosA;
      srPosA.x = std::max<int32_t>(0, sr.left);
      srPosA.y = std::max<int32_t>(0, sr.top);

      VkOffset2D srPosB;
      srPosB.x = std::max<int32_t>(srPosA.x, sr.right);
      srPosB.y = std::max<int32_t>(srPosA.y, sr.bottom);

      VkExtent2D srSize;
      srSize.width = uint32_t(srPosB.x - srPosA.x);
      srSize.height = uint32_t(srPosB.y - srPosA.y);

      scissor = VkRect2D{ srPosA, srSize };
    }
    else {
      scissor = VkRect2D{
        VkOffset2D { 0, 0 },
        VkExtent2D {
          16383,
          16383 } };
    }

    EmitCs([
      cViewport = viewport,
      cScissor = scissor
    ] (DxvkContext * ctx) {
      ctx->setViewports(
        1,
        &cViewport,
        &cScissor);
    });
  }

}
