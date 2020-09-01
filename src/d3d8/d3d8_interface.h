#pragma once

// Implements IDirect3D8

#include "d3d8_include.h"
#include "d3d8_d3d9_util.h"

//#include "../dxvk/dxvk_instance.h"

namespace dxvk {

  /**
  * \brief D3D8 interface implementation
  *
  * Implements the IDirect3DDevice8 interfaces
  * which provides the way to get adapters and create other objects such as \ref IDirect3DDevice8.
  * similar to \ref DxgiFactory but for D3D8.
  */
  class D3D8InterfaceEx final : public ComObjectClamp<IDirect3D8> {

    static constexpr d3d9::D3DFORMAT ADAPTER_FORMATS[] = {
      d3d9::D3DFMT_A1R5G5B5,
      //d3d9::D3DFMT_A2R10G10B10, (not in D3D8)
      d3d9::D3DFMT_A8R8G8B8,
      d3d9::D3DFMT_R5G6B5,
      d3d9::D3DFMT_X1R5G5B5,
      d3d9::D3DFMT_X8R8G8B8
    };

  public:
    D3D8InterfaceEx(UINT SDKVersion);

    // IUnknown methods //
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    // IDirect3D8 methods //

    // TODO: RegisterSoftwareDevice //
    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void* pInitializeFunction) {
      dxvk::Logger::warn("D3D8InterfaceEx::RegisterSoftwareDevice: stub");
      return D3DERR_INVALIDCALL;
    }

    UINT STDMETHODCALLTYPE GetAdapterCount() {
      return m_d3d9ex->GetAdapterCount();
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
      return m_d3d9ex->GetAdapterDisplayMode(Adapter, (d3d9::D3DDISPLAYMODE*)pMode);
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceType(
        UINT        Adapter,
        D3DDEVTYPE  DevType,
        D3DFORMAT   AdapterFormat,
        D3DFORMAT   BackBufferFormat,
        BOOL        bWindowed) {
      return m_d3d9ex->CheckDeviceType(
          Adapter,
          (d3d9::D3DDEVTYPE)DevType,
          (d3d9::D3DFORMAT)AdapterFormat,
          (d3d9::D3DFORMAT)BackBufferFormat,
          bWindowed
      );
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(
        UINT            Adapter,
        D3DDEVTYPE      DeviceType,
        D3DFORMAT       AdapterFormat,
        DWORD           Usage,
        D3DRESOURCETYPE RType,
        D3DFORMAT       CheckFormat) {
      return m_d3d9ex->CheckDeviceFormat(
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
      return m_d3d9ex->CheckDeviceMultiSampleType(
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
      return m_d3d9ex->CheckDepthStencilMatch(
        Adapter,
        (d3d9::D3DDEVTYPE)DeviceType,
        (d3d9::D3DFORMAT)AdapterFormat,
        (d3d9::D3DFORMAT)RenderTargetFormat,
        (d3d9::D3DFORMAT)DepthStencilFormat
      );
    }

    HRESULT STDMETHODCALLTYPE GetDeviceCaps(
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        D3DCAPS8* pCaps) {
      d3d9::D3DCAPS9 caps9;
      HRESULT res = m_d3d9ex->GetDeviceCaps(Adapter, (d3d9::D3DDEVTYPE)DeviceType, &caps9);
      dxvk::ConvertCaps8(caps9, pCaps);
      return res;
    }

    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT Adapter) {
      return m_d3d9ex->GetAdapterMonitor(Adapter);
    }

    HRESULT STDMETHODCALLTYPE CreateDevice(
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        HWND hFocusWindow,
        DWORD BehaviorFlags,
        D3DPRESENT_PARAMETERS* pPresentationParameters,
        IDirect3DDevice8** ppReturnedDeviceInterface);


    //Rc<DxvkInstance> GetInstance() { return m_instance; }

  private:

    UINT m_adapterCount;
    std::vector<UINT> m_adapterModeCounts;
    std::vector<std::vector<d3d9::D3DDISPLAYMODE>> m_adapterModes;

    //void CacheModes(D3D9Format Format);

    //static const char* GetDriverDllName(DxvkGpuVendor vendor);

    d3d9::IDirect3D9Ex* m_d3d9ex;

    bool m_extended;

    //D3D9Options m_d3d9Options;

    //std::vector<D3D9Adapter> m_adapters;
  };

} // namespace dxvk