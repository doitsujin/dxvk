#include <cstring>

#include <d3dcompiler.h>
#include <d3d11.h>

#include <windows.h>
#include <windowsx.h>

#include "../test_utils.h"

using namespace dxvk;

const std::string g_computeShaderCode =
  "StructuredBuffer<uint> buf_in : register(t0);\n"
  "RWStructuredBuffer<uint> buf_out : register(u0);\n"
  "groupshared uint tmp[64];\n"
  "[numthreads(64,1,1)]\n"
  "void main(uint localId : SV_GroupIndex, uint3 globalId : SV_DispatchThreadID) {\n"
  "  tmp[localId] = buf_in[2 * globalId.x + 0]\n"
  "               + buf_in[2 * globalId.x + 1];\n"
  "  GroupMemoryBarrierWithGroupSync();\n"
  "  uint activeGroups = 32;\n"
  "  while (activeGroups != 0) {\n"
  "    if (localId < activeGroups)\n"
  "      tmp[localId] += tmp[localId + activeGroups];\n"
  "    GroupMemoryBarrierWithGroupSync();\n"
  "    activeGroups >>= 1;\n"
  "  }\n"
  "  if (localId == 0)\n"
  "    buf_out[0] = tmp[0];\n"
  "}\n";
  
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow) {
  Com<ID3D11Device>         device;
  Com<ID3D11DeviceContext>  context;
  Com<ID3D11ComputeShader>  computeShader;
  
  Com<ID3D11Buffer> srcBuffer;
  Com<ID3D11Buffer> dstBuffer;
  Com<ID3D11Buffer> readBuffer;
  
  Com<ID3D11ShaderResourceView> srcView;
  Com<ID3D11UnorderedAccessView> dstView;
  
  if (FAILED(D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE,
        nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
        &device, nullptr, &context))) {
    std::cerr << "Failed to create D3D11 device" << std::endl;
    return 1;
  }
  
  Com<ID3DBlob> computeShaderBlob;
  
  if (FAILED(D3DCompile(
        g_computeShaderCode.data(),
        g_computeShaderCode.size(),
        "Compute shader",
        nullptr, nullptr,
        "main", "cs_5_0", 0, 0,
        &computeShaderBlob,
        nullptr))) {
    std::cerr << "Failed to compile compute shader" << std::endl;
    return 1;
  }
  
  if (FAILED(device->CreateComputeShader(
        computeShaderBlob->GetBufferPointer(),
        computeShaderBlob->GetBufferSize(),
        nullptr, &computeShader))) {
    std::cerr << "Failed to create compute shader" << std::endl;
    return 1;
  }
  
  std::array<uint32_t, 128> srcData;
  for (uint32_t i = 0; i < srcData.size(); i++)
    srcData[i] = i + 1;
  
  D3D11_BUFFER_DESC srcBufferDesc;
  srcBufferDesc.ByteWidth            = sizeof(uint32_t) * srcData.size();
  srcBufferDesc.Usage                = D3D11_USAGE_IMMUTABLE;
  srcBufferDesc.BindFlags            = D3D11_BIND_SHADER_RESOURCE;
  srcBufferDesc.CPUAccessFlags       = 0;
  srcBufferDesc.MiscFlags            = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  srcBufferDesc.StructureByteStride  = sizeof(uint32_t);
  
  D3D11_SUBRESOURCE_DATA srcDataInfo;
  srcDataInfo.pSysMem          = srcData.data();
  srcDataInfo.SysMemPitch      = 0;
  srcDataInfo.SysMemSlicePitch = 0;
  
  if (FAILED(device->CreateBuffer(&srcBufferDesc, &srcDataInfo, &srcBuffer))) {
    std::cerr << "Failed to create source buffer" << std::endl;
    return 1;
  }
  
  D3D11_BUFFER_DESC dstBufferDesc;
  dstBufferDesc.ByteWidth            = sizeof(uint32_t);
  dstBufferDesc.Usage                = D3D11_USAGE_DEFAULT;
  dstBufferDesc.BindFlags            = D3D11_BIND_UNORDERED_ACCESS;
  dstBufferDesc.CPUAccessFlags       = 0;
  dstBufferDesc.MiscFlags            = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  dstBufferDesc.StructureByteStride  = sizeof(uint32_t);
  
  if (FAILED(device->CreateBuffer(&dstBufferDesc, &srcDataInfo, &dstBuffer))) {
    std::cerr << "Failed to create destination buffer" << std::endl;
    return 1;
  }
  
  D3D11_BUFFER_DESC readBufferDesc;
  readBufferDesc.ByteWidth            = sizeof(uint32_t);
  readBufferDesc.Usage                = D3D11_USAGE_STAGING;
  readBufferDesc.BindFlags            = 0;
  readBufferDesc.CPUAccessFlags       = D3D11_CPU_ACCESS_READ;
  readBufferDesc.MiscFlags            = 0;
  readBufferDesc.StructureByteStride  = 0;
  
  if (FAILED(device->CreateBuffer(&readBufferDesc, nullptr, &readBuffer))) {
    std::cerr << "Failed to create readback buffer" << std::endl;
    return 1;
  }
  
  D3D11_SHADER_RESOURCE_VIEW_DESC srcViewDesc;
  srcViewDesc.Format                = DXGI_FORMAT_UNKNOWN;
  srcViewDesc.ViewDimension         = D3D11_SRV_DIMENSION_BUFFEREX;
  srcViewDesc.BufferEx.FirstElement = 0;
  srcViewDesc.BufferEx.NumElements  = srcData.size();
  srcViewDesc.BufferEx.Flags        = 0;
  
  if (FAILED(device->CreateShaderResourceView(srcBuffer.ptr(), &srcViewDesc, &srcView))) {
    std::cerr << "Failed to create shader resource view" << std::endl;
    return 1;
  }
  
  D3D11_UNORDERED_ACCESS_VIEW_DESC dstViewDesc;
  dstViewDesc.Format                = DXGI_FORMAT_UNKNOWN;
  dstViewDesc.ViewDimension         = D3D11_UAV_DIMENSION_BUFFER;
  dstViewDesc.Buffer.FirstElement   = 0;
  dstViewDesc.Buffer.NumElements    = 1;
  dstViewDesc.Buffer.Flags          = 0;
  
  if (FAILED(device->CreateUnorderedAccessView(dstBuffer.ptr(), &dstViewDesc, &dstView))) {
    std::cerr << "Failed to create unordered access view" << std::endl;
    return 1;
  }
  
  // Compute sum of the source buffer values
  context->CSSetShader(computeShader.ptr(), nullptr, 0);
  context->CSSetShaderResources(0, 1, &srcView);
  context->CSSetUnorderedAccessViews(0, 1, &dstView, nullptr);
  context->Dispatch(1, 1, 1);
  
  // Write data to the readback buffer and query the result
  context->CopyResource(readBuffer.ptr(), dstBuffer.ptr());
  
  D3D11_MAPPED_SUBRESOURCE mappedResource;
  if (FAILED(context->Map(readBuffer.ptr(), 0, D3D11_MAP_READ, 0, &mappedResource))) {
    std::cerr << "Failed to map readback buffer" << std::endl;
    return 1;
  }
  
  uint32_t result = 0;
  std::memcpy(&result, mappedResource.pData, sizeof(result));
  context->Unmap(readBuffer.ptr(), 0);
  
  std::cout << "Sum of the numbers 1 to " << srcData.size() << " = " << result << std::endl;
  context->ClearState();
  return 0;
}
