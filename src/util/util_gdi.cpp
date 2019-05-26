#include "util_gdi.h"
#include "log/log.h"

namespace dxvk {

  HMODULE GetGDIModule() {
    static HMODULE module = LoadLibraryA("gdi32.dll");
    return module;
  }

  NTSTATUS D3DKMTCreateDCFromMemory(D3DKMT_CREATEDCFROMMEMORY* Arg1) {
    static auto func = (D3DKMTCreateDCFromMemoryType)
      GetProcAddress(GetGDIModule(), "D3DKMTCreateDCFromMemory");

    if (func != nullptr)
      return func(Arg1);

    Logger::warn("D3DKMTCreateDCFromMemory: Unable to query proc address.");
    return -1;
  }

  NTSTATUS D3DKMTDestroyDCFromMemory(D3DKMT_DESTROYDCFROMMEMORY* Arg1) {
    static auto func = (D3DKMTDestroyDCFromMemoryType)
      GetProcAddress(GetGDIModule(), "D3DKMTDestroyDCFromMemory");

    if (func != nullptr)
      return func(Arg1);

    Logger::warn("D3DKMTDestroyDCFromMemory: Unable to query proc address.");
    return -1;
  }

}