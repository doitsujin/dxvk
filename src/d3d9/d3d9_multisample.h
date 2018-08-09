#pragma once

#include "d3d9_include.h"

namespace dxvk {
  /// Converts a D3D9 multisample type / quality description to a DXGI-compatible multisample description.
  DXGI_SAMPLE_DESC D3D9ToDXGISampleDesc(D3DMULTISAMPLE_TYPE MultiSampleType, DWORD MultiSampleQuality);
}
