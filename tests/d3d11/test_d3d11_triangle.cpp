#include <d3dcompiler.h>
#include <d3d11.h>

#include <windows.h>
#include <windowsx.h>

#include <thread>

#include "../../src/util/thread.h"
#include "../test_utils.h"

using namespace dxvk;

struct Extent2D {
  uint32_t w, h;
};

struct Vertex {
  float x, y, z, w;
};

struct Color {
  uint8_t r, g, b, a;
};

  bool m_report = false;
  
const std::string g_vertexShaderCode =
  "Buffer<float4> buf : register(t0);\n"
  "struct vs_out {\n"
  "  float4 pos   : POSITION;\n"
  "  float4 color : COLOR;\n"
  "};\n"
  "vs_out main(float4 vsIn : IN_POSITION,\n"
  "    uint vid : SV_VERTEXID,\n"
  "    uint iid : SV_INSTANCEID) {\n"
  "  vs_out result;\n"
  "  result.pos = vsIn;\n"
  "  result.color.x = buf[vid].x;\n"
  "  result.color.y = buf[iid * 3].y;\n"
  "  result.color.z = buf[0].z;\n"
  "  result.color.w = 1.0f;\n"
  "  return result;\n"
  "}\n";

const std::string g_hullShaderCode =
  "struct vs_out {\n"
  "  float4 pos   : POSITION;\n"
  "  float4 color : COLOR;\n"
  "};\n"
  "struct hs_vtx {\n"
  "  float4 pos   : POSITION;\n"
  "};\n"
  "struct hs_patch {\n"
  "  float4 color  : COLOR;\n"
  "  float  tessEdge[3] : SV_TessFactor;\n"
  "  float  tessInner : SV_InsideTessFactor;\n"
  "};\n"
  "hs_patch main_pc(InputPatch<vs_out, 3> ip) {\n"
  "  hs_patch ov;\n"
  "  ov.color = ip[0].color;\n"
  "  ov.tessEdge[0] = 4.0f;\n"
  "  ov.tessEdge[1] = 4.0f;\n"
  "  ov.tessEdge[2] = 4.0f;\n"
  "  ov.tessInner = 4.0f;\n"
  "  return ov;\n"
  "}\n"
  "[domain(\"tri\")]\n"
  "[partitioning(\"fractional_odd\")]\n"
  "[outputtopology(\"triangle_cw\")]\n"
  "[outputcontrolpoints(3)]\n"
  "[patchconstantfunc(\"main_pc\")]\n"
  "hs_vtx main(InputPatch<vs_out, 3> ip, uint i : SV_OutputControlPointID) {\n"
  "  hs_vtx ov;\n"
  "  ov.pos = ip[i].pos;\n"
  "  return ov;\n"
  "}\n";

const std::string g_domainShaderCode =
  "struct ds_out {\n"
  "  float4 pos   : SV_POSITION;\n"
  "  float4 color : COLOR;\n"
  "};\n"
  "struct hs_vtx {\n"
  "  float4 pos   : POSITION;\n"
  "};\n"
  "struct hs_patch {\n"
  "  float4 color  : COLOR;\n"
  "  float  tessEdge[3] : SV_TessFactor;\n"
  "  float  tessInner : SV_InsideTessFactor;\n"
  "};\n"
  "[domain(\"tri\")]\n"
  "ds_out main(float3 p : SV_DomainLocation, OutputPatch<hs_vtx, 3> ip, hs_patch pc) {\n"
  "  ds_out ov;\n"
  "  ov.pos = ip[0].pos * p.x\n"
  "         + ip[1].pos * p.y\n"
  "         + ip[2].pos * p.z;\n"
  "  ov.color = pc.color;\n"
  "  return ov;\n"
  "}\n";


const std::string g_pixelShaderCode =
  "struct vs_out {\n"
  "  float4 pos   : SV_POSITION;\n"
  "  float4 color : COLOR;\n"
  "};\n"
  "float4 main(vs_out ps_in) : SV_TARGET {\n"
  "  return ps_in.color;\n"
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
    swapDesc.SampleDesc.Count             = 8;
    swapDesc.SampleDesc.Quality           = 0;
    swapDesc.BufferUsage                  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount                  = 2;
    swapDesc.OutputWindow                 = window;
    swapDesc.Windowed                     = true;
    swapDesc.SwapEffect                   = DXGI_SWAP_EFFECT_DISCARD;
    swapDesc.Flags                        = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    
    if (FAILED(m_factory->CreateSwapChain(m_device.ptr(), &swapDesc, &m_swapChain)))
      throw DxvkError("Failed to create DXGI swap chain");
    
    if (FAILED(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&m_buffer))))
      throw DxvkError("Failed to get swap chain back buffer");
    
    if (FAILED(m_device->CreateRenderTargetView(m_buffer.ptr(), nullptr, &m_bufferView)))
      throw DxvkError("Failed to create render target view");
    
    if (FAILED(m_swapChain->ResizeTarget(&swapDesc.BufferDesc)))
      throw DxvkError("Failed to resize window");
    
    std::array<Vertex, 21> vertexData = {{
      { -0.25f, -0.15f, 0.0f, 1.0f },
      { -0.50f, -0.65f, 0.0f, 1.0f },
      { -0.75f, -0.15f, 0.0f, 1.0f },
      {  0.75f, -0.15f, 0.0f, 1.0f },
      {  0.50f, -0.65f, 0.0f, 1.0f },
      {  0.25f, -0.15f, 0.0f, 1.0f },
      { -0.75f,  0.15f, 0.0f, 1.0f },
      { -0.50f,  0.65f, 0.0f, 1.0f },
      { -0.25f,  0.15f, 0.0f, 1.0f },
      {  0.25f,  0.15f, 0.0f, 1.0f },
      {  0.50f,  0.65f, 0.0f, 1.0f },
      {  0.75f,  0.15f, 0.0f, 1.0f },
      {  0.25f,  0.75f, 0.0f, 1.0f },
      {  0.00f,  0.25f, 0.0f, 1.0f },
      { -0.25f,  0.75f, 0.0f, 1.0f },
      { -0.25f, -0.75f, 0.0f, 1.0f },
      {  0.00f, -0.25f, 0.0f, 1.0f },
      {  0.25f, -0.75f, 0.0f, 1.0f },
      { -0.25f, -0.25f, 0.0f, 1.0f },
      {  0.00f,  0.25f, 0.0f, 1.0f },
      {  0.25f, -0.25f, 0.0f, 1.0f },
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
    
    std::array<uint32_t, 9> indexData = {{
      0, 1, 2, 0, 1, 2, 2, 1, 0
    }};
    
    D3D11_BUFFER_DESC indexDesc;
    indexDesc.ByteWidth            = sizeof(uint32_t) * indexData.size();
    indexDesc.Usage                = D3D11_USAGE_IMMUTABLE;
    indexDesc.BindFlags            = D3D11_BIND_INDEX_BUFFER;
    indexDesc.CPUAccessFlags       = 0;
    indexDesc.MiscFlags            = 0;
    indexDesc.StructureByteStride  = 0;
    
    D3D11_SUBRESOURCE_DATA indexDataInfo;
    indexDataInfo.pSysMem          = indexData.data();
    indexDataInfo.SysMemPitch      = 0;
    indexDataInfo.SysMemSlicePitch = 0;
    
    if (FAILED(m_device->CreateBuffer(&indexDesc, &indexDataInfo, &m_indexBuffer)))
      throw DxvkError("Failed to create index buffer");
    
    std::array<Color, 6> resourceData = {{
      { 0x20, 0x20, 0x20, 0xFF },
      { 0x20, 0x20, 0x20, 0xFF },
      { 0x20, 0x20, 0x20, 0xFF },
      { 0xFF, 0xFF, 0x00, 0xFF },
      { 0xFF, 0xFF, 0x00, 0xFF },
      { 0xFF, 0xFF, 0x00, 0xFF },
    }};
    
    D3D11_BUFFER_DESC resourceDesc;
    resourceDesc.ByteWidth            = sizeof(Color) * resourceData.size();
    resourceDesc.Usage                = D3D11_USAGE_IMMUTABLE;
    resourceDesc.BindFlags            = D3D11_BIND_SHADER_RESOURCE;
    resourceDesc.CPUAccessFlags       = 0;
    resourceDesc.MiscFlags            = 0;
    resourceDesc.StructureByteStride  = 0;
    
    D3D11_SUBRESOURCE_DATA resourceDataInfo;
    resourceDataInfo.pSysMem          = resourceData.data();
    resourceDataInfo.SysMemPitch      = 0;
    resourceDataInfo.SysMemSlicePitch = 0;
    
    if (FAILED(m_device->CreateBuffer(&resourceDesc, &resourceDataInfo, &m_resourceBuffer)))
      throw DxvkError("Failed to create resource buffer");
    
    D3D11_SHADER_RESOURCE_VIEW_DESC resourceViewDesc;
    resourceViewDesc.Format        = DXGI_FORMAT_R8G8B8A8_UNORM;
    resourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    resourceViewDesc.Buffer.FirstElement = 0;
    resourceViewDesc.Buffer.NumElements  = resourceData.size();
    
    if (FAILED(m_device->CreateShaderResourceView(m_resourceBuffer.ptr(), &resourceViewDesc, &m_resourceView)))
      throw DxvkError("Failed to create resource buffer view");
    
    Com<ID3DBlob> vertexShaderBlob;
    Com<ID3DBlob> hullShaderBlob;
    Com<ID3DBlob> domainShaderBlob;
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
          g_hullShaderCode.data(),
          g_hullShaderCode.size(),
          "Hull shader",
          nullptr, nullptr,
          "main", "hs_5_0", 0, 0,
          &hullShaderBlob,
          nullptr)))
      throw DxvkError("Failed to compile hull shader");
    
    if (FAILED(D3DCompile(
          g_domainShaderCode.data(),
          g_domainShaderCode.size(),
          "Domain shader",
          nullptr, nullptr,
          "main", "ds_5_0", 0, 0,
          &domainShaderBlob,
          nullptr)))
      throw DxvkError("Failed to compile domain shader");
    
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
    
    if (FAILED(m_device->CreateHullShader(
          hullShaderBlob->GetBufferPointer(),
          hullShaderBlob->GetBufferSize(),
          nullptr, &m_hullShader)))
      throw DxvkError("Failed to create hull shader");
    
    if (FAILED(m_device->CreateDomainShader(
          domainShaderBlob->GetBufferPointer(),
          domainShaderBlob->GetBufferSize(),
          nullptr, &m_domainShader)))
      throw DxvkError("Failed to create domain shader");
    
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
    
    D3D11_QUERY_DESC queryDesc;
    queryDesc.Query = D3D11_QUERY_OCCLUSION;
    queryDesc.MiscFlags = 0;
    
    if (FAILED(m_device->CreateQuery(&queryDesc, &m_query)))
      throw DxvkError("Failed to create occlusion query");
  }
  
  
  ~TriangleApp() {
    m_context->ClearState();
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
    m_context->HSSetShader(m_hullShader.ptr(), nullptr, 0);
    m_context->DSSetShader(m_domainShader.ptr(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.ptr(), nullptr, 0);
    
    m_context->VSSetShaderResources(0, 1, &m_resourceView);
    
    UINT vsStride = sizeof(Vertex);
    UINT vsOffset = 0;
    
    // Test normal draws with base vertex
    m_context->Begin(m_query.ptr());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
    m_context->IASetInputLayout(m_vertexFormat.ptr());
    m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &vsStride, &vsOffset);
    m_context->Draw(3, 0);
    m_context->Draw(3, 3);
    m_context->End(m_query.ptr());
    
    // Test instanced draws with base instance and base vertex
    vsOffset = 6 * sizeof(Vertex);
    m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &vsStride, &vsOffset);
    m_context->DrawInstanced(3, 1, 0, 1);
    m_context->DrawInstanced(3, 1, 3, 1);
    
    // Test indexed draws with base vertex and base index
    vsOffset = 12 * sizeof(Vertex);
    m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &vsStride, &vsOffset);
    m_context->IASetIndexBuffer(m_indexBuffer.ptr(), DXGI_FORMAT_R32_UINT, 0);
    m_context->DrawIndexed(3, 0, 0);
    m_context->DrawIndexed(3, 3, 3);
    
    // Test default backface culling
    vsOffset = 18 * sizeof(Vertex);
    m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &vsStride, &vsOffset);
    m_context->DrawIndexed(3, 6, 0);
    
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    
    m_swapChain->Present(1, 0);
    
    // Test query results
    while (true) {
      UINT64 samplesPassed = 0;
      
      UINT queryStatus = m_context->GetData(
        m_query.ptr(), &samplesPassed, sizeof(samplesPassed),
        D3D11_ASYNC_GETDATA_DONOTFLUSH);
      
      if (queryStatus == S_OK) {
        if (samplesPassed == 0)
          std::cerr << "Occlusion query returned 0 samples" << std::endl;
        break;
      } else if (queryStatus == S_FALSE) {
        std::this_thread::yield();
      } else {
        std::cerr << "Occlusion query failed" << std::endl;
        break;
      }
    }
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
  
  HWND                          m_window;
  Extent2D                      m_windowSize = { 1024, 600 };
  
  Com<IDXGIFactory>             m_factory;
  Com<IDXGIAdapter>             m_adapter;
  Com<ID3D11Device>             m_device;
  Com<ID3D11DeviceContext>      m_context;
  Com<IDXGISwapChain>           m_swapChain;
    
  Com<ID3D11Texture2D>          m_buffer;
  Com<ID3D11RenderTargetView>   m_bufferView;
  Com<ID3D11Buffer>             m_resourceBuffer;
  Com<ID3D11ShaderResourceView> m_resourceView;
  Com<ID3D11Buffer>             m_indexBuffer;
  Com<ID3D11Buffer>             m_vertexBuffer;
  Com<ID3D11InputLayout>        m_vertexFormat;
  
  Com<ID3D11VertexShader>       m_vertexShader;
  Com<ID3D11HullShader>         m_hullShader;
  Com<ID3D11DomainShader>       m_domainShader;
  Com<ID3D11PixelShader>        m_pixelShader;
  
  Com<ID3D11Query>              m_query;
  
  D3D_FEATURE_LEVEL             m_featureLevel;
  
  uint32_t m_frameId = 0;
  
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
  if (m_report) {
    std::cout << "hwnd: " << hWnd    << std::endl;
    std::cout << "msg:  " << message << std::endl;
    std::cout << "wp:   " << wParam  << std::endl;
    std::cout << "lp:   " << lParam  << std::endl;
  }
  
  switch (message) {
    case WM_CLOSE:
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}
