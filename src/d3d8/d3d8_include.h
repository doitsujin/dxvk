#pragma once

#ifndef _MSC_VER
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0A00
#endif

#include <stdint.h>
#include <d3d8.h>

// Declare __uuidof for D3D8 interfaces
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IDirect3D8,              0x1DD9E8DA, 0x1C77, 0x4D40, 0xB0, 0xCF, 0x98, 0xFE, 0xFD, 0xFF, 0x95, 0x12);
__CRT_UUID_DECL(IDirect3DDevice8,        0x7385E5DF, 0x8FE8, 0x41D5, 0x86, 0xB6, 0xD7, 0xB4, 0x85, 0x47, 0xB6, 0xCF);
__CRT_UUID_DECL(IDirect3DResource8,      0x1B36BB7B, 0x09B7, 0x410A, 0xB4, 0x45, 0x7D, 0x14, 0x30, 0xD7, 0xB3, 0x3F);
__CRT_UUID_DECL(IDirect3DVertexBuffer8,  0x8AEEEAC7, 0x05F9, 0x44D4, 0xB5, 0x91, 0x00, 0x0B, 0x0D, 0xF1, 0xCB, 0x95);
__CRT_UUID_DECL(IDirect3DVolume8,        0xBD7349F5, 0x14F1, 0x42E4, 0x9C, 0x79, 0x97, 0x23, 0x80, 0xDB, 0x40, 0xC0);
__CRT_UUID_DECL(IDirect3DSwapChain8,     0x928C088B, 0x76B9, 0x4C6B, 0xA5, 0x36, 0xA5, 0x90, 0x85, 0x38, 0x76, 0xCD);
__CRT_UUID_DECL(IDirect3DSurface8,       0xB96EEBCA, 0xB326, 0x4EA5, 0x88, 0x2F, 0x2F, 0xF5, 0xBA, 0xE0, 0x21, 0xDD);
__CRT_UUID_DECL(IDirect3DIndexBuffer8,   0x0E689C9A, 0x053D, 0x44A0, 0x9D, 0x92, 0xDB, 0x0E, 0x3D, 0x75, 0x0F, 0x86);
__CRT_UUID_DECL(IDirect3DBaseTexture8,   0xB4211CFA, 0x51B9, 0x4A9F, 0xAB, 0x78, 0xDB, 0x99, 0xB2, 0xBB, 0x67, 0x8E);
__CRT_UUID_DECL(IDirect3DTexture8,       0xE4CDD575, 0x2866, 0x4F01, 0xB1, 0x2E, 0x7E, 0xEC, 0xE1, 0xEC, 0x93, 0x58);
__CRT_UUID_DECL(IDirect3DCubeTexture8,   0x3EE5B968, 0x2ACA, 0x4C34, 0x8B, 0xB5, 0x7E, 0x0C, 0x3D, 0x19, 0xB7, 0x50);
__CRT_UUID_DECL(IDirect3DVolumeTexture8, 0x4B8AAAFA, 0x140F, 0x42BA, 0x91, 0x31, 0x59, 0x7E, 0xAF, 0xAA, 0x2E, 0xAD);
#elif defined(_MSC_VER)
interface DECLSPEC_UUID("1DD9E8DA-1C77-4D40-B0CF-98FEFDFF9512") IDirect3D8;
interface DECLSPEC_UUID("7385E5DF-8FE8-41D5-86B6-D7B48547B6CF") IDirect3DDevice8;
interface DECLSPEC_UUID("1B36BB7B-09B7-410A-B445-7D1430D7B33F") IDirect3DResource8;
interface DECLSPEC_UUID("8AEEEAC7-05F9-44D4-B591-000B0DF1CB95") IDirect3DVertexBuffer8;
interface DECLSPEC_UUID("BD7349F5-14F1-42E4-9C79-972380DB40C0") IDirect3DVolume8;
interface DECLSPEC_UUID("928C088B-76B9-4C6B-A536-A590853876CD") IDirect3DSwapChain8;
interface DECLSPEC_UUID("B96EEBCA-B326-4EA5-882F-2FF5BAE021DD") IDirect3DSurface8;
interface DECLSPEC_UUID("0E689C9A-053D-44A0-9D92-DB0E3D750F86") IDirect3DIndexBuffer8;
interface DECLSPEC_UUID("B4211CFA-51B9-4A9F-AB78-DB99B2BB678E") IDirect3DBaseTexture8;
interface DECLSPEC_UUID("E4CDD575-2866-4F01-B12E-7EECE1EC9358") IDirect3DTexture8;
interface DECLSPEC_UUID("3EE5B968-2ACA-4C34-8BB5-7E0C3D19B750") IDirect3DCubeTexture8;
interface DECLSPEC_UUID("4B8AAAFA-140F-42BA-9131-597EAFAA2EAD") IDirect3DVolumeTexture8;
#endif

// Undefine D3D8 macros
#undef DIRECT3D_VERSION
#undef D3D_SDK_VERSION

#undef D3DCS_ALL            // parentheses added in D3D9
#undef D3DFVF_POSITION_MASK // changed from 0x00E to 0x400E in D3D9
#undef D3DFVF_RESERVED2     // reduced from 4 to 2 in DX9

#undef D3DSP_REGNUM_MASK    // changed from 0x00000FFF to 0x000007FF in D3D9


#if defined(__MINGW32__) || defined(__GNUC__)

// Avoid redundant definitions (add D3D*_DEFINED macros here)
#define D3DRECT_DEFINED
#define D3DMATRIX_DEFINED

// Temporarily undefine __CRT_UUID_DECL
// to allow imports in the d3d9 namespace
#pragma push_macro("__CRT_UUID_DECL")
#ifdef __CRT_UUID_DECL
#undef __CRT_UUID_DECL
#endif

#endif // defined(__MINGW32__) || defined(__GNUC__)


/**
* \brief Direct3D 9
*
* All D3D9 interfaces are included within
* a namespace, so as not to collide with
* D3D8 interfaces.
*/
namespace d3d9 {
#include <d3d9.h>
}

// Indicates d3d9:: namespace is in-use.
#define DXVK_D3D9_NAMESPACE


#if defined(__MINGW32__) || defined(__GNUC__)
#pragma pop_macro("__CRT_UUID_DECL")

// Declare __uuidof for D3D9 interfaces, directly within the d3d9:: namespace
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(d3d9::IDirect3D9,                  0x81BDCBCA, 0x64D4, 0x426D, 0xAE, 0x8D, 0xAD, 0x01, 0x47, 0xF4, 0x27, 0x5C);
__CRT_UUID_DECL(d3d9::IDirect3DVolume9,            0x24F416E6, 0x1F67, 0x4AA7, 0xB8, 0x8E, 0xD3, 0x3F, 0x6F, 0x31, 0x28, 0xA1);
__CRT_UUID_DECL(d3d9::IDirect3DSwapChain9,         0x794950F2, 0xADFC, 0x458A, 0x90, 0x5E, 0x10, 0xA1, 0x0B, 0x0B, 0x50, 0x3B);
__CRT_UUID_DECL(d3d9::IDirect3DResource9,          0x05EEC05D, 0x8F7D, 0x4362, 0xB9, 0x99, 0xD1, 0xBA, 0xF3, 0x57, 0xC7, 0x04);
__CRT_UUID_DECL(d3d9::IDirect3DSurface9,           0x0CFBAF3A, 0x9FF6, 0x429A, 0x99, 0xB3, 0xA2, 0x79, 0x6A, 0xF8, 0xB8, 0x9B);
__CRT_UUID_DECL(d3d9::IDirect3DVertexBuffer9,      0xB64BB1B5, 0xFD70, 0x4DF6, 0xBF, 0x91, 0x19, 0xD0, 0xA1, 0x24, 0x55, 0xE3);
__CRT_UUID_DECL(d3d9::IDirect3DIndexBuffer9,       0x7C9DD65E, 0xD3F7, 0x4529, 0xAC, 0xEE, 0x78, 0x58, 0x30, 0xAC, 0xDE, 0x35);
__CRT_UUID_DECL(d3d9::IDirect3DBaseTexture9,       0x580CA87E, 0x1D3C, 0x4D54, 0x99, 0x1D, 0xB7, 0xD3, 0xE3, 0xC2, 0x98, 0xCE);
__CRT_UUID_DECL(d3d9::IDirect3DCubeTexture9,       0xFFF32F81, 0xD953, 0x473A, 0x92, 0x23, 0x93, 0xD6, 0x52, 0xAB, 0xA9, 0x3F);
__CRT_UUID_DECL(d3d9::IDirect3DTexture9,           0x85C31227, 0x3DE5, 0x4F00, 0x9B, 0x3A, 0xF1, 0x1A, 0xC3, 0x8C, 0x18, 0xB5);
__CRT_UUID_DECL(d3d9::IDirect3DVolumeTexture9,     0x2518526C, 0xE789, 0x4111, 0xA7, 0xB9, 0x47, 0xEF, 0x32, 0x8D, 0x13, 0xE6);
__CRT_UUID_DECL(d3d9::IDirect3DVertexDeclaration9, 0xDD13C59C, 0x36FA, 0x4098, 0xA8, 0xFB, 0xC7, 0xED, 0x39, 0xDC, 0x85, 0x46);
__CRT_UUID_DECL(d3d9::IDirect3DVertexShader9,      0xEFC5557E, 0x6265, 0x4613, 0x8A, 0x94, 0x43, 0x85, 0x78, 0x89, 0xEB, 0x36);
__CRT_UUID_DECL(d3d9::IDirect3DPixelShader9,       0x6D3BDBDC, 0x5B02, 0x4415, 0xB8, 0x52, 0xCE, 0x5E, 0x8B, 0xCC, 0xB2, 0x89);
__CRT_UUID_DECL(d3d9::IDirect3DStateBlock9,        0xB07C4FE5, 0x310D, 0x4BA8, 0xA2, 0x3C, 0x4F, 0x0F, 0x20, 0x6F, 0x21, 0x8B);
__CRT_UUID_DECL(d3d9::IDirect3DQuery9,             0xD9771460, 0xA695, 0x4F26, 0xBB, 0xD3, 0x27, 0xB8, 0x40, 0xB5, 0x41, 0xCC);
__CRT_UUID_DECL(d3d9::IDirect3DDevice9,            0xD0223B96, 0xBF7A, 0x43FD, 0x92, 0xBD, 0xA4, 0x3B, 0x0D, 0x82, 0xB9, 0xEB);
__CRT_UUID_DECL(d3d9::IDirect3D9Ex,                0x02177241, 0x69FC, 0x400C, 0x8F, 0xF1, 0x93, 0xA4, 0x4D, 0xF6, 0x86, 0x1D);
__CRT_UUID_DECL(d3d9::IDirect3DSwapChain9Ex,       0x91886CAF, 0x1C3D, 0x4D2E, 0xA0, 0xAB, 0x3E, 0x4C, 0x7D, 0x8D, 0x33, 0x03);
__CRT_UUID_DECL(d3d9::IDirect3DDevice9Ex,          0xB18B10CE, 0x2649, 0x405A, 0x87, 0x0F, 0x95, 0xF7, 0x77, 0xD4, 0x31, 0x3A);
#endif

#endif // defined(__MINGW32__) || defined(__GNUC__)


//for some reason we need to specify __declspec(dllexport) for MinGW
#if defined(__WINE__) || !defined(_WIN32)
  #define DLLEXPORT __attribute__((visibility("default")))
#else
  #define DLLEXPORT
#endif


#include "../util/com/com_guid.h"
#include "../util/com/com_object.h"
#include "../util/com/com_pointer.h"

#include "../util/log/log.h"
#include "../util/log/log_debug.h"

#include "../util/sync/sync_recursive.h"

#include "../util/util_error.h"
#include "../util/util_likely.h"
#include "../util/util_string.h"

// Missed definitions in Wine/MinGW.

#ifndef D3DPRESENT_BACK_BUFFERS_MAX_EX
#define D3DPRESENT_BACK_BUFFERS_MAX_EX    30
#endif

#ifndef D3DSI_OPCODE_MASK
#define D3DSI_OPCODE_MASK                 0x0000FFFF
#endif

#ifndef D3DSP_TEXTURETYPE_MASK
#define D3DSP_TEXTURETYPE_MASK            0x78000000
#endif

#ifndef D3DUSAGE_AUTOGENMIPMAP
#define D3DUSAGE_AUTOGENMIPMAP            0x00000400L
#endif

#ifndef D3DSP_DCL_USAGE_MASK
#define D3DSP_DCL_USAGE_MASK              0x0000000f
#endif

#ifndef D3DSP_OPCODESPECIFICCONTROL_MASK
#define D3DSP_OPCODESPECIFICCONTROL_MASK  0x00ff0000
#endif

#ifndef D3DSP_OPCODESPECIFICCONTROL_SHIFT
#define D3DSP_OPCODESPECIFICCONTROL_SHIFT 16
#endif

#ifndef D3DCURSOR_IMMEDIATE_UPDATE
#define D3DCURSOR_IMMEDIATE_UPDATE        0x00000001L
#endif

#ifndef D3DPRESENT_FORCEIMMEDIATE
#define D3DPRESENT_FORCEIMMEDIATE         0x00000100L
#endif

// From d3dtypes.h

#ifndef D3DDEVINFOID_TEXTUREMANAGER
#define D3DDEVINFOID_TEXTUREMANAGER       1
#endif

#ifndef D3DDEVINFOID_D3DTEXTUREMANAGER
#define D3DDEVINFOID_D3DTEXTUREMANAGER    2
#endif

#ifndef D3DDEVINFOID_TEXTURING
#define D3DDEVINFOID_TEXTURING            3
#endif

// From d3dhal.h

#ifndef D3DDEVINFOID_VCACHE
#define D3DDEVINFOID_VCACHE               4
#endif

// MinGW headers are broken. Who'dve guessed?
#ifndef _MSC_VER

// Missing from d3d8types.h
#ifndef D3DDEVINFOID_RESOURCEMANAGER
#define D3DDEVINFOID_RESOURCEMANAGER      5
#endif

#ifndef D3DDEVINFOID_VERTEXSTATS
#define D3DDEVINFOID_VERTEXSTATS          6 // Aka D3DDEVINFOID_D3DVERTEXSTATS
#endif

#ifndef D3DPRESENT_RATE_UNLIMITED
#define D3DPRESENT_RATE_UNLIMITED         0x7FFFFFFF
#endif

#else // _MSC_VER

// These are enum typedefs in the MinGW headers, but not defined by Microsoft
#define D3DVSDT_TYPE                      DWORD
#define D3DVSDE_REGISTER                  DWORD

#endif
