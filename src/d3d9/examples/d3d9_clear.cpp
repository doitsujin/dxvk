/**
 * Minimal SpockD3D9 / DXVK-native smoke test.
 *
 * Creates an SDL2 window, a D3D9 device, clears the back buffer, and presents.
 * Exits after a configurable number of frames (default: 60).
 */

#include <cstdio>
#include <cstdlib>

#include <SDL.h>
#include <windows.h>
#include <d3d9.h>

namespace {

  void configureWsiDriver() {
#if defined(_WIN32)
    _putenv_s("DXVK_WSI_DRIVER", "SDL2");
#else
    setenv("DXVK_WSI_DRIVER", "SDL2", 1);
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

} // namespace

int main(int argc, char** argv) {
  const int frameCount = parseFrameCount(argc, argv);

  configureWsiDriver();

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::fprintf(stderr, "d3d9-clear: SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  constexpr uint32_t kWidth  = 640;
  constexpr uint32_t kHeight = 480;

  SDL_Window* window = SDL_CreateWindow(
    "SpockD3D9 d3d9-clear",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    kWidth,
    kHeight,
    SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);

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
      if (event.type == SDL_QUIT)
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
  SDL_Quit();

  std::printf("d3d9-clear: OK\n");
  return 0;
}
