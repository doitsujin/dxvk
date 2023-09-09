#include "d3d9_fixed_function.h"

#include "d3d9_device.h"
#include "d3d9_util.h"
#include "d3d9_spec_constants.h"

#include "../dxvk/dxvk_hash.h"

#include "../spirv/spirv_module.h"

#include <cfloat>

namespace dxvk {

  D3D9FixedFunctionOptions::D3D9FixedFunctionOptions(const D3D9Options* options) {
    invariantPosition = options->invariantPosition;
    forceSampleRateShading = options->forceSampleRateShading;
  }

  uint32_t DoFixedFunctionFog(D3D9ShaderSpecConstantManager& spec, SpirvModule& spvModule, const D3D9FogContext& fogCtx) {
    uint32_t floatType  = spvModule.defFloatType(32);
    uint32_t vec3Type   = spvModule.defVectorType(floatType, 3);
    uint32_t vec4Type   = spvModule.defVectorType(floatType, 4);
    uint32_t floatPtr   = spvModule.defPointerType(floatType, spv::StorageClassPushConstant);
    uint32_t vec3Ptr    = spvModule.defPointerType(vec3Type,  spv::StorageClassPushConstant);

    uint32_t fogColorMember = spvModule.constu32(uint32_t(D3D9RenderStateItem::FogColor));
    uint32_t fogColor = spvModule.opLoad(vec3Type,
      spvModule.opAccessChain(vec3Ptr, fogCtx.RenderState, 1, &fogColorMember));

    uint32_t fogScaleMember = spvModule.constu32(uint32_t(D3D9RenderStateItem::FogScale));
    uint32_t fogScale = spvModule.opLoad(floatType,
      spvModule.opAccessChain(floatPtr, fogCtx.RenderState, 1, &fogScaleMember));

    uint32_t fogEndMember = spvModule.constu32(uint32_t(D3D9RenderStateItem::FogEnd));
    uint32_t fogEnd = spvModule.opLoad(floatType,
      spvModule.opAccessChain(floatPtr, fogCtx.RenderState, 1, &fogEndMember));

    uint32_t fogDensityMember = spvModule.constu32(uint32_t(D3D9RenderStateItem::FogDensity));
    uint32_t fogDensity = spvModule.opLoad(floatType,
      spvModule.opAccessChain(floatPtr, fogCtx.RenderState, 1, &fogDensityMember));

    uint32_t fogMode = spec.get(
      spvModule, fogCtx.SpecUBO,
      fogCtx.IsPixel ? SpecPixelFogMode : SpecVertexFogMode);

    uint32_t fogEnabled = spec.get(spvModule, fogCtx.SpecUBO, SpecFogEnabled);
    fogEnabled = spvModule.opINotEqual(spvModule.defBoolType(), fogEnabled, spvModule.constu32(0));

    uint32_t doFog   = spvModule.allocateId();
    uint32_t skipFog = spvModule.allocateId();

    uint32_t returnType     = fogCtx.IsPixel ? vec4Type : floatType;
    uint32_t returnTypePtr  = spvModule.defPointerType(returnType, spv::StorageClassPrivate);
    uint32_t returnValuePtr = spvModule.newVar(returnTypePtr, spv::StorageClassPrivate);
    spvModule.opStore(returnValuePtr, fogCtx.IsPixel ? fogCtx.oColor : spvModule.constf32(0.0f));

    // Actually do the fog now we have all the vars in-place.

    spvModule.opSelectionMerge(skipFog, spv::SelectionControlMaskNone);
    spvModule.opBranchConditional(fogEnabled, doFog, skipFog);

    spvModule.opLabel(doFog);

    uint32_t wIndex = 3;
    uint32_t zIndex = 2;

    uint32_t w = spvModule.opCompositeExtract(floatType, fogCtx.vPos, 1, &wIndex);
    uint32_t z = spvModule.opCompositeExtract(floatType, fogCtx.vPos, 1, &zIndex);

    uint32_t depth = 0;
    if (fogCtx.IsPixel)
      depth = spvModule.opFMul(floatType, z, spvModule.opFDiv(floatType, spvModule.constf32(1.0f), w));
    else {
      if (fogCtx.RangeFog) {
        std::array<uint32_t, 3> indices = { 0, 1, 2 };
        uint32_t pos3 = spvModule.opVectorShuffle(vec3Type, fogCtx.vPos, fogCtx.vPos, indices.size(), indices.data());
        depth = spvModule.opLength(floatType, pos3);
      }
      else
        depth = fogCtx.HasFogInput
          ? fogCtx.vFog
          : spvModule.opFAbs(floatType, z);
    }
    uint32_t fogFactor;
    if (!fogCtx.IsPixel && fogCtx.IsFixedFunction && fogCtx.IsPositionT) {
      fogFactor = fogCtx.HasSpecular
        ? spvModule.opCompositeExtract(floatType, fogCtx.Specular, 1, &wIndex)
        : spvModule.constf32(1.0f);
    } else {
      uint32_t applyFogFactor = spvModule.allocateId();

      std::array<SpirvPhiLabel, 4> fogVariables;

      std::array<SpirvSwitchCaseLabel, 4> fogCaseLabels = { {
        { uint32_t(D3DFOG_NONE),      spvModule.allocateId() },
        { uint32_t(D3DFOG_EXP),       spvModule.allocateId() },
        { uint32_t(D3DFOG_EXP2),      spvModule.allocateId() },
        { uint32_t(D3DFOG_LINEAR),    spvModule.allocateId() },
      } };

      spvModule.opSelectionMerge(applyFogFactor, spv::SelectionControlMaskNone);
      spvModule.opSwitch(fogMode,
        fogCaseLabels[D3DFOG_NONE].labelId,
        fogCaseLabels.size(),
        fogCaseLabels.data());

      for (uint32_t i = 0; i < fogCaseLabels.size(); i++) {
        spvModule.opLabel(fogCaseLabels[i].labelId);
        
        fogVariables[i].labelId = fogCaseLabels[i].labelId;
        fogVariables[i].varId   = [&] {
          auto mode = D3DFOGMODE(fogCaseLabels[i].literal);
          switch (mode) {
            default:
            // vFog
            case D3DFOG_NONE: {
              if (fogCtx.IsPixel)
                return fogCtx.vFog;

              if (fogCtx.IsFixedFunction && fogCtx.HasSpecular)
                return spvModule.opCompositeExtract(floatType, fogCtx.Specular, 1, &wIndex);

              return spvModule.constf32(1.0f);
            }

            // (end - d) / (end - start)
            case D3DFOG_LINEAR: {
              uint32_t fogFactor = spvModule.opFSub(floatType, fogEnd, depth);
              fogFactor = spvModule.opFMul(floatType, fogFactor, fogScale);
              fogFactor = spvModule.opNClamp(floatType, fogFactor, spvModule.constf32(0.0f), spvModule.constf32(1.0f));
              return fogFactor;
            }

            // 1 / (e^[d * density])^2
            case D3DFOG_EXP2:
            // 1 / (e^[d * density])
            case D3DFOG_EXP: {
              uint32_t fogFactor = spvModule.opFMul(floatType, depth, fogDensity);

              if (mode == D3DFOG_EXP2)
                fogFactor = spvModule.opFMul(floatType, fogFactor, fogFactor);

              // Provides the rcp.
              fogFactor = spvModule.opFNegate(floatType, fogFactor);
              fogFactor = spvModule.opExp(floatType, fogFactor);
              return fogFactor;
            }
          }
        }();
        
        spvModule.opBranch(applyFogFactor);
      }

      spvModule.opLabel(applyFogFactor);

      fogFactor = spvModule.opPhi(floatType,
        fogVariables.size(),
        fogVariables.data());
    }

    uint32_t fogRetValue = 0;

    // Return the new color if we are doing this in PS
    // or just the fog factor for oFog in VS
    if (fogCtx.IsPixel) {
      std::array<uint32_t, 4> indices = { 0, 1, 2, 6 };

      uint32_t color = fogCtx.oColor;

      uint32_t color3 = spvModule.opVectorShuffle(vec3Type, color, color, 3, indices.data());

      std::array<uint32_t, 3> fogFacIndices = { fogFactor, fogFactor, fogFactor };
      uint32_t fogFact3 = spvModule.opCompositeConstruct(vec3Type, fogFacIndices.size(), fogFacIndices.data());

      uint32_t lerpedFrog = spvModule.opFMix(vec3Type, fogColor, color3, fogFact3);

      fogRetValue = spvModule.opVectorShuffle(vec4Type, lerpedFrog, color, indices.size(), indices.data());
    }
    else
      fogRetValue = fogFactor;

    spvModule.opStore(returnValuePtr, fogRetValue);

    spvModule.opBranch(skipFog);

    spvModule.opLabel(skipFog);

    return spvModule.opLoad(returnType, returnValuePtr);
  }


  void DoFixedFunctionAlphaTest(SpirvModule& spvModule, const D3D9AlphaTestContext& ctx) {
    // Labels for the alpha test
    std::array<SpirvSwitchCaseLabel, 8> atestCaseLabels = {{
      { uint32_t(VK_COMPARE_OP_NEVER),            spvModule.allocateId() },
      { uint32_t(VK_COMPARE_OP_LESS),             spvModule.allocateId() },
      { uint32_t(VK_COMPARE_OP_EQUAL),            spvModule.allocateId() },
      { uint32_t(VK_COMPARE_OP_LESS_OR_EQUAL),    spvModule.allocateId() },
      { uint32_t(VK_COMPARE_OP_GREATER),          spvModule.allocateId() },
      { uint32_t(VK_COMPARE_OP_NOT_EQUAL),        spvModule.allocateId() },
      { uint32_t(VK_COMPARE_OP_GREATER_OR_EQUAL), spvModule.allocateId() },
      { uint32_t(VK_COMPARE_OP_ALWAYS),           spvModule.allocateId() },
    }};

    uint32_t atestBeginLabel   = spvModule.allocateId();
    uint32_t atestTestLabel    = spvModule.allocateId();
    uint32_t atestDiscardLabel = spvModule.allocateId();
    uint32_t atestKeepLabel    = spvModule.allocateId();
    uint32_t atestSkipLabel    = spvModule.allocateId();

    // if (alpha_func != ALWAYS) { ... }
    uint32_t boolType = spvModule.defBoolType();
    uint32_t isNotAlways = spvModule.opINotEqual(boolType, ctx.alphaFuncId, spvModule.constu32(VK_COMPARE_OP_ALWAYS));
    spvModule.opSelectionMerge(atestSkipLabel, spv::SelectionControlMaskNone);
    spvModule.opBranchConditional(isNotAlways, atestBeginLabel, atestSkipLabel);
    spvModule.opLabel(atestBeginLabel);

    // The lower 8 bits of the alpha ref contain the actual reference value
    // from the API, the upper bits store the accuracy bit count minus 8.
    // So if we want 12 bits of accuracy (i.e. 0-4095), that value will be 4.
    uint32_t uintType = spvModule.defIntType(32, 0);

    // Check if the given bit precision is supported
    uint32_t precisionIntLabel = spvModule.allocateId();
    uint32_t precisionFloatLabel = spvModule.allocateId();
    uint32_t precisionEndLabel = spvModule.allocateId();

    uint32_t useIntPrecision = spvModule.opULessThanEqual(boolType,
      ctx.alphaPrecisionId, spvModule.constu32(8));

    spvModule.opSelectionMerge(precisionEndLabel, spv::SelectionControlMaskNone);
    spvModule.opBranchConditional(useIntPrecision, precisionIntLabel, precisionFloatLabel);
    spvModule.opLabel(precisionIntLabel);

    // Adjust alpha ref to the given range
    uint32_t alphaRefIdInt = spvModule.opBitwiseOr(uintType,
      spvModule.opShiftLeftLogical(uintType, ctx.alphaRefId, ctx.alphaPrecisionId),
      spvModule.opShiftRightLogical(uintType, ctx.alphaRefId,
        spvModule.opISub(uintType, spvModule.constu32(8), ctx.alphaPrecisionId)));

    // Convert alpha ref to float since we'll do the comparison based on that
    uint32_t floatType = spvModule.defFloatType(32);
    alphaRefIdInt = spvModule.opConvertUtoF(floatType, alphaRefIdInt);

    // Adjust alpha to the given range and round
    uint32_t alphaFactorId = spvModule.opISub(uintType,
      spvModule.opShiftLeftLogical(uintType, spvModule.constu32(256), ctx.alphaPrecisionId),
      spvModule.constu32(1));
    alphaFactorId = spvModule.opConvertUtoF(floatType, alphaFactorId);

    uint32_t alphaIdInt = spvModule.opRoundEven(floatType,
      spvModule.opFMul(floatType, ctx.alphaId, alphaFactorId));

    spvModule.opBranch(precisionEndLabel);
    spvModule.opLabel(precisionFloatLabel);

    // If we're not using integer precision, normalize the alpha ref
    uint32_t alphaRefIdFloat = spvModule.opFDiv(floatType,
      spvModule.opConvertUtoF(floatType, ctx.alphaRefId),
      spvModule.constf32(255.0f));

    spvModule.opBranch(precisionEndLabel);
    spvModule.opLabel(precisionEndLabel);

    std::array<SpirvPhiLabel, 2> alphaRefLabels = {
      SpirvPhiLabel { alphaRefIdInt,    precisionIntLabel   },
      SpirvPhiLabel { alphaRefIdFloat,  precisionFloatLabel },
    };

    uint32_t alphaRefId = spvModule.opPhi(floatType,
      alphaRefLabels.size(),
      alphaRefLabels.data());

    std::array<SpirvPhiLabel, 2> alphaIdLabels = {
      SpirvPhiLabel { alphaIdInt,  precisionIntLabel   },
      SpirvPhiLabel { ctx.alphaId, precisionFloatLabel },
    };

    uint32_t alphaId = spvModule.opPhi(floatType,
      alphaIdLabels.size(),
      alphaIdLabels.data());

    // switch (alpha_func) { ... }
    spvModule.opSelectionMerge(atestTestLabel, spv::SelectionControlMaskNone);
    spvModule.opSwitch(ctx.alphaFuncId,
      atestCaseLabels[uint32_t(VK_COMPARE_OP_ALWAYS)].labelId,
      atestCaseLabels.size(),
      atestCaseLabels.data());

    std::array<SpirvPhiLabel, 8> atestVariables;

    for (uint32_t i = 0; i < atestCaseLabels.size(); i++) {
      spvModule.opLabel(atestCaseLabels[i].labelId);

      atestVariables[i].labelId = atestCaseLabels[i].labelId;
      atestVariables[i].varId   = [&] {
        switch (VkCompareOp(atestCaseLabels[i].literal)) {
          case VK_COMPARE_OP_NEVER:            return spvModule.constBool(false);
          case VK_COMPARE_OP_LESS:             return spvModule.opFOrdLessThan        (boolType, alphaId, alphaRefId);
          case VK_COMPARE_OP_EQUAL:            return spvModule.opFOrdEqual           (boolType, alphaId, alphaRefId);
          case VK_COMPARE_OP_LESS_OR_EQUAL:    return spvModule.opFOrdLessThanEqual   (boolType, alphaId, alphaRefId);
          case VK_COMPARE_OP_GREATER:          return spvModule.opFOrdGreaterThan     (boolType, alphaId, alphaRefId);
          case VK_COMPARE_OP_NOT_EQUAL:        return spvModule.opFOrdNotEqual        (boolType, alphaId, alphaRefId);
          case VK_COMPARE_OP_GREATER_OR_EQUAL: return spvModule.opFOrdGreaterThanEqual(boolType, alphaId, alphaRefId);
          default:
          case VK_COMPARE_OP_ALWAYS:           return spvModule.constBool(true);
        }
      }();

      spvModule.opBranch(atestTestLabel);
    }

    // end switch
    spvModule.opLabel(atestTestLabel);

    uint32_t atestResult = spvModule.opPhi(boolType,
      atestVariables.size(),
      atestVariables.data());
    uint32_t atestDiscard = spvModule.opLogicalNot(boolType, atestResult);

    // if (do_discard) { ... }
    spvModule.opSelectionMerge(atestKeepLabel, spv::SelectionControlMaskNone);
    spvModule.opBranchConditional(atestDiscard, atestDiscardLabel, atestKeepLabel);

    spvModule.opLabel(atestDiscardLabel);
    spvModule.opDemoteToHelperInvocation();
    spvModule.opBranch(atestKeepLabel);

    // end if (do_discard)
    spvModule.opLabel(atestKeepLabel);
    spvModule.opBranch(atestSkipLabel);

    // end if (alpha_test)
    spvModule.opLabel(atestSkipLabel);
  }


  uint32_t SetupRenderStateBlock(SpirvModule& spvModule, uint32_t count) {
    uint32_t floatType = spvModule.defFloatType(32);
    uint32_t uintType  = spvModule.defIntType(32, 0);
    uint32_t vec3Type  = spvModule.defVectorType(floatType, 3);

    std::array<uint32_t, 11> rsMembers = {{
      vec3Type,
      floatType,
      floatType,
      floatType,

      uintType,

      floatType,
      floatType,
      floatType,
      floatType,
      floatType,
      floatType,
    }};

    uint32_t rsStruct = spvModule.defStructTypeUnique(count, rsMembers.data());
    uint32_t rsBlock = spvModule.newVar(
      spvModule.defPointerType(rsStruct, spv::StorageClassPushConstant),
      spv::StorageClassPushConstant);
    
    spvModule.setDebugName         (rsBlock, "render_state");

    spvModule.setDebugName         (rsStruct, "render_state_t");
    spvModule.decorate             (rsStruct, spv::DecorationBlock);

    uint32_t memberIdx = 0;
    auto SetMemberName = [&](const char* name, uint32_t offset) {
      if (memberIdx >= count)
        return;

      spvModule.setDebugMemberName   (rsStruct, memberIdx, name);
      spvModule.memberDecorateOffset (rsStruct, memberIdx, offset);
      memberIdx++;
    };

    SetMemberName("fog_color",      offsetof(D3D9RenderStateInfo, fogColor));
    SetMemberName("fog_scale",      offsetof(D3D9RenderStateInfo, fogScale));
    SetMemberName("fog_end",        offsetof(D3D9RenderStateInfo, fogEnd));
    SetMemberName("fog_density",    offsetof(D3D9RenderStateInfo, fogDensity));
    SetMemberName("alpha_ref",      offsetof(D3D9RenderStateInfo, alphaRef));
    SetMemberName("point_size",     offsetof(D3D9RenderStateInfo, pointSize));
    SetMemberName("point_size_min", offsetof(D3D9RenderStateInfo, pointSizeMin));
    SetMemberName("point_size_max", offsetof(D3D9RenderStateInfo, pointSizeMax));
    SetMemberName("point_scale_a",  offsetof(D3D9RenderStateInfo, pointScaleA));
    SetMemberName("point_scale_b",  offsetof(D3D9RenderStateInfo, pointScaleB));
    SetMemberName("point_scale_c",  offsetof(D3D9RenderStateInfo, pointScaleC));

    return rsBlock;
  }


  uint32_t SetupSpecUBO(SpirvModule& spvModule, std::vector<DxvkBindingInfo>& bindings) {
    uint32_t uintType = spvModule.defIntType(32, 0);

    std::array<uint32_t, SpecConstantCount> specMembers;
    for (auto& x : specMembers)
      x = uintType;

    uint32_t specStruct = spvModule.defStructTypeUnique(uint32_t(specMembers.size()), specMembers.data());

    spvModule.setDebugName         (specStruct, "spec_state_t");
    spvModule.decorate             (specStruct, spv::DecorationBlock);

    for (uint32_t i = 0; i < SpecConstantCount; i++) {
      std::string name = str::format("dword", i);
      spvModule.setDebugMemberName   (specStruct, i, name.c_str());
      spvModule.memberDecorateOffset (specStruct, i, sizeof(uint32_t) * i);
    }

    uint32_t specBlock = spvModule.newVar(
      spvModule.defPointerType(specStruct, spv::StorageClassUniform),
      spv::StorageClassUniform);

    spvModule.setDebugName         (specBlock, "spec_state");
    spvModule.decorateDescriptorSet(specBlock, 0);
    spvModule.decorateBinding      (specBlock, getSpecConstantBufferSlot());

    DxvkBindingInfo binding = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
    binding.resourceBinding = getSpecConstantBufferSlot();
    binding.viewType        = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    binding.access          = VK_ACCESS_UNIFORM_READ_BIT;
    binding.uboSet          = VK_TRUE;
    bindings.push_back(binding);

    return specBlock;
  }


  D3D9PointSizeInfoVS GetPointSizeInfoVS(D3D9ShaderSpecConstantManager& spec, SpirvModule& spvModule, uint32_t vPos, uint32_t vtx, uint32_t perVertPointSize, uint32_t rsBlock, uint32_t specUbo, bool isFixedFunction) {
    uint32_t floatType  = spvModule.defFloatType(32);
    uint32_t floatPtr   = spvModule.defPointerType(floatType, spv::StorageClassPushConstant);
    uint32_t vec3Type   = spvModule.defVectorType(floatType, 3);
    uint32_t vec4Type   = spvModule.defVectorType(floatType, 4);
    uint32_t uint32Type = spvModule.defIntType(32, 0);
    uint32_t boolType   = spvModule.defBoolType();

    auto LoadFloat = [&](D3D9RenderStateItem item) {
      uint32_t index = spvModule.constu32(uint32_t(item));
      return spvModule.opLoad(floatType, spvModule.opAccessChain(floatPtr, rsBlock, 1, &index));
    };

    uint32_t value = perVertPointSize != 0 ? perVertPointSize : LoadFloat(D3D9RenderStateItem::PointSize);

    if (isFixedFunction) {
      uint32_t pointMode = spec.get(spvModule, specUbo, SpecPointMode);

      uint32_t scaleBit  = spvModule.opBitFieldUExtract(uint32Type, pointMode, spvModule.consti32(0), spvModule.consti32(1));
      uint32_t isScale   = spvModule.opIEqual(boolType, scaleBit, spvModule.constu32(1));

      uint32_t scaleC = LoadFloat(D3D9RenderStateItem::PointScaleC);
      uint32_t scaleB = LoadFloat(D3D9RenderStateItem::PointScaleB);
      uint32_t scaleA = LoadFloat(D3D9RenderStateItem::PointScaleA);

      std::array<uint32_t, 4> indices = { 0, 1, 2, 3 };

      uint32_t vtx3;
      if (vPos != 0) {
        vPos = spvModule.opLoad(vec4Type, vPos);

        uint32_t rhw  = spvModule.opCompositeExtract(floatType, vPos, 1, &indices[3]);
                 rhw  = spvModule.opFDiv(floatType, spvModule.constf32(1.0f), rhw);
        uint32_t pos3 = spvModule.opVectorShuffle(vec3Type, vPos, vPos, 3, indices.data());
                 vtx3 = spvModule.opVectorTimesScalar(vec3Type, pos3, rhw);
      } else {
                 vtx3 = spvModule.opVectorShuffle(vec3Type, vtx, vtx, 3, indices.data());
      }

      uint32_t DeSqr      = spvModule.opDot (floatType, vtx3, vtx3);
      uint32_t De         = spvModule.opSqrt(floatType, DeSqr);
      uint32_t scaleValue = spvModule.opFMul(floatType, scaleC, DeSqr);
               scaleValue = spvModule.opFFma(floatType, scaleB, De, scaleValue);
               scaleValue = spvModule.opFAdd(floatType, scaleA, scaleValue);
               scaleValue = spvModule.opSqrt(floatType, scaleValue);
               scaleValue = spvModule.opFDiv(floatType, value, scaleValue);

      value = spvModule.opSelect(floatType, isScale, scaleValue, value);
    }

    uint32_t min   = LoadFloat(D3D9RenderStateItem::PointSizeMin);
    uint32_t max   = LoadFloat(D3D9RenderStateItem::PointSizeMax);

    D3D9PointSizeInfoVS info;
    info.defaultValue = value;
    info.min          = min;
    info.max          = max;

    return info;
  }


  D3D9PointSizeInfoPS GetPointSizeInfoPS(D3D9ShaderSpecConstantManager& spec, SpirvModule& spvModule, uint32_t rsBlock, uint32_t specUbo) {
    uint32_t uint32Type = spvModule.defIntType(32, 0);
    uint32_t boolType   = spvModule.defBoolType();
    uint32_t boolVec4   = spvModule.defVectorType(boolType, 4);

    uint32_t pointMode = spec.get(spvModule, specUbo, SpecPointMode);

    uint32_t spriteBit  = spvModule.opBitFieldUExtract(uint32Type, pointMode, spvModule.consti32(1), spvModule.consti32(1));
    uint32_t isSprite   = spvModule.opIEqual(boolType, spriteBit, spvModule.constu32(1));

    std::array<uint32_t, 4> isSpriteIndices;
    for (uint32_t i = 0; i < isSpriteIndices.size(); i++)
      isSpriteIndices[i] = isSprite;

    isSprite = spvModule.opCompositeConstruct(boolVec4, isSpriteIndices.size(), isSpriteIndices.data());

    D3D9PointSizeInfoPS info;
    info.isSprite = isSprite;

    return info;
  }


  uint32_t GetPointCoord(SpirvModule& spvModule) {
    uint32_t floatType  = spvModule.defFloatType(32);
    uint32_t vec2Type   = spvModule.defVectorType(floatType, 2);
    uint32_t vec4Type   = spvModule.defVectorType(floatType, 4);
    uint32_t vec2Ptr    = spvModule.defPointerType(vec2Type, spv::StorageClassInput);

    uint32_t pointCoordPtr = spvModule.newVar(vec2Ptr, spv::StorageClassInput);

    spvModule.decorateBuiltIn(pointCoordPtr, spv::BuiltInPointCoord);

    uint32_t pointCoord    = spvModule.opLoad(vec2Type, pointCoordPtr);

    std::array<uint32_t, 4> indices = { 0, 1, 2, 3 };

    std::array<uint32_t, 4> pointCoordIndices = {
      spvModule.opCompositeExtract(floatType, pointCoord, 1, &indices[0]),
      spvModule.opCompositeExtract(floatType, pointCoord, 1, &indices[1]),
      spvModule.constf32(0.0f),
      spvModule.constf32(0.0f)
    };

    return spvModule.opCompositeConstruct(vec4Type, pointCoordIndices.size(), pointCoordIndices.data());
  }


  uint32_t GetSharedConstants(SpirvModule& spvModule) {
    uint32_t float_t = spvModule.defFloatType(32);
    uint32_t vec2_t  = spvModule.defVectorType(float_t, 2);
    uint32_t vec4_t  = spvModule.defVectorType(float_t, 4);

    std::array<uint32_t, D3D9SharedPSStages_Count> stageMembers = {
      vec4_t,

      vec2_t,
      vec2_t,

      float_t,
      float_t,
    };

    std::array<decltype(stageMembers), caps::TextureStageCount> members;

    for (auto& member : members)
      member = stageMembers;

    const uint32_t structType =
      spvModule.defStructType(members.size() * stageMembers.size(), members[0].data());

    spvModule.decorateBlock(structType);

    uint32_t offset = 0;
    for (uint32_t stage = 0; stage < caps::TextureStageCount; stage++) {
      spvModule.memberDecorateOffset(structType, stage * D3D9SharedPSStages_Count + D3D9SharedPSStages_Constant, offset);
      offset += sizeof(float) * 4;

      spvModule.memberDecorateOffset(structType, stage * D3D9SharedPSStages_Count + D3D9SharedPSStages_BumpEnvMat0, offset);
      offset += sizeof(float) * 2;

      spvModule.memberDecorateOffset(structType, stage * D3D9SharedPSStages_Count + D3D9SharedPSStages_BumpEnvMat1, offset);
      offset += sizeof(float) * 2;

      spvModule.memberDecorateOffset(structType, stage * D3D9SharedPSStages_Count + D3D9SharedPSStages_BumpEnvLScale, offset);
      offset += sizeof(float);

      spvModule.memberDecorateOffset(structType, stage * D3D9SharedPSStages_Count + D3D9SharedPSStages_BumpEnvLOffset, offset);
      offset += sizeof(float);

      // Padding...
      offset += sizeof(float) * 2;
    }

    uint32_t sharedState = spvModule.newVar(
      spvModule.defPointerType(structType, spv::StorageClassUniform),
      spv::StorageClassUniform);

    spvModule.setDebugName(sharedState, "D3D9SharedPS");

    return sharedState;
  }


  enum class D3D9FFVSMembers {
    WorldViewMatrix,
    NormalMatrix,
    InverseViewMatrix,
    ProjMatrix,
      
    Texcoord0,
    Texcoord1,
    Texcoord2,
    Texcoord3,
    Texcoord4,
    Texcoord5,
    Texcoord6,
    Texcoord7,

    InverseOffset,
    InverseExtent,

    GlobalAmbient,

    Light0,
    Light1,
    Light2,
    Light3,
    Light4,
    Light5,
    Light6,
    Light7,

    MaterialDiffuse,
    MaterialAmbient,
    MaterialSpecular,
    MaterialEmissive,
    MaterialPower,

    TweenFactor,

    MemberCount
  };

  struct D3D9FFVertexData {
    uint32_t constantBuffer;
    uint32_t vertexBlendData;
    uint32_t lightType;

    struct {
      uint32_t worldview;
      uint32_t normal;
      uint32_t inverseView;
      uint32_t proj;

      uint32_t texcoord[8];

      uint32_t invOffset;
      uint32_t invExtent;

      uint32_t globalAmbient;

      uint32_t materialDiffuse;
      uint32_t materialSpecular;
      uint32_t materialAmbient;
      uint32_t materialEmissive;
      uint32_t materialPower;
      uint32_t tweenFactor;
    } constants;

    struct {
      uint32_t POSITION;
      uint32_t POSITION1;
      uint32_t POINTSIZE;
      uint32_t NORMAL;
      uint32_t NORMAL1;
      uint32_t TEXCOORD[8];
      uint32_t COLOR[2];
      uint32_t FOG;

      uint32_t BLENDWEIGHT;
      uint32_t BLENDINDICES;
    } in;

    struct {
      uint32_t POSITION;
      uint32_t POINTSIZE;
      uint32_t NORMAL;
      uint32_t TEXCOORD[8];
      uint32_t COLOR[2];
      uint32_t FOG;
    } out;
  };

  enum D3D9FFPSMembers {
    TextureFactor = 0,

    MemberCount
  };

  struct D3D9FFPixelData {
    uint32_t constantBuffer;
    uint32_t sharedState;

    struct {
      uint32_t textureFactor;
    } constants;

    struct {
      uint32_t TEXCOORD[8];
      uint32_t COLOR[2];
      uint32_t FOG;
      uint32_t POS;
    } in;

    struct {
      uint32_t texcoordCnt;
      uint32_t typeId;
      uint32_t varId;
    } samplers[8];

    struct {
      uint32_t COLOR;
    } out;
  };

  class D3D9FFShaderCompiler {

  public:

    D3D9FFShaderCompiler(
            Rc<DxvkDevice>           Device,
      const D3D9FFShaderKeyVS&       Key,
      const std::string&             Name,
            D3D9FixedFunctionOptions Options);

    D3D9FFShaderCompiler(
            Rc<DxvkDevice>           Device,
      const D3D9FFShaderKeyFS&       Key,
      const std::string&             Name,
            D3D9FixedFunctionOptions Options);

    Rc<DxvkShader> compile();

    DxsoIsgn isgn() { return m_isgn; }

  private:

    // Returns value for inputs
    // Returns ptr for outputs
    uint32_t declareIO(bool input, DxsoSemantic semantic, spv::BuiltIn builtin = spv::BuiltInMax);

    void compileVS();

    void setupRenderStateInfo();

    void emitLightTypeDecl();

    void emitBaseBufferDecl();

    void emitVertexBlendDecl();

    void setupVS();

    void compilePS();

    void setupPS();

    void emitPsSharedConstants();

    void emitVsClipping(uint32_t vtx);

    void alphaTestPS();

    uint32_t emitMatrixTimesVector(uint32_t rowCount, uint32_t colCount, uint32_t matrix, uint32_t vector);
    uint32_t emitVectorTimesMatrix(uint32_t rowCount, uint32_t colCount, uint32_t vector, uint32_t matrix);

    bool isVS() { return m_programType == DxsoProgramType::VertexShader; }
    bool isPS() { return !isVS(); }

    std::string           m_filename;

    SpirvModule           m_module;
    std::vector
      <DxvkBindingInfo>   m_bindings;

    uint32_t              m_inputMask = 0u;
    uint32_t              m_outputMask = 0u;
    uint32_t              m_flatShadingMask = 0u;
    uint32_t              m_pushConstOffset = 0u;
    uint32_t              m_pushConstSize = 0u;

    DxsoProgramType       m_programType;
    D3D9FFShaderKeyVS     m_vsKey;
    D3D9FFShaderKeyFS     m_fsKey;

    D3D9FFVertexData      m_vs = { };
    D3D9FFPixelData       m_ps = { };

    DxsoIsgn              m_isgn;
    DxsoIsgn              m_osgn;

    uint32_t              m_floatType;
    uint32_t              m_uint32Type;
    uint32_t              m_vec4Type;
    uint32_t              m_vec3Type;
    uint32_t              m_vec2Type;
    uint32_t              m_mat3Type;
    uint32_t              m_mat4Type;

    uint32_t              m_entryPointId;

    uint32_t              m_rsBlock;
    uint32_t              m_specUbo;
    uint32_t              m_mainFuncLabel;

    D3D9FixedFunctionOptions m_options;

    D3D9ShaderSpecConstantManager m_spec;
  };

  D3D9FFShaderCompiler::D3D9FFShaderCompiler(
          Rc<DxvkDevice>           Device,
    const D3D9FFShaderKeyVS&       Key,
    const std::string&             Name,
          D3D9FixedFunctionOptions Options)
  : m_module(spvVersion(1, 3)), m_options(Options) {
    m_programType = DxsoProgramTypes::VertexShader;
    m_vsKey    = Key;
    m_filename = Name;
  }


  D3D9FFShaderCompiler::D3D9FFShaderCompiler(
          Rc<DxvkDevice>           Device,
    const D3D9FFShaderKeyFS&       Key,
    const std::string&             Name,
          D3D9FixedFunctionOptions Options)
  : m_module(spvVersion(1, 3)), m_options(Options) {
    m_programType = DxsoProgramTypes::PixelShader;
    m_fsKey    = Key;
    m_filename = Name;
  }


  Rc<DxvkShader> D3D9FFShaderCompiler::compile() {
    m_floatType  = m_module.defFloatType(32);
    m_uint32Type = m_module.defIntType(32, 0);
    m_vec4Type   = m_module.defVectorType(m_floatType, 4);
    m_vec3Type   = m_module.defVectorType(m_floatType, 3);
    m_vec2Type   = m_module.defVectorType(m_floatType, 2);
    m_mat3Type   = m_module.defMatrixType(m_vec3Type, 3);
    m_mat4Type   = m_module.defMatrixType(m_vec4Type, 4);

    m_entryPointId = m_module.allocateId();

    // Set the shader name so that we recognize it in renderdoc
    m_module.setDebugSource(
      spv::SourceLanguageUnknown, 0,
      m_module.addDebugString(m_filename.c_str()),
      nullptr);

    // Set the memory model. This is the same for all shaders.
    m_module.setMemoryModel(
      spv::AddressingModelLogical,
      spv::MemoryModelGLSL450);

    m_module.enableCapability(spv::CapabilityShader);
    m_module.enableCapability(spv::CapabilityImageQuery);

    m_module.functionBegin(
      m_module.defVoidType(), m_entryPointId, m_module.defFunctionType(
        m_module.defVoidType(), 0, nullptr),
      spv::FunctionControlMaskNone);
    m_module.setDebugName(m_entryPointId, "main");

    m_mainFuncLabel = m_module.allocateId();
    m_module.opLabel(m_mainFuncLabel);

    if (isVS())
      compileVS();
    else
      compilePS();

    m_module.opReturn();
    m_module.functionEnd();

    // Declare the entry point, we now have all the
    // information we need, including the interfaces
    m_module.addEntryPoint(m_entryPointId,
      isVS() ? spv::ExecutionModelVertex : spv::ExecutionModelFragment, "main");

    // Create the shader module object
    DxvkShaderCreateInfo info;
    info.stage = isVS() ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
    info.bindingCount = m_bindings.size();
    info.bindings = m_bindings.data();
    info.inputMask = m_inputMask;
    info.outputMask = m_outputMask;
    info.flatShadingInputs = m_flatShadingMask;
    info.pushConstOffset = m_pushConstOffset;
    info.pushConstSize = m_pushConstSize;

    return new DxvkShader(info, m_module.compile());
  }


  uint32_t D3D9FFShaderCompiler::declareIO(bool input, DxsoSemantic semantic, spv::BuiltIn builtin) {
    // Declare in ISGN and do linkage
    auto& sgn = input
      ? m_isgn : m_osgn;

    uint32_t& slots = input
      ? m_inputMask
      : m_outputMask;

    uint32_t i = sgn.elemCount++;

    uint32_t slot = i;

    if (builtin == spv::BuiltInMax) {
      if (input != isVS()) {
        slot = RegisterLinkerSlot(semantic); // Requires linkage...
      }

      slots |= 1u << slot;
    }

    auto& elem = sgn.elems[i];
    elem.slot = slot;
    elem.semantic = semantic;

    // Declare variable
    spv::StorageClass storageClass = input ?
      spv::StorageClassInput : spv::StorageClassOutput;

    const bool scalar = semantic.usage == DxsoUsage::Fog || semantic.usage == DxsoUsage::PointSize;
    uint32_t type = scalar ? m_floatType : m_vec4Type;

    uint32_t ptrType = m_module.defPointerType(type, storageClass);

    uint32_t ptr = m_module.newVar(ptrType, storageClass);

    if (builtin == spv::BuiltInMax) {
      m_module.decorateLocation(ptr, slot);

      if (isPS() && input && m_options.forceSampleRateShading) {
        m_module.enableCapability(spv::CapabilitySampleRateShading);
        m_module.decorate(ptr, spv::DecorationSample);
      }
    } else {
      m_module.decorateBuiltIn(ptr, builtin);
    }

    bool diffuseOrSpec = semantic == DxsoSemantic{ DxsoUsage::Color, 0 }
                      || semantic == DxsoSemantic{ DxsoUsage::Color, 1 };

    if (diffuseOrSpec && input)
      m_flatShadingMask |= 1u << slot;

    std::string name = str::format(input ? "in_" : "out_", semantic.usage, semantic.usageIndex);
    m_module.setDebugName(ptr, name.c_str());

    if (input)
      return m_module.opLoad(type, ptr);

    return ptr;
  }


  void D3D9FFShaderCompiler::compileVS() {
    setupVS();

    std::array<uint32_t, 4> indices = { 0, 1, 2, 3 };

    uint32_t gl_Position = m_vs.in.POSITION;
    uint32_t vtx         = m_vs.in.POSITION;
    uint32_t normal      = m_module.opVectorShuffle(m_vec3Type, m_vs.in.NORMAL, m_vs.in.NORMAL, 3, indices.data());

    if (m_vsKey.Data.Contents.VertexBlendMode == D3D9FF_VertexBlendMode_Tween) {
      uint32_t vtx1    = m_vs.in.POSITION1;
      uint32_t normal1 = m_module.opVectorShuffle(m_vec3Type, m_vs.in.NORMAL1, m_vs.in.NORMAL1, 3, indices.data());

      vtx    = m_module.opFMix(m_vec3Type, vtx,    vtx1,    m_vs.constants.tweenFactor);
      normal = m_module.opFMix(m_vec3Type, normal, normal1, m_vs.constants.tweenFactor);
    }

    const uint32_t wIndex = 3;

    if (!m_vsKey.Data.Contents.HasPositionT) {
      if (m_vsKey.Data.Contents.VertexBlendMode == D3D9FF_VertexBlendMode_Normal) {
        uint32_t blendWeightRemaining = m_module.constf32(1);
        uint32_t vtxSum               = 0;
        uint32_t nrmSum               = 0;

        for (uint32_t i = 0; i <= m_vsKey.Data.Contents.VertexBlendCount; i++) {
          std::array<uint32_t, 2> arrayIndices;

          if (m_vsKey.Data.Contents.VertexBlendIndexed) {
            uint32_t index = m_module.opCompositeExtract(m_floatType, m_vs.in.BLENDINDICES, 1, &i);
                     index = m_module.opConvertFtoU(m_uint32Type, m_module.opRound(m_floatType, index));

            arrayIndices = { m_module.constu32(0), index };
          }
          else
            arrayIndices = { m_module.constu32(0), m_module.constu32(i) };

          uint32_t worldview = m_module.opLoad(m_mat4Type,
            m_module.opAccessChain(
              m_module.defPointerType(m_mat4Type, spv::StorageClassUniform), m_vs.vertexBlendData, arrayIndices.size(), arrayIndices.data()));

          uint32_t nrmMtx = worldview;

          std::array<uint32_t, 3> mtxIndices;
          for (uint32_t i = 0; i < 3; i++) {
            mtxIndices[i] = m_module.opCompositeExtract(m_vec4Type, nrmMtx, 1, &i);
            mtxIndices[i] = m_module.opVectorShuffle(m_vec3Type, mtxIndices[i], mtxIndices[i], 3, indices.data());
          }
          nrmMtx = m_module.opCompositeConstruct(m_mat3Type, mtxIndices.size(), mtxIndices.data());

          uint32_t vtxResult = emitVectorTimesMatrix(4, 4, vtx, worldview);
          uint32_t nrmResult = m_module.opVectorTimesMatrix(m_vec3Type, normal, nrmMtx);

          uint32_t weight;
          if (i != m_vsKey.Data.Contents.VertexBlendCount) {
            weight = m_module.opCompositeExtract(m_floatType, m_vs.in.BLENDWEIGHT, 1, &i);
            blendWeightRemaining = m_module.opFSub(m_floatType, blendWeightRemaining, weight);
          }
          else
            weight = blendWeightRemaining;

          std::array<uint32_t, 4> weightIds = { weight, weight, weight, weight };
          uint32_t weightVec4 = m_module.opCompositeConstruct(m_vec4Type, 4, weightIds.data());
          uint32_t weightVec3 = m_module.opCompositeConstruct(m_vec3Type, 3, weightIds.data());

          vtxSum = vtxSum
            ? m_module.opFFma(m_vec4Type, vtxResult, weightVec4, vtxSum)
            : m_module.opFMul(m_vec4Type, vtxResult, weightVec4);

          nrmSum = nrmSum
            ? m_module.opFFma(m_vec3Type, nrmResult, weightVec3, nrmSum)
            : m_module.opFMul(m_vec3Type, nrmResult, weightVec3);

          m_module.decorate(vtxSum, spv::DecorationNoContraction);
        }

        vtx    = vtxSum;
        normal = nrmSum;
      }
      else {
        vtx = emitVectorTimesMatrix(4, 4, vtx, m_vs.constants.worldview);

        uint32_t nrmMtx = m_vs.constants.normal;

        std::array<uint32_t, 3> mtxIndices;
        for (uint32_t i = 0; i < 3; i++) {
          mtxIndices[i] = m_module.opCompositeExtract(m_vec4Type, nrmMtx, 1, &i);
          mtxIndices[i] = m_module.opVectorShuffle(m_vec3Type, mtxIndices[i], mtxIndices[i], 3, indices.data());
        }
        nrmMtx = m_module.opCompositeConstruct(m_mat3Type, mtxIndices.size(), mtxIndices.data());

        normal = m_module.opMatrixTimesVector(m_vec3Type, nrmMtx, normal);
      }

      // Some games rely no normals not being normal.
      if (m_vsKey.Data.Contents.NormalizeNormals) {
        uint32_t bool_t = m_module.defBoolType();
        uint32_t bool3_t = m_module.defVectorType(bool_t, 3);

        uint32_t isZeroNormal = m_module.opAll(bool_t, m_module.opFOrdEqual(bool3_t, normal, m_module.constvec3f32(0.0f, 0.0f, 0.0f)));

        std::array<uint32_t, 3> members = { isZeroNormal, isZeroNormal, isZeroNormal };
        uint32_t isZeroNormal3 = m_module.opCompositeConstruct(bool3_t, members.size(), members.data());

        normal = m_module.opNormalize(m_vec3Type, normal);
        normal = m_module.opSelect(m_vec3Type, isZeroNormal3, m_module.constvec3f32(0.0f, 0.0f, 0.0f), normal);
      }
      
      gl_Position = emitVectorTimesMatrix(4, 4, vtx, m_vs.constants.proj);
    } else {
      gl_Position = m_module.opFMul(m_vec4Type, gl_Position, m_vs.constants.invExtent);
      gl_Position = m_module.opFAdd(m_vec4Type, gl_Position, m_vs.constants.invOffset);

      // We still need to account for perspective correction here...

      // gl_Position.w    = 1.0f / gl_Position.w
      // gl_Position.xyz *= gl_Position.w;

      uint32_t bool_t  = m_module.defBoolType();

      uint32_t w   = m_module.opCompositeExtract (m_floatType, gl_Position, 1, &wIndex);      // w = gl_Position.w
      uint32_t is0 = m_module.opFOrdEqual        (bool_t,      w, m_module.constf32(0));      // is0 = w == 0
      uint32_t rhw = m_module.opFDiv             (m_floatType, m_module.constf32(1.0f), w);   // rhw = 1.0f / w
               rhw = m_module.opSelect           (m_floatType, is0, m_module.constf32(1.0), rhw); // rhw = w == 0 ? 1.0 : rhw
      gl_Position  = m_module.opVectorTimesScalar(m_vec4Type,  gl_Position, rhw);             // gl_Position.xyz *= rhw
      gl_Position  = m_module.opCompositeInsert  (m_vec4Type,  rhw, gl_Position, 1, &wIndex); // gl_Position.w = rhw
    }

    m_module.opStore(m_vs.out.POSITION, gl_Position);

    std::array<uint32_t, 4> outNrmIndices;
    for (uint32_t i = 0; i < 3; i++)
      outNrmIndices[i] = m_module.opCompositeExtract(m_floatType, normal, 1, &i);
    outNrmIndices[3] = m_module.constf32(1.0f);

    uint32_t outNrm = m_module.opCompositeConstruct(m_vec4Type, outNrmIndices.size(), outNrmIndices.data());

    m_module.opStore(m_vs.out.NORMAL, outNrm);

    for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
      uint32_t inputIndex = (m_vsKey.Data.Contents.TexcoordIndices     >> (i * 3)) & 0b111;
      uint32_t inputFlags = (m_vsKey.Data.Contents.TexcoordFlags       >> (i * 3)) & 0b111;
      uint32_t texcoordCount = (m_vsKey.Data.Contents.TexcoordDeclMask >> (inputIndex * 3)) & 0b111;

      uint32_t transformed;

      const uint32_t wIndex = 3;

      uint32_t flags = (m_vsKey.Data.Contents.TransformFlags >> (i * 3)) & 0b111;
      uint32_t count;
      switch (inputFlags) {
        default:
        case (DXVK_TSS_TCI_PASSTHRU >> TCIOffset):
          transformed = m_vs.in.TEXCOORD[inputIndex & 0xFF];
          // flags is actually the number of elements that get passed
          // to the rasterizer.
          count = flags;
          if (texcoordCount) {
            // Clamp by the number of elements in the texcoord input.
            if (!count || count > texcoordCount)
              count = texcoordCount;
          }
          else
            flags = D3DTTFF_DISABLE;
          break;

        case (DXVK_TSS_TCI_CAMERASPACENORMAL >> TCIOffset):
          transformed = outNrm;
          count = 4;
          break;

        case (DXVK_TSS_TCI_CAMERASPACEPOSITION >> TCIOffset):
          transformed = m_module.opCompositeInsert(m_vec4Type, m_module.constf32(1.0f), vtx, 1, &wIndex);
          count = 4;
          break;

        case (DXVK_TSS_TCI_CAMERASPACEREFLECTIONVECTOR >> TCIOffset): {
          uint32_t vtx3 = m_module.opVectorShuffle(m_vec3Type, vtx, vtx, 3, indices.data());
          vtx3 = m_module.opNormalize(m_vec3Type, vtx3);
          
          uint32_t reflection = m_module.opReflect(m_vec3Type, vtx3, normal);

          std::array<uint32_t, 4> transformIndices;
          for (uint32_t i = 0; i < 3; i++)
            transformIndices[i] = m_module.opCompositeExtract(m_floatType, reflection, 1, &i);
          transformIndices[3] = m_module.constf32(1.0f);

          transformed = m_module.opCompositeConstruct(m_vec4Type, transformIndices.size(), transformIndices.data());
          count = 4;
          break;
        }

        case (DXVK_TSS_TCI_SPHEREMAP >> TCIOffset): {
          uint32_t vtx3 = m_module.opVectorShuffle(m_vec3Type, vtx, vtx, 3, indices.data());
          vtx3 = m_module.opNormalize(m_vec3Type, vtx3);

          uint32_t reflection = m_module.opReflect(m_vec3Type, vtx3, normal);
          uint32_t m = m_module.opFAdd(m_vec3Type, reflection, m_module.constvec3f32(0, 0, 1));
          m = m_module.opLength(m_floatType, m);
          m = m_module.opFMul(m_floatType, m, m_module.constf32(2.0f));

          std::array<uint32_t, 4> transformIndices;
          for (uint32_t i = 0; i < 2; i++) {
            transformIndices[i] = m_module.opCompositeExtract(m_floatType, reflection, 1, &i);
            transformIndices[i] = m_module.opFDiv(m_floatType, transformIndices[i], m);
            transformIndices[i] = m_module.opFAdd(m_floatType, transformIndices[i], m_module.constf32(0.5f));
          }

          transformIndices[2] = m_module.constf32(0.0f);
          transformIndices[3] = m_module.constf32(1.0f);

          transformed = m_module.opCompositeConstruct(m_vec4Type, transformIndices.size(), transformIndices.data());
          count = 4;
          break;
        }
      }

      uint32_t type = flags;
      if (type != D3DTTFF_DISABLE) {
        if (!m_vsKey.Data.Contents.HasPositionT) {
          for (uint32_t j = count; j < 4; j++) {
            // If we're outside the component count of the vertex decl for this texcoord then we pad with zeroes.
            // Otherwise, pad with ones.

            // Very weird quirk in order to get texcoord transforms to work like they do in native.
            // In future, maybe we could sort this out properly by chopping matrices of different sizes, but thats
            // a project for another day.
            uint32_t texcoordCount = (m_vsKey.Data.Contents.TexcoordDeclMask >> (3 * inputIndex)) & 0x7;
            uint32_t value = j > texcoordCount ? m_module.constf32(0) : m_module.constf32(1);
            transformed = m_module.opCompositeInsert(m_vec4Type, value, transformed, 1, &j);
          }

          transformed = m_module.opVectorTimesMatrix(m_vec4Type, transformed, m_vs.constants.texcoord[i]);
        }

        // Pad the unused section of it with the value for projection.
        uint32_t lastIdx = count - 1;
        uint32_t projValue = m_module.opCompositeExtract(m_floatType, transformed, 1, &lastIdx);

        for (uint32_t j = count; j < 4; j++)
          transformed = m_module.opCompositeInsert(m_vec4Type, projValue, transformed, 1, &j);
      }

      m_module.opStore(m_vs.out.TEXCOORD[i], transformed);
    }

    if (m_vsKey.Data.Contents.UseLighting) {
      auto PickSource = [&](uint32_t Source, uint32_t Material) {
        if (Source == D3DMCS_MATERIAL)
          return Material;
        else if (Source == D3DMCS_COLOR1)
          return m_vs.in.COLOR[0];
        else
          return m_vs.in.COLOR[1];
      };

      uint32_t diffuseValue  = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);
      uint32_t specularValue = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);
      uint32_t ambientValue  = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);

      for (uint32_t i = 0; i < m_vsKey.Data.Contents.LightCount; i++) {
        uint32_t light_ptr_t = m_module.defPointerType(m_vs.lightType, spv::StorageClassUniform);

        uint32_t indexVal = m_module.constu32(uint32_t(D3D9FFVSMembers::Light0) + i);
        uint32_t lightPtr = m_module.opAccessChain(light_ptr_t, m_vs.constantBuffer, 1, &indexVal);

        auto LoadLightItem = [&](uint32_t type, uint32_t idx) {
          uint32_t typePtr = m_module.defPointerType(type, spv::StorageClassUniform);

          idx = m_module.constu32(idx);

          return m_module.opLoad(type,
            m_module.opAccessChain(typePtr, lightPtr, 1, &idx));
        };

        uint32_t diffuse   = LoadLightItem(m_vec4Type,   0);
        uint32_t specular  = LoadLightItem(m_vec4Type,   1);
        uint32_t ambient   = LoadLightItem(m_vec4Type,   2);
        uint32_t position  = LoadLightItem(m_vec4Type,   3);
        uint32_t direction = LoadLightItem(m_vec4Type,   4);
        uint32_t type      = LoadLightItem(m_uint32Type, 5);
        uint32_t range     = LoadLightItem(m_floatType,  6);
        uint32_t falloff   = LoadLightItem(m_floatType,  7);
        uint32_t atten0    = LoadLightItem(m_floatType,  8);
        uint32_t atten1    = LoadLightItem(m_floatType,  9);
        uint32_t atten2    = LoadLightItem(m_floatType,  10);
        uint32_t theta     = LoadLightItem(m_floatType, 11);
        uint32_t phi       = LoadLightItem(m_floatType, 12);

        uint32_t bool_t  = m_module.defBoolType();
        uint32_t bool3_t = m_module.defVectorType(bool_t, 3);

        uint32_t isSpot        = m_module.opIEqual(bool_t, type, m_module.constu32(D3DLIGHT_SPOT));
        uint32_t isDirectional = m_module.opIEqual(bool_t, type, m_module.constu32(D3DLIGHT_DIRECTIONAL));

        std::array<uint32_t, 3> members = { isDirectional, isDirectional, isDirectional };

        uint32_t isDirectional3 = m_module.opCompositeConstruct(bool3_t, members.size(), members.data());

        uint32_t vtx3      = m_module.opVectorShuffle(m_vec3Type, vtx, vtx, 3, indices.data());
                 position  = m_module.opVectorShuffle(m_vec3Type, position, position, 3, indices.data());
                 direction = m_module.opVectorShuffle(m_vec3Type, direction, direction, 3, indices.data());

        uint32_t delta  = m_module.opFSub(m_vec3Type, position, vtx3);
        uint32_t d      = m_module.opLength(m_floatType, delta);
        uint32_t hitDir = m_module.opFNegate(m_vec3Type, direction);
                 hitDir = m_module.opSelect(m_vec3Type, isDirectional3, hitDir, delta);
                 hitDir = m_module.opNormalize(m_vec3Type, hitDir);

        uint32_t atten  = m_module.opFFma  (m_floatType, d, atten2, atten1);
                 atten  = m_module.opFFma  (m_floatType, d, atten,  atten0);
                 atten  = m_module.opFDiv  (m_floatType, m_module.constf32(1.0f), atten);
                 atten  = m_module.opNMin  (m_floatType, atten, m_module.constf32(FLT_MAX));

                 atten  = m_module.opSelect(m_floatType, m_module.opFOrdGreaterThan(bool_t, d, range), m_module.constf32(0.0f), atten);
                 atten  = m_module.opSelect(m_floatType, isDirectional, m_module.constf32(1.0f), atten);

        // Spot Lighting
        {
          uint32_t rho        = m_module.opDot (m_floatType, m_module.opFNegate(m_vec3Type, hitDir), direction);
          uint32_t spotAtten  = m_module.opFSub(m_floatType, rho, phi);
                   spotAtten  = m_module.opFDiv(m_floatType, spotAtten, m_module.opFSub(m_floatType, theta, phi));
                   spotAtten  = m_module.opPow (m_floatType, spotAtten, falloff);

          uint32_t insideThetaAndPhi = m_module.opFOrdLessThanEqual(bool_t, rho, theta);
          uint32_t insidePhi         = m_module.opFOrdGreaterThan(bool_t, rho, phi);
                   spotAtten  = m_module.opSelect(m_floatType, insidePhi,         spotAtten, m_module.constf32(0.0f));
                   spotAtten  = m_module.opSelect(m_floatType, insideThetaAndPhi, spotAtten, m_module.constf32(1.0f));
                   spotAtten  = m_module.opFClamp(m_floatType, spotAtten, m_module.constf32(0.0f), m_module.constf32(1.0f));

                   spotAtten = m_module.opFMul(m_floatType, atten, spotAtten);
                   atten     = m_module.opSelect(m_floatType, isSpot, spotAtten, atten);
        }


        uint32_t hitDot = m_module.opDot(m_floatType, normal, hitDir);
                 hitDot = m_module.opFClamp(m_floatType, hitDot, m_module.constf32(0.0f), m_module.constf32(1.0f));

        uint32_t diffuseness = m_module.opFMul(m_floatType, hitDot, atten);

        uint32_t mid;
        if (m_vsKey.Data.Contents.LocalViewer) {
          mid = m_module.opNormalize(m_vec3Type, vtx3);
          mid = m_module.opFSub(m_vec3Type, hitDir, mid);
        }
        else
          mid = m_module.opFSub(m_vec3Type, hitDir, m_module.constvec3f32(0.0f, 0.0f, 1.0f));

        mid = m_module.opNormalize(m_vec3Type, mid);

        uint32_t midDot = m_module.opDot(m_floatType, normal, mid);
                 midDot = m_module.opFClamp(m_floatType, midDot, m_module.constf32(0.0f), m_module.constf32(1.0f));
        uint32_t doSpec = m_module.opFOrdGreaterThan(bool_t, midDot, m_module.constf32(0.0f));
        uint32_t specularness = m_module.opPow(m_floatType, midDot, m_vs.constants.materialPower);
                 specularness = m_module.opFMul(m_floatType, specularness, atten);
                 specularness = m_module.opSelect(m_floatType, doSpec, specularness, m_module.constf32(0.0f));

        uint32_t lightAmbient  = m_module.opVectorTimesScalar(m_vec4Type, ambient,  atten);
        uint32_t lightDiffuse  = m_module.opVectorTimesScalar(m_vec4Type, diffuse,  diffuseness);
        uint32_t lightSpecular = m_module.opVectorTimesScalar(m_vec4Type, specular, specularness);

        ambientValue  = m_module.opFAdd(m_vec4Type, ambientValue,  lightAmbient);
        diffuseValue  = m_module.opFAdd(m_vec4Type, diffuseValue,  lightDiffuse);
        specularValue = m_module.opFAdd(m_vec4Type, specularValue, lightSpecular);
      }

      uint32_t mat_diffuse  = PickSource(m_vsKey.Data.Contents.DiffuseSource,  m_vs.constants.materialDiffuse);
      uint32_t mat_ambient  = PickSource(m_vsKey.Data.Contents.AmbientSource,  m_vs.constants.materialAmbient);
      uint32_t mat_emissive = PickSource(m_vsKey.Data.Contents.EmissiveSource, m_vs.constants.materialEmissive);
      uint32_t mat_specular = PickSource(m_vsKey.Data.Contents.SpecularSource, m_vs.constants.materialSpecular);
      
      std::array<uint32_t, 4> alphaSwizzle = {0, 1, 2, 7};
      uint32_t finalColor0 = m_module.opFFma(m_vec4Type, mat_ambient, m_vs.constants.globalAmbient, mat_emissive);
               finalColor0 = m_module.opFFma(m_vec4Type, mat_ambient, ambientValue, finalColor0);
               finalColor0 = m_module.opFFma(m_vec4Type, mat_diffuse, diffuseValue, finalColor0);
               finalColor0 = m_module.opVectorShuffle(m_vec4Type, finalColor0, mat_diffuse, alphaSwizzle.size(), alphaSwizzle.data());

      uint32_t finalColor1 = m_module.opFMul(m_vec4Type, mat_specular, specularValue);

      // Saturate
      finalColor0 = m_module.opFClamp(m_vec4Type, finalColor0,
        m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f),
        m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f));

      finalColor1 = m_module.opFClamp(m_vec4Type, finalColor1,
        m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f),
        m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f));

      m_module.opStore(m_vs.out.COLOR[0], finalColor0);
      m_module.opStore(m_vs.out.COLOR[1], finalColor1);
    }
    else {
      m_module.opStore(m_vs.out.COLOR[0], m_vs.in.COLOR[0]);
      m_module.opStore(m_vs.out.COLOR[1], m_vs.in.COLOR[1]);
    }

    D3D9FogContext fogCtx;
    fogCtx.IsPixel     = false;
    fogCtx.RangeFog    = m_vsKey.Data.Contents.RangeFog;
    fogCtx.RenderState = m_rsBlock;
    fogCtx.vPos        = vtx;
    fogCtx.HasFogInput = m_vsKey.Data.Contents.HasFog;
    fogCtx.vFog        = m_vs.in.FOG;
    fogCtx.oColor      = 0;
    fogCtx.IsFixedFunction = true;
    fogCtx.IsPositionT = m_vsKey.Data.Contents.HasPositionT;
    fogCtx.HasSpecular = m_vsKey.Data.Contents.HasColor1;
    fogCtx.Specular    = m_vs.in.COLOR[1];
    fogCtx.SpecUBO     = m_specUbo;
    m_module.opStore(m_vs.out.FOG, DoFixedFunctionFog(m_spec, m_module, fogCtx));

    auto pointInfo = GetPointSizeInfoVS(m_spec, m_module, 0, vtx, m_vs.in.POINTSIZE, m_rsBlock, m_specUbo, true);

    uint32_t pointSize = m_module.opFClamp(m_floatType, pointInfo.defaultValue, pointInfo.min, pointInfo.max);
    m_module.opStore(m_vs.out.POINTSIZE, pointSize);

    if (m_vsKey.Data.Contents.VertexClipping)
      emitVsClipping(vtx);
  }


  void D3D9FFShaderCompiler::setupRenderStateInfo() {
    uint32_t count;

    if (m_programType == DxsoProgramType::PixelShader) {
      m_pushConstOffset = 0;
      m_pushConstSize   = offsetof(D3D9RenderStateInfo, pointSize);
      count = 5;
    }
    else {
      m_pushConstOffset = offsetof(D3D9RenderStateInfo, pointSize);
      m_pushConstSize   = sizeof(float) * 6;
      count = 11;
    }

    m_rsBlock = SetupRenderStateBlock(m_module, count);
  }


  void D3D9FFShaderCompiler::emitLightTypeDecl() {
    std::array<uint32_t, 13> light_members = {
      m_vec4Type,   // Diffuse
      m_vec4Type,   // Specular
      m_vec4Type,   // Ambient
      m_vec4Type,   // Position
      m_vec4Type,   // Direction
      m_uint32Type, // Type
      m_floatType,  // Range
      m_floatType,  // Falloff
      m_floatType,  // Attenuation0
      m_floatType,  // Attenuation1
      m_floatType,  // Attenuation2
      m_floatType,  // Theta
      m_floatType,  // Phi
    };

    m_vs.lightType =
      m_module.defStructType(light_members.size(), light_members.data());

    m_module.setDebugName(m_vs.lightType, "light_t");

    uint32_t offset = 0;

    m_module.memberDecorateOffset(m_vs.lightType, 0, offset);  offset += 4 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 0, "Diffuse");
    m_module.memberDecorateOffset(m_vs.lightType, 1, offset);  offset += 4 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 1, "Specular");
    m_module.memberDecorateOffset(m_vs.lightType, 2, offset);  offset += 4 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 2, "Ambient");

    m_module.memberDecorateOffset(m_vs.lightType, 3, offset);  offset += 4 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 3, "Position");
    m_module.memberDecorateOffset(m_vs.lightType, 4, offset);  offset += 4 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 4, "Direction");

    m_module.memberDecorateOffset(m_vs.lightType, 5, offset);  offset += 1 * sizeof(uint32_t);
    m_module.setDebugMemberName  (m_vs.lightType, 5, "Type");

    m_module.memberDecorateOffset(m_vs.lightType, 6, offset);  offset += 1 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 6, "Range");
    m_module.memberDecorateOffset(m_vs.lightType, 7, offset);  offset += 1 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 7, "Falloff");

    m_module.memberDecorateOffset(m_vs.lightType, 8, offset);  offset += 1 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 8, "Attenuation0");
    m_module.memberDecorateOffset(m_vs.lightType, 9, offset);  offset += 1 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 9, "Attenuation1");
    m_module.memberDecorateOffset(m_vs.lightType, 10, offset); offset += 1 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 10, "Attenuation2");

    m_module.memberDecorateOffset(m_vs.lightType, 11, offset); offset += 1 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 11, "Theta");
    m_module.memberDecorateOffset(m_vs.lightType, 12, offset); offset += 1 * sizeof(float);
    m_module.setDebugMemberName  (m_vs.lightType, 12, "Phi");
  }


  void D3D9FFShaderCompiler::emitBaseBufferDecl() {
    // Constant Buffer for VS.
    std::array<uint32_t, uint32_t(D3D9FFVSMembers::MemberCount)> members = {
      m_mat4Type, // World
      m_mat4Type, // View
      m_mat4Type, // InverseView
      m_mat4Type, // Proj

      m_mat4Type, // Texture0
      m_mat4Type, // Texture1
      m_mat4Type, // Texture2
      m_mat4Type, // Texture3
      m_mat4Type, // Texture4
      m_mat4Type, // Texture5
      m_mat4Type, // Texture6
      m_mat4Type, // Texture7

      m_vec4Type, // Inverse Offset
      m_vec4Type, // Inverse Extent

      m_vec4Type, // Global Ambient

      m_vs.lightType, // Light0
      m_vs.lightType, // Light1
      m_vs.lightType, // Light2
      m_vs.lightType, // Light3
      m_vs.lightType, // Light4
      m_vs.lightType, // Light5
      m_vs.lightType, // Light6
      m_vs.lightType, // Light7

      m_vec4Type,  // Material Diffuse
      m_vec4Type,  // Material Ambient
      m_vec4Type,  // Material Specular
      m_vec4Type,  // Material Emissive
      m_floatType, // Material Power

      m_floatType, // Tween Factor
    };

    const uint32_t structType =
      m_module.defStructType(members.size(), members.data());

    m_module.decorateBlock(structType);

    uint32_t offset = 0;

    for (uint32_t i = 0; i < uint32_t(D3D9FFVSMembers::InverseOffset); i++) {
      m_module.memberDecorateOffset(structType, i, offset);
      offset += sizeof(Matrix4);
      m_module.memberDecorateMatrixStride(structType, i, 16);
      m_module.memberDecorate(structType, i, spv::DecorationRowMajor);
    }

    for (uint32_t i = uint32_t(D3D9FFVSMembers::InverseOffset); i < uint32_t(D3D9FFVSMembers::Light0); i++) {
      m_module.memberDecorateOffset(structType, i, offset);
      offset += sizeof(Vector4);
    }

    for (uint32_t i = 0; i < caps::MaxEnabledLights; i++) {
      m_module.memberDecorateOffset(structType, uint32_t(D3D9FFVSMembers::Light0) + i, offset);
      offset += sizeof(D3D9Light);
    }

    for (uint32_t i = uint32_t(D3D9FFVSMembers::MaterialDiffuse); i < uint32_t(D3D9FFVSMembers::MaterialPower); i++) {
      m_module.memberDecorateOffset(structType, i, offset);
      offset += sizeof(Vector4);
    }

    m_module.memberDecorateOffset(structType, uint32_t(D3D9FFVSMembers::MaterialPower), offset);
    offset += sizeof(float);

    m_module.memberDecorateOffset(structType, uint32_t(D3D9FFVSMembers::TweenFactor), offset);
    offset += sizeof(float);

    m_module.setDebugName(structType, "D3D9FixedFunctionVS");
    uint32_t member = 0;
    m_module.setDebugMemberName(structType, member++, "WorldView");
    m_module.setDebugMemberName(structType, member++, "Normal");
    m_module.setDebugMemberName(structType, member++, "InverseView");
    m_module.setDebugMemberName(structType, member++, "Projection");

    m_module.setDebugMemberName(structType, member++, "TexcoordTransform0");
    m_module.setDebugMemberName(structType, member++, "TexcoordTransform1");
    m_module.setDebugMemberName(structType, member++, "TexcoordTransform2");
    m_module.setDebugMemberName(structType, member++, "TexcoordTransform3");
    m_module.setDebugMemberName(structType, member++, "TexcoordTransform4");
    m_module.setDebugMemberName(structType, member++, "TexcoordTransform5");
    m_module.setDebugMemberName(structType, member++, "TexcoordTransform6");
    m_module.setDebugMemberName(structType, member++, "TexcoordTransform7");

    m_module.setDebugMemberName(structType, member++, "ViewportInfo_InverseOffset");
    m_module.setDebugMemberName(structType, member++, "ViewportInfo_InverseExtent");

    m_module.setDebugMemberName(structType, member++, "GlobalAmbient");

    m_module.setDebugMemberName(structType, member++, "Light0");
    m_module.setDebugMemberName(structType, member++, "Light1");
    m_module.setDebugMemberName(structType, member++, "Light2");
    m_module.setDebugMemberName(structType, member++, "Light3");
    m_module.setDebugMemberName(structType, member++, "Light4");
    m_module.setDebugMemberName(structType, member++, "Light5");
    m_module.setDebugMemberName(structType, member++, "Light6");
    m_module.setDebugMemberName(structType, member++, "Light7");

    m_module.setDebugMemberName(structType, member++, "Material_Diffuse");
    m_module.setDebugMemberName(structType, member++, "Material_Ambient");
    m_module.setDebugMemberName(structType, member++, "Material_Specular");
    m_module.setDebugMemberName(structType, member++, "Material_Emissive");
    m_module.setDebugMemberName(structType, member++, "Material_Power");

    m_module.setDebugMemberName(structType, member++, "TweenFactor");

    m_vs.constantBuffer = m_module.newVar(
      m_module.defPointerType(structType, spv::StorageClassUniform),
      spv::StorageClassUniform);

    m_module.setDebugName(m_vs.constantBuffer, "consts");

    const uint32_t bindingId = computeResourceSlotId(
      DxsoProgramType::VertexShader, DxsoBindingType::ConstantBuffer,
      DxsoConstantBuffers::VSFixedFunction);

    m_module.decorateDescriptorSet(m_vs.constantBuffer, 0);
    m_module.decorateBinding(m_vs.constantBuffer, bindingId);

    DxvkBindingInfo binding = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
    binding.resourceBinding = bindingId;
    binding.viewType        = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    binding.access          = VK_ACCESS_UNIFORM_READ_BIT;
    binding.uboSet          = VK_TRUE;
    m_bindings.push_back(binding);
  }


  void D3D9FFShaderCompiler::emitVertexBlendDecl() {
    const uint32_t arrayType = m_module.defRuntimeArrayTypeUnique(m_mat4Type);
    m_module.decorateArrayStride(arrayType, sizeof(Matrix4));

    const uint32_t structType = m_module.defStructTypeUnique(1, &arrayType);

    m_module.memberDecorateMatrixStride(structType, 0, 16);
    m_module.memberDecorate(structType, 0, spv::DecorationRowMajor);

    m_module.decorate(structType, spv::DecorationBufferBlock);

    m_module.memberDecorateOffset(structType, 0, 0);

    m_module.setDebugName(structType, "D3D9FF_VertexBlendData");
    m_module.setDebugMemberName(structType, 0, "WorldViewArray");

    m_vs.vertexBlendData = m_module.newVar(
      m_module.defPointerType(structType, spv::StorageClassUniform),
      spv::StorageClassUniform);

    m_module.setDebugName(m_vs.vertexBlendData, "VertexBlendData");

    const uint32_t bindingId = computeResourceSlotId(
      DxsoProgramType::VertexShader, DxsoBindingType::ConstantBuffer,
      DxsoConstantBuffers::VSVertexBlendData);

    m_module.decorateDescriptorSet(m_vs.vertexBlendData, 0);
    m_module.decorateBinding(m_vs.vertexBlendData, bindingId);

    m_module.decorate(m_vs.vertexBlendData, spv::DecorationNonWritable);

    DxvkBindingInfo binding = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };
    binding.resourceBinding = bindingId;
    binding.viewType        = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    binding.access          = VK_ACCESS_SHADER_READ_BIT;
    binding.uboSet          = VK_TRUE;
    m_bindings.push_back(binding);
  }


  void D3D9FFShaderCompiler::setupVS() {
    setupRenderStateInfo();
    m_specUbo = SetupSpecUBO(m_module, m_bindings);

    // VS Caps
    m_module.enableCapability(spv::CapabilityClipDistance);

    emitLightTypeDecl();
    emitBaseBufferDecl();

    if (m_vsKey.Data.Contents.VertexBlendMode == D3D9FF_VertexBlendMode_Normal)
      emitVertexBlendDecl();

    // Load constants
    auto LoadConstant = [&](uint32_t type, uint32_t idx) {
      uint32_t offset  = m_module.constu32(idx);
      uint32_t typePtr = m_module.defPointerType(type, spv::StorageClassUniform);

      return m_module.opLoad(type,
        m_module.opAccessChain(typePtr, m_vs.constantBuffer, 1, &offset));
    };

    m_vs.constants.worldview = LoadConstant(m_mat4Type, uint32_t(D3D9FFVSMembers::WorldViewMatrix));
    m_vs.constants.normal    = LoadConstant(m_mat4Type, uint32_t(D3D9FFVSMembers::NormalMatrix));
    m_vs.constants.inverseView = LoadConstant(m_mat4Type, uint32_t(D3D9FFVSMembers::InverseViewMatrix));
    m_vs.constants.proj      = LoadConstant(m_mat4Type, uint32_t(D3D9FFVSMembers::ProjMatrix));

    for (uint32_t i = 0; i < caps::TextureStageCount; i++)
      m_vs.constants.texcoord[i] = LoadConstant(m_mat4Type, uint32_t(D3D9FFVSMembers::Texcoord0) + i);

    m_vs.constants.invOffset = LoadConstant(m_vec4Type, uint32_t(D3D9FFVSMembers::InverseOffset));
    m_vs.constants.invExtent = LoadConstant(m_vec4Type, uint32_t(D3D9FFVSMembers::InverseExtent));

    m_vs.constants.globalAmbient = LoadConstant(m_vec4Type, uint32_t(D3D9FFVSMembers::GlobalAmbient));

    m_vs.constants.materialDiffuse  = LoadConstant(m_vec4Type,  uint32_t(D3D9FFVSMembers::MaterialDiffuse));
    m_vs.constants.materialAmbient  = LoadConstant(m_vec4Type,  uint32_t(D3D9FFVSMembers::MaterialAmbient));
    m_vs.constants.materialSpecular = LoadConstant(m_vec4Type,  uint32_t(D3D9FFVSMembers::MaterialSpecular));
    m_vs.constants.materialEmissive = LoadConstant(m_vec4Type,  uint32_t(D3D9FFVSMembers::MaterialEmissive));
    m_vs.constants.materialPower    = LoadConstant(m_floatType, uint32_t(D3D9FFVSMembers::MaterialPower));
    m_vs.constants.tweenFactor      = LoadConstant(m_floatType, uint32_t(D3D9FFVSMembers::TweenFactor));

    // Do IO
    m_vs.in.POSITION  = declareIO(true, DxsoSemantic{ DxsoUsage::Position, 0 });
    m_vs.in.NORMAL    = declareIO(true, DxsoSemantic{ DxsoUsage::Normal, 0 });

    if (m_vsKey.Data.Contents.VertexBlendMode == D3D9FF_VertexBlendMode_Tween) {
      m_vs.in.POSITION1 = declareIO(true, DxsoSemantic{ DxsoUsage::Position, 1 });
      m_vs.in.NORMAL1   = declareIO(true, DxsoSemantic{ DxsoUsage::Normal, 1 });
    }
    else {
      m_isgn.elemCount++;
      m_isgn.elemCount++;
    }

    for (uint32_t i = 0; i < caps::TextureStageCount; i++)
      m_vs.in.TEXCOORD[i] = declareIO(true, DxsoSemantic{ DxsoUsage::Texcoord, i });

    if (m_vsKey.Data.Contents.HasColor0)
      m_vs.in.COLOR[0] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 0 });
    else {
      m_vs.in.COLOR[0] = m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f);
      m_isgn.elemCount++;
    }

    if (m_vsKey.Data.Contents.HasColor1)
      m_vs.in.COLOR[1] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 1 });
    else {
      m_vs.in.COLOR[1] = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);
      m_isgn.elemCount++;
    }

    if (m_vsKey.Data.Contents.HasFog)
      m_vs.in.FOG = declareIO(true, DxsoSemantic{ DxsoUsage::Fog,   0 });
    else
      m_isgn.elemCount++;

    if (m_vsKey.Data.Contents.HasPointSize)
      m_vs.in.POINTSIZE = declareIO(true, DxsoSemantic{ DxsoUsage::PointSize, 0 });
    else
      m_isgn.elemCount++;

    if (m_vsKey.Data.Contents.VertexBlendMode == D3D9FF_VertexBlendMode_Normal) {
      m_vs.in.BLENDWEIGHT  = declareIO(true, DxsoSemantic{ DxsoUsage::BlendWeight, 0 });
      m_vs.in.BLENDINDICES = declareIO(true, DxsoSemantic{ DxsoUsage::BlendIndices, 0 });
    }
    else {
      m_isgn.elemCount++;
      m_isgn.elemCount++;
    }

    // Declare Outputs
    m_vs.out.POSITION = declareIO(false, DxsoSemantic{ DxsoUsage::Position, 0 }, spv::BuiltInPosition);
    if (m_options.invariantPosition)
      m_module.decorate(m_vs.out.POSITION, spv::DecorationInvariant);

    m_vs.out.POINTSIZE = declareIO(false, DxsoSemantic{ DxsoUsage::PointSize, 0 }, spv::BuiltInPointSize);

    m_vs.out.NORMAL   = declareIO(false, DxsoSemantic{ DxsoUsage::Normal, 0 });

    for (uint32_t i = 0; i < caps::TextureStageCount; i++)
      m_vs.out.TEXCOORD[i] = declareIO(false, DxsoSemantic{ DxsoUsage::Texcoord, i });

    m_vs.out.COLOR[0] = declareIO(false, DxsoSemantic{ DxsoUsage::Color, 0 });
    m_vs.out.COLOR[1] = declareIO(false, DxsoSemantic{ DxsoUsage::Color, 1 });

    m_vs.out.FOG      = declareIO(false, DxsoSemantic{ DxsoUsage::Fog,   0 });
  }


  void D3D9FFShaderCompiler::compilePS() {
    setupPS();

    uint32_t diffuse  = m_ps.in.COLOR[0];
    uint32_t specular = m_ps.in.COLOR[1];

    // Current starts of as equal to diffuse.
    uint32_t current = diffuse;
    // Temp starts off as equal to vec4(0)
    uint32_t temp  = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f);

    uint32_t texture = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 1.0f);

    uint32_t unboundTextureConstId = m_module.constvec4f32(0.0f, 0.0f, 0.0f, 1.0f);

    for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
      const auto& stage = m_fsKey.Stages[i].Contents;

      bool processedTexture = false;

      auto DoBumpmapCoords = [&](uint32_t typeId, uint32_t baseCoords) {
        uint32_t stage = i - 1;

        uint32_t coords = baseCoords;
        for (uint32_t i = 0; i < 2; i++) {
          std::array<uint32_t, 4> indices = { 0, 1, 2, 3 };

          uint32_t tc_m_n = m_module.opCompositeExtract(m_floatType, coords, 1, &i);

          uint32_t offset = m_module.constu32(D3D9SharedPSStages_Count * stage + D3D9SharedPSStages_BumpEnvMat0 + i);
          uint32_t bm     = m_module.opAccessChain(m_module.defPointerType(m_vec2Type, spv::StorageClassUniform),
                                                   m_ps.sharedState, 1, &offset);
                   bm     = m_module.opLoad(m_vec2Type, bm);

          uint32_t t      = m_module.opVectorShuffle(m_vec2Type, texture, texture, 2, indices.data());

          uint32_t dot    = m_module.opDot(m_floatType, bm, t);

          uint32_t result = m_module.opFAdd(m_floatType, tc_m_n, dot);
          coords  = m_module.opCompositeInsert(typeId, result, coords, 1, &i);
        }

        return coords;
      };

      auto GetTexture = [&]() {
        if (!processedTexture) {
          SpirvImageOperands imageOperands;
          uint32_t imageVarId = m_module.opLoad(m_ps.samplers[i].typeId, m_ps.samplers[i].varId);

          uint32_t texcoordCnt = m_ps.samplers[i].texcoordCnt;

          // Add one for the texcoord count
          // if we need to include the divider
          if (m_fsKey.Stages[i].Contents.Projected)
            texcoordCnt++;

          std::array<uint32_t, 4> indices = { 0, 1, 2, 3 };

          uint32_t texcoord   = m_ps.in.TEXCOORD[i];
          uint32_t texcoord_t = m_module.defVectorType(m_floatType, texcoordCnt);
          texcoord = m_module.opVectorShuffle(texcoord_t,
            texcoord, texcoord, texcoordCnt, indices.data());

          uint32_t projIdx = m_fsKey.Stages[i].Contents.ProjectedCount;
          if (projIdx == 0 || projIdx > texcoordCnt) {
            projIdx = 4; // Always use w if ProjectedCount is 0.
          }
          --projIdx;

          uint32_t projValue = 0;

          if (m_fsKey.Stages[i].Contents.Projected) {
            projValue = m_module.opCompositeExtract(m_floatType, m_ps.in.TEXCOORD[i], 1, &projIdx);
            uint32_t insertIdx = texcoordCnt - 1;
            texcoord = m_module.opCompositeInsert(texcoord_t, projValue, texcoord, 1, &insertIdx);
          }

          bool shouldProject = m_fsKey.Stages[i].Contents.Projected;

          if (i != 0 && (
            m_fsKey.Stages[i - 1].Contents.ColorOp == D3DTOP_BUMPENVMAP ||
            m_fsKey.Stages[i - 1].Contents.ColorOp == D3DTOP_BUMPENVMAPLUMINANCE)) {
            if (shouldProject) {
              uint32_t projRcp = m_module.opFDiv(m_floatType, m_module.constf32(1.0), projValue);
              texcoord = m_module.opVectorTimesScalar(texcoord_t, texcoord, projRcp);
            }

            texcoord = DoBumpmapCoords(texcoord_t, texcoord);

            shouldProject = false;
          }

          if (shouldProject)
            texture = m_module.opImageSampleProjImplicitLod(m_vec4Type, imageVarId, texcoord, imageOperands);
          else
            texture = m_module.opImageSampleImplicitLod(m_vec4Type, imageVarId, texcoord, imageOperands);

          if (i != 0 && m_fsKey.Stages[i - 1].Contents.ColorOp == D3DTOP_BUMPENVMAPLUMINANCE) {
            uint32_t index = m_module.constu32(D3D9SharedPSStages_Count * (i - 1) + D3D9SharedPSStages_BumpEnvLScale);
            uint32_t lScale = m_module.opAccessChain(m_module.defPointerType(m_floatType, spv::StorageClassUniform),
                                                     m_ps.sharedState, 1, &index);
                     lScale = m_module.opLoad(m_floatType, lScale);

                     index = m_module.constu32(D3D9SharedPSStages_Count * (i - 1) + D3D9SharedPSStages_BumpEnvLOffset);
            uint32_t lOffset = m_module.opAccessChain(m_module.defPointerType(m_floatType, spv::StorageClassUniform),
                                                     m_ps.sharedState, 1, &index);
                     lOffset = m_module.opLoad(m_floatType, lOffset);
            
            uint32_t zIndex = 2;
            uint32_t scale = m_module.opCompositeExtract(m_floatType, texture, 1, &zIndex);
                     scale = m_module.opFMul(m_floatType, scale, lScale);
                     scale = m_module.opFAdd(m_floatType, scale, lOffset);
                     scale = m_module.opFClamp(m_floatType, scale, m_module.constf32(0.0f), m_module.constf32(1.0));

            texture = m_module.opVectorTimesScalar(m_vec4Type, texture, scale);
          }
        }

        processedTexture = true;

        return texture;
      };

      auto ScalarReplicate = [&](uint32_t reg) {
        std::array<uint32_t, 4> replicant = { reg, reg, reg, reg };
        return m_module.opCompositeConstruct(m_vec4Type, replicant.size(), replicant.data());
      };

      auto AlphaReplicate = [&](uint32_t reg) {
        uint32_t alphaComponentId = 3;
        uint32_t alpha = m_module.opCompositeExtract(m_floatType, reg, 1, &alphaComponentId);

        return ScalarReplicate(alpha);
      };

      auto Complement = [&](uint32_t reg) {
        return m_module.opFSub(m_vec4Type,
          m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f),
          reg);
      };

      auto Saturate = [&](uint32_t reg) {
        return m_module.opFClamp(m_vec4Type, reg,
          m_module.constvec4f32(0.0f, 0.0f, 0.0f, 0.0f),
          m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f));
      };

      auto GetArg = [&] (uint32_t arg) {
        uint32_t reg = m_module.constvec4f32(1.0f, 1.0f, 1.0f, 1.0f);

        switch (arg & D3DTA_SELECTMASK) {
          case D3DTA_CONSTANT: {
            uint32_t offset = m_module.constu32(D3D9SharedPSStages_Count * i + D3D9SharedPSStages_Constant);
            uint32_t ptr    = m_module.opAccessChain(m_module.defPointerType(m_vec4Type, spv::StorageClassUniform),
              m_ps.sharedState, 1, &offset);

            reg = m_module.opLoad(m_vec4Type, ptr);
            break;
          }
          case D3DTA_CURRENT:
            reg = current;
            break;
          case D3DTA_DIFFUSE:
            reg = diffuse;
            break;
          case D3DTA_SPECULAR:
            reg = specular;
            break;
          case D3DTA_TEMP:
            reg = temp;
            break;
          case D3DTA_TEXTURE:
            if (stage.TextureBound != 0) {
              reg = GetTexture();
            } else {
              reg = unboundTextureConstId;
            }
            break;
          case D3DTA_TFACTOR:
            reg = m_ps.constants.textureFactor;
            break;
          default:
            break;
        }

        // reg = 1 - reg
        if (arg & D3DTA_COMPLEMENT)
          reg = Complement(reg);

        // reg = reg.wwww
        if (arg & D3DTA_ALPHAREPLICATE)
          reg = AlphaReplicate(reg);

        return reg;
      };

      auto DoOp = [&](D3DTEXTUREOP op, uint32_t dst, std::array<uint32_t, TextureArgCount> arg) {
        switch (op) {
          case D3DTOP_SELECTARG1:
            dst = arg[1];
            break;

          case D3DTOP_SELECTARG2:
            dst = arg[2];
            break;

          case D3DTOP_MODULATE4X:
            dst = m_module.opFMul(m_vec4Type, arg[1], arg[2]);
            dst = m_module.opVectorTimesScalar(m_vec4Type, dst, m_module.constf32(4.0f));
            dst = Saturate(dst);
            break;

          case D3DTOP_MODULATE2X:
            dst = m_module.opFMul(m_vec4Type, arg[1], arg[2]);
            dst = m_module.opVectorTimesScalar(m_vec4Type, dst, m_module.constf32(2.0f));
            dst = Saturate(dst);
            break;

          case D3DTOP_MODULATE:
            dst = m_module.opFMul(m_vec4Type, arg[1], arg[2]);
            break;

          case D3DTOP_ADDSIGNED2X:
            arg[2] = m_module.opFSub(m_vec4Type, arg[2],
              m_module.constvec4f32(0.5f, 0.5f, 0.5f, 0.5f));

            dst = m_module.opFAdd(m_vec4Type, arg[1], arg[2]);
            dst = m_module.opVectorTimesScalar(m_vec4Type, dst, m_module.constf32(2.0f));
            dst = Saturate(dst);
            break;

          case D3DTOP_ADDSIGNED:
            arg[2] = m_module.opFSub(m_vec4Type, arg[2],
              m_module.constvec4f32(0.5f, 0.5f, 0.5f, 0.5f));

            dst = m_module.opFAdd(m_vec4Type, arg[1], arg[2]);
            dst = Saturate(dst);
            break;

          case D3DTOP_ADD:
            dst = m_module.opFAdd(m_vec4Type, arg[1], arg[2]);
            dst = Saturate(dst);
            break;

          case D3DTOP_SUBTRACT:
            dst = m_module.opFSub(m_vec4Type, arg[1], arg[2]);
            dst = Saturate(dst);
            break;

          case D3DTOP_ADDSMOOTH:
            dst = m_module.opFFma(m_vec4Type, Complement(arg[1]), arg[2], arg[1]);
            dst = Saturate(dst);
            break;

          case D3DTOP_BLENDDIFFUSEALPHA:
            dst = m_module.opFMix(m_vec4Type, arg[2], arg[1], AlphaReplicate(diffuse));
            break;

          case D3DTOP_BLENDTEXTUREALPHA:
            dst = m_module.opFMix(m_vec4Type, arg[2], arg[1], AlphaReplicate(GetTexture()));
            break;

          case D3DTOP_BLENDFACTORALPHA:
            dst = m_module.opFMix(m_vec4Type, arg[2], arg[1], AlphaReplicate(m_ps.constants.textureFactor));
            break;

          case D3DTOP_BLENDTEXTUREALPHAPM:
            dst = m_module.opFFma(m_vec4Type, arg[2], Complement(AlphaReplicate(GetTexture())), arg[1]);
            dst = Saturate(dst);
            break;

          case D3DTOP_BLENDCURRENTALPHA:
            dst = m_module.opFMix(m_vec4Type, arg[2], arg[1], AlphaReplicate(current));
            break;

          case D3DTOP_PREMODULATE:
            Logger::warn("D3DTOP_PREMODULATE: not implemented");
            break;

          case D3DTOP_MODULATEALPHA_ADDCOLOR:
            dst = m_module.opFFma(m_vec4Type, AlphaReplicate(arg[1]), arg[2], arg[1]);
            dst = Saturate(dst);
            break;

          case D3DTOP_MODULATECOLOR_ADDALPHA:
            dst = m_module.opFFma(m_vec4Type, arg[1], arg[2], AlphaReplicate(arg[1]));
            dst = Saturate(dst);
            break;

          case D3DTOP_MODULATEINVALPHA_ADDCOLOR:
            dst = m_module.opFFma(m_vec4Type, Complement(AlphaReplicate(arg[1])), arg[2], arg[1]);
            dst = Saturate(dst);
            break;

          case D3DTOP_MODULATEINVCOLOR_ADDALPHA:
            dst = m_module.opFFma(m_vec4Type, Complement(arg[1]), arg[2], AlphaReplicate(arg[1]));
            dst = Saturate(dst);
            break;

          case D3DTOP_BUMPENVMAPLUMINANCE:
          case D3DTOP_BUMPENVMAP:
            // Load texture for the next stage...
            texture = GetTexture();
            break;

          case D3DTOP_DOTPRODUCT3: {
            // Get vec3 of arg1 & 2
            uint32_t vec3Type = m_module.defVectorType(m_floatType, 3);
            std::array<uint32_t, 3> indices = { 0, 1, 2 };
            arg[1] = m_module.opVectorShuffle(vec3Type, arg[1], arg[1], indices.size(), indices.data());
            arg[2] = m_module.opVectorShuffle(vec3Type, arg[2], arg[2], indices.size(), indices.data());

            // Bias according to spec.
            arg[1] = m_module.opFSub(vec3Type, arg[1], m_module.constvec3f32(0.5f, 0.5f, 0.5f));
            arg[2] = m_module.opFSub(vec3Type, arg[2], m_module.constvec3f32(0.5f, 0.5f, 0.5f));

            // Do the dotting!
            dst = m_module.opDot(m_floatType, arg[1], arg[2]);

            // Multiply by 4 and replicate -> vec4
            dst = m_module.opFMul(m_floatType, dst, m_module.constf32(4.0f));
            dst = ScalarReplicate(dst);

            // Saturate
            dst = Saturate(dst);

            break;
          }

          case D3DTOP_MULTIPLYADD:
            dst = m_module.opFFma(m_vec4Type, arg[1], arg[2], arg[0]);
            dst = Saturate(dst);
            break;

          case D3DTOP_LERP:
            dst = m_module.opFMix(m_vec4Type, arg[2], arg[1], arg[0]);
            break;

          default:
            Logger::warn("Unhandled texture op!");
            break;
        }

        return dst;
      };

      uint32_t& dst = stage.ResultIsTemp ? temp : current;

      D3DTEXTUREOP colorOp = (D3DTEXTUREOP)stage.ColorOp;

      // This cancels all subsequent stages.
      if (colorOp == D3DTOP_DISABLE)
        break;

      std::array<uint32_t, TextureArgCount> colorArgs = {
          stage.ColorArg0,
          stage.ColorArg1,
          stage.ColorArg2};

      D3DTEXTUREOP alphaOp = (D3DTEXTUREOP)stage.AlphaOp;
      std::array<uint32_t, TextureArgCount> alphaArgs = {
          stage.AlphaArg0,
          stage.AlphaArg1,
          stage.AlphaArg2};

      auto ProcessArgs = [&](auto op, auto& args) {
        for (uint32_t& arg : args)
          arg = GetArg(arg);
      };

      // Fast path if alpha/color path is identical.
      // D3DTOP_DOTPRODUCT3 also has special quirky behaviour here.
      const bool fastPath = colorOp == alphaOp && colorArgs == alphaArgs;
      if (fastPath || colorOp == D3DTOP_DOTPRODUCT3) {
        if (colorOp != D3DTOP_DISABLE) {
          ProcessArgs(colorOp, colorArgs);
          dst = DoOp(colorOp, dst, colorArgs);
        }
      }
      else {
        std::array<uint32_t, 4> indices = { 0, 1, 2, 4 + 3 };

        uint32_t colorResult = dst;
        uint32_t alphaResult = dst;
        if (colorOp != D3DTOP_DISABLE) {
          ProcessArgs(colorOp, colorArgs);
          colorResult = DoOp(colorOp, dst, colorArgs);
        }

        if (alphaOp != D3DTOP_DISABLE) {
          ProcessArgs(alphaOp, alphaArgs);
          alphaResult = DoOp(alphaOp, dst, alphaArgs);
        }

        // src0.x, src0.y, src0.z src1.w
        if (colorResult != dst)
          dst = m_module.opVectorShuffle(m_vec4Type, colorResult, dst, indices.size(), indices.data());

        // src0.x, src0.y, src0.z src1.w
        // But we flip src0, src1 to be inverse of color.
        if (alphaResult != dst)
          dst = m_module.opVectorShuffle(m_vec4Type, dst, alphaResult, indices.size(), indices.data());
      }
    }

    if (m_fsKey.Stages[0].Contents.GlobalSpecularEnable) {
      uint32_t specular = m_module.opFMul(m_vec4Type, m_ps.in.COLOR[1], m_module.constvec4f32(1.0f, 1.0f, 1.0f, 0.0f));

      current = m_module.opFAdd(m_vec4Type, current, specular);
    }

    D3D9FogContext fogCtx;
    fogCtx.IsPixel     = true;
    fogCtx.RangeFog    = false;
    fogCtx.RenderState = m_rsBlock;
    fogCtx.vPos        = m_ps.in.POS;
    fogCtx.vFog        = m_ps.in.FOG;
    fogCtx.oColor      = current;
    fogCtx.IsFixedFunction = true;
    fogCtx.IsPositionT = false;
    fogCtx.HasSpecular = false;
    fogCtx.Specular    = 0;
    fogCtx.SpecUBO     = m_specUbo;
    current = DoFixedFunctionFog(m_spec, m_module, fogCtx);

    m_module.opStore(m_ps.out.COLOR, current);

    alphaTestPS();
  }

  void D3D9FFShaderCompiler::setupPS() {
    setupRenderStateInfo();
    m_specUbo = SetupSpecUBO(m_module, m_bindings);

    // PS Caps
    m_module.enableExtension("SPV_EXT_demote_to_helper_invocation");
    m_module.enableCapability(spv::CapabilityDemoteToHelperInvocationEXT);
    m_module.enableCapability(spv::CapabilityDerivativeControl);

    m_module.setExecutionMode(m_entryPointId,
      spv::ExecutionModeOriginUpperLeft);

    uint32_t pointCoord = GetPointCoord(m_module);
    auto pointInfo = GetPointSizeInfoPS(m_spec, m_module, m_rsBlock, m_specUbo);

    // We need to replace TEXCOORD inputs with gl_PointCoord
    // if D3DRS_POINTSPRITEENABLE is set.
    for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
      m_ps.in.TEXCOORD[i] = declareIO(true, DxsoSemantic{ DxsoUsage::Texcoord, i });
      m_ps.in.TEXCOORD[i] = m_module.opSelect(m_vec4Type, pointInfo.isSprite, pointCoord, m_ps.in.TEXCOORD[i]);
    }

    m_ps.in.COLOR[0] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 0 });
    m_ps.in.COLOR[1] = declareIO(true, DxsoSemantic{ DxsoUsage::Color, 1 });

    m_ps.in.FOG      = declareIO(true, DxsoSemantic{ DxsoUsage::Fog, 0 });
    m_ps.in.POS      = declareIO(true, DxsoSemantic{ DxsoUsage::Position, 0 }, spv::BuiltInFragCoord);

    m_ps.out.COLOR   = declareIO(false, DxsoSemantic{ DxsoUsage::Color, 0 });

    // Constant Buffer for PS.
    std::array<uint32_t, uint32_t(D3D9FFPSMembers::MemberCount)> members = {
      m_vec4Type // Texture Factor
    };

    const uint32_t structType =
      m_module.defStructType(members.size(), members.data());

    m_module.decorateBlock(structType);
    uint32_t offset = 0;

    for (uint32_t i = 0; i < uint32_t(D3D9FFPSMembers::MemberCount); i++) {
      m_module.memberDecorateOffset(structType, i, offset);
      offset += sizeof(Vector4);
    }

    m_module.setDebugName(structType, "D3D9FixedFunctionPS");
    m_module.setDebugMemberName(structType, 0, "textureFactor");

    m_ps.constantBuffer = m_module.newVar(
      m_module.defPointerType(structType, spv::StorageClassUniform),
      spv::StorageClassUniform);

    m_module.setDebugName(m_ps.constantBuffer, "consts");

    const uint32_t bindingId = computeResourceSlotId(
      DxsoProgramType::PixelShader, DxsoBindingType::ConstantBuffer,
      DxsoConstantBuffers::PSFixedFunction);

    m_module.decorateDescriptorSet(m_ps.constantBuffer, 0);
    m_module.decorateBinding(m_ps.constantBuffer, bindingId);

    DxvkBindingInfo binding = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
    binding.resourceBinding = bindingId;
    binding.viewType        = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    binding.access          = VK_ACCESS_UNIFORM_READ_BIT;
    binding.uboSet          = VK_TRUE;
    m_bindings.push_back(binding);

    // Load constants
    auto LoadConstant = [&](uint32_t type, uint32_t idx) {
      uint32_t offset  = m_module.constu32(idx);
      uint32_t typePtr = m_module.defPointerType(type, spv::StorageClassUniform);

      return m_module.opLoad(type,
        m_module.opAccessChain(typePtr, m_ps.constantBuffer, 1, &offset));
    };

    m_ps.constants.textureFactor = LoadConstant(m_vec4Type, uint32_t(D3D9FFPSMembers::TextureFactor));

    // Samplers
    for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
      auto& sampler = m_ps.samplers[i];
      D3DRESOURCETYPE type = D3DRESOURCETYPE(m_fsKey.Stages[i].Contents.Type + D3DRTYPE_TEXTURE);

      spv::Dim dimensionality;
      VkImageViewType viewType;

      switch (type) {
        default:
        case D3DRTYPE_TEXTURE:
          dimensionality = spv::Dim2D;
          sampler.texcoordCnt = 2;
          viewType       = VK_IMAGE_VIEW_TYPE_2D;
          break;
        case D3DRTYPE_CUBETEXTURE:
          dimensionality = spv::DimCube;
          sampler.texcoordCnt = 3;
          viewType       = VK_IMAGE_VIEW_TYPE_CUBE;
          break;
        case D3DRTYPE_VOLUMETEXTURE:
          dimensionality = spv::Dim3D;
          sampler.texcoordCnt = 3;
          viewType       = VK_IMAGE_VIEW_TYPE_3D;
          break;
      }

      sampler.typeId = m_module.defImageType(
        m_module.defFloatType(32),
        dimensionality, 0, 0, 0, 1,
        spv::ImageFormatUnknown);

      sampler.typeId = m_module.defSampledImageType(sampler.typeId);

      sampler.varId = m_module.newVar(
        m_module.defPointerType(
          sampler.typeId, spv::StorageClassUniformConstant),
        spv::StorageClassUniformConstant);

      std::string name = str::format("s", i);
      m_module.setDebugName(sampler.varId, name.c_str());

      const uint32_t bindingId = computeResourceSlotId(DxsoProgramType::PixelShader,
        DxsoBindingType::Image, i);

      m_module.decorateDescriptorSet(sampler.varId, 0);
      m_module.decorateBinding(sampler.varId, bindingId);

      // Store descriptor info for the shader interface
      DxvkBindingInfo binding = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER };
      binding.resourceBinding = bindingId;
      binding.viewType        = viewType;
      binding.access          = VK_ACCESS_SHADER_READ_BIT;
      m_bindings.push_back(binding);
    }

    emitPsSharedConstants();
  }


  void D3D9FFShaderCompiler::emitPsSharedConstants() {
    m_ps.sharedState = GetSharedConstants(m_module);

    const uint32_t bindingId = computeResourceSlotId(
      m_programType, DxsoBindingType::ConstantBuffer,
      PSShared);

    m_module.decorateDescriptorSet(m_ps.sharedState, 0);
    m_module.decorateBinding(m_ps.sharedState, bindingId);

    DxvkBindingInfo binding = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
    binding.resourceBinding = bindingId;
    binding.viewType        = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    binding.access          = VK_ACCESS_UNIFORM_READ_BIT;
    binding.uboSet          = VK_TRUE;
    m_bindings.push_back(binding);
  }


  void D3D9FFShaderCompiler::emitVsClipping(uint32_t vtx) {
    uint32_t worldPos = emitMatrixTimesVector(4, 4, m_vs.constants.inverseView, vtx);

    uint32_t clipPlaneCountId = m_module.constu32(caps::MaxClipPlanes);
    
    uint32_t floatType = m_module.defFloatType(32);
    uint32_t vec4Type  = m_module.defVectorType(floatType, 4);
    
    // Declare uniform buffer containing clip planes
    uint32_t clipPlaneArray  = m_module.defArrayTypeUnique(vec4Type, clipPlaneCountId);
    uint32_t clipPlaneStruct = m_module.defStructTypeUnique(1, &clipPlaneArray);
    uint32_t clipPlaneBlock  = m_module.newVar(
      m_module.defPointerType(clipPlaneStruct, spv::StorageClassUniform),
      spv::StorageClassUniform);
    
    m_module.decorateArrayStride  (clipPlaneArray, 16);
    
    m_module.setDebugName         (clipPlaneStruct, "clip_info_t");
    m_module.setDebugMemberName   (clipPlaneStruct, 0, "clip_planes");
    m_module.decorate             (clipPlaneStruct, spv::DecorationBlock);
    m_module.memberDecorateOffset (clipPlaneStruct, 0, 0);
    
    uint32_t bindingId = computeResourceSlotId(
      DxsoProgramType::VertexShader,
      DxsoBindingType::ConstantBuffer,
      DxsoConstantBuffers::VSClipPlanes);
    
    m_module.setDebugName         (clipPlaneBlock, "clip_info");
    m_module.decorateDescriptorSet(clipPlaneBlock, 0);
    m_module.decorateBinding      (clipPlaneBlock, bindingId);
    
    DxvkBindingInfo binding = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
    binding.resourceBinding = bindingId;
    binding.viewType        = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    binding.access          = VK_ACCESS_UNIFORM_READ_BIT;
    binding.uboSet          = VK_TRUE;
    m_bindings.push_back(binding);

    // Declare output array for clip distances
    uint32_t clipDistArray = m_module.newVar(
      m_module.defPointerType(
        m_module.defArrayType(floatType, clipPlaneCountId),
        spv::StorageClassOutput),
      spv::StorageClassOutput);

    m_module.decorateBuiltIn(clipDistArray, spv::BuiltInClipDistance);

    // Compute clip distances
    for (uint32_t i = 0; i < caps::MaxClipPlanes; i++) {
      std::array<uint32_t, 2> blockMembers = {{
        m_module.constu32(0),
        m_module.constu32(i),
      }};
      
      uint32_t planeId = m_module.opLoad(vec4Type,
        m_module.opAccessChain(
          m_module.defPointerType(vec4Type, spv::StorageClassUniform),
          clipPlaneBlock, blockMembers.size(), blockMembers.data()));
      
      uint32_t distId = m_module.opDot(floatType, worldPos, planeId);
      
      m_module.opStore(
        m_module.opAccessChain(
          m_module.defPointerType(floatType, spv::StorageClassOutput),
          clipDistArray, 1, &blockMembers[1]),
        distId);
    }
  }


  void D3D9FFShaderCompiler::alphaTestPS() {
    uint32_t uintPtr = m_module.defPointerType(m_uint32Type, spv::StorageClassPushConstant);

    auto oC0 = m_ps.out.COLOR;

    uint32_t alphaComponentId = 3;
    uint32_t alphaRefMember = m_module.constu32(uint32_t(D3D9RenderStateItem::AlphaRef));

    D3D9AlphaTestContext alphaTestContext;
    alphaTestContext.alphaFuncId = m_spec.get(m_module, m_specUbo, SpecAlphaCompareOp);
    alphaTestContext.alphaPrecisionId = m_spec.get(m_module, m_specUbo, SpecAlphaPrecisionBits);
    alphaTestContext.alphaRefId = m_module.opLoad(m_uint32Type,
      m_module.opAccessChain(uintPtr, m_rsBlock, 1, &alphaRefMember));
    alphaTestContext.alphaId = m_module.opCompositeExtract(m_floatType,
      m_module.opLoad(m_vec4Type, oC0),
      1, &alphaComponentId);

    DoFixedFunctionAlphaTest(m_module, alphaTestContext);
  }


  uint32_t D3D9FFShaderCompiler::emitMatrixTimesVector(uint32_t rowCount, uint32_t colCount, uint32_t matrix, uint32_t vector) {
    uint32_t f32Type = m_module.defFloatType(32);
    uint32_t vecType = m_module.defVectorType(f32Type, rowCount);
    uint32_t accum = 0;

    for (uint32_t i = 0; i < colCount; i++) {
      std::array<uint32_t, 4> indices = { i, i, i, i };

      uint32_t a = m_module.opVectorShuffle(vecType, vector, vector, rowCount, indices.data());
      uint32_t b = m_module.opCompositeExtract(vecType, matrix, 1, &i);

      accum = accum
        ? m_module.opFFma(vecType, a, b, accum)
        : m_module.opFMul(vecType, a, b);

      m_module.decorate(accum, spv::DecorationNoContraction);
    }

    return accum;
  }


  uint32_t D3D9FFShaderCompiler::emitVectorTimesMatrix(uint32_t rowCount, uint32_t colCount, uint32_t vector, uint32_t matrix) {
    uint32_t f32Type = m_module.defFloatType(32);
    uint32_t vecType = m_module.defVectorType(f32Type, colCount);
    uint32_t matType = m_module.defMatrixType(vecType, rowCount);

    matrix = m_module.opTranspose(matType, matrix);
    return emitMatrixTimesVector(colCount, rowCount, matrix, vector);
  }


  D3D9FFShader::D3D9FFShader(
          D3D9DeviceEx*         pDevice,
    const D3D9FFShaderKeyVS&    Key) {
    Sha1Hash hash = Sha1Hash::compute(&Key, sizeof(Key));
    DxvkShaderKey shaderKey = { VK_SHADER_STAGE_VERTEX_BIT, hash };

    std::string name = str::format("FF_", shaderKey.toString());

    D3D9FFShaderCompiler compiler(
      pDevice->GetDXVKDevice(),
      Key, name,
      pDevice->GetOptions());

    m_shader = compiler.compile();
    m_isgn   = compiler.isgn();

    Dump(pDevice, Key, name);

    m_shader->setShaderKey(shaderKey);
    pDevice->GetDXVKDevice()->registerShader(m_shader);
  }


  D3D9FFShader::D3D9FFShader(
          D3D9DeviceEx*         pDevice,
    const D3D9FFShaderKeyFS&    Key) {
    Sha1Hash hash = Sha1Hash::compute(&Key, sizeof(Key));
    DxvkShaderKey shaderKey = { VK_SHADER_STAGE_FRAGMENT_BIT, hash };

    std::string name = str::format("FF_", shaderKey.toString());

    D3D9FFShaderCompiler compiler(
      pDevice->GetDXVKDevice(),
      Key, name,
      pDevice->GetOptions());

    m_shader = compiler.compile();
    m_isgn   = compiler.isgn();

    Dump(pDevice, Key, name);

    m_shader->setShaderKey(shaderKey);
    pDevice->GetDXVKDevice()->registerShader(m_shader);
  }

  template <typename T>
  void D3D9FFShader::Dump(D3D9DeviceEx* pDevice, const T& Key, const std::string& Name) {
    const std::string& dumpPath = pDevice->GetOptions()->shaderDumpPath;

    if (dumpPath.size() != 0) {
      std::ofstream dumpStream(
        str::topath(str::format(dumpPath, "/", Name, ".spv").c_str()).c_str(),
        std::ios_base::binary | std::ios_base::trunc);
      
      m_shader->dump(dumpStream);
    }
  }


  D3D9FFShader D3D9FFShaderModuleSet::GetShaderModule(
          D3D9DeviceEx*         pDevice,
    const D3D9FFShaderKeyVS&    ShaderKey) {
    // Use the shader's unique key for the lookup
    auto entry = m_vsModules.find(ShaderKey);
    if (entry != m_vsModules.end())
      return entry->second;
    
    D3D9FFShader shader(
      pDevice, ShaderKey);

    m_vsModules.insert({ShaderKey, shader});

    return shader;
  }


  D3D9FFShader D3D9FFShaderModuleSet::GetShaderModule(
          D3D9DeviceEx*         pDevice,
    const D3D9FFShaderKeyFS&    ShaderKey) {
    // Use the shader's unique key for the lookup
    auto entry = m_fsModules.find(ShaderKey);
    if (entry != m_fsModules.end())
      return entry->second;
    
    D3D9FFShader shader(
      pDevice, ShaderKey);

    m_fsModules.insert({ShaderKey, shader});

    return shader;
  }


  size_t D3D9FFShaderKeyHash::operator () (const D3D9FFShaderKeyVS& key) const {
    DxvkHashState state;

    std::hash<uint32_t> uint32hash;

    for (uint32_t i = 0; i < std::size(key.Data.Primitive); i++)
      state.add(uint32hash(key.Data.Primitive[i]));

    return state;
  }


  size_t D3D9FFShaderKeyHash::operator () (const D3D9FFShaderKeyFS& key) const {
    DxvkHashState state;

    std::hash<uint32_t> uint32hash;

    for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
      for (uint32_t j = 0; j < std::size(key.Stages[i].Primitive); j++)
        state.add(uint32hash(key.Stages[i].Primitive[j]));
    }

    return state;
  }


  bool operator == (const D3D9FFShaderKeyVS& a, const D3D9FFShaderKeyVS& b) {
    return std::memcmp(&a, &b, sizeof(D3D9FFShaderKeyVS)) == 0;
  }


  bool operator == (const D3D9FFShaderKeyFS& a, const D3D9FFShaderKeyFS& b) {
    return std::memcmp(&a, &b, sizeof(D3D9FFShaderKeyFS)) == 0;
  }


  bool operator != (const D3D9FFShaderKeyVS& a, const D3D9FFShaderKeyVS& b) {
    return !(a == b);
  }


  bool operator != (const D3D9FFShaderKeyFS& a, const D3D9FFShaderKeyFS& b) {
    return !(a == b);
  }


  bool D3D9FFShaderKeyEq::operator () (const D3D9FFShaderKeyVS& a, const D3D9FFShaderKeyVS& b) const {
    return a == b;
  }


  bool D3D9FFShaderKeyEq::operator () (const D3D9FFShaderKeyFS& a, const D3D9FFShaderKeyFS& b) const {
    return a == b;
  }


  static inline DxsoIsgn CreateFixedFunctionIsgn() {
    DxsoIsgn ffIsgn;

    ffIsgn.elems[ffIsgn.elemCount++].semantic = DxsoSemantic{ DxsoUsage::Position, 0 };
    ffIsgn.elems[ffIsgn.elemCount++].semantic = DxsoSemantic{ DxsoUsage::Normal, 0 };
    ffIsgn.elems[ffIsgn.elemCount++].semantic = DxsoSemantic{ DxsoUsage::Position, 1 };
    ffIsgn.elems[ffIsgn.elemCount++].semantic = DxsoSemantic{ DxsoUsage::Normal, 1 };
    for (uint32_t i = 0; i < 8; i++)
      ffIsgn.elems[ffIsgn.elemCount++].semantic = DxsoSemantic{ DxsoUsage::Texcoord, i };
    ffIsgn.elems[ffIsgn.elemCount++].semantic = DxsoSemantic{ DxsoUsage::Color, 0 };
    ffIsgn.elems[ffIsgn.elemCount++].semantic = DxsoSemantic{ DxsoUsage::Color, 1 };
    ffIsgn.elems[ffIsgn.elemCount++].semantic = DxsoSemantic{ DxsoUsage::Fog, 0 };
    ffIsgn.elems[ffIsgn.elemCount++].semantic = DxsoSemantic{ DxsoUsage::PointSize, 0 };
    ffIsgn.elems[ffIsgn.elemCount++].semantic = DxsoSemantic{ DxsoUsage::BlendWeight, 0 };
    ffIsgn.elems[ffIsgn.elemCount++].semantic = DxsoSemantic{ DxsoUsage::BlendIndices, 0 };

    return ffIsgn;
  }


  DxsoIsgn g_ffIsgn = CreateFixedFunctionIsgn();

}
