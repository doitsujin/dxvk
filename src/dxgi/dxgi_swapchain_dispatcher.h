#pragma once

#include "dxgi_swapchain.h"

namespace dxvk {

  class DxgiSwapChainDispatcher : public IDXGISwapChain4 {

  public:

    DxgiSwapChainDispatcher(IDXGISwapChain4* dispatch, IUnknown* device)
      : m_device(device),
        m_dispatch(dispatch) {
    }

    virtual ~DxgiSwapChainDispatcher() {
    }

    DXVK_HOTPATCHABLE
    ULONG STDMETHODCALLTYPE AddRef() {
      if (likely(m_dispatch != nullptr))
        return m_dispatch->AddRef();

      return 0;
    }

    DXVK_HOTPATCHABLE
    ULONG STDMETHODCALLTYPE Release() {
      if (likely(m_dispatch != nullptr)) {
        ULONG refCount = m_dispatch->Release();

        if (unlikely(!refCount)) {
          // Clear the underlying swap chain pointer. Helps in
          // use-after-free situations encountered in Divinity
          // Original Sin Enhanced Edition.
          m_dispatch = nullptr;
          delete this;
        }

        return refCount;
      }

      return ~0u;
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                    riid,
            void**                    ppvObject) final {
      if (ppvObject == nullptr)
        return E_POINTER;

      *ppvObject = nullptr;

      if (riid == __uuidof(IUnknown)
       || riid == __uuidof(IDXGIObject)
       || riid == __uuidof(IDXGIDeviceSubObject)
       || riid == __uuidof(IDXGISwapChain)
       || riid == __uuidof(IDXGISwapChain1)
       || riid == __uuidof(IDXGISwapChain2)
       || riid == __uuidof(IDXGISwapChain3)
       || riid == __uuidof(IDXGISwapChain4)) {
        *ppvObject = ref(this);
        return S_OK;
      }

      if (logQueryInterfaceError(__uuidof(IDXGISwapChain), riid)) {
        Logger::warn("DxgiSwapChainDispatcher::QueryInterface: Unknown interface query");
        Logger::warn(str::format(riid));
      }

      return m_dispatch->QueryInterface(riid, ppvObject);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID       Name,
            UINT*         pDataSize,
            void*         pData) final {
      return m_dispatch->GetPrivateData(Name, pDataSize, pData);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID       Name,
            UINT          DataSize,
      const void*         pData) final {
      return m_dispatch->SetPrivateData(Name, DataSize, pData);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID       Name,
      const IUnknown*     pUnknown) final {
      return m_dispatch->SetPrivateDataInterface(Name, pUnknown);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetParent(
            REFIID                    riid,
            void**                    ppParent) final {
      return m_dispatch->GetParent(riid, ppParent);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetDevice(
            REFIID                    riid,
            void**                    ppDevice) final {
      return m_dispatch->GetDevice(riid, ppDevice);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetBuffer(
            UINT                      Buffer,
            REFIID                    riid,
            void**                    ppSurface) final {
      return m_dispatch->GetBuffer(Buffer, riid, ppSurface);
    }

    DXVK_HOTPATCHABLE
    UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex() final {
      return m_dispatch->GetCurrentBackBufferIndex();
    }

    HRESULT STDMETHODCALLTYPE GetContainingOutput(
            IDXGIOutput**             ppOutput) final {
      return m_dispatch->GetContainingOutput(ppOutput);
    }

    HRESULT STDMETHODCALLTYPE GetDesc(
            DXGI_SWAP_CHAIN_DESC*     pDesc) final {
      return m_dispatch->GetDesc(pDesc);
    }

    HRESULT STDMETHODCALLTYPE GetDesc1(
            DXGI_SWAP_CHAIN_DESC1*    pDesc) final {
      return m_dispatch->GetDesc1(pDesc);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetFullscreenState(
            BOOL*                     pFullscreen,
            IDXGIOutput**             ppTarget) final {
      if (likely(m_dispatch != nullptr))
        return m_dispatch->GetFullscreenState(pFullscreen, ppTarget);

      if (pFullscreen)
        *pFullscreen = FALSE;

      if (ppTarget)
        *ppTarget = nullptr;

      return S_OK;
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetFullscreenDesc(
            DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc) final {
      return m_dispatch->GetFullscreenDesc(pDesc);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetHwnd(
            HWND*                     pHwnd) final {
      return m_dispatch->GetHwnd(pHwnd);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetCoreWindow(
            REFIID                    refiid,
            void**                    ppUnk) final {
      return m_dispatch->GetCoreWindow(refiid, ppUnk);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetBackgroundColor(
            DXGI_RGBA*                pColor) final {
      return m_dispatch->GetBackgroundColor(pColor);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetRotation(
            DXGI_MODE_ROTATION*       pRotation) final {
      return m_dispatch->GetRotation(pRotation);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetRestrictToOutput(
            IDXGIOutput**             ppRestrictToOutput) final {
      return m_dispatch->GetRestrictToOutput(ppRestrictToOutput);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(
            DXGI_FRAME_STATISTICS*    pStats) final {
      return m_dispatch->GetFrameStatistics(pStats);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetLastPresentCount(
            UINT*                     pLastPresentCount) final {
      return m_dispatch->GetLastPresentCount(pLastPresentCount);
    }


    DXVK_HOTPATCHABLE
    BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() final {
      return m_dispatch->IsTemporaryMonoSupported();
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE Present(
            UINT                      SyncInterval,
            UINT                      Flags) final {
      return m_dispatch->Present(SyncInterval, Flags);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE Present1(
            UINT                      SyncInterval,
            UINT                      PresentFlags,
      const DXGI_PRESENT_PARAMETERS*  pPresentParameters) final {
      return m_dispatch->Present1(SyncInterval, PresentFlags, pPresentParameters);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE ResizeBuffers(
            UINT                      BufferCount,
            UINT                      Width,
            UINT                      Height,
            DXGI_FORMAT               NewFormat,
            UINT                      SwapChainFlags) final {
      return m_dispatch->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE ResizeBuffers1(
            UINT                      BufferCount,
            UINT                      Width,
            UINT                      Height,
            DXGI_FORMAT               Format,
            UINT                      SwapChainFlags,
      const UINT*                     pCreationNodeMask,
            IUnknown* const*          ppPresentQueue) final {
      return m_dispatch->ResizeBuffers1(BufferCount, Width, Height, Format, SwapChainFlags, pCreationNodeMask, ppPresentQueue);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE ResizeTarget(
      const DXGI_MODE_DESC*           pNewTargetParameters) final {
      return m_dispatch->ResizeTarget(pNewTargetParameters);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE SetFullscreenState(
            BOOL                      Fullscreen,
            IDXGIOutput*              pTarget) final {
      if (likely(m_dispatch != nullptr))
        return m_dispatch->SetFullscreenState(Fullscreen, pTarget);
      return S_OK;
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE SetBackgroundColor(
      const DXGI_RGBA*                pColor) final {
      return m_dispatch->SetBackgroundColor(pColor);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE SetRotation(
            DXGI_MODE_ROTATION        Rotation) final {
      return m_dispatch->SetRotation(Rotation);
    }

    DXVK_HOTPATCHABLE
    HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() final {
      return m_dispatch->GetFrameLatencyWaitableObject();
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetMatrixTransform(
            DXGI_MATRIX_3X2_F*        pMatrix) final {
      return m_dispatch->GetMatrixTransform(pMatrix);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(
            UINT*                     pMaxLatency) final {
      return m_dispatch->GetMaximumFrameLatency(pMaxLatency);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE GetSourceSize(
            UINT*                     pWidth,
            UINT*                     pHeight) final {
      return m_dispatch->GetSourceSize(pWidth, pHeight);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE SetMatrixTransform(
      const DXGI_MATRIX_3X2_F*        pMatrix) final {
      return m_dispatch->SetMatrixTransform(pMatrix);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(
            UINT                      MaxLatency) final {
      return m_dispatch->SetMaximumFrameLatency(MaxLatency);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE SetSourceSize(
            UINT                      Width,
            UINT                      Height) final {
      return m_dispatch->SetSourceSize(Width, Height);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(
            DXGI_COLOR_SPACE_TYPE     ColorSpace,
            UINT*                     pColorSpaceSupport) final {
      return m_dispatch->CheckColorSpaceSupport(ColorSpace, pColorSpaceSupport);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE SetColorSpace1(
            DXGI_COLOR_SPACE_TYPE     ColorSpace) final {
      return m_dispatch->SetColorSpace1(ColorSpace);
    }

    DXVK_HOTPATCHABLE
    HRESULT STDMETHODCALLTYPE SetHDRMetaData(
            DXGI_HDR_METADATA_TYPE    Type,
            UINT                      Size,
            void*                     pMetaData) final {
      return m_dispatch->SetHDRMetaData(Type, Size, pMetaData);
    }

  private:

    // m_device is not used or reference counted, provided for compatibility with mod engines
    // which expect to find device pointer in the memory after swapchain interface.
    IUnknown* m_device;

    IDXGISwapChain4* m_dispatch;

  };

}
