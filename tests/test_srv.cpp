#include "d3d11_device.h"
#include <iostream>

int main() {
    ID3D11Device* device = nullptr;
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    UINT featureFlags = 0;

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        &featureLevel,
        1,
        D3D11_SDK_VERSION,
        &device,
        nullptr,
        nullptr
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 device" << std::endl;
        return 1;
    }

    dxvk::D3D11Device* dxvkDevice = static_cast<dxvk::D3D11Device*>(device);

    ID3D11Resource* resource;
    // Create a dummy resource
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = 16;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    hr = dxvkDevice->CreateBuffer(&desc, nullptr, (ID3D11Buffer**)&resource);

    if (FAILED(hr)) {
        std::cerr << "Failed to create buffer" << std::endl;
        return 1;
    }

    // This should not crash
    dxvkDevice->CreateShaderResourceView(resource, nullptr, nullptr);

    std::cout << "Test passed!" << std::endl;

    return 0;
}
