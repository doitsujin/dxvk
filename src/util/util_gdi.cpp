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

  NTSTATUS D3DKMTDestroyDCFromMemory(const D3DKMT_DESTROYDCFROMMEMORY *desc) {
    Logger::warn("D3DKMTDestroyDCFromMemory: Not available on this platform.");
    return -1;
  }

  NTSTATUS D3DKMTDestroyDevice(const D3DKMT_DESTROYDEVICE *desc) {
    Logger::warn("D3DKMTDestroyDevice: Not available on this platform.");
    return -1;
  }

  NTSTATUS WINAPI D3DKMTDestroySynchronizationObject(const D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *desc) {
    Logger::warn("D3DKMTDestroySynchronizationObject: Not available on this platform.");
    return -1;
  }

  NTSTATUS D3DKMTOpenAdapterFromLuid(D3DKMT_OPENADAPTERFROMLUID *desc) {
    Logger::warn("D3DKMTOpenAdapterFromLuid: Not available on this platform.");
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
#endif

}
