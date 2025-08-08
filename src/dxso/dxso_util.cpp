#include "dxso_util.h"

#include "dxso_include.h"

namespace dxvk {

  dxvk::mutex                  g_linkerSlotMutex;
  uint32_t                     g_linkerSlotCount = 12;
  std::array<DxsoSemantic, 32> g_linkerSlots = {
    {
      {DxsoUsage::Normal,   0},
      {DxsoUsage::Texcoord,   0},
      {DxsoUsage::Texcoord,   1},
      {DxsoUsage::Texcoord,   2},
      {DxsoUsage::Texcoord,   3},
      {DxsoUsage::Texcoord,   4},
      {DxsoUsage::Texcoord,   5},
      {DxsoUsage::Texcoord,   6},
      {DxsoUsage::Texcoord,   7},

      {DxsoUsage::Color,      0},
      {DxsoUsage::Color,      1},

      {DxsoUsage::Fog,        0},
    }};
  // We set fixed locations for the outputs that fixed function vertex shaders
  // can produce so the uber shader doesn't need to be patched at runtime.

  uint32_t RegisterLinkerSlot(DxsoSemantic semantic) {
    // Lock, because games could be trying
    // to make multiple shaders at a time.
    std::lock_guard<dxvk::mutex> lock(g_linkerSlotMutex);

    // Need to choose a slot that maps nicely and similarly
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