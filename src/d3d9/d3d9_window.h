#pragma once

#include <windows.h>

namespace dxvk {

  class D3D9SwapChainEx;

  class D3D9WindowMessageFilter {

  public:

    D3D9WindowMessageFilter(HWND window, bool filter = true);
    ~D3D9WindowMessageFilter();

    D3D9WindowMessageFilter             (const D3D9WindowMessageFilter&) = delete;
    D3D9WindowMessageFilter& operator = (const D3D9WindowMessageFilter&) = delete;

  private:

    HWND m_window;
    bool m_filter = false;

  };

  template <typename T, typename J, typename ... Args>
  auto CallCharsetFunction(T unicode, J ascii, bool isUnicode, Args... args) {
    return isUnicode
      ? unicode(args...)
      : ascii  (args...);
  }

  void ResetWindowProc(HWND window);
  void HookWindowProc(HWND window, D3D9SwapChainEx* swapchain);
  void SetActivateProcessed(HWND window, bool processed);

}
