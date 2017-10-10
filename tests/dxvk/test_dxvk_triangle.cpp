#include <dxvk_framebuffer.h>
#include <dxvk_instance.h>
#include <dxvk_main.h>
#include <dxvk_surface.h>

#include <windows.h>
#include <windowsx.h>

using namespace dxvk;

class TriangleApp {
  
public:
  
  TriangleApp(HINSTANCE instance, HWND window)
  : m_dxvkInstance    (new DxvkInstance()),
    m_dxvkAdapter     (m_dxvkInstance->enumAdapters().at(0)),
    m_dxvkDevice      (m_dxvkAdapter->createDevice()),
    m_dxvkSurface     (m_dxvkAdapter->createSurface(instance, window)),
    m_dxvkSwapchain   (m_dxvkDevice->createSwapchain(m_dxvkSurface,
      DxvkSwapchainProperties {
        VkSurfaceFormatKHR { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        VK_PRESENT_MODE_FIFO_KHR,
        VkExtent2D { 640, 480 },
      })),
    m_dxvkContext     (m_dxvkDevice->createContext()),
    m_dxvkCommandList (m_dxvkDevice->createCommandList()) {
    
  }
  
  ~TriangleApp() {
    
  }
  
  void run() {
    auto sync1 = m_dxvkDevice->createSemaphore();
    auto sync2 = m_dxvkDevice->createSemaphore();
    
    m_dxvkContext->beginRecording(m_dxvkCommandList);
    m_dxvkContext->setFramebuffer(
      m_dxvkSwapchain->getFramebuffer(sync1));
    m_dxvkContext->endRecording();
    
    auto fence = m_dxvkDevice->submitCommandList(
      m_dxvkCommandList, sync1, sync2);
    m_dxvkSwapchain->present(sync2);
    m_dxvkDevice->waitForIdle();
    m_dxvkCommandList->reset();
  }
  
private:
  
  Rc<DxvkInstance>    m_dxvkInstance;
  Rc<DxvkAdapter>     m_dxvkAdapter;
  Rc<DxvkDevice>      m_dxvkDevice;
  Rc<DxvkSurface>     m_dxvkSurface;
  Rc<DxvkSwapchain>   m_dxvkSwapchain;
  Rc<DxvkContext>     m_dxvkContext;
  Rc<DxvkCommandList> m_dxvkCommandList;
  
  Rc<DxvkBuffer>      m_vertexBuffer;
  
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
  
  TriangleApp app(hInstance, hWnd);
  
  try {
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
    dxvk::log(e.message());
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
