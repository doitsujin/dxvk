#include <cstring>
#include <d3d9.h>

#include "../test_utils.h"

using namespace dxvk;

struct Extent2D {
  uint32_t w, h;
};

DWORD g_UsagePermuatations[] = {
  0,
  D3DUSAGE_DYNAMIC,
  D3DUSAGE_WRITEONLY,
  D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
};

DWORD g_MapFlagPermutations[] = {
  0,
  D3DLOCK_DISCARD,
  D3DLOCK_DONOTWAIT,
  D3DLOCK_NOOVERWRITE
};

class BufferApp {
  
public:
  
  BufferApp(HINSTANCE instance, HWND window)
  : m_window(window) {
    HRESULT status = Direct3DCreate9Ex(D3D_SDK_VERSION, &m_d3d);

    if (FAILED(status))
      throw DxvkError("Failed to create D3D9 interface");

    D3DPRESENT_PARAMETERS params;
    getPresentParams(params);

    status = m_d3d->CreateDeviceEx(
      D3DADAPTER_DEFAULT,
      D3DDEVTYPE_HAL,
      m_window,
      D3DCREATE_HARDWARE_VERTEXPROCESSING,
      &params,
      nullptr,
      &m_device);
    
    if (FAILED(status))
      throw DxvkError("Failed to create D3D9 device");

    uint8_t* data = new uint8_t[512];
    std::memset(data, 0xFC, 512);

    for (uint32_t i = 0; i < ARRAYSIZE(g_UsagePermuatations); i++) {
      for (uint32_t j = 0; j < ARRAYSIZE(g_MapFlagPermutations); j++) {
        testBuffer(data, g_UsagePermuatations[i], g_MapFlagPermutations[j]);
      }
    }

    delete[] data;
  }

  void testBuffer(uint8_t* data, DWORD usage, DWORD mapFlags) {
    Com<IDirect3DVertexBuffer9> buffer;
    HRESULT status = m_device->CreateVertexBuffer(512, usage, 0, D3DPOOL_DEFAULT, &buffer, nullptr);

    if (FAILED(status))
      throw DxvkError("Failed to create buffer");

    void* bufferMem = nullptr;
    status = buffer->Lock(0, 0, &bufferMem, mapFlags);

    if (FAILED(status) || bufferMem == nullptr)
      throw DxvkError("Failed to lock buffer");

    std::memcpy(bufferMem, data, 512);

    status = buffer->Unlock();

    if (FAILED(status))
      throw DxvkError("Failed to unlock buffer");
  }
  
  void run() {
    this->adjustBackBuffer();

    m_device->BeginScene();

    m_device->Clear(
      0,
      nullptr,
      D3DCLEAR_TARGET,
      D3DCOLOR_RGBA(255, 50, 139, 0),
      0.0f,
      0);

    m_device->EndScene();

    m_device->PresentEx(
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      0);
  }
  
  void adjustBackBuffer() {
    RECT windowRect = { 0, 0, 1024, 600 };
    GetClientRect(m_window, &windowRect);

    Extent2D newSize = {
      static_cast<uint32_t>(windowRect.right - windowRect.left),
      static_cast<uint32_t>(windowRect.bottom - windowRect.top),
    };

    if (m_windowSize.w != newSize.w
     || m_windowSize.h != newSize.h) {
      m_windowSize = newSize;

      D3DPRESENT_PARAMETERS params;
      getPresentParams(params);
      HRESULT status = m_device->ResetEx(&params, nullptr);

      if (FAILED(status))
        throw DxvkError("Device reset failed");
    }
  }
  
  void getPresentParams(D3DPRESENT_PARAMETERS& params) {
    params.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
    params.BackBufferCount = 1;
    params.BackBufferFormat = D3DFMT_X8R8G8B8;
    params.BackBufferWidth = m_windowSize.w;
    params.BackBufferHeight = m_windowSize.h;
    params.EnableAutoDepthStencil = FALSE;
    params.Flags = 0;
    params.FullScreen_RefreshRateInHz = 0;
    params.hDeviceWindow = m_window;
    params.MultiSampleQuality = 0;
    params.MultiSampleType = D3DMULTISAMPLE_NONE;
    params.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.Windowed = TRUE;
  }
    
private:
  
  HWND                          m_window;
  Extent2D                      m_windowSize = { 1024, 600 };
  
  Com<IDirect3D9Ex>             m_d3d;
  Com<IDirect3DDevice9Ex>       m_device;
  
};

LRESULT CALLBACK WindowProc(HWND hWnd,
                            UINT message,
                            WPARAM wParam,
                            LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow) {
  HWND hWnd;
  WNDCLASSEXW wc;
  ZeroMemory(&wc, sizeof(WNDCLASSEX));
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
  wc.lpszClassName = L"WindowClass1";
  RegisterClassExW(&wc);

  hWnd = CreateWindowExW(0,
    L"WindowClass1",
    L"Our First Windowed Program",
    WS_OVERLAPPEDWINDOW,
    300, 300,
    640, 480,
    nullptr,
    nullptr,
    hInstance,
    nullptr);
  ShowWindow(hWnd, nCmdShow);

  MSG msg;
  
  try {
    BufferApp app(hInstance, hWnd);
  
    while (true) {
      if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
        if (msg.message == WM_QUIT)
          return msg.wParam;
      } else {
        app.run();
      }
    }
  } catch (const dxvk::DxvkError& e) {
    std::cerr << e.message() << std::endl;
    return msg.wParam;
  }
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CLOSE:
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}
