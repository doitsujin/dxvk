#include "d3d9_multisample.h"

namespace dxvk {
  DXGI_SAMPLE_DESC D3D9ToDXGISampleDesc(D3DMULTISAMPLE_TYPE MultiSampleType, DWORD MultiSampleQuality) {
    DXGI_SAMPLE_DESC sampleDesc;

    // D3D9 allows for non-power of two sample counts.
    // With Vulkan, we have to round this up to the next power-of-two.

    switch (MultiSampleType) {
      case 0:
        sampleDesc.Count = 1;
      case 1: case 2:
        sampleDesc.Count = 2;
      case 3: case 4:
        sampleDesc.Count = 4;
      default:
        // Clamp to 8x, which is what D3D11-level hardware guarantees.
        sampleDesc.Count = 8;
    }

    // DXVK ignores the sample quality.
    sampleDesc.Quality = 0;

    return sampleDesc;
  }
}
