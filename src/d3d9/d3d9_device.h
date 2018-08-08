#pragma once

#include "d3d9_include.h"
#include "d3d9_adapter.h"

namespace dxvk {
  /// Class representing a logical graphics device.
  ///
  /// This class is abstract, use the factory function to create it.
  ///
  /// To simplify implementation of this huge class and help with modularity,
  /// it is broken up into multiple parts which virtually inherit from it.
  class D3D9Device: public virtual IDirect3DDevice9 {
  public:
    virtual ~D3D9Device();

    HRESULT STDMETHODCALLTYPE TestCooperativeLevel() final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS *pPresentationParameters) final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }


    UINT STDMETHODCALLTYPE GetAvailableTextureMem() final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

    HRESULT STDMETHODCALLTYPE EvictManagedResources() final override {
      Logger::err(str::format(__func__, " stub"));
      throw DxvkError("Not supported");
    }

  protected:
    // This constructor should be called first in the class hierarchy.
    // It initializes the associated D3D11 device.
    D3D9Device(D3D9Adapter& adapter, HWND hFocusWindow, D3DPRESENT_PARAMETERS& pp);

    D3D9Adapter& m_adapter;
    Com<ID3D11Device> m_device;
    Com<ID3D11DeviceContext> m_ctx;
  };
}
