#pragma once

#include "d3d9_resource.h"

namespace dxvk {
  /// This abstract class is the base of all the other texture classes.
  /// It doesn't have a real D3D11 correspondent, but it can be thought as
  /// the equivalent of the DXVK `D3D11CommonTexture` class.
  template <typename T>
  class D3D9TextureBase: public D3D9Resource<T> {
  public:
    DWORD STDMETHODCALLTYPE GetLevelCount() final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    void STDMETHODCALLTYPE GenerateMipSubLevels() final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    DWORD STDMETHODCALLTYPE GetLOD() final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }
  };

  /// Simply a 2D texture.
  class D3D9Texture final: public ComObject<D3D9TextureBase<IDirect3DTexture9>> {
  public:
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) final override;
    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT Level, IDirect3DSurface9** ppSurfaceLevel) final override;

    HRESULT STDMETHODCALLTYPE AddDirtyRect(const RECT* pDirtyRect) final override;
    HRESULT STDMETHODCALLTYPE LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect,
      const RECT* pRect, DWORD Flags) final override;
    HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level) final override;
  };
}
