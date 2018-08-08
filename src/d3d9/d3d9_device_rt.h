#pragma once

#include "d3d9_device.h"
#include "d3d9_surface.h"

namespace dxvk {
  /// (Multiple) Render Target support.
  class D3D9DeviceRenderTarget: public virtual IDirect3DDevice9, public virtual D3D9Device {
  public:
    ~D3D9DeviceRenderTarget();

    HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT Width, UINT Height,
      D3DFORMAT Format,
      D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality,
      BOOL Lockable,
      IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) final override;

    HRESULT STDMETHODCALLTYPE SetRenderTarget(DWORD RenderTargetIndex,
      IDirect3DSurface9 *pRenderTarget) final override;

    HRESULT STDMETHODCALLTYPE GetRenderTarget(DWORD RenderTargetIndex,
      IDirect3DSurface9** ppRenderTarget) final override;

    HRESULT STDMETHODCALLTYPE GetRenderTargetData(IDirect3DSurface9* pRenderTarget,
      IDirect3DSurface9* pDestSurface) final override;

    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT Width, UINT Height,
      D3DFORMAT Format,
      D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality,
      BOOL Discard,
      IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle);

    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) final override;

    HRESULT STDMETHODCALLTYPE SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) final override;

  protected:
    // (Re)Creates the render target associated with the back buffer,
    // and stores it at the first render target index.
    // Can also create the default depth / stencil buffer, if requested.
    void CreateBackBufferRT(BOOL AutoDepthStencil, D3DFORMAT DepthStencil);

  private:
    // Rebinds all of the render targets.
    // This should be called to synchronise D3D9 state with D3D11 state.
    void UpdateOutputMergerState();

    /// How many render targets to allow.
    static constexpr auto MAX_RTS = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;

    std::array<Com<D3D9Surface>, MAX_RTS> m_rts{};
  };
}
