/**
 * Minimal SpockD3D9 / DXVK-native smoke test.
 *
 * Creates an SDL2 or SDL3 window, a D3D9 device, clears the back buffer, and
 * presents. Exits after a configurable number of frames (default: 60).
 *
 * Compiled twice: the default build targets SDL2; defining D3D9_CLEAR_SDL3
 * targets SDL3 (and selects the matching DXVK_WSI_DRIVER).
 */

#include <cstdio>
#include <cstdlib>

#if defined(D3D9_CLEAR_SDL3)
#include <SDL3/SDL.h>
#if defined(__APPLE__)
#include <SDL3/SDL_vulkan.h>
#endif
#define D3D9_CLEAR_WSI_DRIVER "SDL3"
#else
#include <SDL.h>
#if defined(__APPLE__)
#include <SDL_vulkan.h>
#endif
#define D3D9_CLEAR_WSI_DRIVER "SDL2"
#endif
#include <windows.h>
#include <d3d9.h>

namespace {

  void configureWsiDriver() {
#if defined(_WIN32)
    _putenv_s("DXVK_WSI_DRIVER", D3D9_CLEAR_WSI_DRIVER);
#else
    setenv("DXVK_WSI_DRIVER", D3D9_CLEAR_WSI_DRIVER, 1);
#endif
  }

  int parseFrameCount(int argc, char** argv) {
    if (argc < 2)
      return 60;

    char* end = nullptr;
    const long value = std::strtol(argv[1], &end, 10);
    if (end == argv[1] || value < 1)
      return 60;

    return int(value);
  }

#if defined(__APPLE__)
  // SDL3's SDL_Vulkan_LoadLibrary returns true on success; SDL2's returns 0.
  bool vulkanLibraryLoaded(const char* path) {
#if defined(D3D9_CLEAR_SDL3)
    return SDL_Vulkan_LoadLibrary(path);
#else
    return SDL_Vulkan_LoadLibrary(path) == 0;
#endif
  }

  bool loadVulkanPortabilityLibrary() {
    if (vulkanLibraryLoaded(nullptr))
      return true;

    const char* const candidates[] = {
      std::getenv("SDL_VULKAN_LIBRARY"),
      "/opt/homebrew/lib/libMoltenVK.dylib",
      "/usr/local/lib/libMoltenVK.dylib",
      "/opt/homebrew/lib/libvulkan.1.dylib",
      "/usr/local/lib/libvulkan.1.dylib",
    };

    for (const char* path : candidates) {
      if (path == nullptr || path[0] == '\0')
        continue;

      if (vulkanLibraryLoaded(path))
        return true;
    }

    return false;
  }
#endif

} // namespace

int main(int argc, char** argv) {
  const int frameCount = parseFrameCount(argc, argv);

  configureWsiDriver();

#if defined(D3D9_CLEAR_SDL3)
  if (!SDL_Init(SDL_INIT_VIDEO)) {
#else
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
#endif
    std::fprintf(stderr, "d3d9-clear: SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

#if defined(__APPLE__)
  if (!loadVulkanPortabilityLibrary()) {
    std::fprintf(stderr, "d3d9-clear: failed to load Vulkan portability library: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
#endif

  constexpr uint32_t kWidth  = 640;
  constexpr uint32_t kHeight = 480;

#if defined(D3D9_CLEAR_SDL3)
  SDL_Window* window = SDL_CreateWindow(
    "SpockD3D9 d3d9-clear",
    int(kWidth),
    int(kHeight),
    SDL_WINDOW_VULKAN);
#else
  SDL_Window* window = SDL_CreateWindow(
    "SpockD3D9 d3d9-clear",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    kWidth,
    kHeight,
    SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
#endif

  if (!window) {
    std::fprintf(stderr, "d3d9-clear: SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  IDirect3D9* d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
  if (!d3d9) {
    std::fprintf(stderr, "d3d9-clear: Direct3DCreate9 failed\n");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  HWND hwnd = reinterpret_cast<HWND>(window);

  D3DPRESENT_PARAMETERS presentParams = { };
  presentParams.Windowed                    = TRUE;
  presentParams.SwapEffect                  = D3DSWAPEFFECT_DISCARD;
  presentParams.BackBufferCount             = 1;
  presentParams.BackBufferFormat            = D3DFMT_X8R8G8B8;
  presentParams.BackBufferWidth             = kWidth;
  presentParams.BackBufferHeight            = kHeight;
  presentParams.hDeviceWindow               = hwnd;
  presentParams.EnableAutoDepthStencil      = FALSE;
  presentParams.PresentationInterval        = D3DPRESENT_INTERVAL_IMMEDIATE;

  IDirect3DDevice9* device = nullptr;
  const HRESULT hr = d3d9->CreateDevice(
    D3DADAPTER_DEFAULT,
    D3DDEVTYPE_HAL,
    hwnd,
    D3DCREATE_HARDWARE_VERTEXPROCESSING,
    &presentParams,
    &device);

  if (FAILED(hr) || !device) {
    std::fprintf(stderr, "d3d9-clear: CreateDevice failed (HRESULT 0x%08lx)\n", hr);
    d3d9->Release();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  std::printf("d3d9-clear: presenting %d frame(s)\n", frameCount);

  bool quit = false;
  for (int frame = 0; frame < frameCount && !quit; frame++) {
    SDL_Event event = { };
    while (SDL_PollEvent(&event)) {
#if defined(D3D9_CLEAR_SDL3)
      if (event.type == SDL_EVENT_QUIT)
#else
      if (event.type == SDL_QUIT)
#endif
        quit = true;
    }

    device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(32, 64, 192), 1.0f, 0);

    const HRESULT presentHr = device->Present(nullptr, nullptr, nullptr, nullptr);
    if (FAILED(presentHr)) {
      std::fprintf(stderr, "d3d9-clear: Present failed (HRESULT 0x%08lx)\n", presentHr);
      device->Release();
      d3d9->Release();
      SDL_DestroyWindow(window);
      SDL_Quit();
      return 1;
    }
  }

  device->Release();
  d3d9->Release();
  SDL_DestroyWindow(window);
#if defined(__APPLE__)
  SDL_Vulkan_UnloadLibrary();
#endif
  SDL_Quit();

  std::printf("d3d9-clear: OK\n");
  return 0;
}
