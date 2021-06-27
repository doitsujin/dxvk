#include <d3dcompiler.h>
#include <d3d11_1.h>
#include <dxgi1_3.h>

#include <windows.h>
#include <windowsx.h>

#include <cstring>
#include <string>
#include <sstream>

#include "../test_utils.h"

using namespace dxvk;

struct Vertex {
  float x, y;
};

struct VsConstants {
  float x, y;
  float w, h;
};

struct VsConstantsPad {
  VsConstants data;
  uint32_t pad[60];
};

struct PsConstants {
  float r, g, b, a;
};

struct DrawOptions {
  bool mapDiscardOnce;
  bool sortByTexture;
  bool drawIndexed;
};

const std::string g_vertexShaderCode =
  "cbuffer vs_cb : register(b0) {\n"
  "  float2 v_offset;\n"
  "  float2 v_scale;\n"
  "};\n"
  "float4 main(float4 v_pos : IN_POSITION) : SV_POSITION {\n"
  "  float2 coord = 2.0f * (v_pos * v_scale + v_offset) - 1.0f;\n"
  "  return float4(coord, 0.0f, 1.0f);\n"
  "}\n";

const std::string g_pixelShaderCode =
  "Texture2D<float4> tex0 : register(t0);"
  "cbuffer ps_cb : register(b0) {\n"
  "  float4 color;\n"
  "};\n"
  "float4 main() : SV_TARGET {\n"
  "  return color * tex0.Load(int3(0, 0, 0));\n"
  "}\n";

class TriangleApp {
  
public:
  
  TriangleApp(HINSTANCE instance, HWND window)
  : m_window(window) {
    Com<ID3D11Device> device;

    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_1;

    HRESULT status = D3D11CreateDevice(
      nullptr, D3D_DRIVER_TYPE_HARDWARE,
      nullptr, 0, &fl, 1, D3D11_SDK_VERSION,
      &device, nullptr, nullptr);

    if (FAILED(status)) {
      std::cerr << "Failed to create D3D11 device" << std::endl;
      return;
    }
    
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&m_device)))) {
      std::cerr << "Failed to query ID3D11DeviceContext1" << std::endl;
      return;
    }

    Com<IDXGIDevice> dxgiDevice;

    if (FAILED(m_device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))) {
      std::cerr << "Failed to query DXGI device" << std::endl;
      return;
    }

    if (FAILED(dxgiDevice->GetAdapter(&m_adapter))) {
      std::cerr << "Failed to query DXGI adapter" << std::endl;
      return;
    }

    if (FAILED(m_adapter->GetParent(IID_PPV_ARGS(&m_factory)))) {
      std::cerr << "Failed to query DXGI factory" << std::endl;
      return;
    }

    m_device->GetImmediateContext1(&m_context);

    DXGI_SWAP_CHAIN_DESC1 swapDesc;
    swapDesc.Width          = m_windowSizeW;
    swapDesc.Height         = m_windowSizeH;
    swapDesc.Format         = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.Stereo         = FALSE;
    swapDesc.SampleDesc     = { 1, 0 };
    swapDesc.BufferUsage    = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount    = 3;
    swapDesc.Scaling        = DXGI_SCALING_STRETCH;
    swapDesc.SwapEffect     = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.AlphaMode      = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapDesc.Flags          = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
                            | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsDesc;
    fsDesc.RefreshRate      = { 0, 0 };
    fsDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    fsDesc.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;
    fsDesc.Windowed         = TRUE;
    
    Com<IDXGISwapChain1> swapChain;
    if (FAILED(m_factory->CreateSwapChainForHwnd(m_device.ptr(), m_window, &swapDesc, &fsDesc, nullptr, &swapChain))) {
      std::cerr << "Failed to create DXGI swap chain" << std::endl;
      return;
    }
    
    if (FAILED(swapChain->QueryInterface(IID_PPV_ARGS(&m_swapChain)))) {
      std::cerr << "Failed to query DXGI swap chain interface" << std::endl;
      return;
    }

    m_factory->MakeWindowAssociation(m_window, 0);

    Com<ID3DBlob> vertexShaderBlob;
    Com<ID3DBlob> pixelShaderBlob;
    
    if (FAILED(D3DCompile(g_vertexShaderCode.data(), g_vertexShaderCode.size(),
        "Vertex shader", nullptr, nullptr, "main", "vs_5_0", 0, 0, &vertexShaderBlob, nullptr))) {
      std::cerr << "Failed to compile vertex shader" << std::endl;
      return;
    }
    
    if (FAILED(D3DCompile(g_pixelShaderCode.data(), g_pixelShaderCode.size(),
        "Pixel shader", nullptr, nullptr, "main", "ps_5_0", 0, 0, &pixelShaderBlob, nullptr))) {
      std::cerr << "Failed to compile pixel shader" << std::endl;
      return;
    }
    
    if (FAILED(m_device->CreateVertexShader(
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(),
        nullptr, &m_vs))) {
      std::cerr << "Failed to create vertex shader" << std::endl;
      return;
    }
    
    if (FAILED(m_device->CreatePixelShader(
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize(),
        nullptr, &m_ps))) {
      std::cerr << "Failed to create pixel shader" << std::endl;
      return;
    }
    
    std::array<D3D11_INPUT_ELEMENT_DESC, 1> vertexFormatDesc = {{
      { "IN_POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    }};
    
    if (FAILED(m_device->CreateInputLayout(
        vertexFormatDesc.data(),
        vertexFormatDesc.size(),
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(),
        &m_vertexFormat))) {
      std::cerr << "Failed to create input layout" << std::endl;
      return;
    }

    std::array<Vertex, 6> vertexData = {{
      Vertex { -0.3f, 0.1f },
      Vertex {  0.5f, 0.9f },
      Vertex {  1.3f, 0.1f },
      Vertex { -0.3f, 0.9f },
      Vertex {  1.3f, 0.9f },
      Vertex {  0.5f, 0.1f },
    }};

    D3D11_BUFFER_DESC vboDesc;
    vboDesc.ByteWidth           = sizeof(vertexData);
    vboDesc.Usage               = D3D11_USAGE_IMMUTABLE;
    vboDesc.BindFlags           = D3D11_BIND_VERTEX_BUFFER;
    vboDesc.CPUAccessFlags      = 0;
    vboDesc.MiscFlags           = 0;
    vboDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA vboData;
    vboData.pSysMem             = vertexData.data();
    vboData.SysMemPitch         = vboDesc.ByteWidth;
    vboData.SysMemSlicePitch    = vboDesc.ByteWidth;

    if (FAILED(m_device->CreateBuffer(&vboDesc, &vboData, &m_vbo))) {
      std::cerr << "Failed to create index buffer" << std::endl;
      return;
    }

    std::array<uint32_t, 6> indexData = {{ 0, 1, 2, 3, 4, 5 }};

    D3D11_BUFFER_DESC iboDesc;
    iboDesc.ByteWidth           = sizeof(indexData);
    iboDesc.Usage               = D3D11_USAGE_IMMUTABLE;
    iboDesc.BindFlags           = D3D11_BIND_INDEX_BUFFER;
    iboDesc.CPUAccessFlags      = 0;
    iboDesc.MiscFlags           = 0;
    iboDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA iboData;
    iboData.pSysMem             = indexData.data();
    iboData.SysMemPitch         = iboDesc.ByteWidth;
    iboData.SysMemSlicePitch    = iboDesc.ByteWidth;

    if (FAILED(m_device->CreateBuffer(&iboDesc, &iboData, &m_ibo))) {
      std::cerr << "Failed to create index buffer" << std::endl;
      return;
    }

    D3D11_BUFFER_DESC cbDesc;
    cbDesc.ByteWidth            = sizeof(PsConstants);
    cbDesc.Usage                = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags            = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags       = D3D11_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags            = 0;
    cbDesc.StructureByteStride  = 0;

    if (FAILED(m_device->CreateBuffer(&cbDesc, nullptr, &m_cbPs))) {
      std::cerr << "Failed to create constant buffer" << std::endl;
      return;
    }

    cbDesc.ByteWidth            = sizeof(VsConstantsPad) * 128 * 8;

    if (FAILED(m_device->CreateBuffer(&cbDesc, nullptr, &m_cbVs))) {
      std::cerr << "Failed to create constant buffer" << std::endl;
      return;
    }

    std::array<uint32_t, 2> colors = { 0xFFFFFFFF, 0xFFC0C0C0 };

    D3D11_SUBRESOURCE_DATA texData;
    texData.pSysMem             = &colors[0];
    texData.SysMemPitch         = sizeof(colors[0]);
    texData.SysMemSlicePitch    = sizeof(colors[0]);

    D3D11_TEXTURE2D_DESC texDesc;
    texDesc.Width               = 1;
    texDesc.Height              = 1;
    texDesc.MipLevels           = 1;
    texDesc.ArraySize           = 1;
    texDesc.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc          = { 1, 0 };
    texDesc.Usage               = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags      = 0;
    texDesc.MiscFlags           = 0;

    if (FAILED(m_device->CreateTexture2D(&texDesc, &texData, &m_tex0))) {
      std::cerr << "Failed to create texture" << std::endl;
      return;
    }

    texData.pSysMem             = &colors[1];

    if (FAILED(m_device->CreateTexture2D(&texDesc, &texData, &m_tex1))) {
      std::cerr << "Failed to create texture" << std::endl;
      return;
    }

    if (FAILED(m_device->CreateShaderResourceView(m_tex0.ptr(), nullptr, &m_srv0))
     || FAILED(m_device->CreateShaderResourceView(m_tex1.ptr(), nullptr, &m_srv1))) {
      std::cerr << "Failed to create SRV" << std::endl;
      return;
    }

    m_initialized = true;
  }
  
  
  ~TriangleApp() {
    m_context->ClearState();
  }
  
  
  bool run() {
    if (!m_initialized)
      return false;

    if (m_occluded && (m_occluded = isOccluded()))
      return true;

    if (!beginFrame())
      return true;

    std::array<PsConstants, 2> colors = {{
      PsConstants { 0.25f, 0.25f, 0.25f, 1.0f },
      PsConstants { 0.40f, 0.40f, 0.40f, 1.0f },
    }};

    for (uint32_t i = 0; i < 8; i++) {
      DrawOptions options;
      options.sortByTexture = i & 1;
      options.drawIndexed = i & 2;
      options.mapDiscardOnce = i & 4;
      drawLines(colors[i & 1], options, i);
    }

    if (!endFrame())
      return false;

    updateFps();
    return true;
  }


  void drawLines(const PsConstants& psData, const DrawOptions& options, uint32_t baseY) {
    D3D11_MAPPED_SUBRESOURCE sr;

    // Update color for the row
    m_context->PSSetConstantBuffers(0, 1, &m_cbPs);
    m_context->Map(m_cbPs.ptr(), 0, D3D11_MAP_WRITE_DISCARD, 0, &sr);
    std::memcpy(sr.pData, &psData, sizeof(psData));
    m_context->Unmap(m_cbPs.ptr(), 0);

    baseY *= 8;

    if (options.mapDiscardOnce) {
      uint32_t drawIndex = 0;

      // Discard and map the entire vertex constant buffer
      // once, then bind sub-ranges while emitting draw calls
      m_context->Map(m_cbVs.ptr(), 0, D3D11_MAP_WRITE_DISCARD, 0, &sr);
      auto vsData = reinterpret_cast<VsConstantsPad*>(sr.pData);

      for (uint32_t y = 0; y < 8; y++) {
        for (uint32_t x = 0; x < 128; x++)
          vsData[drawIndex++].data = getVsConstants(x, baseY + y);
      }

      m_context->Unmap(m_cbVs.ptr(), 0);
    }

    if (options.drawIndexed)
      m_context->IASetIndexBuffer(m_ibo.ptr(), DXGI_FORMAT_R32_UINT, 0);

    uint32_t vsStride = sizeof(Vertex);
    uint32_t vsOffset = 0;
    m_context->IASetVertexBuffers(0, 1, &m_vbo, &vsStride, &vsOffset);

    uint32_t maxZ = options.sortByTexture ? 2 : 1;

    for (uint32_t z = 0; z < maxZ; z++) {
      uint32_t drawIndex = z;

      if (options.sortByTexture) {
        ID3D11ShaderResourceView* view = z ? m_srv1.ptr() : m_srv0.ptr();
        m_context->PSSetShaderResources(0, 1, &view);
      }

      for (uint32_t y = 0; y < 8; y++) {
        for (uint32_t x = z; x < 128; x += maxZ) {
          uint32_t triIndex = (x ^ y) & 1;

          if (!options.mapDiscardOnce) {
            D3D11_MAP mapMode = drawIndex ? D3D11_MAP_WRITE_NO_OVERWRITE : D3D11_MAP_WRITE_DISCARD;
            m_context->Map(m_cbVs.ptr(), 0, mapMode, 0, &sr);
            auto vsData = reinterpret_cast<VsConstantsPad*>(sr.pData);
            vsData[drawIndex].data = getVsConstants(x, baseY + y);
            m_context->Unmap(m_cbVs.ptr(), 0);
          }

          uint32_t constantOffset = 16 * drawIndex;
          uint32_t constantCount  = 16;
          m_context->VSSetConstantBuffers1(0, 1, &m_cbVs, &constantOffset, &constantCount);

          if (!options.sortByTexture) {
            ID3D11ShaderResourceView* view = triIndex ? m_srv1.ptr() : m_srv0.ptr();
            m_context->PSSetShaderResources(0, 1, &view);
          }

          // Submit draw call
          uint32_t baseIndex = 3 * triIndex;

          if (options.drawIndexed)
            m_context->DrawIndexed(3, baseIndex, 0);
          else
            m_context->Draw(3, baseIndex);

          drawIndex += maxZ;
        }
      }
    }
  }


  static VsConstants getVsConstants(uint32_t x, uint32_t y) {
    VsConstants result;
    result.x = float(x) / 128.0f;
    result.y = float(y) / 64.0f;
    result.w = 1.0f / 128.0f;
    result.h = 1.0f / 64.0f;
    return result;
  }


  bool beginFrame() {
    // Make sure we can actually render to the window
    RECT windowRect = { 0, 0, 1024, 600 };
    GetClientRect(m_window, &windowRect);
    
    uint32_t newWindowSizeW = uint32_t(windowRect.right - windowRect.left);
    uint32_t newWindowSizeH = uint32_t(windowRect.bottom - windowRect.top);
    
    if (m_windowSizeW != newWindowSizeW || m_windowSizeH != newWindowSizeH) {
      m_rtv = nullptr;
      m_context->ClearState();

      DXGI_SWAP_CHAIN_DESC1 desc;
      m_swapChain->GetDesc1(&desc);

      if (FAILED(m_swapChain->ResizeBuffers(desc.BufferCount,
          newWindowSizeW, newWindowSizeH, desc.Format, desc.Flags))) {
        std::cerr << "Failed to resize back buffers" << std::endl;
        return false;
      }
      
      Com<ID3D11Texture2D> backBuffer;
      if (FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        std::cerr << "Failed to get swap chain back buffer" << std::endl;
        return false;
      }
      
      D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
      rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
      rtvDesc.Format        = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
      rtvDesc.Texture2D     = { 0u };
      
      if (FAILED(m_device->CreateRenderTargetView(backBuffer.ptr(), &rtvDesc, &m_rtv))) {
        std::cerr << "Failed to create render target view" << std::endl;
        return false;
      }

      m_windowSizeW = newWindowSizeW;
      m_windowSizeH = newWindowSizeH;
    }

    // Set up render state
    FLOAT color[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
    m_context->OMSetRenderTargets(1, &m_rtv, nullptr);
    m_context->ClearRenderTargetView(m_rtv.ptr(), color);

    m_context->VSSetShader(m_vs.ptr(), nullptr, 0);
    m_context->PSSetShader(m_ps.ptr(), nullptr, 0);

    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetInputLayout(m_vertexFormat.ptr());

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX     = 0.0f;
    viewport.TopLeftY     = 0.0f;
    viewport.Width        = float(m_windowSizeW);
    viewport.Height       = float(m_windowSizeH);
    viewport.MinDepth     = 0.0f;
    viewport.MaxDepth     = 1.0f;
    m_context->RSSetViewports(1, &viewport);
    return true;
  }


  bool endFrame() {
    HRESULT hr = m_swapChain->Present(0, DXGI_PRESENT_TEST);

    if (hr == S_OK)
      hr = m_swapChain->Present(0, 0);

    m_occluded = hr == DXGI_STATUS_OCCLUDED;
    return true;
  }

  void updateFps() {
    if (!m_qpcFrequency.QuadPart)
      QueryPerformanceFrequency(&m_qpcFrequency);

    if (!m_qpcLastUpdate.QuadPart)
      QueryPerformanceCounter(&m_qpcLastUpdate);

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    m_frameCount++;

    if (now.QuadPart - m_qpcLastUpdate.QuadPart < m_qpcFrequency.QuadPart)
      return;

    double seconds = double(now.QuadPart - m_qpcLastUpdate.QuadPart) / double(m_qpcFrequency.QuadPart);
    double fps = double(m_frameCount) / seconds;

    std::wstringstream str;
    str << L"D3D11 triangle (" << fps << L" FPS)";

    SetWindowTextW(m_window, str.str().c_str());

    m_qpcLastUpdate = now;
    m_frameCount = 0;
  }

  bool isOccluded() {
    return m_swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED;
  }

private:
  
  HWND                          m_window;
  uint32_t                      m_windowSizeW = 1024;
  uint32_t                      m_windowSizeH = 600;
  bool                          m_initialized = false;
  bool                          m_occluded = false;
  
  Com<IDXGIFactory3>            m_factory;
  Com<IDXGIAdapter>             m_adapter;
  Com<ID3D11Device1>            m_device;
  Com<ID3D11DeviceContext1>     m_context;
  Com<IDXGISwapChain2>          m_swapChain;

  Com<ID3D11RenderTargetView>   m_rtv;
  Com<ID3D11Buffer>             m_ibo;
  Com<ID3D11Buffer>             m_vbo;
  Com<ID3D11InputLayout>        m_vertexFormat;

  Com<ID3D11Texture2D>          m_tex0;
  Com<ID3D11Texture2D>          m_tex1;
  Com<ID3D11ShaderResourceView> m_srv0;
  Com<ID3D11ShaderResourceView> m_srv1;
  
  Com<ID3D11Buffer>             m_cbPs;
  Com<ID3D11Buffer>             m_cbVs;

  Com<ID3D11VertexShader>       m_vs;
  Com<ID3D11PixelShader>        m_ps;

  LARGE_INTEGER                 m_qpcLastUpdate = { };
  LARGE_INTEGER                 m_qpcFrequency  = { };

  uint32_t                      m_frameCount = 0;
  
};

LRESULT CALLBACK WindowProc(HWND hWnd,
                            UINT message,
                            WPARAM wParam,
                            LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  WNDCLASSEXW wc = { };
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = HBRUSH(COLOR_WINDOW);
  wc.lpszClassName = L"WindowClass";
  RegisterClassExW(&wc);

  HWND hWnd = CreateWindowExW(0, L"WindowClass", L"D3D11 triangle",
    WS_OVERLAPPEDWINDOW, 300, 300, 1024, 600,
    nullptr, nullptr, hInstance, nullptr);
  ShowWindow(hWnd, nCmdShow);

  TriangleApp app(hInstance, hWnd);

  MSG msg;

  while (true) {
    if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
      
      if (msg.message == WM_QUIT)
        return msg.wParam;
    } else {
      if (!app.run())
        break;
    }
  }

  return msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CLOSE:
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProcW(hWnd, message, wParam, lParam);
}
