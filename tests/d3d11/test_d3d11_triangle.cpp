#include <d3d11_include.h>

#include <windows.h>
#include <windowsx.h>

using namespace dxvk;

class TriangleApp {
  
public:
  
  TriangleApp(HINSTANCE instance, HWND window) {
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory),
        reinterpret_cast<void**>(&m_factory))))
      throw DxvkError("Failed to create DXGI factory");
    
    if (FAILED(m_factory->EnumAdapters(0, &m_adapter)))
      throw DxvkError("Failed to enumerate DXGI adapter");
    
    HRESULT status = D3D11CreateDevice(
      m_adapter.ptr(),
      D3D_DRIVER_TYPE_UNKNOWN,
      nullptr, 0,
      nullptr, 0, 0,
      &m_device,
      &m_featureLevel,
      &m_context);
    
    if (FAILED(status))
      throw DxvkError("Failed to create D3D11 device");
  }
  
  ~TriangleApp() {
    
  }
  
  void run() {
    
  }
  
private:
  
  Com<IDXGIFactory>         m_factory;
  Com<IDXGIAdapter>         m_adapter;
  Com<ID3D11Device>         m_device;
  Com<ID3D11DeviceContext>  m_context;
  
  D3D_FEATURE_LEVEL       m_featureLevel;
  
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
    Logger::err(e.message());
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
