#include "dxvk_constant_state.h"

namespace dxvk {

  bool DxvkStencilOp::normalize(VkCompareOp depthOp) {
    if (writeMask()) {
      // If the depth test always passes, this is irrelevant
      if (depthOp == VK_COMPARE_OP_ALWAYS)
        setDepthFailOp(VK_STENCIL_OP_KEEP);

      // Also mask out unused ops if the stencil test
      // always pases or always fails
      if (compareOp() == VK_COMPARE_OP_ALWAYS)
        setFailOp(VK_STENCIL_OP_KEEP);
      else if (compareOp() == VK_COMPARE_OP_NEVER)
        setPassOp(VK_STENCIL_OP_KEEP);

      // If all stencil ops are no-ops, clear write mask
      if (passOp() == VK_STENCIL_OP_KEEP
       && failOp() == VK_STENCIL_OP_KEEP
       && depthFailOp() == VK_STENCIL_OP_KEEP)
        setWriteMask(0u);
    } else {
      // Normalize stencil ops if write mask is 0
      setPassOp(VK_STENCIL_OP_KEEP);
      setFailOp(VK_STENCIL_OP_KEEP);
      setDepthFailOp(VK_STENCIL_OP_KEEP);
    }

    // Check if the stencil test for this face is a no-op
    return writeMask() || compareOp() != VK_COMPARE_OP_ALWAYS;
  }


  void DxvkDepthStencilState::normalize() {
    if (depthTest()) {
      // If depth func is equal or if the depth test always fails, depth
      // writes will not have any observable effect so we can skip them.
      if (depthCompareOp() == VK_COMPARE_OP_EQUAL
       || depthCompareOp() == VK_COMPARE_OP_NEVER)
        setDepthWrite(false);

      // If the depth test always passes and no writes are performed, the
      // depth test as a whole is a no-op and can safely be disabled.
      if (depthCompareOp() == VK_COMPARE_OP_ALWAYS && !depthWrite())
        setDepthTest(false);
    } else {
      setDepthWrite(false);
      setDepthCompareOp(VK_COMPARE_OP_ALWAYS);
    }

    if (stencilTest()) {
      // Normalize stencil op and disable stencil testing if both are no-ops.
      bool frontIsNoOp = !m_stencilOpFront.normalize(depthCompareOp());
      bool backIsNoOp = !m_stencilOpBack.normalize(depthCompareOp());

      if (frontIsNoOp && backIsNoOp)
        setStencilTest(false);
    }

    // Normalize stencil ops if stencil test is disabled
    if (!stencilTest()) {
      setStencilOpFront(DxvkStencilOp());
      setStencilOpBack(DxvkStencilOp());
    }
  }


  void DxvkBlendMode::normalize() {
    constexpr VkColorComponentFlags colorMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
    constexpr VkColorComponentFlags alphaMask = VK_COLOR_COMPONENT_A_BIT;

    VkColorComponentFlags newWriteMask = writeMask();

    if (!newWriteMask)
      setBlendEnable(false);

    if (blendEnable()) {
      // If alpha or color are effectively not modified given the blend
      // function, set the corresponding part of the write mask to 0.
      if (colorBlendOp() == VK_BLEND_OP_ADD
       && colorSrcFactor() == VK_BLEND_FACTOR_ZERO
       && colorDstFactor() == VK_BLEND_FACTOR_ONE)
        newWriteMask &= ~colorMask;

      if (alphaBlendOp() == VK_BLEND_OP_ADD
       && alphaSrcFactor() == VK_BLEND_FACTOR_ZERO
       && alphaDstFactor() == VK_BLEND_FACTOR_ONE)
        newWriteMask &= ~alphaMask;

      // Check whether blending is equivalent to passing through
      // the source data as if blending was disabled.
      bool needsBlending = false;

      if (newWriteMask & colorMask) {
        needsBlending |= colorSrcFactor() != VK_BLEND_FACTOR_ONE
                      || colorDstFactor() != VK_BLEND_FACTOR_ZERO
                      || colorBlendOp()   != VK_BLEND_OP_ADD;
      }

      if (newWriteMask & alphaMask) {
        needsBlending |= alphaSrcFactor() != VK_BLEND_FACTOR_ONE
                      || alphaDstFactor() != VK_BLEND_FACTOR_ZERO
                      || alphaBlendOp()   != VK_BLEND_OP_ADD;
      }

      if (!needsBlending)
        setBlendEnable(false);
    }

    if (!blendEnable() || !(newWriteMask & colorMask))
      setColorOp(VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD);

    if (!blendEnable() || !(newWriteMask & alphaMask))
      setAlphaOp(VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD);

    setWriteMask(newWriteMask);
  }

}
