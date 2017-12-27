#pragma once

#include "../dxgi/dxgi_include.h"

#include <d3d11_1.h>

// This is not defined in the mingw headers
#ifndef D3D11_1_UAV_SLOT_COUNT
#define D3D11_1_UAV_SLOT_COUNT 64
#endif

// These were copied from d3d11.h
// For some strange reason, we cannot use the structures
// directly, although others from the same header work.
typedef struct D3D11_FEATURE_DATA_THREADING {
    BOOL DriverConcurrentCreates;
    BOOL DriverCommandLists;
} D3D11_FEATURE_DATA_THREADING;
typedef struct D3D11_FEATURE_DATA_DOUBLES {
    BOOL DoublePrecisionFloatShaderOps;
} D3D11_FEATURE_DATA_DOUBLES;
typedef struct D3D11_FEATURE_DATA_FORMAT_SUPPORT {
    DXGI_FORMAT InFormat;
    UINT OutFormatSupport;
} D3D11_FEATURE_DATA_FORMAT_SUPPORT;

