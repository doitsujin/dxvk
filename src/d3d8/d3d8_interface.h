#pragma once

#include "d3d8_include.h"
#include "d3d8_d3d9_util.h"
#include "d3d8_options.h"
#include "d3d8_format.h"
#include "../d3d9/d3d9_bridge.h"

namespace dxvk {

  /**
  * \brief D3D8 interface implementation
  *
  * Implements the IDirect3DDevice8 interfaces
  * which provides the way to get adapters and create other objects such as \ref IDirect3DDevice8.
  * similar to \ref DxgiFactory but for D3D8.
  */
  class D3D8Interface final : public ComObjectClamp<IDirect3D8> {

    static constexpr d3d9::D3DFORMAT ADAPTER_FORMATS[] = {
      d3d9::D3DFMT_A1R5G5B5,
      //d3d9::D3DFMT_A2R10G10B10, (not in D3D8)
      d3d9::D3DFMT_A8R8G8B8,
      d3d9::D3DFMT_R5G6B5,
      d3d9::D3DFMT_X1R5G5B5,
      d3d9::D3DFMT_X8R8G8B8
    };

  public:
    D3D8Interface();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void* pInitializeFunction) {
      return m_d3d9->RegisterSoftwareDevice(pInitializeFunction);
    }

    UINT STDMETHODCALLTYPE GetAdapterCount() {
      return m_d3d9->GetAdapterCount();
    }

    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(
      UINT                    Adapter,
      DWORD                   Flags,
      D3DADAPTER_IDENTIFIER8* pIdentifier);

    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT Adapter) {
      return m_adapterModeCounts[Adapter];
    }

    HRESULT STDMETHODCALLTYPE EnumAdapterModes(
      UINT            Adapter,
      UINT            Mode,
      D3DDISPLAYMODE* pMode);

    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode) {
      return m_d3d9->GetAdapterDisplayMode(Adapter, (d3d9::D3DDISPLAYMODE*)pMode);
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceType(
        UINT        Adapter,
        D3DDEVTYPE  DevType,
        D3DFORMAT   AdapterFormat,
        D3DFORMAT   BackBufferFormat,
        BOOL        bWindowed) {
      // Ignore the bWindowed parameter when querying D3D9. D3D8 does
      // identical validations between windowed and fullscreen modes, adhering
      // to the stricter fullscreen adapter and back buffer format validations.
      return m_d3d9->CheckDeviceType(
          Adapter,
          (d3d9::D3DDEVTYPE)DevType,
          (d3d9::D3DFORMAT)AdapterFormat,
          (d3d9::D3DFORMAT)BackBufferFormat,
          FALSE
      );
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(
        UINT            Adapter,
        D3DDEVTYPE      DeviceType,
        D3DFORMAT       AdapterFormat,
        DWORD           Usage,
        D3DRESOURCETYPE RType,
        D3DFORMAT       CheckFormat) {
      return m_d3d9->CheckDeviceFormat(
        Adapter,
        (d3d9::D3DDEVTYPE)DeviceType,
        (d3d9::D3DFORMAT)AdapterFormat,
        Usage,
        (d3d9::D3DRESOURCETYPE)RType,
        (d3d9::D3DFORMAT)CheckFormat
      );
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(
            UINT                Adapter,
            D3DDEVTYPE          DeviceType,
            D3DFORMAT           SurfaceFormat,
            BOOL                Windowed,
            D3DMULTISAMPLE_TYPE MultiSampleType) {
      DWORD* pQualityLevels = nullptr;
      return m_d3d9->CheckDeviceMultiSampleType(
        Adapter,
        (d3d9::D3DDEVTYPE)DeviceType,
        (d3d9::D3DFORMAT)SurfaceFormat,
        Windowed,
        (d3d9::D3DMULTISAMPLE_TYPE)MultiSampleType,
        pQualityLevels
      );
    }

    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        D3DFORMAT AdapterFormat,
        D3DFORMAT RenderTargetFormat,
        D3DFORMAT DepthStencilFormat) {
      if (isSupportedDepthStencilFormat(DepthStencilFormat))
        return m_d3d9->CheckDepthStencilMatch(
          Adapter,
          (d3d9::D3DDEVTYPE)DeviceType,
          (d3d9::D3DFORMAT)AdapterFormat,
          (d3d9::D3DFORMAT)RenderTargetFormat,
          (d3d9::D3DFORMAT)DepthStencilFormat
        );

      return D3DERR_NOTAVAILABLE;
    }

    HRESULT STDMETHODCALLTYPE GetDeviceCaps(
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        D3DCAPS8* pCaps) {
      if (unlikely(pCaps == nullptr))
        return D3DERR_INVALIDCALL;

      d3d9::D3DCAPS9 caps9;
      HRESULT res = m_d3d9->GetDeviceCaps(Adapter, (d3d9::D3DDEVTYPE)DeviceType, &caps9);

      if (likely(SUCCEEDED(res)))
        ConvertCaps8(caps9, pCaps);

      return res;
    }

    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT Adapter) {
      return m_d3d9->GetAdapterMonitor(Adapter);
    }

    HRESULT STDMETHODCALLTYPE CreateDevice(
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        HWND hFocusWindow,
        DWORD BehaviorFlags,
        D3DPRESENT_PARAMETERS* pPresentationParameters,
        IDirect3DDevice8** ppReturnedDeviceInterface);


    const D3D8Options& GetOptions() { return m_d3d8Options; }

  private:

    UINT                                            m_adapterCount;
    std::vector<UINT>                               m_adapterModeCounts;
    std::vector<std::vector<d3d9::D3DDISPLAYMODE>>  m_adapterModes;

    Com<d3d9::IDirect3D9>                           m_d3d9;
    Com<IDxvkD3D8InterfaceBridge>                   m_bridge;
    D3D8Options                                     m_d3d8Options;
  };

}