#include "d3d9_rt.h"

#include "d3d9_device.h"

// Macro to ensure a given render target's index is within the maximum.
#define CHECK_RT_INDEX(index) { if ((index) > D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT) { return D3DERR_INVALIDCALL; } }

namespace dxvk {
  D3D9RenderTarget::D3D9RenderTarget(IDirect3DDevice9* parent, ID3D11Texture2D* surface,
    Com<ID3D11RenderTargetView>&& view)
    : D3D9Surface(parent, surface, D3DUSAGE_RENDERTARGET), m_view(std::move(view)) {
  }

  HRESULT D3D9Device::CreateDefaultRT() {
    // Get the back buffer surface.
    Com<ID3D11Texture2D> backBufferSurface;
    if (FAILED(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBufferSurface))) {
      Logger::err("Failed to get back buffer");
      return D3DERR_DRIVERINTERNALERROR;
    }

    // Create the RT view.
    Com<ID3D11RenderTargetView> view;
    if (FAILED(m_device->CreateRenderTargetView(backBufferSurface.ref(), nullptr, &view))) {
      Logger::err("Failed to create render target view");
      return D3DERR_DRIVERINTERNALERROR;
    }

    // Create the actual object.
    // Note that we can't use CreateRenderTarget,
    // since we use the swap chain's existing surface.
    auto rt = new D3D9RenderTarget(this, backBufferSurface.ptr(), std::move(view));

    // Propagate the changes.
    if (FAILED(SetRenderTarget(0, rt))) {
      Logger::err("Failed to set default render target");
      return D3DERR_DRIVERINTERNALERROR;
    }

    return D3D_OK;
  }

  // This function creates a new render target.
  // In D3D9, only 2D textures are render targets.
  HRESULT D3D9Device::CreateRenderTarget(UINT Width, UINT Height,
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

  // This function updates a single render target.
  HRESULT D3D9Device::SetRenderTarget(DWORD RenderTargetIndex,
    IDirect3DSurface9* pRenderTarget) {
    CHECK_RT_INDEX(RenderTargetIndex);

    // Default render target must never be set to null.
    if (RenderTargetIndex == 0) {
      CHECK_NOT_NULL(pRenderTarget);
    }

    if (RenderTargetIndex > 0) {
      Logger::err("Multiple render targets not yet supported");
      return D3DERR_INVALIDCALL;
    }

    m_renderTarget = static_cast<D3D9RenderTarget*>(pRenderTarget);

    // TODO: update the Output Merger state.

    return D3D_OK;
  }

  HRESULT D3D9Device::GetRenderTarget(DWORD RenderTargetIndex,
    IDirect3DSurface9** ppRenderTarget) {
    CHECK_RT_INDEX(RenderTargetIndex);
    InitReturnPtr(ppRenderTarget);
    CHECK_NOT_NULL(ppRenderTarget);

    if (RenderTargetIndex > 0) {
      Logger::err("Multiple render targets not yet supported");
      return D3DERR_INVALIDCALL;
    }

    if (!m_renderTarget.ptr()) {
      Logger::err("Requested inexistent render target");
      return D3DERR_NOTFOUND;
    }

    *ppRenderTarget = m_renderTarget.ref();

    return D3D_OK;
  }

  HRESULT D3D9Device::GetRenderTargetData(IDirect3DSurface9* pRenderTarget,
    IDirect3DSurface9* pDestSurface) {
    CHECK_NOT_NULL(pRenderTarget);
    CHECK_NOT_NULL(pDestSurface);

    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
}
