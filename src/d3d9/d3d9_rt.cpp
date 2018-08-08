#include "d3d9_device.h"

// Macro to ensure a given render target's index is within the maximum.
#define CHECK_RT_INDEX(index) { if ((index) > D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT) { return D3DERR_INVALIDCALL; } }

namespace dxvk {
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

    m_renderTarget = pRenderTarget;

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

    *ppRenderTarget = m_renderTarget.ref();

    return D3D_OK;
  }

  HRESULT D3D9Device::GetRenderTargetData(IDirect3DSurface9* pRenderTarget,
    IDirect3DSurface9* pDestSurface) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
}
