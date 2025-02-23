#include "dxvk_framepacer_mode_low_latency.h"

namespace dxvk {


  bool getLowLatencyOffsetFromEnv( int32_t& offset ) {
    if (!FramePacerMode::getIntFromEnv("DXVK_LOW_LATENCY_OFFSET", &offset))
      return false;
    return true;
  }


  int32_t LowLatencyMode::getLowLatencyOffset( const DxvkOptions& options ) {
    int32_t offset = options.lowLatencyOffset;
    int32_t o;
    if (getLowLatencyOffsetFromEnv(o))
      offset = o;

    offset = std::max( -10000, offset );
    offset = std::min(  10000, offset );
    return offset;
  }

}
