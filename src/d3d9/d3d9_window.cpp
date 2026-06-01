#include "d3d9_window.h"

#include "d3d9_swapchain.h"

#ifndef _WIN32
#include "../wsi/wsi_window.h"
#include "../util/thread.h"
#include <unordered_map>
#endif

namespace dxvk
{

#ifdef _WIN32
  struct D3D9WindowData {
    bool unicode;
    bool filter;
    bool activateProcessed;
    bool deactivateProcessed;
    WNDPROC proc;
    D3D9SwapChainEx* swapchain;
  };

  static dxvk::recursive_mutex g_windowProcMapMutex;
  static std::unordered_map<HWND, D3D9WindowData> g_windowProcMap;

  D3D9WindowMessageFilter::D3D9WindowMessageFilter(HWND window, bool filter)
    : m_window(window) {
    std::lock_guard lock(g_windowProcMapMutex);
    auto it = g_windowProcMap.find(m_window);
    m_filter = std::exchange(it->second.filter, filter);
  }

  D3D9WindowMessageFilter::~D3D9WindowMessageFilter() {
    std::lock_guard lock(g_windowProcMapMutex);
    auto it = g_windowProcMap.find(m_window);
    it->second.filter = m_filter;
  }

  LRESULT CALLBACK D3D9WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCALCSIZE && wparam == TRUE)
      return 0;

    D3D9WindowData windowData = {};

    {
      std::lock_guard lock(g_windowProcMapMutex);

      auto it = g_windowProcMap.find(window);
      if (it != g_windowProcMap.end())
        windowData = it->second;
    }

    bool unicode = windowData.proc
      ? windowData.unicode
      : IsWindowUnicode(window);

    if (!windowData.proc || windowData.filter)
      return CallCharsetFunction(
        DefWindowProcW, DefWindowProcA, unicode,
          window, message, wparam, lparam);

    
    D3D9DeviceEx* device = windowData.swapchain->GetParent();

    if (message == WM_DESTROY)
      ResetWindowProc(window);
    else if (message == WM_ACTIVATEAPP) {
      D3DDEVICE_CREATION_PARAMETERS create_parms;
      device->GetCreationParameters(&create_parms);

      if (!(create_parms.BehaviorFlags & D3DCREATE_NOWINDOWCHANGES)) {
        D3D9WindowMessageFilter filter(window);
        if (wparam && !windowData.activateProcessed) {
          // Heroes of Might and Magic V needs this to resume drawing after a focus loss
          D3DPRESENT_PARAMETERS params;
          RECT rect;

          wsi::getDesktopCoordinates(wsi::getDefaultMonitor(), &rect);
          windowData.swapchain->GetPresentParameters(&params);
          SetWindowPos(window, nullptr, rect.left, rect.top, params.BackBufferWidth, params.BackBufferHeight,
                       SWP_NOACTIVATE | SWP_NOZORDER);
        }
        else if (!wparam) {
          if (IsWindowVisible(window))
            ShowWindow(window, SW_MINIMIZE);
        }
      }

      if ((wparam && !windowData.activateProcessed)
        || (!wparam && !windowData.deactivateProcessed)) {
        device->NotifyWindowActivated(window, wparam);
      }

      SetActivateProcessed(window, !!wparam);
    }
    else if (message == WM_SIZE)
    {
      D3DDEVICE_CREATION_PARAMETERS create_parms;
      device->GetCreationParameters(&create_parms);

      if (!(create_parms.BehaviorFlags & D3DCREATE_NOWINDOWCHANGES) && !IsIconic(window))
        PostMessageW(window, WM_ACTIVATEAPP, 1, GetCurrentThreadId());
    }

    return CallCharsetFunction(
      CallWindowProcW, CallWindowProcA, unicode,
        windowData.proc, window, message, wparam, lparam);
  }

  void ResetWindowProc(HWND window) {
    std::lock_guard lock(g_windowProcMapMutex);

    auto it = g_windowProcMap.find(window);
    if (it == g_windowProcMap.end())
      return;

    auto proc = reinterpret_cast<WNDPROC>(
      CallCharsetFunction(
      GetWindowLongPtrW, GetWindowLongPtrA, it->second.unicode,
        window, GWLP_WNDPROC));


    if (proc == D3D9WindowProc)
      CallCharsetFunction(
        SetWindowLongPtrW, SetWindowLongPtrA, it->second.unicode,
          window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(it->second.proc));

    g_windowProcMap.erase(window);
  }


  void HookWindowProc(HWND window, D3D9SwapChainEx* swapchain) {
    std::lock_guard lock(g_windowProcMapMutex);

    ResetWindowProc(window);

    D3D9WindowData windowData;
    windowData.unicode = IsWindowUnicode(window);
    windowData.filter  = false;
    windowData.activateProcessed = false;
    windowData.deactivateProcessed = false;
    windowData.proc = reinterpret_cast<WNDPROC>(
      CallCharsetFunction(
      SetWindowLongPtrW, SetWindowLongPtrA, windowData.unicode,
        window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(D3D9WindowProc)));
    windowData.swapchain = swapchain;

    g_windowProcMap[window] = std::move(windowData);
  }

  void SetActivateProcessed(HWND window, bool processed)
  {
      std::lock_guard lock(g_windowProcMapMutex);
      auto it = g_windowProcMap.find(window);
      if (it != g_windowProcMap.end()) {
        it->second.activateProcessed = processed;
        it->second.deactivateProcessed = !processed;
      }
  }

  void ActivateFocusWindow(HWND window) {
      SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
  }

  // On Win32 focus transitions are delivered via the subclassed WndProc;
  // this polling path is only used on non-Win32 backends.
  void PollWindowFocusForHook(HWND window) {
  }

#else
  // ---------------------------------------------------------------------------
  // Non-Win32 (SDL2 / SDL3 / GLFW) window focus tracking
  //
  // On non-Win32 backends there is no passive WndProc hook, so we maintain a
  // simple {HWND → {swapchain, lastFocused}} map and poll WSI each frame from
  // Present via PollWindowFocusForHook.
  // ---------------------------------------------------------------------------

  struct NativeWindowData {
    D3D9SwapChainEx* swapchain = nullptr;
    bool             lastFocus = false;
  };

  static dxvk::mutex                                        g_nativeWindowMutex;
  static std::unordered_map<HWND, NativeWindowData>         g_nativeWindowMap;

  D3D9WindowMessageFilter::D3D9WindowMessageFilter(HWND window, bool filter) {
  }

  D3D9WindowMessageFilter::~D3D9WindowMessageFilter() {
  }

  void ResetWindowProc(HWND window) {
    std::lock_guard lock(g_nativeWindowMutex);
    g_nativeWindowMap.erase(window);
  }

  void HookWindowProc(HWND window, D3D9SwapChainEx* swapchain) {
    std::lock_guard lock(g_nativeWindowMutex);

    // Seed lastFocus from current WSI state so we don't fire a spurious
    // de-activation on the first poll for a newly-created fullscreen window.
    NativeWindowData data;
    data.swapchain  = swapchain;
    data.lastFocus  = !wsi::isOccluded(window);
    g_nativeWindowMap[window] = data;
  }

  void SetActivateProcessed(HWND window, bool processed) {
  }

  void ActivateFocusWindow(HWND window) {
  }

  void PollWindowFocusForHook(HWND window) {
    NativeWindowData entry;

    {
      std::lock_guard lock(g_nativeWindowMutex);
      auto it = g_nativeWindowMap.find(window);
      if (it == g_nativeWindowMap.end())
        return;
      entry = it->second;
    }

    // isOccluded returns true only after the window has lost focus for >100ms,
    // giving the same hysteresis as the Win32 GetForegroundWindow approach.
    const bool hasFocus = !wsi::isOccluded(window);

    if (hasFocus == entry.lastFocus)
      return;

    {
      std::lock_guard lock(g_nativeWindowMutex);
      auto it = g_nativeWindowMap.find(window);
      if (it != g_nativeWindowMap.end())
        it->second.lastFocus = hasFocus;
    }

    if (entry.swapchain)
      entry.swapchain->GetParent()->NotifyWindowActivated(window, hasFocus);
  }

#endif

}
