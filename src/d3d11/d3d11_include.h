#pragma once

#include "../dxgi/dxgi_include.h"

#include <d3d11_4.h>

// This is not defined in the mingw headers
#ifndef D3D11_1_UAV_SLOT_COUNT
#define D3D11_1_UAV_SLOT_COUNT 64
#endif

#ifndef D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL
#define D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL 0xFFFFFFFF
#endif

#ifndef D3D11_KEEP_UNORDERED_ACCESS_VIEWS
#define D3D11_KEEP_UNORDERED_ACCESS_VIEWS 0xFFFFFFFF
#endif

#define D3D11_DXVK_USE_REMAINING_LAYERS 0xFFFFFFFF
#define D3D11_DXVK_USE_REMAINING_LEVELS 0xFFFFFFFF

// Most of these were copied from d3d11.h
// For some strange reason, we cannot use the structures
// directly, although others from the same header work.
// Some structures are missing from the mingw headers.
#ifndef _MSC_VER
#if !defined(__MINGW64_VERSION_MAJOR) || __MINGW64_VERSION_MAJOR < 9
typedef enum D3D11_FORMAT_SUPPORT2 { 
  D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_ADD                                = 0x1,
  D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS                        = 0x2,
  D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE  = 0x4,
  D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE                           = 0x8,
  D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_SIGNED_MIN_OR_MAX                  = 0x10,
  D3D11_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX                = 0x20,
  D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD                                = 0x40,
  D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE                               = 0x80,
  D3D11_FORMAT_SUPPORT2_OUTPUT_MERGER_LOGIC_OP                        = 0x100,
  D3D11_FORMAT_SUPPORT2_TILED                                         = 0x200,
  D3D11_FORMAT_SUPPORT2_SHAREABLE                                     = 0x400,
  D3D11_FORMAT_SUPPORT2_MULTIPLANE_OVERLAY                            = 0x4000
} D3D11_FORMAT_SUPPORT2;
#define D3D11_RESOURCE_MISC_TILE_POOL (0x20000)
#define D3D11_RESOURCE_MISC_TILED     (0x40000)
#endif // !defined(__MINGW64_VERSION_MAJOR) || __MINGW64_VERSION_MAJOR < 9
#endif // _MSC_VER
