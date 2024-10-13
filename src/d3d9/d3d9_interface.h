#pragma once

#include "d3d9_adapter.h"
#include "d3d9_bridge.h"
#include "d3d9_interop.h"

#include "../dxvk/dxvk_instance.h"

namespace dxvk {

  /**
  * \brief D3D9 interface implementation
  *
  * Implements the IDirect3DDevice9Ex interfaces
  * which provides the way to get adapters and create other objects such as \ref IDirect3DDevice9Ex.
  * similar to \ref DxgiFactory but for D3D9.
  */
  class D3D9InterfaceEx final : public ComObjectClamp<IDirect3D9Ex> {

  public:

    D3D9InterfaceEx(bool bExtended);

    ~D3D9InterfaceEx();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void* pInitializeFunction);

    UINT STDMETHODCALLTYPE GetAdapterCount();

    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(
            UINT                    Adapter,
            DWORD                   Flags,
            D3DADAPTER_IDENTIFIER9* pIdentifier);

    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT Adapter, D3DFORMAT Format);

    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode);

    HRESULT STDMETHODCALLTYPE CheckDeviceType(
            UINT       Adapter,
            D3DDEVTYPE DevType,
            D3DFORMAT  AdapterFormat,
            D3DFORMAT  BackBufferFormat,
            BOOL       bWindowed);

    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(
            UINT            Adapter,
            D3DDEVTYPE      DeviceType,
            D3DFORMAT       AdapterFormat,
            DWORD           Usage,
            D3DRESOURCETYPE RType,
            D3DFORMAT       CheckFormat);

    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(
            UINT                Adapter,
            D3DDEVTYPE          DeviceType,
            D3DFORMAT           SurfaceFormat,
            BOOL                Windowed,
            D3DMULTISAMPLE_TYPE MultiSampleType,
            DWORD*              pQualityLevels);

    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(
            UINT       Adapter,
            D3DDEVTYPE DeviceType,
            D3DFORMAT  AdapterFormat,
            D3DFORMAT  RenderTargetFormat,
            D3DFORMAT  DepthStencilFormat);

    HRESULT STDMETHODCALLTYPE CheckDeviceFormatConversion(
            UINT       Adapter,
            D3DDEVTYPE DeviceType,
            D3DFORMAT  SourceFormat,
            D3DFORMAT  TargetFormat);

    HRESULT STDMETHODCALLTYPE GetDeviceCaps(
            UINT       Adapter,
            D3DDEVTYPE DeviceType,
            D3DCAPS9*  pCaps);

    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT Adapter);

    HRESULT STDMETHODCALLTYPE CreateDevice(
            UINT                   Adapter,
            D3DDEVTYPE             DeviceType,
            HWND                   hFocusWindow,
            DWORD                  BehaviorFlags,
            D3DPRESENT_PARAMETERS* pPresentationParameters,
            IDirect3DDevice9**     ppReturnedDeviceInterface);

    HRESULT STDMETHODCALLTYPE EnumAdapterModes(
            UINT            Adapter,
            D3DFORMAT       Format,
            UINT            Mode,
            D3DDISPLAYMODE* pMode);

    // Ex Methods

    UINT STDMETHODCALLTYPE GetAdapterModeCountEx(UINT Adapter, CONST D3DDISPLAYMODEFILTER* pFilter);

    HRESULT STDMETHODCALLTYPE EnumAdapterModesEx(
            UINT                  Adapter,
      const D3DDISPLAYMODEFILTER* pFilter,
            UINT                  Mode,
            D3DDISPLAYMODEEX*     pMode);

    HRESULT STDMETHODCALLTYPE GetAdapterDisplayModeEx(
            UINT                Adapter,
            D3DDISPLAYMODEEX*   pMode,
            D3DDISPLAYROTATION* pRotation);

    HRESULT STDMETHODCALLTYPE CreateDeviceEx(
            UINT                   Adapter,
            D3DDEVTYPE             DeviceType,
            HWND                   hFocusWindow,
            DWORD                  BehaviorFlags,
            D3DPRESENT_PARAMETERS* pPresentationParameters,
            D3DDISPLAYMODEEX*      pFullscreenDisplayMode,
            IDirect3DDevice9Ex**   ppReturnedDeviceInterface);

    HRESULT STDMETHODCALLTYPE GetAdapterLUID(UINT Adapter, LUID* pLUID);

    HRESULT ValidatePresentationParameters(D3DPRESENT_PARAMETERS* pPresentationParameters) {
      if (m_extended) {
        // The swap effect value on a D3D9Ex device
        // can not be higher than D3DSWAPEFFECT_FLIPEX.
        if (unlikely(pPresentationParameters->SwapEffect > D3DSWAPEFFECT_FLIPEX))
          return D3DERR_INVALIDCALL;
      } else {
        // The swap effect value on a non-Ex D3D9 device
        // can not be higher than D3DSWAPEFFECT_COPY.
        if (unlikely(pPresentationParameters->SwapEffect > D3DSWAPEFFECT_COPY))
          return D3DERR_INVALIDCALL;
      }

      // The swap effect value can not be 0.
      if (unlikely(!pPresentationParameters->SwapEffect))
        return D3DERR_INVALIDCALL;

      // D3DSWAPEFFECT_COPY can not be used with more than one back buffer.
      // Allow D3DSWAPEFFECT_COPY to bypass this restriction in D3D8 compatibility
      // mode, since it may be a remapping of D3DSWAPEFFECT_COPY_VSYNC and RC Cars
      // depends on it not being validated.
      if (unlikely(!IsD3D8Compatible()
                && pPresentationParameters->SwapEffect == D3DSWAPEFFECT_COPY
                && pPresentationParameters->BackBufferCount > 1))
        return D3DERR_INVALIDCALL;

      // 3 is the highest supported back buffer count.
      if (unlikely(pPresentationParameters->BackBufferCount > 3))
        return D3DERR_INVALIDCALL;

      // Valid fullscreen presentation intervals must be known values.
      if (unlikely(!pPresentationParameters->Windowed
              && !(pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_DEFAULT
                || pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_ONE
                || pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_TWO
                || pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_THREE
                || pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_FOUR
                || pPresentationParameters->PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE)))
        return D3DERR_INVALIDCALL;

      return D3D_OK;
    }

    const D3D9Options& GetOptions() { return m_d3d9Options; }

    D3D9Adapter* GetAdapter(UINT Ordinal) {
      return Ordinal < m_adapters.size()
        ? &m_adapters[Ordinal]
        : nullptr;
    }

    bool IsExtended() { return m_extended; }

    bool IsD3D8Compatible() const {
      return m_isD3D8Compatible;
    }

    void SetD3D8CompatibilityMode(bool compatMode) {
      if (compatMode)
        Logger::info("The D3D9 interface is now operating in D3D8 compatibility mode.");

      m_isD3D8Compatible = compatMode;
    }

    Rc<DxvkInstance> GetInstance() { return m_instance; }

  private:

    Rc<DxvkInstance>              m_instance;

    DxvkD3D8InterfaceBridge       m_d3d8Bridge;

    bool                          m_extended;

    bool                          m_isD3D8Compatible = false;

    D3D9Options                   m_d3d9Options;

    std::vector<D3D9Adapter>      m_adapters;

    D3D9VkInteropInterface        m_d3d9Interop;

  };

}