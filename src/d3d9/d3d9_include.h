#pragma once

#ifndef _MSC_VER
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0A00
#endif

#include <stdint.h>
#include <d3d9.h>

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

#include "../util/rc/util_rc.h"
#include "../util/rc/util_rc_ptr.h"

#include "../util/sync/sync_recursive.h"

#include "../util/util_env.h"
#include "../util/util_enum.h"
#include "../util/util_error.h"
#include "../util/util_flags.h"
#include "../util/util_likely.h"
#include "../util/util_math.h"
#include "../util/util_string.h"

// Missed definitions in Wine/MinGW.

#ifndef D3DPRESENT_FORCEIMMEDIATE
#define D3DPRESENT_FORCEIMMEDIATE              0x00000100L
#endif

// Missing in some versions of mingw headers
#ifndef _MSC_VER
namespace dxvk {
  typedef struct _D3DDEVINFO_RESOURCEMANAGER
  {
    char dummy;
  } D3DDEVINFO_RESOURCEMANAGER, * LPD3DDEVINFO_RESOURCEMANAGER;
}
#endif

// This is the managed pool on D3D9Ex, it's just hidden!
#define D3DPOOL_MANAGED_EX D3DPOOL(6)

using D3D9VertexElements = std::vector<D3DVERTEXELEMENT9>;

/////////////////////
// D3D9On12 content
/////////////////////

#include <d3d12.h>

#define MAX_D3D9ON12_QUEUES 2

struct D3D9ON12_ARGS {
  BOOL Enable9On12;
  IUnknown* pD3D12Device;
  IUnknown* ppD3D12Queues[MAX_D3D9ON12_QUEUES];
  UINT NumQueues;
  UINT NodeMask;
};

extern "C" {

  // Ordinal 20
  typedef IDirect3D9* (WINAPI* PFN_Direct3DCreate9On12)(UINT sdk_version, D3D9ON12_ARGS* override_list, UINT override_entry_count);
  IDirect3D9* WINAPI Direct3DCreate9On12(UINT sdk_version, D3D9ON12_ARGS* override_list, UINT override_entry_count);

  // Ordinal 21
  typedef HRESULT(WINAPI* PFN_Direct3DCreate9On12Ex)(UINT sdk_version, D3D9ON12_ARGS* override_list, UINT override_entry_count, IDirect3D9Ex** output);
  HRESULT WINAPI Direct3DCreate9On12Ex(UINT sdk_version, D3D9ON12_ARGS* override_list, UINT override_entry_count, IDirect3D9Ex** output);

}

MIDL_INTERFACE("e7fda234-b589-4049-940d-8878977531c8")
IDirect3DDevice9On12 : public IUnknown {

  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) = 0;
  virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
  virtual ULONG STDMETHODCALLTYPE Release() = 0;

  virtual HRESULT STDMETHODCALLTYPE GetD3D12Device(REFIID riid, void** object) = 0;
  virtual HRESULT STDMETHODCALLTYPE UnwrapUnderlyingResource(IDirect3DResource9* resource, ID3D12CommandQueue* command_queue, REFIID riid, void** object) = 0;
  virtual HRESULT STDMETHODCALLTYPE ReturnUnderlyingResource(IDirect3DResource9* resource, UINT num_sync, UINT64* signal_values, ID3D12Fence** fences) = 0;
};

#ifndef _MSC_VER
__CRT_UUID_DECL(IDirect3DDevice9On12,      0xe7fda234,0xb589,0x4049,0x94,0x0d,0x88,0x78,0x97,0x75,0x31,0xc8);
#endif

