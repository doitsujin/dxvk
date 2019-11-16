#pragma once

#include <cstdint>
#include <cstring>

// GCC complains about the COM interfaces
// not having virtual destructors

// and class conversion for C...DESC helper types
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Wclass-conversion"
#endif // __GNUC__

struct SDL_Window;

using INT     = int32_t;
using UINT    = uint32_t;

using LONG    = int32_t;
using ULONG   = uint32_t;

using HRESULT = int32_t;

using WCHAR   = wchar_t;

using BOOL    = INT;
using WINBOOL = BOOL;

using UINT16  = uint16_t;
using UINT32  = uint32_t;
using UINT64  = uint64_t;
using VOID    = void;

using SIZE_T  = size_t;

using UINT8   = uint8_t;
using BYTE    = uint8_t;

using SHORT   = int16_t;
using USHORT  = uint16_t;

using LONGLONG  = int64_t;
using ULONGLONG = uint64_t;

using FLOAT    = float;

struct GUID {
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  uint8_t  Data4[8];
};

using UUID    = GUID;
using REFIID  = const GUID&;
using REFGUID = REFIID;

template <typename T>
constexpr GUID __uuidof_helper();

#define __uuidof(T) __uuidof_helper<T>()

inline bool operator==(const GUID& a, const GUID& b) { return std::memcmp(&a, &b, sizeof(GUID)) == 0; }
inline bool operator!=(const GUID& a, const GUID& b) { return std::memcmp(&a, &b, sizeof(GUID)) != 0; }

using DWORD   = uint32_t;
using WORD    = int32_t;

using HANDLE   = void*;
using HMONITOR = HANDLE;
using HDC      = HANDLE;
using HMODULE  = HANDLE;

using LPSTR    = char*;
using LPCSTR   = const char*;
using LPCWSTR  = const wchar_t*;

struct LUID {
  DWORD LowPart;
  LONG  HighPart;
};

struct POINT {
  LONG x;
  LONG y;
};

struct RECT {
  LONG left;
  LONG top;
  LONG right;
  LONG bottom;
};

struct SIZE {
  LONG cx;
  LONG cy;
};

union LARGE_INTEGER {
  struct {
    DWORD LowPart;
    LONG HighPart;
  } u;
  LONGLONG QuadPart;
};

struct SECURITY_ATTRIBUTES {
  DWORD nLength;
  void* lpSecurityDescriptor;
  BOOL  bInheritHandle;
};

struct PALETTEENTRY {
  BYTE peRed;
  BYTE peGreen;
  BYTE peBlue;
  BYTE peFlags;
};

struct RGNDATAHEADER {
  DWORD dwSize;
  DWORD iType;
  DWORD nCount;
  DWORD nRgnSize;
  RECT  rcBound;
};

struct RGNDATA {
  RGNDATAHEADER rdh;
  char          Buffer[1];
};

// Ignore these.
#define STDMETHODCALLTYPE
#define __stdcall

#define CONST const

constexpr BOOL TRUE  = 1;
constexpr BOOL FALSE = 0;

#define interface struct
#define MIDL_INTERFACE(x) struct
#define DEFINE_GUID(iid, a, b, c, d, e, f, g, h, i, j, k) constexpr GUID iid = {a,b,c,{d,e,f,g,h,i,j,k}};

#define DECLARE_UUIDOF_HELPER(type, a, b, c, d, e, f, g, h, i, j, k) extern "C++" { template <> constexpr GUID __uuidof_helper<type>() { return GUID{a,b,c,{d,e,f,g,h,i,j,k}}; } }

#define __CRT_UUID_DECL(type, a, b, c, d, e, f, g, h, i, j, k) DECLARE_UUIDOF_HELPER(type, a, b, c, d, e, f, g, h, i, j, k)

constexpr HRESULT S_OK     = 0;
constexpr HRESULT S_FALSE  = 1;

constexpr HRESULT E_INVALIDARG  = 0x80070057;
constexpr HRESULT E_FAIL        = 0x80004005;
constexpr HRESULT E_NOINTERFACE = 0x80004002;
constexpr HRESULT E_NOTIMPL     = 0x80004001;
constexpr HRESULT E_OUTOFMEMORY = 0x8007000E;
constexpr HRESULT E_POINTER     = 0x80004003;

constexpr HRESULT DXGI_STATUS_OCCLUDED                     = 0x087a0001;
constexpr HRESULT DXGI_STATUS_CLIPPED                      = 0x087a0002;
constexpr HRESULT DXGI_STATUS_NO_REDIRECTION               = 0x087a0004;
constexpr HRESULT DXGI_STATUS_NO_DESKTOP_ACCESS            = 0x087a0005;
constexpr HRESULT DXGI_STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE = 0x087a0006;
constexpr HRESULT DXGI_STATUS_MODE_CHANGED                 = 0x087a0007;
constexpr HRESULT DXGI_STATUS_MODE_CHANGE_IN_PROGRESS      = 0x087a0008;
constexpr HRESULT DXGI_STATUS_UNOCCLUDED                   = 0x087a0009;
constexpr HRESULT DXGI_STATUS_DDA_WAS_STILL_DRAWING        = 0x087a000a;
constexpr HRESULT DXGI_STATUS_PRESENT_REQUIRED             = 0x087a002f;

constexpr HRESULT DXGI_ERROR_INVALID_CALL                  = 0x887A0001;
constexpr HRESULT DXGI_ERROR_NOT_FOUND                     = 0x887A0002;
constexpr HRESULT DXGI_ERROR_MORE_DATA                     = 0x887A0003;
constexpr HRESULT DXGI_ERROR_UNSUPPORTED                   = 0x887A0004;
constexpr HRESULT DXGI_ERROR_DEVICE_REMOVED                = 0x887A0005;
constexpr HRESULT DXGI_ERROR_DEVICE_HUNG                   = 0x887A0006;
constexpr HRESULT DXGI_ERROR_DEVICE_RESET                  = 0x887A0007;
constexpr HRESULT DXGI_ERROR_WAS_STILL_DRAWING             = 0x887A000A;
constexpr HRESULT DXGI_ERROR_FRAME_STATISTICS_DISJOINT     = 0x887A000B;
constexpr HRESULT DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE  = 0x887A000C;
constexpr HRESULT DXGI_ERROR_DRIVER_INTERNAL_ERROR         = 0x887A0020;
constexpr HRESULT DXGI_ERROR_NONEXCLUSIVE                  = 0x887A0021;
constexpr HRESULT DXGI_ERROR_NOT_CURRENTLY_AVAILABLE       = 0x887A0022;
constexpr HRESULT DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED    = 0x887A0023;
constexpr HRESULT DXGI_ERROR_REMOTE_OUTOFMEMORY            = 0x887A0024;
constexpr HRESULT DXGI_ERROR_ACCESS_LOST                   = 0x887A0026;
constexpr HRESULT DXGI_ERROR_WAIT_TIMEOUT                  = 0x887A0027;
constexpr HRESULT DXGI_ERROR_SESSION_DISCONNECTED          = 0x887A0028;
constexpr HRESULT DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE      = 0x887A0029;
constexpr HRESULT DXGI_ERROR_CANNOT_PROTECT_CONTENT        = 0x887A002A;
constexpr HRESULT DXGI_ERROR_ACCESS_DENIED                 = 0x887A002B;
constexpr HRESULT DXGI_ERROR_NAME_ALREADY_EXISTS           = 0x887A002C;
constexpr HRESULT DXGI_ERROR_SDK_COMPONENT_MISSING         = 0x887A002D;

#define WINAPI
#define WINUSERAPI

#define MAKE_HRESULT(sev,fac,code) \
    ((HRESULT) (((unsigned long)(sev)<<31) | ((unsigned long)(fac)<<16) | ((unsigned long)(code))) )

#define STDMETHOD(name) virtual HRESULT name
#define STDMETHOD_(type, name) virtual type name

#define THIS_
#define THIS

#define __C89_NAMELESSUNIONNAME
#define __C89_NAMELESSUNIONNAME1
#define __C89_NAMELESSUNIONNAME2
#define __C89_NAMELESSUNIONNAME3
#define __C89_NAMELESSUNIONNAME4
#define __C89_NAMELESSUNIONNAME5
#define __C89_NAMELESSUNIONNAME6
#define __C89_NAMELESSUNIONNAME7
#define __C89_NAMELESSUNIONNAME8
#define __C89_NAMELESS

#define DECLARE_INTERFACE(x)     struct x
#define DECLARE_INTERFACE_(x, y) struct x : public y

#define BEGIN_INTERFACE
#define END_INTERFACE

#define PURE = 0

#define DECLSPEC_SELECTANY

#define __MSABI_LONG(x) x

#define ENUM_CURRENT_SETTINGS ((DWORD)-1)
#define ENUM_REGISTRY_SETTINGS ((DWORD)-2)

template <typename T>
inline bool FAILED(T hr) { return HRESULT(hr) < 0; }

template <typename T>
inline bool SUCCEEDED(T hr) { return !FAILED<T>(hr); }