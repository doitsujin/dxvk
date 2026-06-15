#include <sm3/sm3_io_map.h>

#include "d3d9_fixed_function.h"

#include "d3d9_device.h"
#include "d3d9_util.h"

#include "../dxvk/dxvk_hash.h"
#include "../dxvk/dxvk_shader_spirv.h"

#include "../util/util_small_vector.h"

#include <cfloat>

#include <d3d9_fixed_function_vert.h>
#include <d3d9_fixed_function_frag.h>
#include <d3d9_fixed_function_frag_sample.h>

namespace dxvk {

  D3D9FFShaderModuleSet::D3D9FFShaderModuleSet(D3D9DeviceEx* pDevice)
    : m_vs(buildVs())
    , m_fs(buildFs(pDevice)) {}


  Rc<DxvkShader> D3D9FFShaderModuleSet::buildVs() {
    small_vector<DxvkBindingInfo, 3> bindings = {};

    auto& fixedFunctionDataBinding = bindings.emplace_back();
    fixedFunctionDataBinding.set             = CbvSet;
    fixedFunctionDataBinding.binding         = D3D9ShaderResourceMapping::CbvIndex::VSFixedFunction;
    fixedFunctionDataBinding.resourceIndex   = D3D9ShaderResourceMapping::CbvIndex::VSFixedFunction;
    fixedFunctionDataBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    fixedFunctionDataBinding.access          = VK_ACCESS_UNIFORM_READ_BIT;
    fixedFunctionDataBinding.flags.set(DxvkDescriptorFlag::UniformBuffer);

    auto& vertexBlendBinding = bindings.emplace_back();
    vertexBlendBinding.set             = CbvSet;
    vertexBlendBinding.binding         = D3D9ShaderResourceMapping::CbvIndex::VSVertexBlendData;
    vertexBlendBinding.resourceIndex   = D3D9ShaderResourceMapping::CbvIndex::VSVertexBlendData;
    vertexBlendBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexBlendBinding.access          = VK_ACCESS_SHADER_READ_BIT;
    vertexBlendBinding.flags.set(DxvkDescriptorFlag::UniformBuffer);

    auto& clipPlanesBinding = bindings.emplace_back();
    clipPlanesBinding.set             = CbvSet;
    clipPlanesBinding.binding         = D3D9ShaderResourceMapping::CbvIndex::VSClipPlanes;
    clipPlanesBinding.resourceIndex   = D3D9ShaderResourceMapping::CbvIndex::VSClipPlanes;
    clipPlanesBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    clipPlanesBinding.access          = VK_ACCESS_UNIFORM_READ_BIT;
    clipPlanesBinding.flags.set(DxvkDescriptorFlag::UniformBuffer);

    DxvkSpirvShaderCreateInfo info;
    info.bindingCount = bindings.size();
    info.bindings = bindings.data();
    info.flatShadingInputs = 0;
    info.sharedPushData = DxvkPushDataBlock(0u, sizeof(D3D9SharedPushData), 4u, 0u);
    info.localPushData = DxvkPushDataBlock(VK_SHADER_STAGE_VERTEX_BIT, MaxSharedPushDataSize,
      D3D9FfvsPushData::Offset + sizeof(D3D9FfvsPushData), 4u, 0u);
    info.samplerHeap = DxvkShaderBinding();
    info.specDataBuffer = DxvkShaderBinding(VK_SHADER_STAGE_VERTEX_BIT, SpecDataSet, 0u);
    info.debugName = "FF VS";

    return new DxvkSpirvShader(info, d3d9_fixed_function_vert);
  }


  Rc<DxvkShader> D3D9FFShaderModuleSet::buildFs(D3D9DeviceEx* pDevice) {
    small_vector<DxvkBindingInfo, 10> bindings = {};

    auto& sharedDataBinding = bindings.emplace_back();
    sharedDataBinding.set             = CbvSet;
    sharedDataBinding.binding         = D3D9ShaderResourceMapping::CbvIndex::PSShared;
    sharedDataBinding.resourceIndex   = D3D9ShaderResourceMapping::CbvIndex::PSShared;
    sharedDataBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sharedDataBinding.access          = VK_ACCESS_SHADER_READ_BIT;
    sharedDataBinding.flags.set(DxvkDescriptorFlag::UniformBuffer);

    uint32_t textureBindingId = D3D9ShaderResourceMapping::computeTextureBinding(D3D9ShaderType::PixelShader, 0u);

    auto& textureBinding = bindings.emplace_back();
    textureBinding.set             = SrvSet;
    textureBinding.binding         = textureBindingId;
    textureBinding.resourceIndex   = textureBindingId;
    textureBinding.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    textureBinding.access          = VK_ACCESS_SHADER_READ_BIT;
    textureBinding.descriptorCount = caps::TextureStageCount;

    for (uint32_t i = 0; i < caps::TextureStageCount; i++) {
      uint32_t samplerBindingId = D3D9ShaderResourceMapping::computeTextureBinding(D3D9ShaderType::PixelShader, i);

      auto& samplerBinding = bindings.emplace_back();
      samplerBinding.resourceIndex   = samplerBindingId;
      samplerBinding.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
      samplerBinding.blockOffset     = GetPushSamplerOffset(i);
      samplerBinding.flags.set(DxvkDescriptorFlag::PushData);
    }

    uint32_t flatShadingMask =
      (1u << dxbc_spv::sm3::IoMap::findFixedFunctionLocation(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eColor, 0u }).value()) |
      (1u << dxbc_spv::sm3::IoMap::findFixedFunctionLocation(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eColor, 1u }).value());

    uint32_t samplerCount = caps::TextureStageCount;
    uint32_t samplerDwordCount = (samplerCount + 1u) / 2u;

    uint32_t pushDataSamplerOffset = GetPushSamplerOffset(0u) - MaxSharedPushDataSize;
    uint32_t pushDataSamplerShift = pushDataSamplerOffset / 4u;
    uint32_t pushDataSize = GetPushSamplerOffset(caps::TextureStageCount) - MaxSharedPushDataSize;

    DxvkSpirvShaderCreateInfo info;
    info.bindingCount = bindings.size();
    info.bindings = bindings.data();
    info.flatShadingInputs = flatShadingMask;
    info.sharedPushData = DxvkPushDataBlock(0u, sizeof(D3D9SharedPushData), 4u, 0u);
    info.localPushData = DxvkPushDataBlock(VK_SHADER_STAGE_FRAGMENT_BIT, MaxSharedPushDataSize,
      pushDataSize, 4u, ((1u << samplerDwordCount) - 1u) << pushDataSamplerShift);
    info.samplerHeap = DxvkShaderBinding(VK_SHADER_STAGE_FRAGMENT_BIT, SamplerSet, 0u);
    info.specDataBuffer = DxvkShaderBinding(VK_SHADER_STAGE_FRAGMENT_BIT, SpecDataSet, 0u);
    info.debugName = "FF FS";

    return pDevice->GetOptions()->forceSampleRateShading
      ? new DxvkSpirvShader(info, d3d9_fixed_function_frag_sample)
      : new DxvkSpirvShader(info, d3d9_fixed_function_frag);
  }


  static inline D3D9InputSignature CreateFixedFunctionIsgn() {
    D3D9InputSignature ffIsgn;

    ffIsgn.add(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::ePosition, 0u });
    ffIsgn.add(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eNormal, 0u });
    ffIsgn.add(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::ePosition, 1u });
    ffIsgn.add(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eNormal, 1u });
    for (uint32_t i = 0u; i < 8u; i++)
      ffIsgn.add(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eTexCoord, i });
    ffIsgn.add(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eColor, 0u });
    ffIsgn.add(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eColor, 1u });
    ffIsgn.add(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eFog, 0u });
    ffIsgn.add(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::ePointSize, 0u });
    ffIsgn.add(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eBlendWeight, 0u });
    ffIsgn.add(dxbc_spv::sm3::Semantic { dxbc_spv::sm3::SemanticUsage::eBlendIndices, 0u });

    return ffIsgn;
  }


  D3D9InputSignature g_ffIsgn = CreateFixedFunctionIsgn();

}
