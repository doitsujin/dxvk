#include "dxvk_shader_io.h"

namespace dxvk {

  DxvkShaderIo::DxvkShaderIo() {

  }


  DxvkShaderIo::~DxvkShaderIo() {

  }


  void DxvkShaderIo::add(DxvkShaderIoVar var) {
    size_t size = m_vars.size();
    size_t index = 0u;

    while (index < size && orderBefore(m_vars[index], var))
      index++;

    m_vars.resize(size + 1u);

    for (size_t i = size; i > index; i--)
      m_vars[i] = m_vars[i - 1u];

    m_vars[index] = var;
  }


  uint32_t DxvkShaderIo::computeMask() const {
    uint32_t result = 0u;

    for (size_t i = 0u; i < m_vars.size(); i++) {
      if (m_vars[i].builtIn == spv::BuiltInMax)
        result |= 1u << m_vars[i].location;
    }

    return result;
  }


  bool DxvkShaderIo::checkStageCompatibility(
          VkShaderStageFlagBits stage,
    const DxvkShaderIo&         inputs,
          VkShaderStageFlagBits prevStage,
    const DxvkShaderIo&         outputs) {
    for (uint32_t i = 0, j = 0; i < inputs.getVarCount(); i++) {
      // Ignore built-ins that don't need to be written by previous stage
      auto input = inputs.getVar(i);

      if (input.builtIn != spv::BuiltInMax) {
        if (isBuiltInInputGenerated(stage, prevStage, input.builtIn))
          continue;
      }

      // Find corresponding output variable
      if (j >= outputs.getVarCount())
        return false;

      while (orderBefore(outputs.getVar(j), input)) {
        if (++j >= outputs.getVarCount())
          return false;
      }

      auto output = outputs.getVar(j);

      if (input.builtIn != spv::BuiltInMax) {
        // Require a full match for built-ins
        if (input.builtIn != output.builtIn || input.componentCount != output.componentCount)
          return false;
      } else {
        // The only legal mismatch is output stage writing more components
        // than the input stage consumes, everything else has to match.
        if (input.isPatchConstant != output.isPatchConstant ||
            input.location        != output.location ||
            input.componentIndex  != output.componentIndex ||
            input.componentCount  >  output.componentCount)
          return false;
      }
    }

    return true;
  }


  DxvkShaderIo DxvkShaderIo::forVertexBindings(uint32_t bindingMask) {
    DxvkShaderIo result;

    for (auto location : bit::BitMask(bindingMask)) {
      DxvkShaderIoVar var = { };
      var.location = location;
      var.componentCount = 4u;

      result.add(var);
    }

    return result;
  }


  bool DxvkShaderIo::isBuiltInInputGenerated(
        VkShaderStageFlagBits stage,
        VkShaderStageFlagBits prevStage,
        spv::BuiltIn          builtIn) {
    switch (builtIn) {
      case spv::BuiltInPrimitiveId: {
        // Must be exported by DS / GS when read in subsequent stage
        return prevStage == VK_SHADER_STAGE_VERTEX_BIT ||
               prevStage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
      }

      case spv::BuiltInPosition:
        return stage == VK_SHADER_STAGE_FRAGMENT_BIT;

      case spv::BuiltInClipDistance:
      case spv::BuiltInCullDistance:
      case spv::BuiltInTessLevelInner:
      case spv::BuiltInTessLevelOuter:
        return false;

      default:
        return true;
    }
  }


  bool DxvkShaderIo::orderBefore(
    const DxvkShaderIoVar&      a,
    const DxvkShaderIoVar&      b) {
    if (a.builtIn != b.builtIn)
      return a.builtIn < b.builtIn;

    if (a.location != b.location)
      return a.location < b.location;

    return a.componentIndex < b.componentIndex;
  }

}
