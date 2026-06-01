#include "d3d9_cursor_native.h"

#include "../util/log/log.h"
#include "../util/util_string.h"

#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <strings.h>

#if defined(DXVK_WSI_SDL2)
#include <SDL.h>
#endif

#if defined(DXVK_WSI_GLFW)
#include <GLFW/glfw3.h>
#endif

namespace dxvk::native_cursor {

  namespace {

    enum class WsiKind {
      None,
      Sdl2,
      Glfw,
    };

    HWND g_window = nullptr;

#if defined(DXVK_WSI_SDL2)
    SDL_Cursor* g_sdlCursor = nullptr;
#endif

#if defined(DXVK_WSI_GLFW)
    GLFWcursor* g_glfwCursor = nullptr;
#endif

    WsiKind getWsiKind() {
      const char* driver = std::getenv("DXVK_WSI_DRIVER");
      if (driver == nullptr)
        return WsiKind::None;

#if defined(DXVK_WSI_SDL2)
      if (!strcasecmp(driver, "SDL2"))
        return WsiKind::Sdl2;
#endif

#if defined(DXVK_WSI_GLFW)
      if (!strcasecmp(driver, "GLFW"))
        return WsiKind::Glfw;
#endif

      return WsiKind::None;
    }

#if defined(DXVK_WSI_SDL2)
    SDL_Window* toSdlWindow(HWND window) {
      return reinterpret_cast<SDL_Window*>(window);
    }


    void destroySdlCursor() {
      if (g_sdlCursor != nullptr) {
        SDL_FreeCursor(g_sdlCursor);
        g_sdlCursor = nullptr;
      }
    }


    void hideSdlCursor(SDL_Window* window) {
      (void)window;
      destroySdlCursor();
      SDL_SetCursor(nullptr);
      SDL_ShowCursor(SDL_DISABLE);
    }
#endif

#if defined(DXVK_WSI_GLFW)
    GLFWwindow* toGlfwWindow(HWND window) {
      return reinterpret_cast<GLFWwindow*>(window);
    }


    void destroyGlfwCursor() {
      if (g_glfwCursor != nullptr) {
        glfwDestroyCursor(g_glfwCursor);
        g_glfwCursor = nullptr;
      }
    }


    void hideGlfwCursor(GLFWwindow* window) {
      glfwSetCursor(window, nullptr);
      destroyGlfwCursor();
    }
#endif

  } // namespace


  void SetWindow(HWND window) {
    g_window = window;
  }


  void Reset() {
    ResetHardwareCursor();
  }


  bool HasHardwareCursor() {
#if defined(DXVK_WSI_SDL2)
    if (g_sdlCursor != nullptr)
      return true;
#endif
#if defined(DXVK_WSI_GLFW)
    if (g_glfwCursor != nullptr)
      return true;
#endif
    return false;
  }


  void ResetHardwareCursor() {
    switch (getWsiKind()) {
#if defined(DXVK_WSI_SDL2)
      case WsiKind::Sdl2:
        destroySdlCursor();
        if (g_window != nullptr)
          SDL_ShowCursor(SDL_DISABLE);
        return;
#endif

#if defined(DXVK_WSI_GLFW)
      case WsiKind::Glfw:
        if (g_window != nullptr)
          hideGlfwCursor(toGlfwWindow(g_window));
        else
          destroyGlfwCursor();
        return;
#endif

      default:
        return;
    }
  }


  void SetHardwareCursor(
          HWND               window,
          UINT               hotX,
          UINT               hotY,
    const CursorBitmap&      bitmap) {
    switch (getWsiKind()) {
#if defined(DXVK_WSI_SDL2)
      case WsiKind::Sdl2: {
        SDL_Window* sdlWindow = toSdlWindow(window);

        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
          const_cast<uint8_t*>(bitmap),
          HardwareCursorWidth,
          HardwareCursorHeight,
          32,
          HardwareCursorPitch,
          SDL_PIXELFORMAT_ABGR8888);

        if (surface == nullptr) {
          Logger::err(str::format("D3D9Cursor: SDL_CreateRGBSurfaceWithFormatFrom: ", SDL_GetError()));
          return;
        }

        destroySdlCursor();
        g_sdlCursor = SDL_CreateColorCursor(surface, int(hotX), int(hotY));
        SDL_FreeSurface(surface);

        if (g_sdlCursor == nullptr) {
          Logger::err(str::format("D3D9Cursor: SDL_CreateColorCursor: ", SDL_GetError()));
          return;
        }

        SDL_SetCursor(g_sdlCursor);
        return;
      }
#endif

#if defined(DXVK_WSI_GLFW)
      case WsiKind::Glfw: {
        GLFWwindow* glfwWindow = toGlfwWindow(window);

        GLFWimage image = { };
        image.width  = HardwareCursorWidth;
        image.height = HardwareCursorHeight;
        image.pixels = const_cast<uint8_t*>(bitmap);

        destroyGlfwCursor();
        g_glfwCursor = glfwCreateCursor(&image, int(hotX), int(hotY));

        if (g_glfwCursor == nullptr) {
          Logger::err("D3D9Cursor: glfwCreateCursor failed.");
          return;
        }

        glfwSetCursor(glfwWindow, g_glfwCursor);
        return;
      }
#endif

      default:
        Logger::warn("D3D9Cursor: Hardware cursor requires DXVK_WSI_DRIVER=SDL2 or GLFW.");
        return;
    }
  }


  void PrepareSoftwareCursor(HWND window) {
    switch (getWsiKind()) {
#if defined(DXVK_WSI_SDL2)
      case WsiKind::Sdl2:
        hideSdlCursor(toSdlWindow(window));
        return;
#endif

#if defined(DXVK_WSI_GLFW)
      case WsiKind::Glfw:
        hideGlfwCursor(toGlfwWindow(window));
        return;
#endif

      default:
        return;
    }
  }


  void ShowHardwareCursor(HWND window, BOOL visible) {
    switch (getWsiKind()) {
#if defined(DXVK_WSI_SDL2)
      case WsiKind::Sdl2:
        if (!HasHardwareCursor())
          return;

        SDL_SetCursor(g_sdlCursor);
        SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
        return;
#endif

#if defined(DXVK_WSI_GLFW)
      case WsiKind::Glfw:
        if (!HasHardwareCursor())
          return;

        glfwSetCursor(toGlfwWindow(window), visible ? g_glfwCursor : nullptr);
        return;
#endif

      default:
        return;
    }
  }


  void UpdateHardwareCursorPosition(HWND window, int x, int y) {
    switch (getWsiKind()) {
#if defined(DXVK_WSI_SDL2)
      case WsiKind::Sdl2:
        if (!HasHardwareCursor())
          return;
        SDL_WarpMouseInWindow(toSdlWindow(window), float(x), float(y));
        return;
#endif

#if defined(DXVK_WSI_GLFW)
      case WsiKind::Glfw: {
        if (!HasHardwareCursor())
          return;

        GLFWwindow* glfwWindow = toGlfwWindow(window);
        int winX = 0;
        int winY = 0;
        glfwGetWindowPos(glfwWindow, &winX, &winY);

        // D3D9 supplies client-area coordinates; GLFW expects screen coordinates.
        glfwSetCursorPos(glfwWindow, double(winX + x), double(winY + y));
        return;
      }
#endif

      default:
        return;
    }
  }

} // namespace dxvk::native_cursor
