#include "dxso_util.h"

#include "dxso_include.h"

namespace dxvk {

  uint32_t computeResourceSlotId(
        DxsoProgramType shaderStage,
        DxsoBindingType bindingType,
        uint32_t        bindingIndex) {
    const uint32_t stageOffset = 12 * uint32_t(shaderStage);

    if (shaderStage == DxsoProgramTypes::VertexShader) {
      switch (bindingType) {
        case DxsoBindingType::ConstantBuffer: return bindingIndex + stageOffset + 0; // 0 + 4 = 4
        case DxsoBindingType::ColorImage:     return bindingIndex + stageOffset + 4; // 4 + 4 = 8
        case DxsoBindingType::DepthImage:     return bindingIndex + stageOffset + 8; // 8 + 4 = 12
        default: Logger::err("computeResourceSlotId: Invalid resource type");
      }
    }
    else { // Pixel Shader
      switch (bindingType) {
      case DxsoBindingType::ConstantBuffer: return bindingIndex + stageOffset + 0;  // 0  + 3 = 3
        // The extra sampler here is being reserved for DMAP stuff later on.
      case DxsoBindingType::ColorImage:     return bindingIndex + stageOffset + 3;  // 3  + 17 = 20
      case DxsoBindingType::DepthImage:     return bindingIndex + stageOffset + 20; // 20 + 17 = 27
      default: Logger::err("computeResourceSlotId: Invalid resource type");
      }
    }

    return 0;
  }

  // TODO: Intergrate into compute resource slot ID/refactor all of this?
  uint32_t getSWVPBufferSlot() {
    return 39;
  }


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