#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_spirv_intrinsics : require

#define GLSL
#include "../d3d9_shader_types.h"

#define FLOAT_MAX_VALUE 340282346638528859811704183484516925440.0


const uint MaxClipPlaneCount = 6;
const uint TextureStageCount = 8;


layout(location = 0) in vec4 in_Position0;
layout(location = 1) in vec4 in_Normal0;
layout(location = 2) in vec4 in_Position1;
layout(location = 3) in vec4 in_Normal1;
layout(location = 4) in vec4 in_Texcoord0;
layout(location = 5) in vec4 in_Texcoord1;
layout(location = 6) in vec4 in_Texcoord2;
layout(location = 7) in vec4 in_Texcoord3;
layout(location = 8) in vec4 in_Texcoord4;
layout(location = 9) in vec4 in_Texcoord5;
layout(location = 10) in vec4 in_Texcoord6;
layout(location = 11) in vec4 in_Texcoord7;
layout(location = 12) in vec4 in_Color0;
layout(location = 13) in vec4 in_Color1;
layout(location = 14) in float in_Fog;
layout(location = 15) in float in_PointSize;
layout(location = 16) in vec4 in_BlendWeight;
layout(location = 17) in vec4 in_BlendIndices;


// The locations need to match with RegisterLinkerSlot in dxso_util.cpp
precise gl_Position;
layout(location = 0) out vec4 out_Normal;
layout(location = 1) out vec4 out_Texcoord0;
layout(location = 2) out vec4 out_Texcoord1;
layout(location = 3) out vec4 out_Texcoord2;
layout(location = 4) out vec4 out_Texcoord3;
layout(location = 5) out vec4 out_Texcoord4;
layout(location = 6) out vec4 out_Texcoord5;
layout(location = 7) out vec4 out_Texcoord6;
layout(location = 8) out vec4 out_Texcoord7;
layout(location = 9) out vec4 out_Color0;
layout(location = 10) out vec4 out_Color1;
layout(location = 11) out float out_Fog;


// Bindings have to match with computeResourceSlotId in dxso_util.h
// computeResourceSlotId(
//     DxsoProgramType::VertexShader,
//     DxsoBindingType::ConstantBuffer,
//     DxsoConstantBuffers::VSFixedFunction
// ) = 4
layout(set = 0, binding = 4, scalar, row_major) uniform ShaderData {
    D3D9FixedFunctionVS data;
};

layout(push_constant, scalar, row_major) uniform RenderStates {
    D3D9RenderStateInfo rs;
};


// Dynamic "spec constants"
// See d3d9_spec_constants.h for packing
// MaxSpecDwords = 6
// Binding has to match with getSpecConstantBufferSlot in dxso_util.h
layout(set = 0, binding = 31, scalar) uniform SpecConsts {
    uint specConstDword[6];
};


// Bindings have to match with computeResourceSlotId in dxso_util.h
// computeResourceSlotId(
//     DxsoProgramType::VertexShader,
//     DxsoBindingType::ConstantBuffer,
//     DxsoConstantBuffers::VSVertexBlendData
// ) = 5
layout(set = 0, binding = 5, std140, row_major) readonly buffer VertexBlendData {
    mat4 WorldViewArray[];
};


// Bindings have to match with computeResourceSlotId in dxso_util.h
// computeResourceSlotId(
//     DxsoProgramType::VertexShader,
//     DxsoBindingType::ConstantBuffer,
//     DxsoConstantBuffers::VSClipPlanes
// ) = 3
layout(set = 0, binding = 3, std140) uniform ClipPlanes {
    vec4 clipPlanes[MaxClipPlaneCount];
};


// Thanks SPIRV-Cross
spirv_instruction(set = "GLSL.std.450", id = 79) float spvNMin(float, float);
spirv_instruction(set = "GLSL.std.450", id = 79) vec2 spvNMin(vec2, vec2);
spirv_instruction(set = "GLSL.std.450", id = 79) vec3 spvNMin(vec3, vec3);
spirv_instruction(set = "GLSL.std.450", id = 79) vec4 spvNMin(vec4, vec4);
spirv_instruction(set = "GLSL.std.450", id = 81) float spvNClamp(float, float, float);
spirv_instruction(set = "GLSL.std.450", id = 81) vec2 spvNClamp(vec2, vec2, vec2);
spirv_instruction(set = "GLSL.std.450", id = 81) vec3 spvNClamp(vec3, vec3, vec3);
spirv_instruction(set = "GLSL.std.450", id = 81) vec4 spvNClamp(vec4, vec4, vec4);


// Functions to extract information from the packed VS key
// See D3D9FFShaderKeyVSData in d3d9_shader_types.h
// Please, dearest compiler, inline all of this.
uint TexcoordIndices() {
    return bitfieldExtract(data.Key.Primitive[0], 0, 24);
}
bool HasPositionT() {
    return bitfieldExtract(data.Key.Primitive[0], 24, 1) != 0;
}
bool HasColor0() {
    return bitfieldExtract(data.Key.Primitive[0], 25, 1) != 0;
}
bool HasColor1() {
    return bitfieldExtract(data.Key.Primitive[0], 26, 1) != 0;
}
bool HasPointSize() {
    return bitfieldExtract(data.Key.Primitive[0], 27, 1) != 0;
}
bool UseLighting() {
    return bitfieldExtract(data.Key.Primitive[0], 28, 1) != 0;
}
bool NormalizeNormals() {
    return bitfieldExtract(data.Key.Primitive[0], 29, 1) != 0;
}
bool LocalViewer() {
    return bitfieldExtract(data.Key.Primitive[0], 30, 1) != 0;
}
bool RangeFog() {
    return bitfieldExtract(data.Key.Primitive[0], 31, 1) != 0;
}

uint TexcoordFlags() {
    return bitfieldExtract(data.Key.Primitive[1], 0, 24);
}
uint DiffuseSource() {
    return bitfieldExtract(data.Key.Primitive[1], 24, 2);
}
uint AmbientSource() {
    return bitfieldExtract(data.Key.Primitive[1], 26, 2);
}
uint SpecularSource() {
    return bitfieldExtract(data.Key.Primitive[1], 28, 2);
}
uint EmissiveSource() {
    return bitfieldExtract(data.Key.Primitive[1], 30, 2);
}

uint TransformFlags() {
    return bitfieldExtract(data.Key.Primitive[2], 0, 24);
}
uint LightCount() {
    return bitfieldExtract(data.Key.Primitive[2], 24, 4);
}

uint TexcoordDeclMask() {
    return bitfieldExtract(data.Key.Primitive[3], 0, 24);
}
bool HasFog() {
    return bitfieldExtract(data.Key.Primitive[3], 24, 1) != 0;
}
D3D9FF_VertexBlendMode BlendMode() {
    return bitfieldExtract(data.Key.Primitive[3], 25, 2);
}
bool VertexBlendIndexed() {
    return bitfieldExtract(data.Key.Primitive[3], 27, 1) != 0;
}
uint VertexBlendCount() {
    return bitfieldExtract(data.Key.Primitive[3], 28, 3);
}
bool VertexClipping() {
    return bitfieldExtract(data.Key.Primitive[3], 31, 1) != 0;
}

uint Projected() {
    return bitfieldExtract(data.Key.Primitive[4], 0, 8);
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


vec4 PickSource(uint Source, vec4 Material) {
    if (Source == D3DMCS_MATERIAL)
        return Material;
    else if (Source == D3DMCS_COLOR1)
        return HasColor0() ? in_Color0 : vec4(0.0);
    else
        return HasColor1() ? in_Color1 : vec4(0.0);
}


void main() {
}
