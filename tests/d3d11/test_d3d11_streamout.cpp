#include <array>
#include <cstring>

#include <d3dcompiler.h>
#include <d3d11.h>

#include <windows.h>
#include <windowsx.h>

#include "../test_utils.h"

using namespace dxvk;

const std::string g_vsCode = 
  "struct VS_IFACE {\n"
  "  float4 pos : VS_POSITION;\n"
  "};\n"
  "VS_IFACE main(VS_IFACE ia_in) {\n"
  "  return ia_in;\n"
  "}\n";

const std::string g_gsCode = 
  "struct GS_IN {\n"
  "  float4 pos : VS_POSITION;\n"
  "};\n"
  "struct GS_OUT_NORMAL {\n"
  "  float3 nor : GS_NORMAL;\n"
  "  float  len : GS_LENGTH;\n"
  "};\n"
  "[maxvertexcount(1)]\n"
  "void main(triangle GS_IN vs_in[3], inout PointStream<GS_OUT_NORMAL> o_normals) {\n"
  "  float3 ds1 = vs_in[1].pos.xyz - vs_in[0].pos.xyz;\n"
  "  float3 ds2 = vs_in[2].pos.xyz - vs_in[0].pos.xyz;\n"
  "  float3 cv = cross(ds1, ds2);\n"
  "  float  cl = length(cv);\n"
  "  GS_OUT_NORMAL normal;\n"
  "  normal.nor = cv / cl;\n"
  "  normal.len = cl;"
  "  o_normals.Append(normal);\n"
  "}\n";

Com<ID3D11Device>           g_d3d11Device;
Com<ID3D11DeviceContext>    g_d3d11Context;

Com<ID3D11VertexShader>     g_vertShader;
Com<ID3D11GeometryShader>   g_geomShader;

Com<ID3D11InputLayout>      g_inputLayout;

Com<ID3D11Buffer>           g_vertexBuffer;
Com<ID3D11Buffer>           g_normalBuffer;
Com<ID3D11Buffer>           g_readBuffer;

Com<ID3D11Query>            g_soStream;
Com<ID3D11Query>            g_soOverflow;

struct Vertex {
  float x, y, z, w;
};

struct Normal {
  float x, y, z, len;
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

  if (FAILED(D3DCompile(g_gsCode.data(), g_gsCode.size(),
      "Geometry shader", nullptr, nullptr, "main", "gs_4_0",
      0, 0, &gsBlob, nullptr))) {
    std::cerr << "Failed to compile geometry shader" << std::endl;
    return 1;
  }

  if (FAILED(g_d3d11Device->CreateVertexShader(
      vsBlob->GetBufferPointer(),
      vsBlob->GetBufferSize(),
      nullptr, &g_vertShader))) {
    std::cerr << "Failed to create vertex shader" << std::endl;
    return 1;
  }

  std::array<D3D11_SO_DECLARATION_ENTRY, 2> soDeclarations = {{
    { 0, "GS_NORMAL", 0, 0, 3, 0 },
    { 0, "GS_LENGTH", 0, 0, 1, 0 },
  }};

  std::array<UINT, 1> soBufferStrides = {{
    sizeof(Normal),
  }};

  if (FAILED(g_d3d11Device->CreateGeometryShaderWithStreamOutput(
      gsBlob->GetBufferPointer(),
      gsBlob->GetBufferSize(),
      soDeclarations.data(),
      soDeclarations.size(),
      soBufferStrides.data(),
      soBufferStrides.size(),
      D3D11_SO_NO_RASTERIZED_STREAM,
      nullptr, &g_geomShader))) {
    std::cerr << "Failed to create geometry shader" << std::endl;
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

  std::array<Vertex, 9> vertexData = {{
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { 1.0f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 1.0f, 0.0f, 1.0f },

    { 0.5f,-1.0f,-0.2f, 1.0f },
    { 3.2f, 2.0f, 0.0f, 1.0f },
    {-1.0f,-1.0f, 0.4f, 1.0f },

    { 0.7f,-0.5f,-0.8f, 1.0f },
    { 1.2f, 1.0f,-1.0f, 1.0f },
    {-0.1f, 1.0f,-2.7f, 1.0f },
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

  std::array<Normal, 2> normalData = { };

  D3D11_BUFFER_DESC normalDesc;
  normalDesc.ByteWidth           = normalData.size() * sizeof(Normal);
  normalDesc.Usage               = D3D11_USAGE_DEFAULT;
  normalDesc.BindFlags           = D3D11_BIND_STREAM_OUTPUT;
  normalDesc.CPUAccessFlags      = 0;
  normalDesc.MiscFlags           = 0;
  normalDesc.StructureByteStride = 0;

  D3D11_SUBRESOURCE_DATA normalInfo;
  normalInfo.pSysMem             = normalData.data();
  normalInfo.SysMemPitch         = normalDesc.ByteWidth;
  normalInfo.SysMemSlicePitch    = normalDesc.ByteWidth;

  if (FAILED(g_d3d11Device->CreateBuffer(&normalDesc, &normalInfo, &g_normalBuffer))) {
    std::cerr << "Failed to create normal buffer" << std::endl;
    return 1;
  }

  D3D11_BUFFER_DESC readDesc;
  readDesc.ByteWidth           = normalDesc.ByteWidth;
  readDesc.Usage               = D3D11_USAGE_STAGING;
  readDesc.BindFlags           = 0;
  readDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_READ;
  readDesc.MiscFlags           = 0;
  readDesc.StructureByteStride = 0;

  if (FAILED(g_d3d11Device->CreateBuffer(&readDesc, nullptr, &g_readBuffer))) {
    std::cerr << "Failed to create readback buffer" << std::endl;
    return 1;
  }

  D3D11_QUERY_DESC soQueryDesc;
  soQueryDesc.Query = D3D11_QUERY_SO_STATISTICS_STREAM0;
  soQueryDesc.MiscFlags = 0;

  if (FAILED(g_d3d11Device->CreateQuery(&soQueryDesc, &g_soStream))) {
    std::cerr << "Failed to create streamout query" << std::endl;
    return 1;
  }

  soQueryDesc.Query = D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0;
  if (FAILED(g_d3d11Device->CreateQuery(&soQueryDesc, &g_soOverflow))) {
    std::cerr << "Failed to create streamout overflow query" << std::endl;
    return 1;
  }

  UINT soOffset = 0;
  UINT vbOffset = 0;
  UINT vbStride = sizeof(Vertex);

  FLOAT omBlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

  D3D11_VIEWPORT omViewport;
  omViewport.TopLeftX =   0.0f;
  omViewport.TopLeftY =   0.0f;
  omViewport.Width    = 256.0f;
  omViewport.Height   = 256.0f;
  omViewport.MinDepth =   0.0f;
  omViewport.MaxDepth =   1.0f;

  g_d3d11Context->IASetInputLayout(g_inputLayout.ptr());
  g_d3d11Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  g_d3d11Context->IASetVertexBuffers(0, 1, &g_vertexBuffer, &vbStride, &vbOffset);

  g_d3d11Context->RSSetState(nullptr);
  g_d3d11Context->RSSetViewports(1, &omViewport);

  g_d3d11Context->OMSetRenderTargets(0, nullptr, nullptr);
  g_d3d11Context->OMSetBlendState(nullptr, omBlendFactor, 0xFFFFFFFF);
  g_d3d11Context->OMSetDepthStencilState(nullptr, 0);

  g_d3d11Context->SOSetTargets(1, &g_normalBuffer, &soOffset);
  
  g_d3d11Context->VSSetShader(g_vertShader.ptr(), nullptr, 0);
  g_d3d11Context->GSSetShader(g_geomShader.ptr(), nullptr, 0);
  
  g_d3d11Context->Begin(g_soStream.ptr());
  g_d3d11Context->Begin(g_soOverflow.ptr());

  g_d3d11Context->Draw(vertexData.size(), 0);

  g_d3d11Context->End(g_soOverflow.ptr());
  g_d3d11Context->End(g_soStream.ptr());

  g_d3d11Context->CopyResource(
    g_readBuffer.ptr(),
    g_normalBuffer.ptr());
  
  D3D11_QUERY_DATA_SO_STATISTICS soQueryData = { };
  BOOL soOverflowData = false;
  
  while (g_d3d11Context->GetData(g_soStream.ptr(), &soQueryData, sizeof(soQueryData), 0) != S_OK
      || g_d3d11Context->GetData(g_soOverflow.ptr(), &soOverflowData, sizeof(soOverflowData), 0) != S_OK)
    continue;
  
  std::cout << "Written:  " << soQueryData.NumPrimitivesWritten << std::endl;
  std::cout << "Needed:   " << soQueryData.PrimitivesStorageNeeded << std::endl;
  std::cout << "Overflow: " << (soOverflowData ? "Yes" : "No") << std::endl;

  D3D11_MAPPED_SUBRESOURCE mapInfo;

  if (FAILED(g_d3d11Context->Map(g_readBuffer.ptr(), 0, D3D11_MAP_READ, 0, &mapInfo))) {
    std::cerr << "Failed to map readback buffer" << std::endl;
    return 1;
  }

  std::memcpy(normalData.data(), mapInfo.pData, normalDesc.ByteWidth);
  g_d3d11Context->Unmap(g_readBuffer.ptr(), 0);
  
  for (uint32_t i = 0; i < normalData.size(); i++) {
    std::cout << i << ": " << normalData[i].x << ","
      << normalData[i].y << "," << normalData[i].z << ","
      << normalData[i].len << std::endl;
  }

  return 0;
}
