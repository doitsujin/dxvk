#pragma once

#ifdef __cplusplus
#include <cstdint>
#include <cstring>
#else
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#endif // __cplusplus

// GCC complains about the COM interfaces
// not having virtual destructors

// and class conversion for C...DESC helper types
#if defined(__GNUC__) && defined(__cplusplus)
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Wclass-conversion"
#endif // __GNUC__ && __cplusplus

typedef int32_t INT;
typedef uint32_t UINT;

typedef int32_t LONG;
typedef uint32_t ULONG;
typedef int32_t *LPLONG;

typedef int32_t HRESULT;

typedef wchar_t WCHAR;
typedef WCHAR *NWPSTR, *LPWSTR, *PWSTR;
typedef unsigned char UCHAR, *PUCHAR;

typedef char CHAR;
typedef const CHAR *LPCSTR, *PCSTR;

typedef INT BOOL;
typedef BOOL WINBOOL;

typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef void VOID;
typedef void* PVOID;
typedef void* LPVOID;
typedef const void* LPCVOID;

typedef size_t SIZE_T;

typedef int8_t INT8;
typedef uint8_t UINT8;
typedef uint8_t BYTE;

typedef int16_t SHORT;
typedef uint16_t USHORT;

typedef int64_t LONGLONG;
typedef int64_t INT64;

typedef uint64_t ULONGLONG;
typedef uint64_t UINT64;

typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;

typedef float FLOAT;

#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct GUID {
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  uint8_t  Data4[8];
} GUID;
#endif // GUID_DEFINED

typedef GUID UUID;
typedef GUID IID;
#ifdef __cplusplus
#define REFIID const IID&
#define REFGUID const GUID&
#define REFCLSID const GUID&
#else
#define REFIID const IID*
#define REFGUID const GUID*
#define REFCLSID const GUID* const
#endif // __cplusplus

#ifdef __cplusplus

template <typename T>
constexpr GUID __uuidof_helper();

#define __uuidof(T) __uuidof_helper<T>()
#define __uuidof_var(T) __uuidof_helper<decltype(T)>()

inline bool operator==(const GUID& a, const GUID& b) { return std::memcmp(&a, &b, sizeof(GUID)) == 0; }
inline bool operator!=(const GUID& a, const GUID& b) { return std::memcmp(&a, &b, sizeof(GUID)) != 0; }

#endif // __cplusplus

typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef DWORD *LPDWORD;

typedef void* HANDLE;
typedef HANDLE HMONITOR;
typedef HANDLE HDC;
typedef HANDLE HMODULE;
typedef HANDLE HINSTANCE;
typedef HANDLE HWND;
typedef HANDLE HKEY;
typedef HANDLE *LPHANDLE;
typedef DWORD COLORREF;

#if INTPTR_MAX == INT64_MAX
typedef int64_t  INT_PTR;
typedef uint64_t UINT_PTR;
#else
typedef int32_t  INT_PTR;
typedef uint32_t UINT_PTR;
#endif
typedef INT_PTR*  PINT_PTR;
typedef UINT_PTR* PUINT_PTR;

#ifdef STRICT
#define DECLARE_HANDLE(a) typedef struct a##__ { int unused; } *a
#else /*STRICT*/
#define DECLARE_HANDLE(a) typedef HANDLE a
#endif /*STRICT*/

typedef char* LPSTR;
typedef wchar_t* LPWSTR;
typedef const char* LPCSTR;
typedef const wchar_t* LPCWSTR;

typedef struct LUID {
  DWORD LowPart;
  LONG  HighPart;
} LUID;

typedef struct POINT {
  LONG x;
  LONG y;
} POINT;

typedef POINT* LPPOINT;

typedef struct RECT {
  LONG left;
  LONG top;
  LONG right;
  LONG bottom;
} RECT,*PRECT,*NPRECT,*LPRECT;

typedef struct SIZE {
  LONG cx;
  LONG cy;
} SIZE,*PSIZE,*LPSIZE;

typedef union {
  struct {
    DWORD LowPart;
    LONG HighPart;
  };

  struct {
    DWORD LowPart;
    LONG HighPart;
  } u;

  LONGLONG QuadPart;
} LARGE_INTEGER;

typedef struct MEMORYSTATUS {
  DWORD  dwLength;
  SIZE_T dwTotalPhys;
} MEMORYSTATUS;

typedef struct SECURITY_ATTRIBUTES {
  DWORD nLength;
  void* lpSecurityDescriptor;
  BOOL  bInheritHandle;
} SECURITY_ATTRIBUTES;

typedef struct PALETTEENTRY {
  BYTE peRed;
  BYTE peGreen;
  BYTE peBlue;
  BYTE peFlags;
} PALETTEENTRY, *PPALETTEENTRY, *LPPALETTEENTRY;

typedef struct RGNDATAHEADER {
  DWORD dwSize;
  DWORD iType;
  DWORD nCount;
  DWORD nRgnSize;
  RECT  rcBound;
} RGNDATAHEADER;

typedef struct RGNDATA {
  RGNDATAHEADER rdh;
  char          Buffer[1];
} RGNDATA,*PRGNDATA,*NPRGNDATA,*LPRGNDATA;

// Ignore these.
#define STDMETHODCALLTYPE
#define __stdcall

#define CONST const
#define CONST_VTBL const

#define TRUE 1
#define FALSE 0

#define WAIT_TIMEOUT    0x00000102
#define WAIT_FAILED	    0xffffffff
#define WAIT_OBJECT_0		0
#define WAIT_ABANDONED  0x00000080

#define interface struct
#define MIDL_INTERFACE(x) struct

#ifdef __cplusplus

#define DEFINE_GUID(iid, a, b, c, d, e, f, g, h, i, j, k) \
  constexpr GUID iid = {a,b,c,{d,e,f,g,h,i,j,k}};

#define DECLARE_UUIDOF_HELPER(type, a, b, c, d, e, f, g, h, i, j, k) \
  extern "C++" { template <> constexpr GUID __uuidof_helper<type>() { return GUID{a,b,c,{d,e,f,g,h,i,j,k}}; } } \
  extern "C++" { template <> constexpr GUID __uuidof_helper<type*>() { return __uuidof_helper<type>(); } } \
  extern "C++" { template <> constexpr GUID __uuidof_helper<const type*>() { return __uuidof_helper<type>(); } } \
  extern "C++" { template <> constexpr GUID __uuidof_helper<type&>() { return __uuidof_helper<type>(); } } \
  extern "C++" { template <> constexpr GUID __uuidof_helper<const type&>() { return __uuidof_helper<type>(); } }

#else
#define DEFINE_GUID(iid, a, b, c, d, e, f, g, h, i, j, k) \
  static const GUID iid = {a,b,c,{d,e,f,g,h,i,j,k}};
#define DECLARE_UUIDOF_HELPER(type, a, b, c, d, e, f, g, h, i, j, k)
#endif // __cplusplus

#define __CRT_UUID_DECL(type, a, b, c, d, e, f, g, h, i, j, k) DECLARE_UUIDOF_HELPER(type, a, b, c, d, e, f, g, h, i, j, k)

#define S_OK     0
#define S_FALSE  1

#define E_INVALIDARG  ((HRESULT)0x80070057)
#define E_FAIL        ((HRESULT)0x80004005)
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define E_NOTIMPL     ((HRESULT)0x80004001)
#define E_OUTOFMEMORY ((HRESULT)0x8007000E)
#define E_POINTER     ((HRESULT)0x80004003)

#define DXGI_STATUS_OCCLUDED                     ((HRESULT)0x087a0001)
#define DXGI_STATUS_CLIPPED                      ((HRESULT)0x087a0002)
#define DXGI_STATUS_NO_REDIRECTION               ((HRESULT)0x087a0004)
#define DXGI_STATUS_NO_DESKTOP_ACCESS            ((HRESULT)0x087a0005)
#define DXGI_STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE ((HRESULT)0x087a0006)
#define DXGI_STATUS_MODE_CHANGED                 ((HRESULT)0x087a0007)
#define DXGI_STATUS_MODE_CHANGE_IN_PROGRESS      ((HRESULT)0x087a0008)
#define DXGI_STATUS_UNOCCLUDED                   ((HRESULT)0x087a0009)
#define DXGI_STATUS_DDA_WAS_STILL_DRAWING        ((HRESULT)0x087a000a)
#define DXGI_STATUS_PRESENT_REQUIRED             ((HRESULT)0x087a002f)

#define DXGI_ERROR_INVALID_CALL                  ((HRESULT)0x887A0001)
#define DXGI_ERROR_NOT_FOUND                     ((HRESULT)0x887A0002)
#define DXGI_ERROR_MORE_DATA                     ((HRESULT)0x887A0003)
#define DXGI_ERROR_UNSUPPORTED                   ((HRESULT)0x887A0004)
#define DXGI_ERROR_DEVICE_REMOVED                ((HRESULT)0x887A0005)
#define DXGI_ERROR_DEVICE_HUNG                   ((HRESULT)0x887A0006)
#define DXGI_ERROR_DEVICE_RESET                  ((HRESULT)0x887A0007)
#define DXGI_ERROR_WAS_STILL_DRAWING             ((HRESULT)0x887A000A)
#define DXGI_ERROR_FRAME_STATISTICS_DISJOINT     ((HRESULT)0x887A000B)
#define DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE  ((HRESULT)0x887A000C)
#define DXGI_ERROR_DRIVER_INTERNAL_ERROR         ((HRESULT)0x887A0020)
#define DXGI_ERROR_NONEXCLUSIVE                  ((HRESULT)0x887A0021)
#define DXGI_ERROR_NOT_CURRENTLY_AVAILABLE       ((HRESULT)0x887A0022)
#define DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED    ((HRESULT)0x887A0023)
#define DXGI_ERROR_REMOTE_OUTOFMEMORY            ((HRESULT)0x887A0024)
#define DXGI_ERROR_ACCESS_LOST                   ((HRESULT)0x887A0026)
#define DXGI_ERROR_WAIT_TIMEOUT                  ((HRESULT)0x887A0027)
#define DXGI_ERROR_SESSION_DISCONNECTED          ((HRESULT)0x887A0028)
#define DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE      ((HRESULT)0x887A0029)
#define DXGI_ERROR_CANNOT_PROTECT_CONTENT        ((HRESULT)0x887A002A)
#define DXGI_ERROR_ACCESS_DENIED                 ((HRESULT)0x887A002B)
#define DXGI_ERROR_NAME_ALREADY_EXISTS           ((HRESULT)0x887A002C)
#define DXGI_ERROR_SDK_COMPONENT_MISSING         ((HRESULT)0x887A002D)

#define D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD ((HRESULT)0x887C0004)

#define WINAPI
#define WINUSERAPI

#define RGB(r,g,b)          ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))

#define MAKE_HRESULT(sev,fac,code) \
    ((HRESULT) (((unsigned long)(sev)<<31) | ((unsigned long)(fac)<<16) | ((unsigned long)(code))) )

#ifdef __cplusplus
#define STDMETHOD(name) virtual HRESULT name
#define STDMETHOD_(type, name) virtual type name
#else
#define STDMETHOD(name) HRESULT (STDMETHODCALLTYPE *name)
#define STDMETHOD_(type, name) type (STDMETHODCALLTYPE *name)
#endif // __cplusplus

#define THIS_
#define THIS

#define __C89_NAMELESSSTRUCTNAME
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
#define DUMMYUNIONNAME
#define DUMMYSTRUCTNAME
#define DUMMYUNIONNAME1
#define DUMMYUNIONNAME2
#define DUMMYUNIONNAME3
#define DUMMYUNIONNAME4
#define DUMMYUNIONNAME5
#define DUMMYUNIONNAME6
#define DUMMYUNIONNAME7
#define DUMMYUNIONNAME8
#define DUMMYUNIONNAME9

#ifdef __cplusplus
#define DECLARE_INTERFACE(x)     struct x
#define DECLARE_INTERFACE_(x, y) struct x : public y
#else
#ifdef CONST_VTABLE
#define DECLARE_INTERFACE(x) \
    typedef interface x { \
        const struct x##Vtbl *lpVtbl; \
    } x; \
    typedef const struct x##Vtbl x##Vtbl; \
    const struct x##Vtbl
#else
#define DECLARE_INTERFACE(x) \
    typedef interface x { \
        struct x##Vtbl *lpVtbl; \
    } x; \
    typedef struct x##Vtbl x##Vtbl; \
    struct x##Vtbl
#endif // CONST_VTABLE
#define DECLARE_INTERFACE_(x, y) DECLARE_INTERFACE(x)
#endif // __cplusplus

#define BEGIN_INTERFACE
#define END_INTERFACE

#ifdef __cplusplus
#define PURE = 0
#else
#define PURE
#endif // __cplusplus

#define DECLSPEC_SELECTANY

#define __MSABI_LONG(x) x

#define ENUM_CURRENT_SETTINGS ((DWORD)-1)
#define ENUM_REGISTRY_SETTINGS ((DWORD)-2)

#define INVALID_HANDLE_VALUE ((HANDLE)-1)

#define DUPLICATE_CLOSE_SOURCE ((DWORD)0x1)
#define DUPLICATE_SAME_ACCESS ((DWORD)0x2)

#define FAILED(hr) ((HRESULT)(hr) < 0)
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)

#define RtlZeroMemory(Destination,Length) memset((Destination),0,(Length))
#define ZeroMemory RtlZeroMemory

#ifndef DEFINE_ENUM_FLAG_OPERATORS

#ifdef __cplusplus
# define DEFINE_ENUM_FLAG_OPERATORS(type) \
extern "C++" \
{ \
    inline type operator &(type x, type y) { return (type)((int)x & (int)y); } \
    inline type operator &=(type &x, type y) { return (type &)((int &)x &= (int)y); } \
    inline type operator ~(type x) { return (type)~(int)x; } \
    inline type operator |(type x, type y) { return (type)((int)x | (int)y); } \
    inline type operator |=(type &x, type y) { return (type &)((int &)x |= (int)y); } \
    inline type operator ^(type x, type y) { return (type)((int)x ^ (int)y); } \
    inline type operator ^=(type &x, type y) { return (type &)((int &)x ^= (int)y); } \
}
#else
# define DEFINE_ENUM_FLAG_OPERATORS(type)
#endif
#endif /* DEFINE_ENUM_FLAG_OPERATORS */
