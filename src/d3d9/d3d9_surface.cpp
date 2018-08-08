#include "d3d9_surface.h"

#include "d3d9_format.h"
#include "d3d9_device.h"

namespace dxvk {
  D3D9Surface::D3D9Surface(IDirect3DDevice9* parent, ID3D11Texture2D* surface,
    DWORD usage)
    : m_surface(surface), m_usage(usage) {
    InitParent(parent);
  }

  HRESULT D3D9Surface::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
      || riid == __uuidof(IDirect3DSurface9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D9Surface::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  HRESULT D3D9Surface::GetDesc(D3DSURFACE_DESC* pDesc) {
    CHECK_NOT_NULL(pDesc);

    auto& d = *pDesc;

    D3D11_TEXTURE2D_DESC td;
    m_surface->GetDesc(&td);

    d.Type = GetType();
    d.Usage = m_usage;

    d.Format = DXGIFormatToSurfaceFormat(td.Format);
    // TODO: remember and return the original memory pool type.
    d.Pool = D3DPOOL_MANAGED;

    const auto& ms = td.SampleDesc;
    d.MultiSampleType = static_cast<D3DMULTISAMPLE_TYPE>(ms.Count);
    d.MultiSampleQuality = ms.Quality;

    d.Width = td.Width;
    d.Height = td.Height;

    return D3D_OK;
  }

  D3DRESOURCETYPE D3D9Surface::GetType() {
    return D3DRTYPE_SURFACE;
  }

  // This function returns the interface of the parent container,
  // if this surface were a level in a mip-map or a texture in a 2D array texture or 3D texture.
  // For back-buffers and such, it returns a pointer to the parent device.
  HRESULT D3D9Surface::GetContainer(REFIID riid, void** ppContainer) {
    InitReturnPtr(ppContainer);
    CHECK_NOT_NULL(ppContainer);

    Logger::err("D3D9Surface::GetContainer not implemented");
    return D3DERR_INVALIDCALL;
  }

  HRESULT D3D9Surface::LockRect(D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags) {
      CHECK_NOT_NULL(pLockedRect);

      Logger::err("Surface locking not yet implemented");

      return D3DERR_INVALIDCALL;
  }

  HRESULT D3D9Surface::UnlockRect() {
      Logger::err("Surface locking not yet implemented");

      return D3DERR_INVALIDCALL;
  }

  // TODO: if DXVK implements IDXGISurface1 / adds support for GDI, then we could
  // also support it here. Until then these methods are just stubs.

  HRESULT D3D9Surface::GetDC(HDC* pHDC) {
    InitReturnPtr(pHDC);
    CHECK_NOT_NULL(pHDC);

    Logger::err("GDI interop is not supported");

    return D3DERR_INVALIDCALL;
  }

  HRESULT D3D9Surface::ReleaseDC(HDC hDC) {
    CHECK_NOT_NULL(hDC);

    Logger::err("GDI interop is not supported");

    return D3DERR_INVALIDCALL;
  }

  HRESULT D3D9Device::StretchRect(IDirect3DSurface9* pSourceSurface, const RECT* pSourceRect,
    IDirect3DSurface9* pDestSurface, const RECT* pDestRect,
    D3DTEXTUREFILTERTYPE Filter) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Device::UpdateSurface(IDirect3DSurface9 *pSourceSurface, const RECT* pSourceRect,
      IDirect3DSurface9* pDestinationSurface, const POINT* pDestPoint) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
}
