#include "dxvk_framepacer_mode_low_latency.h"

namespace dxvk {

  int32_t LowLatencyMode::getLowLatencyOffset( const DxvkOptions& options ) {
    int32_t offset = options.lowLatencyOffset;

    offset = std::max( -10000, offset );
    offset = std::min(  10000, offset );
    return offset;
  }

}
