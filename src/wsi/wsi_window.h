#pragma once

// The windows.h will define HWND/HMONITOR
// to be types that it wants for the wsi we are compiling against.
// ie. HWND     = their own window data ptr (eg. SDL_Window)
//     HMONTIOR = their own monitor data ptr/a display index

#include <windows.h>

#include "wsi_mode.h"

namespace dxvk::wsi {

  /**
    * \brief Impl-dependent state
    */
  struct DxvkWindowState {
#ifdef DXVK_WSI_WIN32
    LONG style   = 0;
    LONG exstyle = 0;
    RECT rect    = { 0, 0, 0, 0 };
#endif
  };

  /**
    * \brief The size of the window
    * 
    * \param [in] hWindow The window
    * \param [out] pWidth The width (optional)
    * \param [out] pHeight The height (optional)
    */
  void getWindowSize(
          HWND      hWindow,
          uint32_t* pWidth,
          uint32_t* pWeight);

  /**
    * \brief Resize a window
    * 
    * \param [in] hWindow The window
    * \param [in] pState The swapchain's window state
    * \param [in] Width The new width
    * \param [in] Height The new height
    */
  void resizeWindow(
          HWND             hWindow,
          DxvkWindowState* pState,
          uint32_t         Width,
          uint32_t         Height);

  /**
    * \brief Sets the display mode for a window/monitor
    * 
    * \param [in] hMonitor The monitor
    * \param [in] hWindow The window
    * \param [in] pMode The mode
    * \param [in] EnteringFullscreen Are we entering fullscreen?
    * \returns \c true on success, \c false on failure
    */
  bool setWindowMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
    const WsiMode*         pMode,
          bool             EnteringFullscreen);

  /**
    * \brief Enter fullscreen mode for a window & monitor
    * 
    * \param [in] hMonitor The monitor
    * \param [in] hWindow The window
    * \param [in] pState The swapchain's window state
    * \param [in] ModeSwitch Whether mode switching is allowed
    * \returns \c true on success, \c false on failure
    */
  bool enterFullscreenMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             ModeSwitch);

  /**
    * \brief Exit fullscreen mode for a window
    * 
    * \param [in] hWindow The window
    * \param [in] pState The swapchain's window state
    * \returns \c true on success, \c false on failure
    */
  bool leaveFullscreenMode(
          HWND             hWindow,
          DxvkWindowState* pState);

  /**
    * \brief Restores the display mode if necessary
    * 
    * \param [in] hMonitor The monitor to restore the mode of
    * \returns \c true on success, \c false on failure
    */
  bool restoreDisplayMode(HMONITOR hMonitor);

  /**
    * \brief The monitor a window is on
    * 
    * \param [in] hWindow The window
    * \returns The monitor the window is on
    */
  HMONITOR getWindowMonitor(HWND hWindow);

  /**
    * \brief Is a HWND a window?
    *
    * \param [in] hWindow The window
    * \returns Is it a window?
    */
  bool isWindow(HWND hWindow);
}