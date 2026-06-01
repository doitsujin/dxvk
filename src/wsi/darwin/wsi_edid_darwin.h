#pragma once

#include "../wsi_edid.h"

#include <cstdint>

namespace dxvk::wsi::darwin {

  /**
   * \brief Read the EDID blob for a display index
   *
   * Display indices follow the same ordering as SDL/GLFW (CG online displays).
   *
   * \returns EDID bytes, or an empty vector if unavailable
   */
  WsiEdidData getMonitorEdidByIndex(uint32_t displayIndex);

}
