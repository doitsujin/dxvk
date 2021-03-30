#pragma once

#include "dxgi_swapchain.h"

namespace dxvk {

  class DxgiSwapChainDispatcher : public IDXGISwapChain4 {

  public:

    DxgiSwapChainDispatcher(IDXGISwapChain4* dispatch)
      : m_dispatch(dispatch) {
    }

    virtual ~DxgiSwapChainDispatcher() {
    }

    ULONG STDMETHODCALLTYPE AddRef() {
      return m_dispatch->AddRef();
    }

    ULONG STDMETHODCALLTYPE Release() {
      ULONG refCount = m_dispatch->Release();

      if (unlikely(!refCount))
        delete this;

      return refCount;
    }

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

      Logger::warn("DxgiSwapChainDispatcher::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
      return m_dispatch->QueryInterface(riid, ppvObject);
    }

    HRESULT STDMETHODCALLTYPE GetPrivateData(
            REFGUID       Name,
            UINT*         pDataSize,
            void*         pData) final {
      return m_dispatch->GetPrivateData(Name, pDataSize, pData);
    }

    HRESULT STDMETHODCALLTYPE SetPrivateData(
            REFGUID       Name,
            UINT          DataSize,
      const void*         pData) final {
      return m_dispatch->SetPrivateData(Name, DataSize, pData);
    }

    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
            REFGUID       Name,
      const IUnknown*     pUnknown) final {
      return m_dispatch->SetPrivateDataInterface(Name, pUnknown);
    }

    HRESULT STDMETHODCALLTYPE GetParent(
            REFIID                    riid,
            void**                    ppParent) final {
      return m_dispatch->GetParent(riid, ppParent);
    }

    HRESULT STDMETHODCALLTYPE GetDevice(
            REFIID                    riid,
            void**                    ppDevice) final {
      return m_dispatch->GetDevice(riid, ppDevice);
    }

    HRESULT STDMETHODCALLTYPE GetBuffer(
            UINT                      Buffer,
            REFIID                    riid,
            void**                    ppSurface) final {
      return m_dispatch->GetBuffer(Buffer, riid, ppSurface);
    }

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

    HRESULT STDMETHODCALLTYPE GetFullscreenState(
            BOOL*                     pFullscreen,
            IDXGIOutput**             ppTarget) final {
      return m_dispatch->GetFullscreenState(pFullscreen, ppTarget);
    }

    HRESULT STDMETHODCALLTYPE GetFullscreenDesc(
            DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc) final {
      return m_dispatch->GetFullscreenDesc(pDesc);
    }

    HRESULT STDMETHODCALLTYPE GetHwnd(
            HWND*                     pHwnd) final {
      return m_dispatch->GetHwnd(pHwnd);
    }

    HRESULT STDMETHODCALLTYPE GetCoreWindow(
            REFIID                    refiid,
            void**                    ppUnk) final {
      return m_dispatch->GetCoreWindow(refiid, ppUnk);
    }

    HRESULT STDMETHODCALLTYPE GetBackgroundColor(
            DXGI_RGBA*                pColor) final {
      return m_dispatch->GetBackgroundColor(pColor);
    }

    HRESULT STDMETHODCALLTYPE GetRotation(
            DXGI_MODE_ROTATION*       pRotation) final {
      return m_dispatch->GetRotation(pRotation);
    }

    HRESULT STDMETHODCALLTYPE GetRestrictToOutput(
            IDXGIOutput**             ppRestrictToOutput) final {
      return m_dispatch->GetRestrictToOutput(ppRestrictToOutput);
    }

    HRESULT STDMETHODCALLTYPE GetFrameStatistics(
            DXGI_FRAME_STATISTICS*    pStats) final {
      return m_dispatch->GetFrameStatistics(pStats);
    }

    HRESULT STDMETHODCALLTYPE GetLastPresentCount(
            UINT*                     pLastPresentCount) final {
      return m_dispatch->GetLastPresentCount(pLastPresentCount);
    }

    BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() final {
      return m_dispatch->IsTemporaryMonoSupported();
    }

    HRESULT STDMETHODCALLTYPE Present(
            UINT                      SyncInterval,
            UINT                      Flags) final {
      return m_dispatch->Present(SyncInterval, Flags);
    }

    HRESULT STDMETHODCALLTYPE Present1(
            UINT                      SyncInterval,
            UINT                      PresentFlags,
      const DXGI_PRESENT_PARAMETERS*  pPresentParameters) final {
      return m_dispatch->Present1(SyncInterval, PresentFlags, pPresentParameters);
    }

    HRESULT STDMETHODCALLTYPE ResizeBuffers(
            UINT                      BufferCount,
            UINT                      Width,
            UINT                      Height,
            DXGI_FORMAT               NewFormat,
            UINT                      SwapChainFlags) final {
      return m_dispatch->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);
    }

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

    HRESULT STDMETHODCALLTYPE ResizeTarget(
      const DXGI_MODE_DESC*           pNewTargetParameters) final {
      return m_dispatch->ResizeTarget(pNewTargetParameters);
    }

    HRESULT STDMETHODCALLTYPE SetFullscreenState(
            BOOL                      Fullscreen,
            IDXGIOutput*              pTarget) final {
      return m_dispatch->SetFullscreenState(Fullscreen, pTarget);
    }


    HRESULT STDMETHODCALLTYPE SetBackgroundColor(
      const DXGI_RGBA*                pColor) final {
      return m_dispatch->SetBackgroundColor(pColor);
    }

    HRESULT STDMETHODCALLTYPE SetRotation(
            DXGI_MODE_ROTATION        Rotation) final {
      return m_dispatch->SetRotation(Rotation);
    }

    HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() final {
      return m_dispatch->GetFrameLatencyWaitableObject();
    }

    HRESULT STDMETHODCALLTYPE GetMatrixTransform(
            DXGI_MATRIX_3X2_F*        pMatrix) final {
      return m_dispatch->GetMatrixTransform(pMatrix);
    }

    HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(
            UINT*                     pMaxLatency) final {
      return m_dispatch->GetMaximumFrameLatency(pMaxLatency);
    }

    HRESULT STDMETHODCALLTYPE GetSourceSize(
            UINT*                     pWidth,
            UINT*                     pHeight) final {
      return m_dispatch->GetSourceSize(pWidth, pHeight);
    }

    HRESULT STDMETHODCALLTYPE SetMatrixTransform(
      const DXGI_MATRIX_3X2_F*        pMatrix) final {
      return m_dispatch->SetMatrixTransform(pMatrix);
    }

    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(
            UINT                      MaxLatency) final {
      return m_dispatch->SetMaximumFrameLatency(MaxLatency);
    }

    HRESULT STDMETHODCALLTYPE SetSourceSize(
            UINT                      Width,
            UINT                      Height) final {
      return m_dispatch->SetSourceSize(Width, Height);
    }

    HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(
            DXGI_COLOR_SPACE_TYPE     ColorSpace,
            UINT*                     pColorSpaceSupport) final {
      return m_dispatch->CheckColorSpaceSupport(ColorSpace, pColorSpaceSupport);
    }

    HRESULT STDMETHODCALLTYPE SetColorSpace1(
            DXGI_COLOR_SPACE_TYPE     ColorSpace) final {
      return m_dispatch->SetColorSpace1(ColorSpace);
    }

    HRESULT STDMETHODCALLTYPE SetHDRMetaData(
            DXGI_HDR_METADATA_TYPE    Type,
            UINT                      Size,
            void*                     pMetaData) final {
      return m_dispatch->SetHDRMetaData(Type, Size, pMetaData);
    }

  private:

    IDXGISwapChain4* m_dispatch;

  };

}
