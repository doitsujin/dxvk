#include "dxso_util.h"

#include "dxso_include.h"

namespace dxvk {

  size_t DXSOBytecodeLength(const uint32_t* pFunction) {
    const uint32_t* start = reinterpret_cast<const uint32_t*>(pFunction);
    const uint32_t* current = start;

    while (*current != 0x0000ffff) // A token of 0x0000ffff indicates the end of the bytecode.
      current++;

    return size_t(current - start) * sizeof(uint32_t);
  }

}