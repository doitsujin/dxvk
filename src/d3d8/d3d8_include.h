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
__CRT_UUID_DECL(IDirect3D8,              0x1DD9E8DA,0x1C77,0x4D40,0xB0,0xCF,0x98,0xFE,0xFD,0xFF,0x95,0x12);
__CRT_UUID_DECL(IDirect3DDevice8,        0x7385E5DF,0x8FE8,0x41D5,0x86,0xB6,0xD7,0xB4,0x85,0x47,0xB6,0xCF);
__CRT_UUID_DECL(IDirect3DResource8,      0x1B36BB7B,0x09B7,0x410A,0xB4,0x45,0x7D,0x14,0x30,0xD7,0xB3,0x3F);
__CRT_UUID_DECL(IDirect3DVertexBuffer8,  0x8AEEEAC7,0x05F9,0x44D4,0xB5,0x91,0x00,0x0B,0x0D,0xF1,0xCB,0x95);
__CRT_UUID_DECL(IDirect3DVolume8,        0xBD7349F5,0x14F1,0x42E4,0x9C,0x79,0x97,0x23,0x80,0xDB,0x40,0xC0);
__CRT_UUID_DECL(IDirect3DSwapChain8,     0x928C088B,0x76B9,0x4C6B,0xA5,0x36,0xA5,0x90,0x85,0x38,0x76,0xCD);
__CRT_UUID_DECL(IDirect3DSurface8,       0xB96EEBCA,0xB326,0x4EA5,0x88,0x2F,0x2F,0xF5,0xBA,0xE0,0x21,0xDD);
__CRT_UUID_DECL(IDirect3DIndexBuffer8,   0x0E689C9A,0x053D,0x44A0,0x9D,0x92,0xDB,0x0E,0x3D,0x75,0x0F,0x86);
__CRT_UUID_DECL(IDirect3DBaseTexture8,   0xB4211CFA,0x51B9,0x4A9F,0xAB,0x78,0xDB,0x99,0xB2,0xBB,0x67,0x8E);
__CRT_UUID_DECL(IDirect3DTexture8,       0xE4CDD575,0x2866,0x4F01,0xB1,0x2E,0x7E,0xEC,0xE1,0xEC,0x93,0x58);
__CRT_UUID_DECL(IDirect3DCubeTexture8,   0x3EE5B968,0x2ACA,0x4C34,0x8B,0xB5,0x7E,0x0C,0x3D,0x19,0xB7,0x50);
__CRT_UUID_DECL(IDirect3DVolumeTexture8, 0x4B8AAAFA,0x140F,0x42BA,0x91,0x31,0x59,0x7E,0xAF,0xAA,0x2E,0xAD);
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

// Temporarily override __CRT_UUID_DECL to allow usage in d3d9 namespace
#pragma push_macro("__CRT_UUID_DECL")
#ifdef __CRT_UUID_DECL
#undef __CRT_UUID_DECL
#endif

#ifdef __MINGW32__
#define __CRT_UUID_DECL(type,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)                                                                               \
}                                                                                                                                           \
  extern "C++" template<> struct __mingw_uuidof_s<d3d9::type> { static constexpr IID __uuid_inst = {l,w1,w2, {b1,b2,b3,b4,b5,b6,b7,b8}}; }; \
  extern "C++" template<> constexpr const GUID &__mingw_uuidof<d3d9::type>() { return __mingw_uuidof_s<d3d9::type>::__uuid_inst; }          \
  extern "C++" template<> constexpr const GUID &__mingw_uuidof<d3d9::type*>() { return __mingw_uuidof_s<d3d9::type>::__uuid_inst; }         \
namespace d3d9 {

#elif defined(__GNUC__)
#define __CRT_UUID_DECL(type, a, b, c, d, e, f, g, h, i, j, k)                                                               \
}                                                                                                                            \
  extern "C++" { template <> constexpr GUID __uuidof_helper<d3d9::type>() { return GUID{a,b,c,{d,e,f,g,h,i,j,k}}; } }        \
  extern "C++" { template <> constexpr GUID __uuidof_helper<d3d9::type*>() { return __uuidof_helper<d3d9::type>(); } }       \
  extern "C++" { template <> constexpr GUID __uuidof_helper<const d3d9::type*>() { return __uuidof_helper<d3d9::type>(); } } \
  extern "C++" { template <> constexpr GUID __uuidof_helper<d3d9::type&>() { return __uuidof_helper<d3d9::type>(); } }       \
  extern "C++" { template <> constexpr GUID __uuidof_helper<const d3d9::type&>() { return __uuidof_helper<d3d9::type>(); } } \
namespace d3d9 {
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
#endif

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
