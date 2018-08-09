#include "d3d9_texture.h"

#include "d3d9_device.h"
#include "d3d9_format.h"

namespace dxvk {
  HRESULT D3D9Device::CreateTexture(UINT Width, UINT Height, UINT Levels,
    DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
    IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) {
    CHECK_NOT_NULL(ppTexture);
    CHECK_SHARED_HANDLE(pSharedHandle);

    if (Usage != 0) {
      // TODO
      Logger::err("Texture usage flags are not yet supported");
      Logger::err(str::format("Usage flags: ", Usage));
      return D3DERR_INVALIDCALL;
    }

    if (Pool != D3DPOOL_MANAGED) {
      // TODO
      Logger::err("Only managed pool is supported for textures for now");
      return D3DERR_INVALIDCALL;
    }

    D3D11_TEXTURE2D_DESC desc;

    desc.Width = Width;
    desc.Height = Height;

    desc.MipLevels = Levels;
    desc.ArraySize = 1;

    desc.Format = SurfaceFormatToDXGIFormat(Format);

    // Fortunately for us multisampled textures don't exist in D3D9.
    desc.SampleDesc = { 1, 0 };

    // TODO: determine what we should set these flags to,
    // based on the `Usage` flags.
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    Com<ID3D11Texture2D> texture;
    if (FAILED(m_device->CreateTexture2D(&desc, nullptr, &texture))) {
      Logger::err("Failed to create 2D texture");
      return D3DERR_DRIVERINTERNALERROR;
    }

    auto surface = Com(new D3D9Surface(this, texture.ptr(), 0));

    *ppTexture = new D3D9Texture(std::move(surface));

    return D3D_OK;
  }

  D3D9Texture::D3D9Texture(Com<D3D9Surface>&& surface)
    : m_surface(std::move(surface)) {
    InitParent(m_surface.ptr());
  }

  HRESULT D3D9Texture::QueryInterface(REFIID riid, void** ppvObject) {
    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDirect3DResource9)
      || riid == __uuidof(IDirect3DBaseTexture9) || riid == __uuidof(IDirect3DTexture9)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    Logger::warn("D3D9Texture::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
    return E_NOINTERFACE;
  }

  D3DRESOURCETYPE D3D9Texture::GetType() {
    return D3DRTYPE_TEXTURE;
  }

  HRESULT D3D9Texture::GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Texture::GetSurfaceLevel(UINT Level, IDirect3DSurface9** ppSurfaceLevel) {
    CHECK_NOT_NULL(ppSurfaceLevel);

    if (Level > 0) {
      Logger::err("Texture sub-levels not yet supported");
      return D3DERR_INVALIDCALL;
    }

    *ppSurfaceLevel = m_surface.ref();

    return D3D_OK;
  }

  HRESULT D3D9Texture::AddDirtyRect(const RECT* pDirtyRect) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Texture::LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect,
    const RECT* pRect, DWORD Flags) {
    CHECK_NOT_NULL(pLockedRect);

    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }

  HRESULT D3D9Texture::UnlockRect(UINT Level) {
    Logger::err(str::format(__func__, " stub"));
    throw DxvkError("Not supported");
  }
}
