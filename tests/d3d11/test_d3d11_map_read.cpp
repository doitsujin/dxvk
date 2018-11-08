#include <array>
#include <cstring>

#include <d3dcompiler.h>
#include <d3d11.h>

#include <windows.h>
#include <windowsx.h>

#include "../test_utils.h"

using namespace dxvk;

const std::string g_vsCode = 
  "float4 main(float4 v_pos : VS_POSITION) : SV_POSITION {\n"
  "  return v_pos;\n"
  "}\n";

Com<ID3D11Device>           g_d3d11Device;
Com<ID3D11DeviceContext>    g_d3d11Context;

Com<ID3D11VertexShader>     g_vertShader;
Com<ID3D11InputLayout>      g_inputLayout;

Com<ID3D11Buffer>           g_vertexBuffer;

Com<ID3D11Texture2D>        g_depthRender;
Com<ID3D11Texture2D>        g_depthRead;
Com<ID3D11DepthStencilView> g_depthView;
Com<ID3D11DepthStencilState>g_depthState;

struct Vertex {
  float x, y, z, w;
};

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow) {
  if (FAILED(D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE,
        nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
        &g_d3d11Device, nullptr, &g_d3d11Context))) {
    std::cerr << "Failed to create D3D11 device" << std::endl;
    return 1;
  }
  
  Com<ID3DBlob> vsBlob;
  Com<ID3DBlob> gsBlob;

  if (FAILED(D3DCompile(g_vsCode.data(), g_vsCode.size(),
      "Vertex shader", nullptr, nullptr, "main", "vs_4_0",
      0, 0, &vsBlob, nullptr))) {
    std::cerr << "Failed to compile vertex shader" << std::endl;
    return 1;
  }

  if (FAILED(g_d3d11Device->CreateVertexShader(
      vsBlob->GetBufferPointer(),
      vsBlob->GetBufferSize(),
      nullptr, &g_vertShader))) {
    std::cerr << "Failed to create vertex shader" << std::endl;
    return 1;
  }

  std::array<D3D11_INPUT_ELEMENT_DESC, 1> iaElements = {{
    { "VS_POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
  }};

  if (FAILED(g_d3d11Device->CreateInputLayout(
      iaElements.data(),
      iaElements.size(),
      vsBlob->GetBufferPointer(),
      vsBlob->GetBufferSize(),
      &g_inputLayout))) {
    std::cerr << "Failed to create input layout" << std::endl;
    return 1;
  }

  std::array<Vertex, 4> vertexData = {{
    { -1.0f, -1.0f, 0.00f, 1.0f },
    { -1.0f,  1.0f, 0.66f, 1.0f },
    {  1.0f, -1.0f, 0.33f, 1.0f },
    {  1.0f,  1.0f, 1.00f, 1.0f },
  }};

  D3D11_BUFFER_DESC vertexDesc;
  vertexDesc.ByteWidth           = vertexData.size() * sizeof(Vertex);
  vertexDesc.Usage               = D3D11_USAGE_IMMUTABLE;
  vertexDesc.BindFlags           = D3D11_BIND_VERTEX_BUFFER;
  vertexDesc.CPUAccessFlags      = 0;
  vertexDesc.MiscFlags           = 0;
  vertexDesc.StructureByteStride = 0;

  D3D11_SUBRESOURCE_DATA vertexInfo;
  vertexInfo.pSysMem             = vertexData.data();
  vertexInfo.SysMemPitch         = vertexDesc.ByteWidth;
  vertexInfo.SysMemSlicePitch    = vertexDesc.ByteWidth;

  if (FAILED(g_d3d11Device->CreateBuffer(&vertexDesc, &vertexInfo, &g_vertexBuffer))) {
    std::cerr << "Failed to create vertex buffer" << std::endl;
    return 1;
  }

  D3D11_TEXTURE2D_DESC depthDesc;
  depthDesc.Width           = 16;
  depthDesc.Height          = 16;
  depthDesc.MipLevels       = 1;
  depthDesc.ArraySize       = 1;
  depthDesc.Format          = DXGI_FORMAT_D24_UNORM_S8_UINT;
  // depthDesc.Format          = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
  depthDesc.SampleDesc      = { 1, 0 };
  depthDesc.Usage           = D3D11_USAGE_DEFAULT;
  depthDesc.BindFlags       = D3D11_BIND_DEPTH_STENCIL;
  depthDesc.CPUAccessFlags  = 0;
  depthDesc.MiscFlags       = 0;

  if (FAILED(g_d3d11Device->CreateTexture2D(&depthDesc, nullptr, &g_depthRender))) {
    std::cerr << "Failed to create render buffer" << std::endl;
    return 1;
  }

  depthDesc.Usage           = D3D11_USAGE_STAGING;
  depthDesc.BindFlags       = 0;
  depthDesc.CPUAccessFlags  = D3D11_CPU_ACCESS_READ;

  if (FAILED(g_d3d11Device->CreateTexture2D(&depthDesc, nullptr, &g_depthRead))) {
    std::cerr << "Failed to create readback buffer" << std::endl;
    return 1;
  }

  if (FAILED(g_d3d11Device->CreateDepthStencilView(g_depthRender.ptr(), nullptr, &g_depthView))) {
    std::cerr << "Failed to create depth-stencil view" << std::endl;
    return 1;
  }

  D3D11_DEPTH_STENCIL_DESC dsDesc;
  dsDesc.DepthEnable      = TRUE;
  dsDesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL;
  dsDesc.DepthFunc        = D3D11_COMPARISON_ALWAYS;
  dsDesc.StencilEnable    = FALSE;
  dsDesc.StencilReadMask  = 0;
  dsDesc.StencilWriteMask = 0;
  dsDesc.FrontFace        = { };
  dsDesc.BackFace         = { };

  if (FAILED(g_d3d11Device->CreateDepthStencilState(&dsDesc, &g_depthState))) {
    std::cerr << "Failed to create depth-stencil state" << std::endl;
    return 1;
  }

  FLOAT omBlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

  D3D11_VIEWPORT omViewport;
  omViewport.TopLeftX =  0.0f;
  omViewport.TopLeftY =  0.0f;
  omViewport.Width    = 16.0f;
  omViewport.Height   = 16.0f;
  omViewport.MinDepth =  0.0f;
  omViewport.MaxDepth =  1.0f;

  UINT vbOffset = 0;
  UINT vbStride = sizeof(Vertex);

  g_d3d11Context->RSSetState(nullptr);
  g_d3d11Context->RSSetViewports(1, &omViewport);

  g_d3d11Context->OMSetRenderTargets(0, nullptr, g_depthView.ptr());
  g_d3d11Context->OMSetBlendState(nullptr, omBlendFactor, 0xFFFFFFFF);
  g_d3d11Context->OMSetDepthStencilState(g_depthState.ptr(), 0);

  g_d3d11Context->ClearDepthStencilView(g_depthView.ptr(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.5f, 0x80);
  
  g_d3d11Context->IASetInputLayout(g_inputLayout.ptr());
  g_d3d11Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  g_d3d11Context->IASetVertexBuffers(0, 1, &g_vertexBuffer, &vbStride, &vbOffset);

  g_d3d11Context->VSSetShader(g_vertShader.ptr(), nullptr, 0);
  g_d3d11Context->Draw(4, 0);

  g_d3d11Context->CopyResource(g_depthRead.ptr(), g_depthRender.ptr());

  D3D11_MAPPED_SUBRESOURCE mapped;

  if (FAILED(g_d3d11Context->Map(g_depthRead.ptr(), 0, D3D11_MAP_READ, 0, &mapped))) {
    std::cerr << "Failed to map image" << std::endl;
    return 1;
  }

  for (uint32_t y = 0; y < 16; y++) {
    auto data = reinterpret_cast<const uint32_t*>(mapped.pData)
      + (y * mapped.RowPitch / 4);

    for (uint32_t x = 0; x < 16; x++)
      std::cout << std::hex << std::setfill('0') << std::setw(8) << data[x] << "  ";

    std::cout << std::endl;
  }

  g_d3d11Context->Unmap(g_depthRead.ptr(), 0);
  g_d3d11Context->ClearState();
  return 0;
}
