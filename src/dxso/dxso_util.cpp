#include "dxso_util.h"

#include "dxso_include.h"

namespace dxvk {

  dxvk::mutex                  g_linkerSlotMutex;
  uint32_t                     g_linkerSlotCount = 0;
  std::array<DxsoSemantic, 32> g_linkerSlots;

  uint32_t RegisterLinkerSlot(DxsoSemantic semantic) {
    // Lock, because games could be trying
    // to make multiple shaders at a time.
    std::lock_guard<dxvk::mutex> lock(g_linkerSlotMutex);

    // Need to chose a slot that maps nicely and similarly
    // between vertex and pixel shaders

    // Find or map a slot.
    uint32_t slot = g_linkerSlotCount;
    for (uint32_t j = 0; j < g_linkerSlotCount; j++) {
      if (g_linkerSlots[j] == semantic) {
        slot = j;
        break;
      }
    }

    if (slot == g_linkerSlotCount)
      g_linkerSlots[g_linkerSlotCount++] = semantic;

    return slot;
  }

}