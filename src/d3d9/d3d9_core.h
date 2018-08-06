#pragma once

#include "d3d9_include.h"
#include "d3d9_adapter.h"

#include <vector>

class IDXGIFactory1;

namespace dxvk {
  class Direct3D9: public ComObject<IDirect3D9> {
  public:
    Direct3D9();
    virtual ~Direct3D9();

    bool ValidAdapter(UINT adapter);

    // Utility function, returns a given graphics adapter.
    D3D9Adapter& GetAdapter(UINT adapter);

    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(
      void *pInitializeFunction) override;

    HRESULT STDMETHODCALLTYPE QueryInterface(
      REFIID riid, void** ppvObject) override;

    UINT STDMETHODCALLTYPE GetAdapterCount() override;

    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT Adapter, DWORD Flags,
      D3DADAPTER_IDENTIFIER9* pIdentifier) override;

    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT Adapter,
      D3DFORMAT Format) override;

    HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT Adapter, D3DFORMAT Format,
      UINT Mode, D3DDISPLAYMODE* pMode) override;

    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT Adapter,
      D3DDISPLAYMODE* pMode) override;

    HRESULT STDMETHODCALLTYPE CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType,
      D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
      BOOL bWindowed) override;

    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DevType,
      D3DFORMAT AdapterFormat, DWORD Usage,
      D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) override;

    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DevType,
      D3DFORMAT SurfaceFormat, BOOL Windowed,
      D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels) override;

    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DevType,
      D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat,
      D3DFORMAT DepthStencilFormat) override;

    HRESULT STDMETHODCALLTYPE CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DevType,
      D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) override;

    HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT Adapter, D3DDEVTYPE DevType,
      D3DCAPS9* pCaps) override;

    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT Adapter) override;

    HRESULT STDMETHODCALLTYPE CreateDevice(UINT Adapter, D3DDEVTYPE DevType,
      HWND hFocusWindow, DWORD BehaviorFlags,
      D3DPRESENT_PARAMETERS* pPresentationParameters,
      IDirect3DDevice9** ppReturnedDeviceInterface) override;

  private:
    Com<IDXGIFactory1> m_factory;
    std::vector<D3D9Adapter> m_adapters;
  };
}
