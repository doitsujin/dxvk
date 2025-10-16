#pragma once

#ifndef _MSC_VER
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0A00
#endif

#include <stdint.h>
#include <d3d.h>

// Declare __uuidof for DDraw/D3D interfaces
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IDirectDraw,             0x6C14DB80, 0xA733, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);
__CRT_UUID_DECL(IDirectDraw2,            0xB3A6F3E0, 0x2B43, 0x11CF, 0xA2, 0xDE, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56);
__CRT_UUID_DECL(IDirectDraw4,            0x9C59509A, 0x39BD, 0x11D1, 0x8C, 0x4A, 0x00, 0xC0, 0x4F, 0xD9, 0x30, 0xC5);
__CRT_UUID_DECL(IDirectDraw7,            0x15E65EC0, 0x3B9C, 0x11D2, 0xB9, 0x2F, 0x00, 0x60, 0x97, 0x97, 0xEA, 0x5B);
__CRT_UUID_DECL(IDirectDrawSurface,      0x6C14DB81, 0xA733, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);
__CRT_UUID_DECL(IDirectDrawSurface2,     0x57805885, 0x6EEC, 0x11CF, 0x94, 0x41, 0xA8, 0x23, 0x03, 0xC1, 0x0E, 0x27);
__CRT_UUID_DECL(IDirectDrawSurface3,     0xDA044E00, 0x69B2, 0x11D0, 0xA1, 0xD5, 0x00, 0xAA, 0x00, 0xB8, 0xDF, 0xBB);
__CRT_UUID_DECL(IDirectDrawSurface4,     0x0B2B8630, 0xAD35, 0x11D0, 0x8E, 0xA6, 0x00, 0x60, 0x97, 0x97, 0xEA, 0x5B);
__CRT_UUID_DECL(IDirectDrawSurface7,     0x06675A80, 0x3B9B, 0x11D2, 0xB9, 0x2F, 0x00, 0x60, 0x97, 0x97, 0xEA, 0x5B);
__CRT_UUID_DECL(IDirectDrawPalette,      0x6C14DB84, 0xA733, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);
__CRT_UUID_DECL(IDirectDrawClipper,      0x6C14DB85, 0xA733, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);
__CRT_UUID_DECL(IDirectDrawGammaControl, 0x69C11C3E, 0xB46B, 0x11D1, 0xAD, 0x7A, 0x00, 0xC0, 0x4F, 0xC2, 0x9B, 0x4E);
__CRT_UUID_DECL(IDirectDrawColorControl, 0x4B9F0EE0, 0x0D7E, 0x11D0, 0x9B, 0x06, 0x00, 0xA0, 0xC9, 0x03, 0xA3, 0xB8);
__CRT_UUID_DECL(IDirect3D,               0x3BBA0080, 0x2421, 0x11CF, 0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56);
__CRT_UUID_DECL(IDirect3D2,              0x6AAE1EC1, 0x662A, 0x11D0, 0x88, 0x9D, 0x00, 0xAA, 0x00, 0xBB, 0xB7, 0x6A);
__CRT_UUID_DECL(IDirect3D3,              0xBB223240, 0xE72B, 0x11D0, 0xA9, 0xB4, 0x00, 0xAA, 0x00, 0xC0, 0x99, 0x3E);
__CRT_UUID_DECL(IDirect3D7,              0xF5049E77, 0x4861, 0x11D2, 0xA4, 0x07, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0xA8);
__CRT_UUID_DECL(IDirect3DDevice,         0x64108800, 0x957D, 0x11D0, 0x89, 0xAB, 0x00, 0xA0, 0xC9, 0x05, 0x41, 0x29);
__CRT_UUID_DECL(IDirect3DDevice2,        0x93281501, 0x8CF8, 0x11D0, 0x89, 0xAB, 0x00, 0xA0, 0xC9, 0x05, 0x41, 0x29);
__CRT_UUID_DECL(IDirect3DDevice3,        0xB0AB3B60, 0x33D7, 0x11D1, 0xA9, 0x81, 0x00, 0xC0, 0x4F, 0xD7, 0xB1, 0x74);
__CRT_UUID_DECL(IDirect3DDevice7,        0xF5049E79, 0x4861, 0x11D2, 0xA4, 0x07, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0xA8);
__CRT_UUID_DECL(IDirect3DLight,          0x4417C142, 0x33AD, 0x11CF, 0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E);
__CRT_UUID_DECL(IDirect3DMaterial,       0x4417C144, 0x33AD, 0x11CF, 0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E);
__CRT_UUID_DECL(IDirect3DMaterial2,      0x93281503, 0x8CF8, 0x11D0, 0x89, 0xAB, 0x00, 0xA0, 0xC9, 0x05, 0x41, 0x29);
__CRT_UUID_DECL(IDirect3DMaterial3,      0xCA9C46f4, 0xD3C5, 0x11D1, 0xB7, 0x5A, 0x00, 0x60, 0x08, 0x52, 0xB3, 0x12);
__CRT_UUID_DECL(IDirect3DExecuteBuffer,  0x4417C145, 0x33AD, 0x11Cf, 0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E);
__CRT_UUID_DECL(IDirect3DVertexBuffer,   0x7A503555, 0x4A83, 0x11D1, 0xA5, 0xDB, 0x00, 0xA0, 0xC9, 0x03, 0x67, 0xF8);
__CRT_UUID_DECL(IDirect3DVertexBuffer7,  0xF5049E7D, 0x4861, 0x11D2, 0xA4, 0x07, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0xA8);
__CRT_UUID_DECL(IDirect3DTexture,        0x2CDCD9E0, 0x25A0, 0x11CF, 0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56);
__CRT_UUID_DECL(IDirect3DTexture2,       0x93281502, 0x8CF8, 0x11D0, 0x89, 0xAB, 0x00, 0xA0, 0xC9, 0x05, 0x41, 0x29);
__CRT_UUID_DECL(IDirect3DViewport,       0x4417C146, 0x33AD, 0x11CF, 0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E);
__CRT_UUID_DECL(IDirect3DViewport2,      0x93281500, 0x8CF8, 0x11D0, 0x89, 0xAB, 0x00, 0xA0, 0xC9, 0x05, 0x41, 0x29);
__CRT_UUID_DECL(IDirect3DViewport3,      0xB0AB3B61, 0x33D7, 0x11D1, 0xA9, 0x81, 0x00, 0xC0, 0x4F, 0xD7, 0xB1, 0x74);
#elif defined(_MSC_VER)
interface DECLSPEC_UUID("6C14DB80-A733-11CE-A521-0020AF0BE560") IDirectDraw;
interface DECLSPEC_UUID("B3A6F3E0-2B43-11CF-A2DE-00AA00B93356") IDirectDraw2;
interface DECLSPEC_UUID("9C59509A-39BD-11D1-8C4A-00C04FD930C5") IDirectDraw4;
interface DECLSPEC_UUID("15E65EC0-3B9C-11D2-B92F-00609797EA5B") IDirectDraw7;
interface DECLSPEC_UUID("6C14DB81-A733-11CE-A521-0020AF0BE560") IDirectDrawSurface;
interface DECLSPEC_UUID("57805885-6EEC-11CF-9441-A82303C10E27") IDirectDrawSurface2;
interface DECLSPEC_UUID("DA044E00-69B2-11D0-A1D5-00AA00B8DFBB") IDirectDrawSurface3;
interface DECLSPEC_UUID("0B2B8630-AD35-11D0-8EA6-00609797EA5B") IDirectDrawSurface4;
interface DECLSPEC_UUID("06675A80-3B9B-11D2-B92F-00609797EA5B") IDirectDrawSurface7;
interface DECLSPEC_UUID("6C14DB84-A733-11CE-A521-0020AF0BE560") IDirectDrawPalette;
interface DECLSPEC_UUID("6C14DB85-A733-11CE-A521-0020AF0BE560") IDirectDrawClipper;
interface DECLSPEC_UUID("69C11C3E-B46B-11D1-AD7A-00C04FC29B4E") IDirectDrawGammaControl;
interface DECLSPEC_UUID("4B9F0EE0-0D7E-11D0-9B06-00A0C903A3B8") IDirectDrawColorControl;
interface DECLSPEC_UUID("3BBA0080-2421-11CF-A31A-00AA00B93356") IDirect3D;
interface DECLSPEC_UUID("6AAE1EC1-662A-11D0-889D-00AA00BBB76A") IDirect3D2;
interface DECLSPEC_UUID("BB223240-E72B-11D0-A9B4-00AA00C0993E") IDirect3D3;
interface DECLSPEC_UUID("F5049E77-4861-11D2-A407-00A0C90629A8") IDirect3D7;
interface DECLSPEC_UUID("64108800-957D-11D0-89AB-00A0C9054129") IDirect3DDevice;
interface DECLSPEC_UUID("93281501-8CF8-11D0-89AB-00A0C9054129") IDirect3DDevice2;
interface DECLSPEC_UUID("B0AB3B60-33D7-11D1-A981-00C04FD7B174") IDirect3DDevice3;
interface DECLSPEC_UUID("F5049E79-4861-11D2-A407-00A0C90629A8") IDirect3DDevice7;
interface DECLSPEC_UUID("4417C142-33AD-11CF-816F-0000C020156E") IDirect3DLight;
interface DECLSPEC_UUID("4417C144-33AD-11CF-816F-0000C020156E") IDirect3DMaterial;
interface DECLSPEC_UUID("93281503-8CF8-11D0-89AB-00A0C90367F8") IDirect3DMaterial2;
interface DECLSPEC_UUID("CA9C46f4-D3C5-11D1-816F-00060852B312") IDirect3DMaterial3;
interface DECLSPEC_UUID("4417C145-33AD-11CF-816F-0000C020156E") IDirect3DExecuteBuffer;
interface DECLSPEC_UUID("7A503555-4A83-11D1-A5DB-00A0C90629A8") IDirect3DVertexBuffer;
interface DECLSPEC_UUID("F5049E7D-4861-11D2-A407-00A0C90629A8") IDirect3DVertexBuffer7;
interface DECLSPEC_UUID("2CDCD9E0-25A0-11CF-A31A-00AA00B93356") IDirect3DTexture;
interface DECLSPEC_UUID("93281502-8CF8-11D0-89AB-00A0C9054129") IDirect3DTexture2;
interface DECLSPEC_UUID("4417C146-33AD-11CF-816F-0000C020156E") IDirect3DViewport;
interface DECLSPEC_UUID("93281500-8CF8-11D0-89AB-00A0C9054129") IDirect3DViewport2;
interface DECLSPEC_UUID("B0AB3B61-33D7-11D1-A981-00C04FD7B174") IDirect3DViewport3;
#endif

// Undefine D3D macros
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
#define D3DCOLORVALUE_DEFINED

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


// for some reason we need to specify __declspec(dllexport) for MinGW
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


// redefine needed D3D macros
#undef  D3DFVF_POSITION_MASK
#define D3DFVF_POSITION_MASK 0x00E

// defined in MinGW headers, but not in MSVC headers...
#ifndef D3DLIGHTCAPS_PARALLELPOINT
#define D3DLIGHTCAPS_PARALLELPOINT 0x00000008
#endif

#ifndef D3DLIGHTCAPS_GLSPOT
#define D3DLIGHTCAPS_GLSPOT        0x00000010
#endif

#ifndef D3DLIGHT_GLSPOT
#define D3DLIGHT_GLSPOT            D3DLIGHTTYPE(5)
#endif

// This render state got moved in D3D6 from 42 to 27 (replacing D3DRENDERSTATE_BLENDENABLE)
#define D3DRENDERSTATE_ALPHABLENDENABLE_OLD D3DRENDERSTATETYPE(42)

// D3DDEVICEDESC actually has 3 versions, one used in D3D2/3,
// another in D3D5, and the last in D3D6. Headers only typically
// contain the last, which is a problem because this struct
// has a size that needs to be validated... and all versions are
// expected to be valid options.

// This is the D3DDEVICEDESC shipped with D3D5
typedef struct _D3DDeviceDesc2 {
        DWORD            dwSize;
        DWORD            dwFlags;
        D3DCOLORMODEL    dcmColorModel;
        DWORD            dwDevCaps;
        D3DTRANSFORMCAPS dtcTransformCaps;
        BOOL             bClipping;
        D3DLIGHTINGCAPS  dlcLightingCaps;
        D3DPRIMCAPS      dpcLineCaps;
        D3DPRIMCAPS      dpcTriCaps;
        DWORD            dwDeviceRenderBitDepth;
        DWORD            dwDeviceZBufferBitDepth;
        DWORD            dwMaxBufferSize;
        DWORD            dwMaxVertexCount;

        DWORD            dwMinTextureWidth,dwMinTextureHeight;
        DWORD            dwMaxTextureWidth,dwMaxTextureHeight;
        DWORD            dwMinStippleWidth,dwMaxStippleWidth;
        DWORD            dwMinStippleHeight,dwMaxStippleHeight;
} D3DDEVICEDESC2;

// This is the D3DDEVICEDESC shipped with D3D2/3
typedef struct _D3DDeviceDesc3 {
        DWORD            dwSize;
        DWORD            dwFlags;
        D3DCOLORMODEL    dcmColorModel;
        DWORD            dwDevCaps;
        D3DTRANSFORMCAPS dtcTransformCaps;
        BOOL             bClipping;
        D3DLIGHTINGCAPS  dlcLightingCaps;
        D3DPRIMCAPS      dpcLineCaps;
        D3DPRIMCAPS      dpcTriCaps;
        DWORD            dwDeviceRenderBitDepth;
        DWORD            dwDeviceZBufferBitDepth;
        DWORD            dwMaxBufferSize;
        DWORD            dwMaxVertexCount;
} D3DDEVICEDESC3;

// Because of the above mess, D3DFINDDEVICERESULT is also affected in D3D5
typedef struct _D3DFindDeviceResult2
{
    DWORD dwSize;
    GUID guid;
    D3DDEVICEDESC2 ddHwDesc;
    D3DDEVICEDESC2 ddSwDesc;
} D3DFINDDEVICERESULT2;

// Because of the above mess, D3DFINDDEVICERESULT is also affected in D3D3
typedef struct _D3DFindDeviceResult3
{
    DWORD dwSize;
    GUID guid;
    D3DDEVICEDESC3 ddHwDesc;
    D3DDEVICEDESC3 ddSwDesc;
} D3DFINDDEVICERESULT3;

namespace dxvk {

  // Some applications use CLSIDs as an entry point with DllGetClassObject and IClassFactory
  static constexpr IID CLSID_DirectDraw           = { 0xD7B70EE0, 0x4340, 0x11CF, {0xB0, 0x63, 0x00, 0x20, 0xAF, 0xC2, 0xCD, 0x35} };
  static constexpr IID CLSID_DirectDraw7          = { 0x3C305196, 0x50DB, 0x11D3, {0x9C, 0xFE, 0x00, 0xC0, 0x4F, 0xD9, 0x30, 0xC5} };
  static constexpr IID CLSID_DirectDrawClipper    = { 0x593817A0, 0x7DB3, 0x11CF, {0xA2, 0xDE, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56} };

  static constexpr IID IID_IDirect3DTnLHalDevice  = { 0xF5049E78, 0x4861, 0x11D2, {0xA4, 0x07, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0xA8} };
  static constexpr IID IID_IDirect3DHALDevice     = { 0x84E63DE0, 0x46AA, 0x11CF, {0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E} };
  static constexpr IID IID_IDirect3DMMXDevice     = { 0x881949A1, 0xD6F3, 0x11D0, {0x89, 0xAB, 0x00, 0xA0, 0xC9, 0x05, 0x41, 0x29} };
  static constexpr IID IID_IDirect3DRGBDevice     = { 0xA4665C60, 0x2673, 0x11CF, {0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56} };
  static constexpr IID IID_IDirect3DRampDevice    = { 0xF2086B20, 0x259F, 0x11CF, {0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56} };
  static constexpr IID IID_WineD3DDevice          = { 0xAEF72D43, 0xB09a, 0x4B7B, {0xB7, 0x98, 0xC6, 0x8A, 0x77, 0x2D, 0x72, 0x2A} };

  static constexpr GUID GUID_IMediaStream         = { 0xB502D1BD, 0x9A57, 0x11D0, {0x8F, 0xDE, 0x00, 0xC0, 0x4F, 0xD9, 0x18, 0x9D} };
  static constexpr GUID GUID_IAMMediaStream       = { 0xBEBE595D, 0x9A6F, 0x11D0, {0x8F, 0xDE, 0x00, 0xC0, 0x4F, 0xD9, 0x18, 0x9D} };

  struct DDrawModeSize {
    DWORD width;
    DWORD height;
  };

}
