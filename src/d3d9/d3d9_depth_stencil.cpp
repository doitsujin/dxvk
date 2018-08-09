#include "d3d9_depth_stencil.h"

#include "d3d9_device.h"
#include "d3d9_format.h"
#include "d3d9_multisample.h"

namespace dxvk {
  D3D9DepthStencil::D3D9DepthStencil(IDirect3DDevice9* parent, ID3D11Texture2D* surface,
    Com<ID3D11DepthStencilView>&& view)
    : D3D9Surface(parent, surface, D3DUSAGE_DEPTHSTENCIL), m_view(std::move(view)) {
  }

  HRESULT D3D9Device::CreateAutoDepthStencil(const D3DPRESENT_PARAMETERS& pp) {
    // We can call CreateDepthStencilSurface with the right parameters
    // to do the heavy work for us.
    Com<IDirect3DSurface9> depthStencil;
    const auto result = CreateDepthStencilSurface(
      pp.BackBufferWidth, pp.BackBufferHeight,
      pp.AutoDepthStencilFormat,
      pp.MultiSampleType, pp.MultiSampleQuality,
      // TODO: the docs don't really tell us what to set this parameter to
      // in case we automatically create the d/s surface.
      true,
      &depthStencil, nullptr);

    if (FAILED(result)) {
      Logger::err("Failed to create auto depth / stencil surface");
      return D3DERR_DRIVERINTERNALERROR;
    }

    if (FAILED(SetDepthStencilSurface(depthStencil.ptr()))) {
      Logger::err("Failed to set auto depth / stencil surface");
      return D3DERR_DRIVERINTERNALERROR;
    }

    return D3D_OK;
  }

  HRESULT D3D9Device::CreateDepthStencilSurface(UINT Width, UINT Height,
    D3DFORMAT Format, D3DMULTISAMPLE_TYPE MSType, DWORD MSQuality,
    BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    CHECK_NOT_NULL(ppSurface);
    CHECK_SHARED_HANDLE(pSharedHandle);

    D3D11_TEXTURE2D_DESC textureDesc;

    textureDesc.Width = Width;
    textureDesc.Height = Height;

    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;

    textureDesc.Format = SurfaceFormatToDXGIFormat(Format);
    textureDesc.SampleDesc = D3D9ToDXGISampleDesc(MSType, MSQuality);

    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = 0;

    Com<ID3D11Texture2D> dsTexture;
    if (FAILED(m_device->CreateTexture2D(&textureDesc, nullptr, &dsTexture))) {
      Logger::err("Failed to create depth / stencil texture");
      return D3DERR_DRIVERINTERNALERROR;
    }

    Com<ID3D11DepthStencilView> dsView;
    if (FAILED(m_device->CreateDepthStencilView(dsTexture.ref(), nullptr, &dsView))) {
      Logger::err("Failed to create depth / stencil view");
      return D3DERR_DRIVERINTERNALERROR;
    }

    auto ds = new D3D9DepthStencil(this, dsTexture.ptr(), std::move(dsView));

    *ppSurface = ds;

    return D3D_OK;
  }

  HRESULT D3D9Device::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) {
    CHECK_NOT_NULL(ppZStencilSurface);

    if (!m_depthStencil.ptr()) {
      Logger::err("Requested inexistent depth / stencil buffer");
      return D3DERR_NOTFOUND;
    }

    *ppZStencilSurface = m_depthStencil.ref();

    return D3D_OK;
  }

  HRESULT D3D9Device::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) {
    m_depthStencil = static_cast<D3D9DepthStencil*>(ref(pNewZStencil));

    // TODO: update the Output Merger state.
    return D3D_OK;
  }
}
