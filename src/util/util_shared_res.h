#pragma once

#include <cstdint>

#include "./com/com_include.h"

#include <d3d11_4.h>

namespace dxvk {

    HANDLE openKmtHandle(HANDLE kmt_handle);

    bool setSharedMetadata(HANDLE handle, void *buf, uint32_t bufSize);
    bool getSharedMetadata(HANDLE handle, void *buf, uint32_t bufSize, uint32_t *metadataSize);

    struct DxvkSharedTextureMetadata {
      UINT             Width;
      UINT             Height;
      UINT             MipLevels;
      UINT             ArraySize;
      DXGI_FORMAT      Format;
      DXGI_SAMPLE_DESC SampleDesc;
      D3D11_USAGE      Usage;
      UINT             BindFlags;
      UINT             CPUAccessFlags;
      UINT             MiscFlags;
      D3D11_TEXTURE_LAYOUT TextureLayout;
    };

}
