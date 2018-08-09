#pragma once

#include "d3d9_resource.h"

namespace dxvk {
  /// This abstract class is the base of all the other texture classes.
  /// It doesn't have a real D3D11 correspondent, but it can be thought as
  /// the equivalent of the DXVK `D3D11CommonTexture` class.
  template <typename T>
  class D3D9TextureBase: public D3D9Resource<T> {
  public:
    virtual ~D3D9TextureBase() = default;

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
}
