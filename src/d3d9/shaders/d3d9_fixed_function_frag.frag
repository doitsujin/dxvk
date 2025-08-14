#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_spirv_intrinsics : require
#extension GL_EXT_demote_to_helper_invocation : require
#extension GL_ARB_derivative_control : require

#extension GL_EXT_nonuniform_qualifier : require // TODO: Get rid of this?

#define GLSL
#include "../d3d9_shader_types.h"

const uint TextureStageCount = 8;
const uint TextureArgCount = 3;

layout(constant_id = 0) const int drefScaling = 0;

// The locations need to match with RegisterLinkerSlot in dxso_util.cpp
layout(location = 0) in vec4 in_Normal;
layout(location = 1) in vec4 in_Texcoord0;
layout(location = 2) in vec4 in_Texcoord1;
layout(location = 3) in vec4 in_Texcoord2;
layout(location = 4) in vec4 in_Texcoord3;
layout(location = 5) in vec4 in_Texcoord4;
layout(location = 6) in vec4 in_Texcoord5;
layout(location = 7) in vec4 in_Texcoord6;
layout(location = 8) in vec4 in_Texcoord7;
layout(location = 9) in vec4 in_Color0;
layout(location = 10) in vec4 in_Color1;
layout(location = 11) in float in_Fog;

layout(location = 0) out vec4 out_Color0;

// Bindings have to match with computeResourceSlotId in dxso_util.h
// computeResourceSlotId(
//     DxsoProgramType::PixelShader,
//     DxsoBindingType::ConstantBuffer,
//     DxsoConstantBuffers::PSFixedFunction
// ) = 11
layout(set = 0, binding = 11, scalar, row_major) uniform ShaderData {
    D3D9FixedFunctionPS data;
};

// Bindings have to match with computeResourceSlotId in dxso_util.h
// computeResourceSlotId(
//     DxsoProgramType::PixelShader,
//     DxsoBindingType::ConstantBuffer,
//     DxsoConstantBuffers::PSShared
// ) = 12
layout(set = 0, binding = 12, scalar, row_major) uniform SharedData {
    D3D9SharedPS sharedData;
};

layout(push_constant, scalar, row_major) uniform RenderStates {
    D3D9RenderStateInfo rs;
    uint packedSamplerIndices[4];
};


// Dynamic "spec constants"
// See d3d9_spec_constants.h for packing
// MaxSpecDwords = 6
// Binding has to match with getSpecConstantBufferSlot in dxso_util.h
layout(set = 0, binding = 31, scalar) uniform SpecConsts {
    uint specConstDword[6];
};


layout(set = 0, binding = 13) uniform texture2D t2d[8];
layout(set = 0, binding = 13) uniform textureCube tcube[8];
layout(set = 0, binding = 13) uniform texture3D t3d[8];


layout(origin_upper_left) in vec4 gl_FragCoord;


layout(set = 15, binding = 0) uniform sampler sampler_heap[];



// Thanks SPIRV-Cross
spirv_instruction(set = "GLSL.std.450", id = 79) float spvNMin(float, float);
spirv_instruction(set = "GLSL.std.450", id = 79) vec2 spvNMin(vec2, vec2);
spirv_instruction(set = "GLSL.std.450", id = 79) vec3 spvNMin(vec3, vec3);
spirv_instruction(set = "GLSL.std.450", id = 79) vec4 spvNMin(vec4, vec4);
spirv_instruction(set = "GLSL.std.450", id = 81) float spvNClamp(float, float, float);
spirv_instruction(set = "GLSL.std.450", id = 81) vec2 spvNClamp(vec2, vec2, vec2);
spirv_instruction(set = "GLSL.std.450", id = 81) vec3 spvNClamp(vec3, vec3, vec3);
spirv_instruction(set = "GLSL.std.450", id = 81) vec4 spvNClamp(vec4, vec4, vec4);


// Functions to extract information from the packed texture stages
// See D3D9FFShaderStage in d3d9_shader_types.h
// Please, dearest compiler, inline all of this.
uint ColorOp(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[0], 0, 5);
}
uint ColorArg0(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[0], 5, 6);
}
uint ColorArg1(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[0], 11, 6);
}
uint ColorArg2(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[0], 17, 6);
}

uint AlphaOp(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[0], 23, 5);
}
uint AlphaArg0(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[1], 0, 6);
}
uint AlphaArg1(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[1], 6, 6);
}
uint AlphaArg2(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[1], 12, 6);
}

uint TextureType(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[1], 18, 2);
}
bool ResultIsTemp(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[1], 20, 1) != 0;
}
bool Projected(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[1], 21, 1) != 0;
}
uint ProjectedCount(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[1], 22, 3);
}
bool SampleDref(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[1], 25, 1) != 0;
}
bool TextureBound(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[1], 26, 1) != 0;
}
bool GlobalSpecularEnable(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[1], 27, 1) != 0;
}


// Functions to extract information from the packed dynamic spec consts
// See d3d9_spec_constants.h for packing
// Please, dearest compiler, inline all of this.
uint SpecSamplerType() {
    return bitfieldExtract(specConstDword[0], 0, 32);
}
uint SpecSamplerDepthMode() {
    return bitfieldExtract(specConstDword[1], 0, 21);
}
uint SpecAlphaCompareOp() {
    return bitfieldExtract(specConstDword[1], 21, 3);
}
uint SpecPointMode() {
    return bitfieldExtract(specConstDword[1], 24, 2);
}
uint SpecVertexFogMode() {
    return bitfieldExtract(specConstDword[1], 26, 2);
}
uint SpecPixelFogMode() {
    return bitfieldExtract(specConstDword[1], 28, 2);
}
bool SpecFogEnabled() {
    return bitfieldExtract(specConstDword[1], 30, 1) != 0;
}
uint SpecSamplerNull() {
    return bitfieldExtract(specConstDword[2], 0, 21);
}
uint SpecProjectionType() {
    return bitfieldExtract(specConstDword[2], 21, 6);
}
uint SpecAlphaPrecisionBits() {
    return bitfieldExtract(specConstDword[2], 27, 4);
}
uint SpecVertexShaderBools() {
    return bitfieldExtract(specConstDword[3], 0, 16);
}
uint SpecPixelShaderBools() {
    return bitfieldExtract(specConstDword[3], 16, 16);
}
uint SpecFetch4() {
    return bitfieldExtract(specConstDword[4], 0, 16);
}
uint SpecDrefClamp() {
    return bitfieldExtract(specConstDword[5], 0, 21);
}
uint SpecClipPlaneCount() {
    return bitfieldExtract(specConstDword[5], 21, 3);
}

void main() {
    out_Color0 = vec4(0.0);
}
