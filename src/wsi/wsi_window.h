#pragma once

#include <windows.h>

#include "wsi_monitor.h"
#include "wsi_platform.h"

#include "../vulkan/vulkan_loader.h"

namespace dxvk::wsi {

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
    * \param [in] width The new width
    * \param [in] height The new height
    */
  void resizeWindow(
          HWND             hWindow,
          DxvkWindowState* pState,
          uint32_t         width,
          uint32_t         weight);

  /**
    * \brief Sets the display mode for a window/monitor
    * 
    * \param [in] hMonitor The monitor
    * \param [in] hWindow The window (may be unused on some platforms)
    * \param [in] mode The mode
    * \returns \c true on success, \c false on failure
    */
  bool setWindowMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
    const WsiMode&         mode);

  /**
    * \brief Enter fullscreen mode for a window & monitor
    * 
    * \param [in] hMonitor The monitor
    * \param [in] hWindow The window (may be unused on some platforms)
    * \param [in] pState The swapchain's window state
    * \param [in] modeSwitch Whether mode switching is allowed
    * \returns \c true on success, \c false on failure
    */
  bool enterFullscreenMode(
          HMONITOR         hMonitor,
          HWND             hWindow,
          DxvkWindowState* pState,
          [[maybe_unused]]
          bool             modeSwitch);

  /**
    * \brief Exit fullscreen mode for a window
    * 
    * \param [in] hWindow The window
    * \param [in] pState The swapchain's window state
    * \returns \c true on success, \c false on failure
    */
  bool leaveFullscreenMode(
          HWND             hWindow,
          DxvkWindowState* pState,
          bool             restoreCoordinates);

  /**
    * \brief Restores the display mode if necessary
    * 
    * \returns \c true on success, \c false on failure
    */
  bool restoreDisplayMode();

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

  /**
    * \brief Update a fullscreen window's position/size
    *
    * \param [in] hMonitor The monitor
    * \param [in] hWindow The window
    * \param [in] forceTopmost Whether to force the window to become topmost again (D3D9 behaviour)
    */
  void updateFullscreenWindow(
          HMONITOR hMonitor,
          HWND     hWindow,
          bool     forceTopmost);

  /**
    * \brief Create a surface for a window
    * 
    * \param [in] hWindow The window
    * \param [in] pfnVkGetInstanceProcAddr \c vkGetInstanceProcAddr pointer
    * \param [in] instance Vulkan instance
    * \param [out] pSurface The surface
    */
  VkResult createSurface(
          HWND                hWindow,
          PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr,
          VkInstance          instance,
          VkSurfaceKHR*       pSurface);

}
