#include "d3d9_device_rt.h"

#define CHECK_RT_INDEX(index) { if ((index) > 8) { return D3DERR_INVALIDCALL; } }

namespace dxvk {
  D3D9DeviceRenderTarget::~D3D9DeviceRenderTarget() = default;

  // Typedefs for common types.
  using RTViews = std::array<Com<ID3D11RenderTargetView>, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT>;
  using DSView = Com<ID3D11DepthStencilView>;

  // This function creates a new render target.
  // In D3D9, only 2D textures are render targets.
  HRESULT D3D9DeviceRenderTarget::CreateRenderTarget(UINT Width, UINT Height,
      D3DFORMAT Format,
      D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality,
      BOOL Lockable,
      IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    InitReturnPtr(ppSurface);
    CHECK_NOT_NULL(ppSurface);
    CHECK_SHARED_HANDLE(pSharedHandle);

    Logger::err(str::format(__func__, " stub"));

    throw DxvkError("Not supported");
  }

  HRESULT D3D9DeviceRenderTarget::SetRenderTarget(DWORD RenderTargetIndex,
    IDirect3DSurface9* pRenderTarget) {
    CHECK_RT_INDEX(RenderTargetIndex);
    if (RenderTargetIndex == 0) {
      CHECK_NOT_NULL(pRenderTarget);
    }

    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9DeviceRenderTarget::GetRenderTarget(DWORD RenderTargetIndex,
    IDirect3DSurface9** ppRenderTarget) {
    CHECK_RT_INDEX(RenderTargetIndex);
    InitReturnPtr(ppRenderTarget);
    CHECK_NOT_NULL(ppRenderTarget);

    *ppRenderTarget = m_rts[RenderTargetIndex].ref();

    return D3D_OK;
  }

  HRESULT D3D9DeviceRenderTarget::GetRenderTargetData(IDirect3DSurface9* pRenderTarget,
    IDirect3DSurface9* pDestSurface) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9DeviceRenderTarget::CreateDepthStencilSurface(UINT Width, UINT Height,
    D3DFORMAT Format,
    D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality,
    BOOL Discard,
    IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9DeviceRenderTarget::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9DeviceRenderTarget::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  void D3D9DeviceRenderTarget::CreateBackBufferRT(BOOL AutoDepthStencil, D3DFORMAT DepthStencil) {
    // Retrieve the back buffer from the swap chain.
    Com<ID3D11Texture2D> backBuffer;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);

    // Create a surface for the render target.
    const auto usage = D3DUSAGE_RENDERTARGET;
    const Com<D3D9Surface> surface = new D3D9Surface(this, backBuffer.ptr(), usage);

    Com<ID3D11RenderTargetView> rtView;

    // Create the RT view.
    if (FAILED(m_device->CreateRenderTargetView(backBuffer.ptr(), nullptr, &rtView)))
      throw DxvkError("Failed to create render target");

    SetInterface(surface.ptr(), rtView.ref());

    m_rts[0] = surface;

    // TODO: support auto creating the depth / stencil buffer.

    UpdateOutputMergerState();
  }

  void D3D9DeviceRenderTarget::UpdateOutputMergerState() {
    std::array<ID3D11RenderTargetView*, MAX_RTS> rtViews{};

    for (UINT i = 0; i < MAX_RTS; ++i) {
      const auto rt = m_rts[i].ptr();
      if (rt) {
        const auto view = GetInterface<ID3D11RenderTargetView>(rt);
        rtViews[i] = view;
      }
    }

    const auto dsView = nullptr;

    m_ctx->OMSetRenderTargets(rtViews.size(), rtViews.data(), dsView);
  }
}
