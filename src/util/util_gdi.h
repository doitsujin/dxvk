#pragma once

#include <d3d9.h>

#ifndef _WIN32
#define EXTERN_C
#define WINBASEAPI
#endif

namespace dxvk {
  using NTSTATUS = LONG;
  using D3DDDIFORMAT = D3DFORMAT;

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

  typedef struct _D3DKMT_DESTROYDCFROMMEMORY
  {
      HDC hDc;
      HANDLE hBitmap;
  } D3DKMT_DESTROYDCFROMMEMORY;

  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTCreateDCFromMemory(D3DKMT_CREATEDCFROMMEMORY *desc);
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTDestroyDCFromMemory(const D3DKMT_DESTROYDCFROMMEMORY *desc);
}
