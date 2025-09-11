#include "util_shared_res.h"
#include "log/log.h"

#ifdef _WIN32
#include <winioctl.h>
#endif

namespace dxvk {

#ifdef _WIN32

  typedef UINT D3DKMT_HANDLE;
  typedef ULONG NTSTATUS;
  typedef ULONGLONG D3DGPU_VIRTUAL_ADDRESS;

  typedef enum _D3DKMT_ESCAPETYPE
  {
      D3DKMT_ESCAPE_UPDATE_RESOURCE_WINE = 0x80000000
  } D3DKMT_ESCAPETYPE;

  typedef struct _D3DDDI_ESCAPEFLAGS
  {
      union
      {
          struct
          {
              UINT HardwareAccess :1;
              UINT Reserved       :31;
          };
          UINT Value;
      };
  } D3DDDI_ESCAPEFLAGS;

  typedef struct _D3DKMT_ESCAPE
  {
      D3DKMT_HANDLE      hAdapter;
      D3DKMT_HANDLE      hDevice;
      D3DKMT_ESCAPETYPE  Type;
      D3DDDI_ESCAPEFLAGS Flags;
      void              *pPrivateDriverData;
      UINT               PrivateDriverDataSize;
      D3DKMT_HANDLE      hContext;
  } D3DKMT_ESCAPE;

  typedef struct _D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE
  {
      D3DKMT_HANDLE hDevice;
      HANDLE hNtHandle;
      void *pPrivateRuntimeData;
      UINT PrivateRuntimeDataSize;
      UINT TotalPrivateDriverDataSize;
      UINT ResourcePrivateDriverDataSize;
      UINT NumAllocations;
  } D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE;

  typedef struct _D3DDDI_OPENALLOCATIONINFO
  {
      D3DKMT_HANDLE hAllocation;
      const void *pPrivateDriverData;
      UINT PrivateDriverDataSize;
  } D3DDDI_OPENALLOCATIONINFO;

  typedef struct _D3DDDI_OPENALLOCATIONINFO2
  {
      D3DKMT_HANDLE hAllocation;
      const void *pPrivateDriverData;
      UINT PrivateDriverDataSize;
      D3DGPU_VIRTUAL_ADDRESS GpuVirtualAddress;
      ULONG_PTR Reserved[6];
  } D3DDDI_OPENALLOCATIONINFO2;

  typedef struct _D3DKMT_OPENRESOURCEFROMNTHANDLE
  {
      D3DKMT_HANDLE hDevice;
      HANDLE hNtHandle;
      UINT NumAllocations;
      D3DDDI_OPENALLOCATIONINFO2 *pOpenAllocationInfo2;
      UINT PrivateRuntimeDataSize;
      void *pPrivateRuntimeData;
      UINT ResourcePrivateDriverDataSize;
      void *pResourcePrivateDriverData;
      UINT TotalPrivateDriverDataBufferSize;
      void *pTotalPrivateDriverDataBuffer;
      D3DKMT_HANDLE hResource;
      D3DKMT_HANDLE hKeyedMutex;
      void *pKeyedMutexPrivateRuntimeData;
      UINT KeyedMutexPrivateRuntimeDataSize;
      D3DKMT_HANDLE hSyncObject;
  } D3DKMT_OPENRESOURCEFROMNTHANDLE;

  typedef struct _D3DKMT_DESTROYALLOCATION
  {
      D3DKMT_HANDLE hDevice;
      D3DKMT_HANDLE hResource;
      const D3DKMT_HANDLE *phAllocationList;
      UINT AllocationCount;
  } D3DKMT_DESTROYALLOCATION;

  typedef struct _D3DKMT_QUERYRESOURCEINFO
  {
      D3DKMT_HANDLE hDevice;
      D3DKMT_HANDLE hGlobalShare;
      void *pPrivateRuntimeData;
      UINT PrivateRuntimeDataSize;
      UINT TotalPrivateDriverDataSize;
      UINT ResourcePrivateDriverDataSize;
      UINT NumAllocations;
  } D3DKMT_QUERYRESOURCEINFO;

  typedef struct _D3DKMT_OPENRESOURCE
  {
      D3DKMT_HANDLE hDevice;
      D3DKMT_HANDLE hGlobalShare;
      UINT NumAllocations;
      union
      {
          D3DDDI_OPENALLOCATIONINFO *pOpenAllocationInfo;
          D3DDDI_OPENALLOCATIONINFO2 *pOpenAllocationInfo2;
      };
      void *pPrivateRuntimeData;
      UINT PrivateRuntimeDataSize;
      void *pResourcePrivateDriverData;
      UINT ResourcePrivateDriverDataSize;
      void *pTotalPrivateDriverDataBuffer;
      UINT TotalPrivateDriverDataBufferSize;
      D3DKMT_HANDLE hResource;
  } D3DKMT_OPENRESOURCE;

  typedef struct _D3DKMT_OPENADAPTERFROMLUID
  {
      LUID AdapterLuid;
      D3DKMT_HANDLE hAdapter;
  } D3DKMT_OPENADAPTERFROMLUID;

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

  typedef struct _D3DKMT_CLOSEADAPTER
  {
      D3DKMT_HANDLE hAdapter;
  } D3DKMT_CLOSEADAPTER;

  typedef struct _D3DKMT_DESTROYDEVICE
  {
      D3DKMT_HANDLE hDevice;
  } D3DKMT_DESTROYDEVICE;

  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTCloseAdapter( const D3DKMT_CLOSEADAPTER *params );
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTCreateDevice( D3DKMT_CREATEDEVICE *params );
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTDestroyAllocation( const D3DKMT_DESTROYALLOCATION *params );
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTDestroyDevice( const D3DKMT_DESTROYDEVICE *params );
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTEscape( const D3DKMT_ESCAPE *params );
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTOpenAdapterFromLuid( D3DKMT_OPENADAPTERFROMLUID *params );
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTOpenResource2( D3DKMT_OPENRESOURCE *params );
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTOpenResourceFromNtHandle( D3DKMT_OPENRESOURCEFROMNTHANDLE *params );
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTQueryResourceInfo( D3DKMT_QUERYRESOURCEINFO *params );
  EXTERN_C WINBASEAPI NTSTATUS WINAPI D3DKMTQueryResourceInfoFromNtHandle( D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *params );

  bool setSharedResourceRuntimeData(HANDLE handle, const void *data, size_t size) {
    D3DKMT_ESCAPE escape = {0};

    escape.Type = D3DKMT_ESCAPE_UPDATE_RESOURCE_WINE;
    escape.hContext = HandleToUlong(handle);
    escape.pPrivateDriverData = (void *)data;
    escape.PrivateDriverDataSize = size;
    return !D3DKMTEscape( &escape );
  }

  static bool is_d3dkmt_global(HANDLE handle) {
    return (HandleToULong(handle) & 0xc0000000) && (HandleToULong(handle) & 0x3f) == 2;
  }

  static bool get_shared_resource_desc(D3DKMT_HANDLE device, HANDLE handle, void *data, size_t *size) {
    D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE query = {0};
    D3DKMT_OPENRESOURCEFROMNTHANDLE open = {0};
    D3DDDI_OPENALLOCATIONINFO2 alloc = {0};
    D3DKMT_DESTROYALLOCATION destroy = {0};

    query.hDevice = device;
    query.hNtHandle = handle;
    query.pPrivateRuntimeData = data;
    query.PrivateRuntimeDataSize = *size;
    if (D3DKMTQueryResourceInfoFromNtHandle(&query) || query.PrivateRuntimeDataSize > *size)
      return false;

    open.hDevice = device;
    open.hNtHandle = handle;
    open.NumAllocations = 1;
    open.pOpenAllocationInfo2 = &alloc;
    open.pPrivateRuntimeData = data;
    open.PrivateRuntimeDataSize = query.PrivateRuntimeDataSize;
    if (D3DKMTOpenResourceFromNtHandle(&open))
      return false;
    *size = open.PrivateRuntimeDataSize;

    destroy.hDevice = device;
    destroy.hResource = open.hResource;
    D3DKMTDestroyAllocation(&destroy);
    return true;
  }

  static bool get_d3dkmt_resource_desc(D3DKMT_HANDLE device, D3DKMT_HANDLE handle, void *data, size_t *size) {
    D3DDDI_OPENALLOCATIONINFO2 alloc = {0};
    D3DKMT_DESTROYALLOCATION destroy = {0};
    D3DKMT_QUERYRESOURCEINFO query = {0};
    D3DKMT_OPENRESOURCE open = {0};

    query.hDevice = device;
    query.hGlobalShare = handle;
    query.pPrivateRuntimeData = data;
    query.PrivateRuntimeDataSize = *size;
    if (D3DKMTQueryResourceInfo(&query) || query.PrivateRuntimeDataSize > *size)
      return false;

    open.hDevice = device;
    open.hGlobalShare = handle;
    open.NumAllocations = 1;
    open.pOpenAllocationInfo2 = &alloc;
    open.pPrivateRuntimeData = data;
    open.PrivateRuntimeDataSize = query.PrivateRuntimeDataSize;
    if (D3DKMTOpenResource2(&open))
      return false;
    *size = open.PrivateRuntimeDataSize;

    destroy.hDevice = device;
    destroy.hResource = open.hResource;
    D3DKMTDestroyAllocation(&destroy);
    return true;
  }

  bool getSharedResourceRuntimeData(LUID luid, HANDLE handle, void *data, size_t *size) {
    D3DKMT_OPENADAPTERFROMLUID open_adapter = {0};
    D3DKMT_CREATEDEVICE create_device = {0};
    D3DKMT_CLOSEADAPTER close_adapter = {0};
    bool ret = false;

    open_adapter.AdapterLuid = luid;
    if (D3DKMTOpenAdapterFromLuid(&open_adapter))
      return false;

    close_adapter.hAdapter = open_adapter.hAdapter;
    create_device.hAdapter = open_adapter.hAdapter;

    if (!D3DKMTCreateDevice(&create_device)) {
      D3DKMT_DESTROYDEVICE destroy_device = {0};
      destroy_device.hDevice = create_device.hDevice;

      if (is_d3dkmt_global(handle)) {
        ret = get_d3dkmt_resource_desc(create_device.hDevice, HandleToUlong(handle), data, size);
      } else {
        ret = get_shared_resource_desc(create_device.hDevice, handle, data, size);
      }

      D3DKMTDestroyDevice(&destroy_device);
    }

    D3DKMTCloseAdapter(&close_adapter);
    return ret;
  }


  /* old legacy Proton version, not compatible with Windows: */

  #define IOCTL_SHARED_GPU_RESOURCE_OPEN             CTL_CODE(FILE_DEVICE_VIDEO, 1, METHOD_BUFFERED, FILE_WRITE_ACCESS)

  HANDLE openKmtHandle(HANDLE kmt_handle) {
    HANDLE handle = ::CreateFileA("\\\\.\\SharedGpuResource", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE)
      return handle;

    struct
    {
        unsigned int kmt_handle;
        WCHAR name[1];
    } shared_resource_open = {0};
    shared_resource_open.kmt_handle = reinterpret_cast<uintptr_t>(kmt_handle);

    bool succeed = ::DeviceIoControl(handle, IOCTL_SHARED_GPU_RESOURCE_OPEN, &shared_resource_open, sizeof(shared_resource_open), NULL, 0, NULL, NULL);
    if (!succeed) {
      ::CloseHandle(handle);
      return INVALID_HANDLE_VALUE;
    }
    return handle; 
  }

  #define IOCTL_SHARED_GPU_RESOURCE_SET_METADATA           CTL_CODE(FILE_DEVICE_VIDEO, 4, METHOD_BUFFERED, FILE_WRITE_ACCESS)

  bool setSharedMetadata(HANDLE handle, void *buf, uint32_t bufSize) {
    DWORD retSize;
    return ::DeviceIoControl(handle, IOCTL_SHARED_GPU_RESOURCE_SET_METADATA, buf, bufSize, NULL, 0, &retSize, NULL);
  }

  #define IOCTL_SHARED_GPU_RESOURCE_GET_METADATA           CTL_CODE(FILE_DEVICE_VIDEO, 5, METHOD_BUFFERED, FILE_READ_ACCESS)

  bool getSharedMetadata(HANDLE handle, void *buf, uint32_t bufSize, uint32_t *metadataSize) {
    DWORD retSize;
    bool ret = ::DeviceIoControl(handle, IOCTL_SHARED_GPU_RESOURCE_GET_METADATA, NULL, 0, buf, bufSize, &retSize, NULL);
    if (metadataSize)
      *metadataSize = retSize;
    return ret;
  }
#else
  bool setSharedResourceRuntimeData(HANDLE handle, const void *data, size_t size) {
    Logger::warn("setSharedResourceRuntimeData: Shared resources not available on this platform.");
    return false;
  }

  bool getSharedResourceRuntimeData(LUID luid, HANDLE handle, void *data, size_t *size) {
    Logger::warn("getSharedResourceRuntimeData: Shared resources not available on this platform.");
    return false;
  }

  HANDLE openKmtHandle(HANDLE kmt_handle) {
    Logger::warn("openKmtHandle: Shared resources not available on this platform.");
    return INVALID_HANDLE_VALUE;
  }

  bool setSharedMetadata(HANDLE handle, void *buf, uint32_t bufSize) {
    Logger::warn("setSharedMetadata: Shared resources not available on this platform.");
    return false;
  }

  bool getSharedMetadata(HANDLE handle, void *buf, uint32_t bufSize, uint32_t *metadataSize) {
    Logger::warn("getSharedMetadata: Shared resources not available on this platform.");
    return false;
  }
#endif

}
