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
    
    DXGI_SWAP_CHAIN_DESC swapDesc;
    swapDesc.BufferDesc.Width             = 1024;
    swapDesc.BufferDesc.Height            = 600;
    swapDesc.BufferDesc.RefreshRate       = { 60, 1 };
    swapDesc.BufferDesc.Format            = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    swapDesc.BufferDesc.ScanlineOrdering  = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapDesc.BufferDesc.Scaling           = DXGI_MODE_SCALING_UNSPECIFIED;
    swapDesc.SampleDesc.Count             = 1;
    swapDesc.SampleDesc.Quality           = 0;
    swapDesc.BufferUsage                  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount                  = 1;
    swapDesc.OutputWindow                 = window;
    swapDesc.Windowed                     = true;
    swapDesc.SwapEffect                   = DXGI_SWAP_EFFECT_DISCARD;
    swapDesc.Flags                        = 0;
    
    if (FAILED(m_factory->CreateSwapChain(m_device.ptr(), &swapDesc, &m_swapChain)))
      throw DxvkError("Failed to create DXGI swap chain");
    
    if (FAILED(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&m_buffer))))
      throw DxvkError("Failed to get swap chain back buffer");
    
    if (FAILED(m_device->CreateRenderTargetView(m_buffer.ptr(), nullptr, &m_bufferView)))
      throw DxvkError("Failed to create render target view");
    
    if (FAILED(m_swapChain->ResizeTarget(&swapDesc.BufferDesc)))
      throw DxvkError("Failed to resize window");
    
  }
  
  
  ~TriangleApp() {
    
  }
  
  
  void run() {
    FLOAT color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
    
    m_context->OMSetRenderTargets(1, &m_bufferView, nullptr);
    m_context->ClearRenderTargetView(m_bufferView.ptr(), color);
    m_swapChain->Present(0, 0);
  }
  
private:
  
  Com<IDXGIFactory>           m_factory;
  Com<IDXGIAdapter>           m_adapter;
  Com<ID3D11Device>           m_device;
  Com<ID3D11DeviceContext>    m_context;
  Com<IDXGISwapChain>         m_swapChain;
    
  Com<ID3D11Texture2D>        m_buffer;
  Com<ID3D11RenderTargetView> m_bufferView;
  
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
  
  try {
    TriangleApp app(hInstance, hWnd);
  
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
