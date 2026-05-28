#include "d3d9_fixed_function.h"

#include "d3d9_device.h"
#include "d3d9_util.h"
#include "d3d9_spec_constants.h"

#include "../dxvk/dxvk_hash.h"
#include "../dxvk/dxvk_shader_spirv.h"

#include "../util/util_small_vector.h"

#include "../spirv/spirv_module.h"

#include <cfloat>

#include <d3d9_fixed_function_vert.h>
#include <d3d9_fixed_function_frag.h>
#include <d3d9_fixed_function_frag_sample.h>

namespace dxvk {

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
          case VK_COMPARE_OP_NOT_EQUAL:        return spvModule.opFUnordNotEqual      (boolType, alphaId, alphaRefId);
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


  std::pair<uint32_t, uint32_t> SetupRenderStateBlock(SpirvModule& spvModule, uint32_t samplerMask) {
    uint32_t floatType = spvModule.defFloatType(32);
    uint32_t uintType  = spvModule.defIntType(32, 0);
    uint32_t vec3Type  = spvModule.defVectorType(floatType, 3);

    small_vector<uint32_t, 32u> rsMembers;
    rsMembers.push_back(vec3Type);
    rsMembers.push_back(floatType);
    rsMembers.push_back(floatType);
    rsMembers.push_back(floatType);

    rsMembers.push_back(uintType);

    rsMembers.push_back(floatType);
    rsMembers.push_back(floatType);
    rsMembers.push_back(floatType);
    rsMembers.push_back(floatType);
    rsMembers.push_back(floatType);
    rsMembers.push_back(floatType);

    // Number of static data members
    uint32_t rsMemberCount = rsMembers.size();

    // Add one dword for each sampler pair
    uint32_t samplerCount = bit::popcnt(samplerMask);

    for (uint32_t i = 0u; i < samplerCount; i += 2u)
      rsMembers.push_back(uintType);

    uint32_t rsStruct = spvModule.defStructTypeUnique(rsMembers.size(), rsMembers.data());
    uint32_t rsBlock = spvModule.newVar(
      spvModule.defPointerType(rsStruct, spv::StorageClassPushConstant),
      spv::StorageClassPushConstant);

    spvModule.setDebugName         (rsBlock, "render_state");

    spvModule.setDebugName         (rsStruct, "render_state_t");
    spvModule.decorate             (rsStruct, spv::DecorationBlock);

    uint32_t memberIdx = 0;
    auto SetMemberName = [&](const char* name, uint32_t offset) {
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

    uint32_t samplerOffset = GetPushSamplerOffset(0u);

    while (samplerMask) {
      uint32_t s0 = bit::tzcnt(samplerMask); samplerMask &= samplerMask - 1u;
      uint32_t s1 = bit::tzcnt(samplerMask); samplerMask &= samplerMask - 1u;

      std::string name = s1 < samplerCount
        ? str::format("s", s0, "_s", s1, "_idx")
        : str::format("s", s0, "_idx");

      SetMemberName(name.c_str(), samplerOffset);
      samplerOffset += sizeof(uint32_t);
    }

    return std::make_pair(rsBlock, rsMemberCount);
  }


  uint32_t SetupSamplerArray(SpirvModule& spvModule) {
    // Old spir-v, need to enable extension
    spvModule.enableExtension("SPV_EXT_descriptor_indexing");
    spvModule.enableCapability(spv::CapabilityRuntimeDescriptorArray);

    uint32_t samplerArray = spvModule.defRuntimeArrayTypeUnique(spvModule.defSamplerType());
    uint32_t samplerPtr = spvModule.defPointerType(samplerArray, spv::StorageClassUniformConstant);

    uint32_t samplerHeap = spvModule.newVar(samplerPtr, spv::StorageClassUniformConstant);
    spvModule.setDebugName(samplerHeap, "sampler_heap");

    spvModule.decorateBinding(samplerHeap, 0u);
    spvModule.decorateDescriptorSet(samplerHeap, GetGlobalSamplerSetIndex());
    return samplerHeap;
  }


  uint32_t LoadSampler(SpirvModule& spvModule, uint32_t descriptorId,
    uint32_t pushBlockId, uint32_t pushMember, uint32_t samplerIndex) {
    uint32_t uintType = spvModule.defIntType(32u, 0);
    uint32_t uintPtr = spvModule.defPointerType(uintType, spv::StorageClassPushConstant);

    uint32_t pushIndexId = spvModule.constu32(pushMember + samplerIndex / 2u);

    uint32_t descriptorIndex = spvModule.opLoad(uintType,
      spvModule.opAccessChain(uintPtr, pushBlockId, 1u, &pushIndexId));

    descriptorIndex = spvModule.opBitFieldUExtract(uintType, descriptorIndex,
      spvModule.constu32(16u * (samplerIndex & 1u)), spvModule.constu32(16u));

    uint32_t samplerType = spvModule.defSamplerType();
    uint32_t samplerPtr = spvModule.defPointerType(samplerType, spv::StorageClassUniformConstant);

    return spvModule.opLoad(samplerType,
      spvModule.opAccessChain(samplerPtr, descriptorId, 1u, &descriptorIndex));
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
    spvModule.decorateBinding (specBlock, D3D9ShaderResourceMapping::getSpecConstantBufferSlot());

    auto& binding = bindings.emplace_back();
    binding.set             = 0u;
    binding.binding         = D3D9ShaderResourceMapping::getSpecConstantBufferSlot();
    binding.resourceIndex   = D3D9ShaderResourceMapping::getSpecConstantBufferSlot();
    binding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.access          = VK_ACCESS_UNIFORM_READ_BIT;
    binding.flags.set(DxvkDescriptorFlag::UniformBuffer);

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


  D3D9FFShaderModuleSet::D3D9FFShaderModuleSet(D3D9DeviceEx* pDevice)
    : m_vs(buildVs())
    , m_fs(buildFs(pDevice)) {}


  Rc<DxvkShader> D3D9FFShaderModuleSet::buildVs() {
    small_vector<DxvkBindingInfo, 4> bindings = {};

    constexpr uint32_t specConstantBufferBindingId = D3D9ShaderResourceMapping::getSpecConstantBufferSlot();

    auto& specConstantBufferBinding = bindings.emplace_back();
    specConstantBufferBinding.set = 0u;
    specConstantBufferBinding.binding        = specConstantBufferBindingId;
    specConstantBufferBinding.resourceIndex  = specConstantBufferBindingId;
    specConstantBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    specConstantBufferBinding.access = VK_ACCESS_UNIFORM_READ_BIT;
    specConstantBufferBinding.flags.set(DxvkDescriptorFlag::UniformBuffer);

    constexpr uint32_t fixedFunctionDataBindingId = D3D9ShaderResourceMapping::computeCbvBinding(
      D3D9ShaderType::VertexShader, D3D9ShaderResourceMapping::ConstantBuffers::VSFixedFunction);

    auto& fixedFunctionDataBinding = bindings.emplace_back();
    fixedFunctionDataBinding.set             = 0u;
    fixedFunctionDataBinding.binding         = fixedFunctionDataBindingId;
    fixedFunctionDataBinding.resourceIndex   = fixedFunctionDataBindingId;
    fixedFunctionDataBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    fixedFunctionDataBinding.access          = VK_ACCESS_UNIFORM_READ_BIT;
    fixedFunctionDataBinding.flags.set(DxvkDescriptorFlag::UniformBuffer);

    constexpr uint32_t vertexBlendBindingId = D3D9ShaderResourceMapping::computeCbvBinding(
      D3D9ShaderType::VertexShader, D3D9ShaderResourceMapping::ConstantBuffers::VSVertexBlendData);

    auto& vertexBlendBinding = bindings.emplace_back();
    vertexBlendBinding.set             = 0u;
    vertexBlendBinding.binding         = vertexBlendBindingId;
    vertexBlendBinding.resourceIndex   = vertexBlendBindingId;
    vertexBlendBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexBlendBinding.access          = VK_ACCESS_SHADER_READ_BIT;
    vertexBlendBinding.flags.set(DxvkDescriptorFlag::UniformBuffer);

    constexpr uint32_t clipPlanesBindingId = D3D9ShaderResourceMapping::computeCbvBinding(
      D3D9ShaderType::VertexShader, D3D9ShaderResourceMapping::ConstantBuffers::VSClipPlanes);

    auto& clipPlanesBinding = bindings.emplace_back();
    clipPlanesBinding.set             = 0u;
    clipPlanesBinding.binding         = clipPlanesBindingId;
    clipPlanesBinding.resourceIndex   = clipPlanesBindingId;
    clipPlanesBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    clipPlanesBinding.access          = VK_ACCESS_UNIFORM_READ_BIT;
    clipPlanesBinding.flags.set(DxvkDescriptorFlag::UniformBuffer);

    DxvkSpirvShaderCreateInfo info;
    info.bindingCount = bindings.size();
    info.bindings = bindings.data();
    info.flatShadingInputs = 0;
    info.sharedPushData = DxvkPushDataBlock(0u, sizeof(D3D9RenderStateInfo), 4u, 0u);
    info.localPushData = DxvkPushDataBlock();
    info.samplerHeap = DxvkShaderBinding();
    info.debugName = "FF VS";

    return new DxvkSpirvShader(info, d3d9_fixed_function_vert);
  }


  Rc<DxvkShader> D3D9FFShaderModuleSet::buildFs(D3D9DeviceEx* pDevice) {
    small_vector<DxvkBindingInfo, 12> bindings = {};

    constexpr uint32_t specConstantBufferBindingId = D3D9ShaderResourceMapping::getSpecConstantBufferSlot();

    auto& specConstantBufferBinding = bindings.emplace_back();
    specConstantBufferBinding.set = 0u;
    specConstantBufferBinding.binding        = specConstantBufferBindingId;
    specConstantBufferBinding.resourceIndex  = specConstantBufferBindingId;
    specConstantBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    specConstantBufferBinding.access = VK_ACCESS_UNIFORM_READ_BIT;
    specConstantBufferBinding.flags.set(DxvkDescriptorFlag::UniformBuffer);

    constexpr uint32_t fixedFunctionDataBindingId = D3D9ShaderResourceMapping::computeCbvBinding(
      D3D9ShaderType::PixelShader, D3D9ShaderResourceMapping::ConstantBuffers::PSFixedFunction);

    auto& fixedFunctionDataBinding = bindings.emplace_back();
    fixedFunctionDataBinding.set             = 0u;
    fixedFunctionDataBinding.binding         = fixedFunctionDataBindingId;
    fixedFunctionDataBinding.resourceIndex   = fixedFunctionDataBindingId;
    fixedFunctionDataBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    fixedFunctionDataBinding.access          = VK_ACCESS_UNIFORM_READ_BIT;
    fixedFunctionDataBinding.flags.set(DxvkDescriptorFlag::UniformBuffer);

    constexpr uint32_t sharedDataBindingId = D3D9ShaderResourceMapping::computeCbvBinding(
      D3D9ShaderType::PixelShader, D3D9ShaderResourceMapping::ConstantBuffers::PSShared);

    auto& sharedDataBinding = bindings.emplace_back();
    sharedDataBinding.set             = 0u;
    sharedDataBinding.binding         = sharedDataBindingId;
    sharedDataBinding.resourceIndex   = sharedDataBindingId;
    sharedDataBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sharedDataBinding.access          = VK_ACCESS_SHADER_READ_BIT;
    sharedDataBinding.flags.set(DxvkDescriptorFlag::UniformBuffer);

    constexpr uint32_t textureBindingId = D3D9ShaderResourceMapping::computeTextureBinding(D3D9ShaderType::PixelShader, 0u);

    auto& textureBinding = bindings.emplace_back();
    textureBinding.set             = 0u;
    textureBinding.binding         = textureBindingId;
    textureBinding.resourceIndex   = textureBindingId;
    textureBinding.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    textureBinding.access       = VK_ACCESS_SHADER_READ_BIT;
    textureBinding.descriptorCount = caps::TextureStageCount;

    for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
      uint32_t samplerBindingId = D3D9ShaderResourceMapping::computeTextureBinding(D3D9ShaderType::PixelShader, i);

      auto& samplerBinding = bindings.emplace_back();
      samplerBinding.resourceIndex   = samplerBindingId;
      samplerBinding.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
      samplerBinding.blockOffset     = GetPushSamplerOffset(i);
      samplerBinding.flags.set(DxvkDescriptorFlag::PushData);
      bindings.push_back(samplerBinding);
    }

    uint32_t flatShadingMask = (1u << RegisterLinkerSlot(DxsoSemantic{ DxsoUsage::Color, 0 }))
      | (1u << RegisterLinkerSlot(DxsoSemantic{ DxsoUsage::Color, 1 }));

    uint32_t samplerCount = caps::TextureStageCount;
    uint32_t samplerDwordCount = (samplerCount + 1u) / 2u;

    DxvkSpirvShaderCreateInfo info;
    info.bindingCount = bindings.size();
    info.bindings = bindings.data();
    info.flatShadingInputs = flatShadingMask;
    info.sharedPushData = DxvkPushDataBlock(0u, sizeof(D3D9RenderStateInfo), 4u, 0u);
    info.localPushData = DxvkPushDataBlock(VK_SHADER_STAGE_FRAGMENT_BIT, GetPushSamplerOffset(0u),
      samplerDwordCount * sizeof(uint32_t), sizeof(uint32_t), (1u << samplerDwordCount) - 1u);
    info.samplerHeap = DxvkShaderBinding(VK_SHADER_STAGE_FRAGMENT_BIT, 1u, 0u);
    info.debugName = "FF FS";

    return pDevice->GetOptions()->forceSampleRateShading
      ? new DxvkSpirvShader(info, d3d9_fixed_function_frag_sample)
      : new DxvkSpirvShader(info, d3d9_fixed_function_frag);
  }


  static inline D3D9InputSignature CreateFixedFunctionIsgn() {
    D3D9InputSignature ffIsgn;

    ffIsgn.push_back(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::ePosition, 0u });
    ffIsgn.push_back(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eNormal, 0u });
    ffIsgn.push_back(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::ePosition, 1u });
    ffIsgn.push_back(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eNormal, 1u });
    for (uint32_t i = 0u; i < 8u; i++)
      ffIsgn.push_back(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eTexCoord, i });
    ffIsgn.push_back(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eColor, 0u });
    ffIsgn.push_back(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eColor, 1u });
    ffIsgn.push_back(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eFog, 0u });
    ffIsgn.push_back(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::ePointSize, 0u });
    ffIsgn.push_back(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eBlendWeight, 0u });
    ffIsgn.push_back(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eBlendIndices, 0u });

    return ffIsgn;
  }


  D3D9InputSignature g_ffIsgn = CreateFixedFunctionIsgn();

}
