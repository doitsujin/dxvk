#pragma once

#include "../dxvk/dxvk_instance.h"

#include "d3d9_include.h"
#include "d3d9_format.h"
#include "d3d9_options.h"

namespace dxvk {

  /**
  * \brief D3D9 interface implementation
  *
  * Implements the IDirect3DDevice9Ex interfaces
  * which provides the way to get adapters and create other objects such as \ref IDirect3DDevice9Ex.
  * similar to \ref DxgiFactory but for D3D9.
  */
  class Direct3D9Ex final : public ComObject<IDirect3D9Ex> {

  public:

    Direct3D9Ex(bool extended);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void* pInitializeFunction);

    UINT STDMETHODCALLTYPE GetAdapterCount();

    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(
      UINT Adapter,
      DWORD Flags,
      D3DADAPTER_IDENTIFIER9* pIdentifier);

    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT Adapter, D3DFORMAT Format);

    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode);

    HRESULT STDMETHODCALLTYPE CheckDeviceType(
      UINT Adapter,
      D3DDEVTYPE DevType,
      D3DFORMAT AdapterFormat,
      D3DFORMAT BackBufferFormat,
      BOOL bWindowed);

    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(
      UINT Adapter,
      D3DDEVTYPE DeviceType,
      D3DFORMAT AdapterFormat,
      DWORD Usage,
      D3DRESOURCETYPE RType,
      D3DFORMAT CheckFormat);

    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(
      UINT Adapter,
      D3DDEVTYPE DeviceType,
      D3DFORMAT SurfaceFormat,
      BOOL Windowed,
      D3DMULTISAMPLE_TYPE MultiSampleType,
      DWORD* pQualityLevels);

    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(
      UINT Adapter,
      D3DDEVTYPE DeviceType,
      D3DFORMAT AdapterFormat,
      D3DFORMAT RenderTargetFormat,
      D3DFORMAT DepthStencilFormat);

    HRESULT STDMETHODCALLTYPE CheckDeviceFormatConversion(
      UINT Adapter,
      D3DDEVTYPE DeviceType,
      D3DFORMAT SourceFormat,
      D3DFORMAT TargetFormat);

    HRESULT STDMETHODCALLTYPE GetDeviceCaps(
      UINT Adapter,
      D3DDEVTYPE DeviceType,
      D3DCAPS9* pCaps);

    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT Adapter);

    HRESULT STDMETHODCALLTYPE CreateDevice(
      UINT Adapter,
      D3DDEVTYPE DeviceType,
      HWND hFocusWindow,
      DWORD BehaviorFlags,
      D3DPRESENT_PARAMETERS* pPresentationParameters,
      IDirect3DDevice9** ppReturnedDeviceInterface);

    HRESULT STDMETHODCALLTYPE EnumAdapterModes(
      UINT Adapter,
      D3DFORMAT Format,
      UINT Mode,
      D3DDISPLAYMODE* pMode);

    // Ex Methods

    UINT STDMETHODCALLTYPE GetAdapterModeCountEx(UINT Adapter, CONST D3DDISPLAYMODEFILTER* pFilter);

    HRESULT STDMETHODCALLTYPE EnumAdapterModesEx(
      UINT Adapter,
      CONST D3DDISPLAYMODEFILTER* pFilter,
      UINT Mode,
      D3DDISPLAYMODEEX* pMode);

    HRESULT STDMETHODCALLTYPE GetAdapterDisplayModeEx(
      UINT Adapter,
      D3DDISPLAYMODEEX* pMode,
      D3DDISPLAYROTATION* pRotation);

    HRESULT STDMETHODCALLTYPE CreateDeviceEx(
      UINT Adapter,
      D3DDEVTYPE DeviceType,
      HWND hFocusWindow,
      DWORD BehaviorFlags,
      D3DPRESENT_PARAMETERS* pPresentationParameters,
      D3DDISPLAYMODEEX* pFullscreenDisplayMode,
      IDirect3DDevice9Ex** ppReturnedDeviceInterface);

    HRESULT STDMETHODCALLTYPE GetAdapterLUID(UINT Adapter, LUID* pLUID);

  private:

    HRESULT createDeviceInternal(
      bool extended,
      UINT adapter,
      D3DDEVTYPE deviceType,
      HWND window,
      DWORD flags,
      D3DPRESENT_PARAMETERS* presentParams,
      D3DDISPLAYMODEEX* displayMode,
      IDirect3DDevice9Ex** outDevice);

    void cacheModes(D3D9Format enumFormat);

    std::vector<D3DDISPLAYMODEEX> m_modes;
    D3D9Format m_modeCacheFormat;

    Rc<DxvkInstance> m_instance;

    bool m_extended;

    D3D9Options m_d3d9Options;

  };

}