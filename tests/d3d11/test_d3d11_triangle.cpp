#include <d3dcompiler.h>
#include <d3d11.h>

#include <windows.h>
#include <windowsx.h>

#include "../test_utils.h"

using namespace dxvk;

struct Extent2D {
  uint32_t w, h;
};

struct Vertex {
  float x, y, z, w;
};

const std::string g_vertexShaderCode =
  "float4 main(float4 vsIn : IN_POSITION) : SV_POSITION {\n"
  "  return vsIn;\n"
  "}\n";

const std::string g_pixelShaderCode =
  "cbuffer c_buffer { float4 ccolor[2]; };\n"
  "float4 main() : SV_TARGET {\n"
  "  return ccolor[0];\n"
  "}\n";

class TriangleApp {
  
public:
  
  TriangleApp(HINSTANCE instance, HWND window)
  : m_window(window) {
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory),
        reinterpret_cast<void**>(&m_factory))))
      throw DxvkError("Failed to create DXGI factory");
    
    if (FAILED(m_factory->EnumAdapters(0, &m_adapter)))
      throw DxvkError("Failed to enumerate DXGI adapter");
    
    HRESULT status = D3D11CreateDevice(
      m_adapter.ptr(),
      D3D_DRIVER_TYPE_UNKNOWN,
      nullptr, 0,
      nullptr, 0,
      D3D11_SDK_VERSION,
      &m_device,
      &m_featureLevel,
      &m_context);
    
    if (FAILED(status))
      throw DxvkError("Failed to create D3D11 device");
    
    DXGI_SWAP_CHAIN_DESC swapDesc;
    swapDesc.BufferDesc.Width             = m_windowSize.w;
    swapDesc.BufferDesc.Height            = m_windowSize.h;
    swapDesc.BufferDesc.RefreshRate       = { 60, 1 };
    swapDesc.BufferDesc.Format            = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    swapDesc.BufferDesc.ScanlineOrdering  = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapDesc.BufferDesc.Scaling           = DXGI_MODE_SCALING_UNSPECIFIED;
    swapDesc.SampleDesc.Count             = 1;
    swapDesc.SampleDesc.Quality           = 0;
    swapDesc.BufferUsage                  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount                  = 2;
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
    
    std::array<Vertex, 3> vertexData = {{
      { -0.5f, -0.5f, 0.0f, 1.0f },
      {  0.0f,  0.5f, 0.0f, 1.0f },
      {  0.5f, -0.5f, 0.0f, 1.0f },
    }};
    
    D3D11_BUFFER_DESC vertexDesc;
    vertexDesc.ByteWidth            = sizeof(Vertex) * vertexData.size();
    vertexDesc.Usage                = D3D11_USAGE_IMMUTABLE;
    vertexDesc.BindFlags            = D3D11_BIND_VERTEX_BUFFER;
    vertexDesc.CPUAccessFlags       = 0;
    vertexDesc.MiscFlags            = 0;
    vertexDesc.StructureByteStride  = 0;
    
    D3D11_SUBRESOURCE_DATA vertexDataInfo;
    vertexDataInfo.pSysMem          = vertexData.data();
    vertexDataInfo.SysMemPitch      = 0;
    vertexDataInfo.SysMemSlicePitch = 0;
    
    if (FAILED(m_device->CreateBuffer(&vertexDesc, &vertexDataInfo, &m_vertexBuffer)))
      throw DxvkError("Failed to create vertex buffer");
    
    std::array<Vertex, 2> constantData = {{
      { 0.03f, 0.03f, 0.03f, 1.0f },
      { 1.00f, 1.00f, 1.00f, 1.0f },
    }};
    
    D3D11_BUFFER_DESC constantDesc;
    constantDesc.ByteWidth            = sizeof(Vertex) * constantData.size();
    constantDesc.Usage                = D3D11_USAGE_IMMUTABLE;
    constantDesc.BindFlags            = D3D11_BIND_CONSTANT_BUFFER;
    constantDesc.CPUAccessFlags       = 0;
    constantDesc.MiscFlags            = 0;
    constantDesc.StructureByteStride  = 0;
    
    D3D11_SUBRESOURCE_DATA constantDataInfo;
    constantDataInfo.pSysMem          = constantData.data();
    constantDataInfo.SysMemPitch      = 0;
    constantDataInfo.SysMemSlicePitch = 0;
    
    if (FAILED(m_device->CreateBuffer(&constantDesc, &constantDataInfo, &m_constantBuffer)))
      throw DxvkError("Failed to create constant buffer");
    
    Com<ID3DBlob> vertexShaderBlob;
    Com<ID3DBlob> pixelShaderBlob;
    
    if (FAILED(D3DCompile(
          g_vertexShaderCode.data(),
          g_vertexShaderCode.size(),
          "Vertex shader",
          nullptr, nullptr,
          "main", "vs_5_0", 0, 0,
          &vertexShaderBlob,
          nullptr)))
      throw DxvkError("Failed to compile vertex shader");
    
    if (FAILED(D3DCompile(
          g_pixelShaderCode.data(),
          g_pixelShaderCode.size(),
          "Pixel shader",
          nullptr, nullptr,
          "main", "ps_5_0", 0, 0,
          &pixelShaderBlob,
          nullptr)))
      throw DxvkError("Failed to compile pixel shader");
    
    if (FAILED(m_device->CreateVertexShader(
          vertexShaderBlob->GetBufferPointer(),
          vertexShaderBlob->GetBufferSize(),
          nullptr, &m_vertexShader)))
      throw DxvkError("Failed to create vertex shader");
    
    if (FAILED(m_device->CreatePixelShader(
          pixelShaderBlob->GetBufferPointer(),
          pixelShaderBlob->GetBufferSize(),
          nullptr, &m_pixelShader)))
      throw DxvkError("Failed to create pixel shader");
      
    std::array<D3D11_INPUT_ELEMENT_DESC, 1> vertexFormatDesc = {{
      { "IN_POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, x), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    }};
    
    if (FAILED(m_device->CreateInputLayout(
          vertexFormatDesc.data(),
          vertexFormatDesc.size(),
          vertexShaderBlob->GetBufferPointer(),
          vertexShaderBlob->GetBufferSize(),
          &m_vertexFormat)))
      throw DxvkError("Failed to create input layout");
    
  }
  
  
  ~TriangleApp() {
    
  }
  
  
  void run() {
    this->adjustBackBuffer();
    
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX     = 0.0f;
    viewport.TopLeftY     = 0.0f;
    viewport.Width        = static_cast<float>(m_windowSize.w);
    viewport.Height       = static_cast<float>(m_windowSize.h);
    viewport.MinDepth     = 0.0f;
    viewport.MaxDepth     = 1.0f;
    m_context->RSSetViewports(1, &viewport);
    
    FLOAT color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
    m_context->OMSetRenderTargets(1, &m_bufferView, nullptr);
    m_context->ClearRenderTargetView(m_bufferView.ptr(), color);
    
    m_context->VSSetShader(m_vertexShader.ptr(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.ptr(), nullptr, 0);
    m_context->PSSetConstantBuffers(0, 1, &m_constantBuffer);
    
    UINT vsStride = sizeof(Vertex);
    UINT vsOffset = 0;
    
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetInputLayout(m_vertexFormat.ptr());
    m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &vsStride, &vsOffset);
    m_context->Draw(3, 0);
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    
    m_swapChain->Present(0, 0);
  }
  
  
  void adjustBackBuffer() {
    RECT windowRect = { 0, 0, 1024, 600 };
    GetClientRect(m_window, &windowRect);
    
    Extent2D newWindowSize = {
      static_cast<uint32_t>(windowRect.right - windowRect.left),
      static_cast<uint32_t>(windowRect.bottom - windowRect.top),
    };
    
    if (m_windowSize.w != newWindowSize.w
     || m_windowSize.h != newWindowSize.h) {
      m_buffer     = nullptr;
      m_bufferView = nullptr;
      
      if (FAILED(m_swapChain->ResizeBuffers(0,
            newWindowSize.w, newWindowSize.h, DXGI_FORMAT_UNKNOWN, 0)))
        throw DxvkError("Failed to resize back buffers");
      
      if (FAILED(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&m_buffer))))
        throw DxvkError("Failed to get swap chain back buffer");
      
      if (FAILED(m_device->CreateRenderTargetView(m_buffer.ptr(), nullptr, &m_bufferView)))
        throw DxvkError("Failed to create render target view");
      m_windowSize = newWindowSize;
    }
  }
    
private:
  
  HWND                        m_window;
  Extent2D                    m_windowSize = { 1024, 600 };
  
  Com<IDXGIFactory>           m_factory;
  Com<IDXGIAdapter>           m_adapter;
  Com<ID3D11Device>           m_device;
  Com<ID3D11DeviceContext>    m_context;
  Com<IDXGISwapChain>         m_swapChain;
    
  Com<ID3D11Texture2D>        m_buffer;
  Com<ID3D11RenderTargetView> m_bufferView;
  Com<ID3D11Buffer>           m_constantBuffer;
  Com<ID3D11Buffer>           m_vertexBuffer;
  Com<ID3D11InputLayout>      m_vertexFormat;
  
  Com<ID3D11VertexShader>     m_vertexShader;
  Com<ID3D11PixelShader>      m_pixelShader;
  
  D3D_FEATURE_LEVEL           m_featureLevel;
  
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
