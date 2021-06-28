#include "util_luid.h"
#include "util_string.h"

#include "./log/log.h"

#include <mutex>
#include <vector>

namespace dxvk {

  LUID GetAdapterLUID(UINT Adapter) {
    static dxvk::mutex       s_mutex;
    static std::vector<LUID> s_luids;

    std::lock_guard<dxvk::mutex> lock(s_mutex);
    uint32_t newLuidCount = Adapter + 1;

    while (s_luids.size() < newLuidCount) {
      LUID luid = { 0, 0 };

      if (!::AllocateLocallyUniqueId(&luid))
        Logger::err("Failed to allocate LUID");
      
        
      Logger::info(str::format("Adapter LUID ", s_luids.size(), ": ",
        std::hex, luid.HighPart, ":", luid.LowPart, std::dec));

      s_luids.push_back(luid);
    }

    return s_luids[Adapter];
  }

}
