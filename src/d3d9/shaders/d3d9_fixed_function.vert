#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : require

#define GLSL
#include "../d3d9_shader_types.h"


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
layout(location = 16) in float in_BlendWeight;
layout(location = 17) in vec4 in_BlendIndices;


// The locations need to match with RegisterLinkerSlot in dxso_util.cpp
invariant gl_Position;
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
layout(location = 11) out vec4 out_Fog;


// Bindings have to match with computeResourceSlotId in dxso_util.h
// computeResourceSlotId(
//     DxsoProgramType::VertexShader,
//     DxsoBindingType::ConstantBuffer,
//     DxsoConstantBuffers::VSFixedFunction
// ) = 4
layout(set = 0, binding = 4, scalar, row_major) uniform ShaderData {
    D3D9FixedFunctionVS data;
};

layout(push_constant, scalar) uniform RenderStates {
    D3D9RenderStateInfo rs;
};


// Dynamic "spec constants"
// See d3d9_spec_constants.h for packing
// MaxSpecDwords = 6
// Binding has to match with getSpecConstantBufferSlot in dxso_util.h
layout( set = 0, binding = 30, scalar) uniform SpecConsts {
    uint dword[6];
};


// TEMP, REFACTOR LATER
struct D3D9FFConstants {
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
};

struct D3D9FFIn {
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
};

struct D3D9FFOut {
    uint32_t POSITION;
    uint32_t POINTSIZE;
    uint32_t NORMAL;
    uint32_t TEXCOORD[8];
    uint32_t COLOR[2];
    uint32_t FOG;
};

struct D3D9FFVertexData {
    uint32_t constantBuffer;
    uint32_t vertexBlendData;
    uint32_t lightType;

    D3D9FFConstants constants;

    D3D9FFIn ff_in;

    D3D9FFOut ff_out;
};
// END TEMP


// Functions to extract information from the packed VS key
// See D3D9FFShaderKeyVSData in d3d9_shader_types.h
// Please, dearest compiler, inline all of this.
D3D9FF_VertexBlendMode BlendMode() {
    return bitfieldExtract(data.Key.Primitive[3], 25, 2);
}
bool HasPositionT() {
    return bitfieldExtract(data.Key.Primitive[0], 24, 1) != 0;
}
uint VertexBlendCount() {
    return bitfieldExtract(data.Key.Primitive[3], 28, 3);
}
bool VertexBlendIndexed() {
    return bitfieldExtract(data.Key.Primitive[3], 27, 1) != 0;
}

void main() {
    vec4 vtx = in_Position0;
    vec3 normal = in_Normal0.xyz;

    if (BlendMode() == D3D9FF_VertexBlendMode_Tween) {
        vec4 vtx1 = in_Position1;
        vec3 normal1 = in_Normal1.xyz;
        vtx = mix(vtx, vtx1, data.TweenFactor);
        normal = mix(normal, normal1, data.TweenFactor);
    }

    if (!HasPositionT()) {
        if (BlendMode() == D3D9FF_VertexBlendMode_Normal) {
            float blendWeightRemaining = 1.0;
            vec4 vtxSum = vec4(0.0);
            vec3 nrmSum = vec3(0.0);

            for (uint i = 0; i < VertexBlendCount(); i++) {
                uint arrayIndex;
                if (VertexBlendIndexed()) {
                    arrayIndex = uint(round(in_BlendIndices[i]));
                } else {
                    arrayIndex = i;
                }
                //mat4 worldView =
            }
        }
    }
}
