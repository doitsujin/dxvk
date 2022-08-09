#include "util_shared_res.h"
#include "log/log.h"

#ifdef _WIN32
#include <winioctl.h>
#endif

namespace dxvk {

#ifdef _WIN32
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
