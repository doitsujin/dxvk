#pragma once

#include <d3d9.h>

#ifndef _WIN32
#define EXTERN_C
#define WINBASEAPI
#endif

namespace dxvk {
  using NTSTATUS = LONG;
  using D3DDDIFORMAT = D3DFORMAT;
  using D3DKMT_HANDLE = UINT;

  typedef struct _D3DKMT_CLOSEADAPTER
  {
      D3DKMT_HANDLE hAdapter;
  } D3DKMT_CLOSEADAPTER;

  typedef struct _D3DKMT_CREATEDCFROMMEMORY
  {
      void *pMemory;
      D3DDDIFORMAT Format;
      UINT Width;
      UINT Height;
      UINT Pitch;
      HDC hDeviceDc;
      PALETTEENTRY *pColorTable;
      HDC hDc;
      HANDLE hBitmap;
  } D3DKMT_CREATEDCFROMMEMORY;

  typedef struct _D3DKMT_CREATEDEVICEFLAGS
  {
      UINT LegacyMode : 1;
      UINT RequestVSync : 1;
      UINT DisableGpuTimeout : 1;
      UINT Reserved : 29;
  } D3DKMT_CREATEDEVICEFLAGS;

  typedef struct _D3DDDI_ALLOCATIONLIST
  {
      D3DKMT_HANDLE hAllocation;
      union
      {
          struct
          {
              UINT WriteOperation : 1;
              UINT DoNotRetireInstance : 1;
              UINT OfferPriority : 3;
              UINT Reserved : 27;
          } DUMMYSTRUCTNAME;
          UINT Value;
      } DUMMYUNIONNAME;
  } D3DDDI_ALLOCATIONLIST;

  typedef struct _D3DDDI_PATCHLOCATIONLIST
  {
      UINT AllocationIndex;
      union
      {
          struct
          {
              UINT SlotId : 24;
              UINT Reserved : 8;
          } DUMMYSTRUCTNAME;
          UINT Value;
      } DUMMYUNIONNAME;
      UINT DriverId;
      UINT AllocationOffset;
      UINT PatchOffset;
      UINT SplitOffset;
  } D3DDDI_PATCHLOCATIONLIST;

  typedef struct _D3DKMT_CREATEDEVICE
  {
      union
      {
          D3DKMT_HANDLE hAdapter;
          VOID *pAdapter;
      } DUMMYUNIONNAME;
      D3DKMT_CREATEDEVICEFLAGS Flags;
      D3DKMT_HANDLE hDevice;
      VOID *pCommandBuffer;
      UINT CommandBufferSize;
      D3DDDI_ALLOCATIONLIST *pAllocationList;
      UINT AllocationListSize;
      D3DDDI_PATCHLOCATIONLIST *pPatchLocationList;
      UINT PatchLocationListSize;
  } D3DKMT_CREATEDEVICE;

  typedef struct _D3DKMT_DESTROYDCFROMMEMORY
  {
      HDC hDc;
      HANDLE hBitmap;
  } D3DKMT_DESTROYDCFROMMEMORY;

  typedef struct _D3DKMT_DESTROYDEVICE
  {
      D3DKMT_HANDLE hDevice;
  } D3DKMT_DESTROYDEVICE;

  typedef struct _D3DKMT_DESTROYSYNCHRONIZATIONOBJECT
  {
      D3DKMT_HANDLE hSyncObject;
  } D3DKMT_DESTROYSYNCHRONIZATIONOBJECT;

  typedef struct _D3DKMT_OPENADAPTERFROMLUID
  {
      LUID AdapterLuid;
      D3DKMT_HANDLE hAdapter;
  } D3DKMT_OPENADAPTERFROMLUID;

  typedef struct _D3DKMT_OPENSYNCHRONIZATIONOBJECT
  {
      D3DKMT_HANDLE hSharedHandle;
      D3DKMT_HANDLE hSyncObject;
      UINT64 Reserved[8];
  } D3DKMT_OPENSYNCHRONIZATIONOBJECT;

  typedef struct _D3DKMT_OPENSYNCOBJECTFROMNTHANDLE
  {
      HANDLE hNtHandle;
      D3DKMT_HANDLE hSyncObject;
  } D3DKMT_OPENSYNCOBJECTFROMNTHANDLE;

  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTCloseAdapter(const D3DKMT_CLOSEADAPTER *desc);
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTCreateDCFromMemory(D3DKMT_CREATEDCFROMMEMORY *desc);
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTCreateDevice(D3DKMT_CREATEDEVICE *desc);
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTDestroyDCFromMemory(const D3DKMT_DESTROYDCFROMMEMORY *desc);
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTDestroyDevice(const D3DKMT_DESTROYDEVICE *desc);
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTDestroySynchronizationObject(const D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *desc);
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTOpenAdapterFromLuid(D3DKMT_OPENADAPTERFROMLUID *desc);
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTOpenSynchronizationObject(D3DKMT_OPENSYNCHRONIZATIONOBJECT *desc);
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTOpenSyncObjectFromNtHandle(D3DKMT_OPENSYNCOBJECTFROMNTHANDLE *desc);
}
