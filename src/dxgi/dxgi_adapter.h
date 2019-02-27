#pragma once

#include <mutex>
#include <unordered_map>

#include "dxgi_format.h"
#include "dxgi_interfaces.h"
#include "dxgi_output.h"

namespace dxvk {
  
  class DxgiAdapter;
  class DxgiFactory;
  class DxgiOutput;

  class WineDXGISwapChainHelper : public IWineDXGISwapChainHelper {

public:

    WineDXGISwapChainHelper(
            DxgiAdapter* pAdapter);

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                  riid,
            void**                  ppvObject);

    HRESULT STDMETHODCALLTYPE GetMonitor(
            HWND      hWnd,
            HMONITOR* pMonitor);

    HRESULT STDMETHODCALLTYPE GetWindowInfo(
            HWND  hWnd,
            RECT* pRect,
            RECT* pClientRect,
            LONG* pStyle,
            LONG* pExStyle);
    
    HRESULT STDMETHODCALLTYPE SetWindowPos(
            HWND hWnd,
            HWND hWndInsertAfter,
            RECT Position,
            UINT Flags);
    
    HRESULT STDMETHODCALLTYPE ResizeWindow(
            HWND hWnd,
            UINT Width,
            UINT Height);

    HRESULT STDMETHODCALLTYPE SetWindowStyles(
            HWND  hWnd,
      const LONG* pStyle,
      const LONG* pExstyle);

    HRESULT STDMETHODCALLTYPE GetDisplayMode(
            HMONITOR        hMonitor,
            DWORD           ModeNum,
            DXGI_MODE_DESC* pMode);

    HRESULT STDMETHODCALLTYPE SetDisplayMode(
            HMONITOR        hMonitor,
      const DXGI_MODE_DESC* pMode);

private:

    DxgiAdapter* m_adapter;
  };
  
  class DxgiAdapter : public DxgiObject<IDXGIVkAdapter> {
    
  public:
    
    DxgiAdapter(
            DxgiFactory*      factory,
      const Rc<DxvkAdapter>&  adapter);
    ~DxgiAdapter();
    
    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                    riid,
            void**                    ppvObject) final;
    
    HRESULT STDMETHODCALLTYPE GetParent(
            REFIID                    riid,
            void**                    ppParent) final;
    
    HRESULT STDMETHODCALLTYPE CheckInterfaceSupport(
            REFGUID                   InterfaceName,
            LARGE_INTEGER*            pUMDVersion) final;
    
    HRESULT STDMETHODCALLTYPE EnumOutputs(
            UINT                      Output,
            IDXGIOutput**             ppOutput) final;
    
    HRESULT STDMETHODCALLTYPE GetDesc(
            DXGI_ADAPTER_DESC*        pDesc) final;
    
    HRESULT STDMETHODCALLTYPE GetDesc1(
            DXGI_ADAPTER_DESC1*       pDesc) final;
    
    HRESULT STDMETHODCALLTYPE GetDesc2(
            DXGI_ADAPTER_DESC2*       pDesc) final;
    
    HRESULT STDMETHODCALLTYPE QueryVideoMemoryInfo(
            UINT                          NodeIndex,
            DXGI_MEMORY_SEGMENT_GROUP     MemorySegmentGroup,
            DXGI_QUERY_VIDEO_MEMORY_INFO* pVideoMemoryInfo) final;

    HRESULT STDMETHODCALLTYPE SetVideoMemoryReservation(
            UINT                          NodeIndex,
            DXGI_MEMORY_SEGMENT_GROUP     MemorySegmentGroup,
            UINT64                        Reservation) final;
    
    HRESULT STDMETHODCALLTYPE RegisterHardwareContentProtectionTeardownStatusEvent(
            HANDLE                        hEvent,
            DWORD*                        pdwCookie) final;

    HRESULT STDMETHODCALLTYPE RegisterVideoMemoryBudgetChangeNotificationEvent(
            HANDLE                        hEvent,
            DWORD*                        pdwCookie) final;
    
    void STDMETHODCALLTYPE UnregisterHardwareContentProtectionTeardownStatus(
            DWORD                         dwCookie) final;

    void STDMETHODCALLTYPE UnregisterVideoMemoryBudgetChangeNotification(
            DWORD                         dwCookie) final;

    Rc<DxvkAdapter> STDMETHODCALLTYPE GetDXVKAdapter() final;
    
  private:
    
    Com<DxgiFactory>        m_factory;
    WineDXGISwapChainHelper m_wineHelper;
    Rc<DxvkAdapter>         m_adapter;
    
    UINT64            m_memReservation[2] = { 0, 0 };
    
  };

}
