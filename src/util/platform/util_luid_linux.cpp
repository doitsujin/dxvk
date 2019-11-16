#include "util_luid.h"

#include "./log/log.h"

namespace dxvk {

  LUID GetAdapterLUID(UINT Adapter) {
    Logger::warn("GetAdapterLUID: native stub");

    return LUID();
  }

}
