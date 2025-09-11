#pragma once

#include <cstdint>

#include "./com/com_include.h"

#include <d3d9.h>
#include <d3d11_4.h>
#include <d3d12.h>

namespace dxvk {

    /* D3DKMT runtime descriptors, mostly compatible with Windows */
    typedef UINT D3DKMT_HANDLE;

    struct d3dkmt_dxgi_desc
    {
        UINT                        size;
        UINT                        version;
        UINT                        width;
        UINT                        height;
        DXGI_FORMAT                 format;
        UINT                        unknown_0;
        UINT                        unknown_1;
        UINT                        keyed_mutex;
        D3DKMT_HANDLE               mutex_handle;
        D3DKMT_HANDLE               sync_handle;
        UINT                        nt_shared;
        UINT                        unknown_2;
        UINT                        unknown_3;
        UINT                        unknown_4;
    };

    struct d3dkmt_d3d9_desc
    {
        struct d3dkmt_dxgi_desc     dxgi;
        D3DFORMAT                   format;
        D3DRESOURCETYPE             type;
        UINT                        usage;
        union
        {
            struct
            {
                UINT                unknown_0;
                UINT                width;
                UINT                height;
                UINT                levels;
                UINT                depth;
            } texture;
            struct
            {
                UINT                unknown_0;
                UINT                unknown_1;
                UINT                unknown_2;
                UINT                width;
                UINT                height;
            } surface;
            struct
            {
                UINT                unknown_0;
                UINT                width;
                UINT                format;
                UINT                unknown_1;
                UINT                unknown_2;
            } buffer;
        };
    };

    C_ASSERT( sizeof(struct d3dkmt_d3d9_desc) == 0x58 );

    struct d3dkmt_d3d11_desc
    {
        struct d3dkmt_dxgi_desc     dxgi;
        D3D11_RESOURCE_DIMENSION    dimension;
        union
        {
            D3D11_BUFFER_DESC       d3d11_buf;
            D3D11_TEXTURE1D_DESC    d3d11_1d;
            D3D11_TEXTURE2D_DESC    d3d11_2d;
            D3D11_TEXTURE3D_DESC    d3d11_3d;
        };
    };

    C_ASSERT( sizeof(struct d3dkmt_d3d11_desc) == 0x68 );

    struct d3dkmt_d3d12_desc
    {
        struct d3dkmt_d3d11_desc    d3d11;
        UINT                        unknown_5[4];
        UINT                        resource_size;
        UINT                        unknown_6[7];
        UINT                        resource_align;
        UINT                        unknown_7[9];
        union
        {
            D3D12_RESOURCE_DESC     desc;
            /* D3D12_RESOURCE_DESC1    desc1; */
            UINT                    __pad[16];
        };
        UINT64                      unknown_8[1];
    };

    C_ASSERT( sizeof(struct d3dkmt_d3d12_desc) == 0x108 );

    union d3dkmt_desc
    {
        struct d3dkmt_dxgi_desc     dxgi;
        struct d3dkmt_d3d9_desc     d3d9;   /* if dxgi.size == sizeof(d3d9)  && dxgi.version == 1 && sizeof(desc) == sizeof(d3d9) */
        struct d3dkmt_d3d11_desc    d3d11;  /* if dxgi.size == sizeof(d3d11) && dxgi.version == 4 && sizeof(desc) >= sizeof(d3d11) */
        struct d3dkmt_d3d12_desc    d3d12;  /* if dxgi.size == sizeof(d3d11) && dxgi.version == 0 && sizeof(desc) == sizeof(d3d12) */
    };

    /* new Wine API, mostly compatible with Windows: */
    bool setSharedResourceRuntimeData(HANDLE handle, const void *data, size_t size);
    bool getSharedResourceRuntimeData(LUID luid, HANDLE handle, void *data, size_t *size);

    /* old legacy Proton version, not compatible with Windows: */
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
