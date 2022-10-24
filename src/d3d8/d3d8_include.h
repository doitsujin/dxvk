#pragma once

#ifndef _MSC_VER
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0A00
#endif

#include <stdint.h>

// Used for some refcounted COM objects that need to be released.
#define SAFE_RELEASE(p)   { if (p) { (p)->Release(); (p) = nullptr; } }

#include <d3d8.h>

// Undefine D3D8 macros //
#undef DIRECT3D_VERSION
#undef D3D_SDK_VERSION

#undef D3DCS_ALL            // parentheses added in DX9
#undef D3DFVF_POSITION_MASK // changed from 0x00E to 0x400E in DX9
#undef D3DFVF_RESERVED2     // reduced from 4 to 2 in DX9

#undef D3DSP_REGNUM_MASK    // changed from 0x00000FFF to 0x000007FF in DX9


#ifdef __MINGW32__

// Avoid redundant definitions (add D3D*_DEFINED macros here) //
#define D3DRECT_DEFINED
#define D3DMATRIX_DEFINED

// Temporarily override __CRT_UUID_DECL to allow usage in d3d9 namespace
#pragma push_macro("__CRT_UUID_DECL")
#ifdef __CRT_UUID_DECL
#undef __CRT_UUID_DECL
#endif
#define __CRT_UUID_DECL(type,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8)         \
}                                                                     \
    extern "C++" {                                                    \
    template<> struct __mingw_uuidof_s<d3d9::type> {                  \
        static constexpr IID __uuid_inst = {                          \
            l,w1,w2, {b1,b2,b3,b4,b5,b6,b7,b8}                        \
        };                                                            \
    };                                                                \
    template<> constexpr const GUID &__mingw_uuidof<d3d9::type>() {   \
        return __mingw_uuidof_s<d3d9::type>::__uuid_inst;             \
    }                                                                 \
    template<> constexpr const GUID &__mingw_uuidof<d3d9::type*>() {  \
        return  __mingw_uuidof_s<d3d9::type>::__uuid_inst;            \
    }                                                                 \
    }                                                                 \
namespace d3d9 {

#endif // __MINGW32__


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

#ifdef __MINGW32__
#pragma pop_macro("__CRT_UUID_DECL")
#endif // __MINGW32__

//for some reason we need to specify __declspec(dllexport) for MinGW
#if defined(__WINE__)
#define DLLEXPORT __attribute__((visibility("default")))
#else
#define DLLEXPORT
#endif


#include "../util/com/com_guid.h"
#include "../util/com/com_object.h"
#include "../util/com/com_pointer.h"

#include "../util/log/log.h"
#include "../util/log/log_debug.h"

#include "../util/rc/util_rc.h"
#include "../util/rc/util_rc_ptr.h"

#include "../util/sync/sync_recursive.h"

#include "../util/util_env.h"
#include "../util/util_enum.h"
#include "../util/util_error.h"
#include "../util/util_flags.h"
#include "../util/util_likely.h"
#include "../util/util_math.h"
#include "../util/util_misc.h"
#include "../util/util_string.h"

// Missed definitions in Wine/MinGW.

#ifndef D3DPRESENT_BACK_BUFFERS_MAX_EX
#define D3DPRESENT_BACK_BUFFERS_MAX_EX 30
#endif

#ifndef D3DSI_OPCODE_MASK
#define D3DSI_OPCODE_MASK 0x0000FFFF
#endif

#ifndef D3DSP_TEXTURETYPE_MASK
#define D3DSP_TEXTURETYPE_MASK 0x78000000
#endif

#ifndef D3DUSAGE_AUTOGENMIPMAP
#define D3DUSAGE_AUTOGENMIPMAP 0x00000400L
#endif

#ifndef D3DSP_DCL_USAGE_MASK
#define D3DSP_DCL_USAGE_MASK 0x0000000f
#endif

#ifndef D3DSP_OPCODESPECIFICCONTROL_MASK
#define D3DSP_OPCODESPECIFICCONTROL_MASK 0x00ff0000
#endif

#ifndef D3DSP_OPCODESPECIFICCONTROL_SHIFT
#define D3DSP_OPCODESPECIFICCONTROL_SHIFT 16
#endif

#ifndef D3DCURSOR_IMMEDIATE_UPDATE
#define D3DCURSOR_IMMEDIATE_UPDATE             0x00000001L
#endif

#ifndef D3DPRESENT_FORCEIMMEDIATE
#define D3DPRESENT_FORCEIMMEDIATE              0x00000100L
#endif

// MinGW headers are broken. Who'dve guessed?
#ifndef _MSC_VER
typedef struct _D3DDEVINFO_RESOURCEMANAGER
{
  char dummy;
} D3DDEVINFO_RESOURCEMANAGER, * LPD3DDEVINFO_RESOURCEMANAGER;

#ifndef __WINE__
extern "C" WINUSERAPI WINBOOL WINAPI SetProcessDPIAware(VOID);
#endif
#endif

// This is the managed pool on D3D9Ex, it's just hidden!
#define D3DPOOL_MANAGED_EX D3DPOOL(6)

//using D3D9VertexElements = std::vector<D3DVERTEXELEMENT9>;
