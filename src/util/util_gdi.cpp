#include "util_gdi.h"
#include "log/log.h"

namespace dxvk {

#ifndef _WIN32
  NTSTATUS D3DKMTCloseAdapter(const D3DKMT_CLOSEADAPTER *desc) {
    Logger::warn("D3DKMTCloseAdapter: Not available on this platform.");
    return -1;
  }

  NTSTATUS D3DKMTCreateDCFromMemory(D3DKMT_CREATEDCFROMMEMORY *desc) {
    Logger::warn("D3DKMTCreateDCFromMemory: Not available on this platform.");
    return -1;
  }

  NTSTATUS D3DKMTCreateDevice(D3DKMT_CREATEDEVICE *desc) {
    Logger::warn("D3DKMTCreateDevice: Not available on this platform.");
    return -1;
  }

  NTSTATUS D3DKMTDestroyAllocation(const D3DKMT_DESTROYALLOCATION *desc) {
    Logger::warn("D3DKMTDestroyAllocation: Not available on this platform.");
    return -1;
  }

  NTSTATUS D3DKMTDestroyDCFromMemory(const D3DKMT_DESTROYDCFROMMEMORY *desc) {
    Logger::warn("D3DKMTDestroyDCFromMemory: Not available on this platform.");
    return -1;
  }

  NTSTATUS D3DKMTDestroyDevice(const D3DKMT_DESTROYDEVICE *desc) {
    Logger::warn("D3DKMTDestroyDevice: Not available on this platform.");
    return -1;
  }

  NTSTATUS D3DKMTDestroyKeyedMutex(const D3DKMT_DESTROYKEYEDMUTEX *desc) {
    Logger::warn("D3DKMTDestroyKeyedMutex: Not available on this platform.");
    return -1;
  }

  NTSTATUS WINAPI D3DKMTDestroySynchronizationObject(const D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *desc) {
    Logger::warn("D3DKMTDestroySynchronizationObject: Not available on this platform.");
    return -1;
  }

  NTSTATUS D3DKMTEscape(const D3DKMT_ESCAPE *desc) {
    Logger::warn("D3DKMTEscape: Not available on this platform.");
    return -1;
  }

  NTSTATUS D3DKMTOpenAdapterFromLuid(D3DKMT_OPENADAPTERFROMLUID *desc) {
    Logger::warn("D3DKMTOpenAdapterFromLuid: Not available on this platform.");
    return -1;
  }

  NTSTATUS D3DKMTOpenResource2(D3DKMT_OPENRESOURCE *desc) {
    Logger::warn("D3DKMTOpenResource2: Not available on this platform.");
    return -1;
  }

  NTSTATUS D3DKMTOpenResourceFromNtHandle(D3DKMT_OPENRESOURCEFROMNTHANDLE *desc) {
    Logger::warn("D3DKMTOpenResourceFromNtHandle: Not available on this platform.");
    return -1;
  }

  NTSTATUS WINAPI D3DKMTOpenSynchronizationObject(D3DKMT_OPENSYNCHRONIZATIONOBJECT *desc) {
    Logger::warn("D3DKMTOpenSynchronizationObject: Not available on this platform.");
    return -1;
  }

  NTSTATUS WINAPI D3DKMTOpenSyncObjectFromNtHandle(D3DKMT_OPENSYNCOBJECTFROMNTHANDLE *desc) {
    Logger::warn("D3DKMTOpenSyncObjectFromNtHandle: Not available on this platform.");
    return -1;
  }

  NTSTATUS D3DKMTQueryResourceInfo(D3DKMT_QUERYRESOURCEINFO *desc) {
    Logger::warn("D3DKMTQueryResourceInfo: Not available on this platform.");
    return -1;
  }

  NTSTATUS WINAPI D3DKMTShareObjects(UINT count, const D3DKMT_HANDLE *handles, OBJECT_ATTRIBUTES *attr, UINT access, HANDLE *handle) {
    Logger::warn("D3DKMTShareObjects: Not available on this platform.");
    return -1;
  }
#endif

}
