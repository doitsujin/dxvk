#include <cstring>

#include <d3d9.h>
#include <d3dcompiler.h>

#include "../test_utils.h"

using namespace dxvk;

struct Extent2D {
  uint32_t w, h;
};

const std::string g_vertexShaderCode = R"(

struct VS_INPUT {
  float3 Position : POSITION;
};

struct VS_OUTPUT {
  float4 Position : POSITION;
  float2 Texcoord : TEXCOORD0;
};

VS_OUTPUT main( VS_INPUT IN ) {
  VS_OUTPUT OUT;
  OUT.Position = float4(IN.Position, 0.6f);
  OUT.Texcoord = IN.Position.xy + float2(0.5, 0.5);
  OUT.Texcoord.y = 1.0 - OUT.Texcoord.y;

  return OUT;
}

)";

const std::string g_pixelShaderCode = R"(

struct VS_OUTPUT {
  float4 Position : POSITION;
  float2 Texcoord : TEXCOORD0;
};

struct PS_OUTPUT {
  float4 Colour   : COLOR;
};

sampler g_frogTex : register( s0 );

PS_OUTPUT main( VS_OUTPUT IN ) {
  PS_OUTPUT OUT;

  OUT.Colour = float4(1, 0, 0, 1);

  return OUT;
}


)";

Logger Logger::s_instance("triangle.log");

class TriangleApp {
  
public:
  
  TriangleApp(HINSTANCE instance, HWND window)
  : m_window(window) {
    HRESULT status = Direct3DCreate9Ex(D3D_SDK_VERSION, &m_d3d);

    if (FAILED(status))
      throw DxvkError("Failed to create D3D9 interface");

    UINT adapter = D3DADAPTER_DEFAULT;

    D3DADAPTER_IDENTIFIER9 adapterId;
    m_d3d->GetAdapterIdentifier(adapter, 0, &adapterId);

    Logger::info(str::format("Using adapter: ", adapterId.Description));

    D3DPRESENT_PARAMETERS params;
    getPresentParams(params);

    status = m_d3d->CreateDeviceEx(
      adapter,
      D3DDEVTYPE_HAL,
      m_window,
      D3DCREATE_HARDWARE_VERTEXPROCESSING,
      &params,
      nullptr,
      &m_device);

    if (FAILED(status))
        throw DxvkError("Failed to create D3D9 device");


    // Vertex Shader
    {
      Com<ID3DBlob> blob;

      status = D3DCompile(
        g_vertexShaderCode.data(),
        g_vertexShaderCode.length(),
        nullptr, nullptr, nullptr,
        "main",
        "vs_2_0",
        0, 0, &blob,
        nullptr);

      if (FAILED(status))
        throw DxvkError("Failed to compile vertex shader");

      status = m_device->CreateVertexShader(reinterpret_cast<const DWORD*>(blob->GetBufferPointer()), &m_vs);

      if (FAILED(status))
        throw DxvkError("Failed to create vertex shader");
    }

    // Pixel Shader
    {
      Com<ID3DBlob> blob;

      status = D3DCompile(
        g_pixelShaderCode.data(),
        g_pixelShaderCode.length(),
        nullptr, nullptr, nullptr,
        "main",
        "ps_3_0",
        0, 0, &blob,
        nullptr);

      if (FAILED(status))
        throw DxvkError("Failed to compile pixel shader");

      status = m_device->CreatePixelShader(reinterpret_cast<const DWORD*>(blob->GetBufferPointer()), &m_ps);

      if (FAILED(status))
        throw DxvkError("Failed to create pixel shader");
    }

    m_device->SetVertexShader(m_vs.ptr());
    m_device->SetPixelShader(m_ps.ptr());

    m_device->AddRef();


    std::array<float, 9> vertices = {
      0.0f, 0.5f, 0.0f,
      0.5f, -0.5f, 0.0f,
      -0.5f, -0.5f, 0.0f,
    };

    const size_t vbSize = vertices.size() * sizeof(float);

    status = m_device->CreateVertexBuffer(vbSize, 0, 0, D3DPOOL_DEFAULT, &m_vb, nullptr);
    if (FAILED(status))
      throw DxvkError("Failed to create vertex buffer");

    void* data = nullptr;
    status = m_vb->Lock(0, 0, &data, 0);
    if (FAILED(status))
      throw DxvkError("Failed to lock vertex buffer");

    std::memcpy(data, vertices.data(), vbSize);

    status = m_vb->Unlock();
    if (FAILED(status))
      throw DxvkError("Failed to unlock vertex buffer");

    m_device->SetStreamSource(0, m_vb.ptr(), 0, 3 * sizeof(float));

    std::array<D3DVERTEXELEMENT9, 2> elements;

    elements[0].Method = 0;
    elements[0].Offset = 0;
    elements[0].Stream = 0;
    elements[0].Type = D3DDECLTYPE_FLOAT3;
    elements[0].Usage = D3DDECLUSAGE_POSITION;
    elements[0].UsageIndex = 0;

    elements[1] = D3DDECL_END();

    HRESULT result = m_device->CreateVertexDeclaration(elements.data(), &m_decl);
    if (FAILED(result))
      throw DxvkError("Failed to create vertex decl");

    m_device->SetVertexDeclaration(m_decl.ptr());

    const uint32_t imageSize = 8;

    Com<IDirect3DTexture9> texDefault;
    Com<IDirect3DSurface9> texDefaultSurf;
    status = m_device->CreateTexture(imageSize, imageSize, 4, 0, D3DFMT_DXT1, D3DPOOL_DEFAULT, &texDefault, nullptr);
    status = texDefault->GetSurfaceLevel(2, &texDefaultSurf);

    Com<IDirect3DTexture9> texSysmem;
    Com<IDirect3DSurface9> texSysmemSurf;
    status = m_device->CreateTexture(imageSize, imageSize, 4, 0, D3DFMT_DXT1, D3DPOOL_SYSTEMMEM, &texSysmem, nullptr);
    status = texSysmem->GetSurfaceLevel(2, &texSysmemSurf);

    D3DLOCKED_RECT rect = {};
    status = texSysmemSurf->LockRect(&rect, nullptr, 0);
    std::memset(rect.pBits, 0xF0, 2 * rect.Pitch);
    status = texSysmemSurf->UnlockRect();

    // 1 returns invalid call
    // 2 succeeds
    RECT sourceRect = { 0, 0, 2, 2 };
    POINT destPoint = { 0, 0 };
    status = m_device->UpdateSurface(texSysmemSurf.ptr(), &sourceRect, texDefaultSurf.ptr(), &destPoint);
  }
  
  void run() {
    this->adjustBackBuffer();

    m_device->BeginScene();

    m_device->Clear(
      0,
      nullptr,
      D3DCLEAR_TARGET,
      D3DCOLOR_RGBA(44, 62, 80, 0),
      0,
      0);

    m_device->Clear(
      0,
      nullptr,
      D3DCLEAR_ZBUFFER,
      0,
      0.5f,
      0);

    m_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);

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
    params.EnableAutoDepthStencil = 0;
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

  Com<IDirect3DVertexShader9>   m_vs;
  Com<IDirect3DPixelShader9>    m_ps;
  Com<IDirect3DVertexBuffer9>   m_vb;
  Com<IDirect3DVertexDeclaration9> m_decl;
  
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
