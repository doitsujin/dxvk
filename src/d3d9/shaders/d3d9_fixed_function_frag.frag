#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_spirv_intrinsics : require
#extension GL_EXT_demote_to_helper_invocation : require
#extension GL_ARB_derivative_control : require
#extension GL_EXT_control_flow_attributes : require

#extension GL_EXT_nonuniform_qualifier : require // TODO: Get rid of this?

#define GLSL
#include "../d3d9_shader_types.h"

const uint TextureStageCount = 8;
const uint TextureArgCount = 3;

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

vec4 texCoords[TextureStageCount] = {
    in_Texcoord0,
    in_Texcoord1,
    in_Texcoord2,
    in_Texcoord3,
    in_Texcoord4,
    in_Texcoord5,
    in_Texcoord6,
    in_Texcoord7
};

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
    uint dynamicSpecConstDword[6];
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

layout (constant_id = 0) const uint SpecConstDword0 = 0;
layout (constant_id = 1) const uint SpecConstDword1 = 0;
layout (constant_id = 2) const uint SpecConstDword2 = 0;
layout (constant_id = 3) const uint SpecConstDword3 = 0;
layout (constant_id = 4) const uint SpecConstDword4 = 0;
layout (constant_id = 5) const uint SpecConstDword5 = 0;
layout (constant_id = 6) const uint SpecConstDword6 = 0;
layout (constant_id = 7) const uint SpecConstDword7 = 0;
layout (constant_id = 8) const uint SpecConstDword8 = 0;
layout (constant_id = 9) const uint SpecConstDword9 = 0;
layout (constant_id = 10) const uint SpecConstDword10 = 0;
layout (constant_id = 11) const uint SpecConstDword11 = 0;
layout (constant_id = 12) const uint SpecConstDword12 = 0;

const int drefScaling = 0; // TODO

bool SpecIsOptimized() {
    return SpecConstDword12 != 0;
}

uint SpecSamplerType() {
    uint dword = SpecIsOptimized() ? SpecConstDword0 : dynamicSpecConstDword[0];
    return bitfieldExtract(dynamicSpecConstDword[0], 0, 32);
}
uint SpecSamplerDepthMode() {
    uint dword = SpecIsOptimized() ? SpecConstDword1 : dynamicSpecConstDword[1];
    return bitfieldExtract(dword, 0, 21);
}
uint SpecAlphaCompareOp() {
    uint dword = SpecIsOptimized() ? SpecConstDword1 : dynamicSpecConstDword[1];
    return bitfieldExtract(dword, 21, 3);
}
uint SpecPointMode() {
    uint dword = SpecIsOptimized() ? SpecConstDword1 : dynamicSpecConstDword[1];
    return bitfieldExtract(dword, 24, 2);
}
uint SpecVertexFogMode() {
    uint dword = SpecIsOptimized() ? SpecConstDword1 : dynamicSpecConstDword[1];
    return bitfieldExtract(dword, 26, 2);
}
uint SpecPixelFogMode() {
    uint dword = SpecIsOptimized() ? SpecConstDword1 : dynamicSpecConstDword[1];
    return bitfieldExtract(dword, 28, 2);
}
bool SpecFogEnabled() {
    uint dword = SpecIsOptimized() ? SpecConstDword1 : dynamicSpecConstDword[1];
    return bitfieldExtract(dword, 30, 1) != 0;
}
uint SpecSamplerNull() {
    uint dword = SpecIsOptimized() ? SpecConstDword2 : dynamicSpecConstDword[2];
    return bitfieldExtract(dword, 0, 21);
}
uint SpecProjectionType() {
    uint dword = SpecIsOptimized() ? SpecConstDword2 : dynamicSpecConstDword[2];
    return bitfieldExtract(dword, 21, 6);
}
uint SpecAlphaPrecisionBits() {
    uint dword = SpecIsOptimized() ? SpecConstDword2 : dynamicSpecConstDword[2];
    return bitfieldExtract(dword, 27, 4);
}
uint SpecVertexShaderBools() {
    uint dword = SpecIsOptimized() ? SpecConstDword3 : dynamicSpecConstDword[3];
    return bitfieldExtract(dword, 0, 16);
}
uint SpecPixelShaderBools() {
    uint dword = SpecIsOptimized() ? SpecConstDword3 : dynamicSpecConstDword[3];
    return bitfieldExtract(dword, 16, 16);
}
uint SpecFetch4() {
    uint dword = SpecIsOptimized() ? SpecConstDword4 : dynamicSpecConstDword[4];
    return bitfieldExtract(dword, 0, 16);
}
uint SpecDrefClamp() {
    uint dword = SpecIsOptimized() ? SpecConstDword5 : dynamicSpecConstDword[5];
    return bitfieldExtract(dword, 0, 21);
}
uint SpecClipPlaneCount() {
    uint dword = SpecIsOptimized() ? SpecConstDword5 : dynamicSpecConstDword[5];
    return bitfieldExtract(dword, 26, 3);
}


vec4 DoFixedFunctionFog(vec4 vPos, vec4 oColor) {
    vec3 fogColor = vec3(rs.fogColor[0], rs.fogColor[1], rs.fogColor[2]);
    float fogScale = rs.fogScale;
    float fogEnd = rs.fogEnd;
    float fogDensity = rs.fogDensity;
    D3DFOGMODE fogMode = SpecPixelFogMode();
    bool fogEnabled = SpecFogEnabled();
    if (!fogEnabled) {
        return oColor;
    }

    float w = vPos.w;
    float z = vPos.z;
    float depth = z * (1.0 / w);
    float fogFactor;
    switch (fogMode) {
        case D3DFOG_NONE:
            fogFactor = in_Fog;
            break;

        // (end - d) / (end - start)
        case D3DFOG_LINEAR:
            fogFactor = fogEnd - depth;
            fogFactor = fogFactor * fogScale;
            fogFactor = spvNClamp(fogFactor, 0.0, 1.0);
            break;

        // 1 / (e^[d * density])^2
        case D3DFOG_EXP2:
        // 1 / (e^[d * density])
        case D3DFOG_EXP:
            fogFactor = depth * fogDensity;

            if (fogMode == D3DFOG_EXP2)
                fogFactor *= fogFactor;

            // Provides the rcp.
            fogFactor = -fogFactor;
            fogFactor = exp(fogFactor);
            break;
    }

    vec4 color = oColor;
    vec3 color3 = color.rgb;
    vec3 fogFact3 = vec3(fogFactor);
    vec3 lerpedFrog = mix(fogColor, color3, fogFact3);
    return vec4(lerpedFrog.r, lerpedFrog.g, lerpedFrog.b, color.a);
}


// [D3D8] Scale Dref to [0..(2^N - 1)] for D24S8 and D16 if Dref scaling is enabled
vec4 scaleDref(vec4 texCoord, int referenceIdx) {
    float reference = texCoord[referenceIdx];
    if (drefScaling == 0) {
        return texCoord;
    }
    float maxDref = 1.0 / (float(1 << drefScaling) - 1.0);
    reference *= maxDref;
    texCoord[referenceIdx] = reference;
    return texCoord;
}


vec4 DoBumpmapCoords(uint stage, vec4 baseCoords, vec4 previousStageTextureVal) {
    stage = stage - 1;

    vec4 coords = baseCoords;
    [[unroll]]
    for (uint i = 0; i < 2; i++) {
        float tc_m_n = coords[i];
        vec2 bm = vec2(sharedData.Stages[stage].BumpEnvMat[0][0], sharedData.Stages[stage].BumpEnvMat[0][1]);
        vec2 t = previousStageTextureVal.xy;
        float result = tc_m_n + dot(bm, t);
        coords[i] = result;
    }
    return coords;
}


uint LoadSamplerHeapIndex(uint samplerBindingIndex) {
    uint packedSamplerIndex = packedSamplerIndices[samplerBindingIndex / 2u];
    return bitfieldExtract(packedSamplerIndex, 16 * (int(samplerBindingIndex) & 1), 16);
}


// TODO: Passing the index here makes non-uniform necessary, solve that
vec4 GetTexture(uint stage, vec4 texcoord, vec4 previousStageTextureVal) {
    uint textureType = D3DRTYPE_TEXTURE + TextureType(stage);

    bool shouldProject = Projected(stage);
    float projValue = 1.0;
    if (shouldProject) {
        // Always use w, the vertex shader puts the correct value there.
        projValue = texcoord.w;
        if (textureType == D3DRTYPE_TEXTURE) {
            // For 2D textures we divide by the z component, so move the w component up by one.
            texcoord.z = projValue;
        }
    }

    if (stage != 0 && (
        ColorOp(stage - 1) == D3DTOP_BUMPENVMAP
        || ColorOp(stage - 1) == D3DTOP_BUMPENVMAPLUMINANCE)) {
        if (shouldProject) {
            float projRcp = 1.0 / projValue;
            texcoord *= projRcp;
        }

        texcoord = DoBumpmapCoords(stage, texcoord, previousStageTextureVal);

        shouldProject = false;
    }

    vec4 texVal;
    switch (textureType) {
        case D3DRTYPE_TEXTURE:
            if (SampleDref(stage))
                texVal = texture(sampler2DShadow(t2d[stage], sampler_heap[LoadSamplerHeapIndex(stage)]), scaleDref(texcoord, 2).xyz).xxxx;
            else if (shouldProject)
                texVal = textureProj(sampler2D(t2d[stage], sampler_heap[LoadSamplerHeapIndex(stage)]), texcoord.xyz);
            else
                texVal = texture(sampler2D(t2d[stage], sampler_heap[LoadSamplerHeapIndex(stage)]), texcoord.xy);
            break;
        case D3DRTYPE_CUBETEXTURE:
            if (SampleDref(stage))
                texVal = texture(samplerCubeShadow(tcube[stage], sampler_heap[LoadSamplerHeapIndex(stage)]), scaleDref(texcoord, 3)).xxxx;
            else if (shouldProject) {
                texVal = texture(samplerCube(tcube[stage], sampler_heap[LoadSamplerHeapIndex(stage)]), texcoord.xyz / texcoord.w); // TODO: ?
                //texVal = textureProj(samplerCube(tcube[stage], sampler_heap[LoadSamplerHeapIndex(stage)]), texcoord.xyzw);
            } else
                texVal = texture(samplerCube(tcube[stage], sampler_heap[LoadSamplerHeapIndex(stage)]), texcoord.xyz);
            break;
        case D3DRTYPE_VOLUMETEXTURE:
            if (SampleDref(stage))
                texVal = vec4(0.0); // TODO: ?
            else if (shouldProject)
                texVal = textureProj(sampler3D(t3d[stage], sampler_heap[LoadSamplerHeapIndex(stage)]), texcoord);
            else
                texVal = texture(sampler3D(t3d[stage], sampler_heap[LoadSamplerHeapIndex(stage)]), texcoord.xyz);
            break;
    }

    if (stage != 0 && ColorOp(stage - 1) == D3DTOP_BUMPENVMAPLUMINANCE) {
        float lScale = sharedData.Stages[stage - 1].BumpEnvLScale;
        float lOffset = sharedData.Stages[stage - 1].BumpEnvLOffset;
        float scale = texVal.z;
        scale *= lScale;
        scale += lOffset;
        scale = clamp(scale, 0.0, 1.0);
        texVal *= scale;
    }

    return texVal;
}


vec4 GetArg(uint stage, uint arg, vec4 current, vec4 temp, vec4 textureVal) {
    vec4 reg = vec4(1.0);
    switch (arg & D3DTA_SELECTMASK) {
        case D3DTA_CONSTANT: {
             reg = vec4(
                sharedData.Stages[stage].Constant[0],
                sharedData.Stages[stage].Constant[1],
                sharedData.Stages[stage].Constant[2],
                sharedData.Stages[stage].Constant[3]
            );
            break;
        }
        case D3DTA_CURRENT:
            reg = current;
            break;
        case D3DTA_DIFFUSE:
            reg = in_Color0;
            break;
        case D3DTA_SPECULAR:
            reg = in_Color1;
            break;
        case D3DTA_TEMP:
            reg = temp;
            break;
        case D3DTA_TEXTURE:
            reg = textureVal;
            break;
        case D3DTA_TFACTOR:
            reg = data.textureFactor;
    }

    // reg = 1 - reg
    if ((arg & D3DTA_COMPLEMENT) != 0)
        reg = vec4(1.0) - reg;

    // reg = reg.wwww
    if ((arg & D3DTA_ALPHAREPLICATE) != 0)
        reg = reg.aaaa;

    return reg;
}

vec4[TextureArgCount] ProcessArgs(uint stage, uint args[TextureArgCount], vec4 current, vec4 temp, vec4 textureVal) {
    vec4 argVals[TextureArgCount];
    [[unroll]]
    for (uint argI = 0; argI < TextureArgCount; argI++) {
        argVals[argI] = GetArg(stage, args[argI], current, temp, textureVal);
    }
    return argVals;
}

vec4 complement(vec4 val) {
    return vec4(1.0) - val;
}

vec4 saturate(vec4 val) {
    return clamp(val, vec4(0.0), vec4(1.0));
}

vec4 DoOp(uint op, vec4 dst, vec4 arg[TextureArgCount], vec4 current, vec4 textureVal) {
    switch (op) {
        case D3DTOP_SELECTARG1:
            return arg[1];

        case D3DTOP_SELECTARG2:
            return arg[2];

        case D3DTOP_MODULATE4X:
            return arg[1] * arg[2] * 4.0;

        case D3DTOP_MODULATE2X:
            return arg[1] * arg[2] * 2.0;

        case D3DTOP_MODULATE:
            return arg[1] * arg[2];

        case D3DTOP_ADDSIGNED2X:
            return saturate(2.0 * (arg[1] + (arg[2] - vec4(0.5))));

        case D3DTOP_ADDSIGNED:
            return saturate(arg[1] + (arg[2] - vec4(0.5)));

        case D3DTOP_ADD:
            return saturate(arg[1] + arg[2]);

        case D3DTOP_SUBTRACT:
            return saturate(arg[1] - arg[2]);

        case D3DTOP_ADDSMOOTH:
            return fma(complement(arg[1]), arg[2], arg[1]);

        case D3DTOP_BLENDDIFFUSEALPHA:
            return mix(arg[2], arg[1], in_Color0.aaaa);

        case D3DTOP_BLENDTEXTUREALPHA:
            return mix(arg[2], arg[1], textureVal.aaaa);

        case D3DTOP_BLENDFACTORALPHA:
            return mix(arg[2], arg[1], data.textureFactor.aaaa);

        case D3DTOP_BLENDTEXTUREALPHAPM:
            return saturate(fma(arg[2], complement(textureVal.aaaa), arg[1]));

        case D3DTOP_BLENDCURRENTALPHA:
            return mix(arg[2], arg[1], current.aaaa);

        case D3DTOP_PREMODULATE:
            return dst; // Not implemented

        case D3DTOP_MODULATEALPHA_ADDCOLOR:
            return saturate(fma(arg[1].aaaa, arg[2], arg[1]));

        case D3DTOP_MODULATECOLOR_ADDALPHA:
            return saturate(fma(arg[1], arg[2], arg[1].aaaa));

        case D3DTOP_MODULATEINVALPHA_ADDCOLOR:
            return saturate(fma(complement(arg[1].aaaa), arg[2], arg[1]));

        case D3DTOP_MODULATEINVCOLOR_ADDALPHA:
            return saturate(fma(complement(arg[1]), arg[2], arg[1].aaaa));

        case D3DTOP_BUMPENVMAPLUMINANCE:
        case D3DTOP_BUMPENVMAP:
            // Load texture for the next stage...
            return dst;

        case D3DTOP_DOTPRODUCT3:
            return saturate(vec4(dot(arg[1].rgb - vec3(0.5), arg[2].rgb - vec3(0.5)) * 4.0));

        case D3DTOP_MULTIPLYADD:
            return saturate(fma(arg[1], arg[2], arg[0]));

        case D3DTOP_LERP:
            return mix(arg[2], arg[1], arg[0]);

        default:
            // Unhandled texture op!
            return dst;

    }

    return vec4(0.0);
}


void alphaTestPS() {
    uint alphaFunc = SpecAlphaCompareOp();
    uint alphaPrecision = SpecAlphaPrecisionBits();
    uint alphaRefInitial = rs.alphaRef;
    float alphaRef;
    float alpha = out_Color0.a;

    if (alphaFunc == VK_COMPARE_OP_ALWAYS) {
        return;
    }

    // Check if the given bit precision is supported
    bool useIntPrecision = alphaPrecision <= 8;
    if (useIntPrecision) {
        // Adjust alpha ref to the given range
        uint alphaRefInt = (alphaRefInitial << alphaPrecision) | (alphaRefInitial >> (8 - alphaPrecision));

        // Convert alpha ref to float since we'll do the comparison based on that
        alphaRef = float(alphaRefInt);

        // Adjust alpha to the given range and round
        float alphaFactor = float((256u << alphaPrecision) - 1u);

        alpha = round(alpha * alphaFactor);
    } else {
        alphaRef = float(alphaRefInitial) / 255.0;
    }

    bool atestResult;
    switch (alphaFunc) {
        case VK_COMPARE_OP_NEVER:
            atestResult = false;

        case VK_COMPARE_OP_LESS:
            atestResult = alpha < alphaRef;

        case VK_COMPARE_OP_EQUAL:
            atestResult = alpha == alphaRef;

        case VK_COMPARE_OP_LESS_OR_EQUAL:
            atestResult = alpha <= alphaRef;

        case VK_COMPARE_OP_GREATER:
            atestResult = alpha > alphaRef;

        case VK_COMPARE_OP_NOT_EQUAL:
            atestResult = alpha != alphaRef;

        case VK_COMPARE_OP_GREATER_OR_EQUAL:
            atestResult = alpha >= alphaRef;

        default:
        case VK_COMPARE_OP_ALWAYS:
            atestResult = true;
    }

    bool atestDiscard = !atestResult;
    if (atestDiscard) {
        discard;
    }
}

void main() {
    // in_Color0 is diffuse
    // in_Color1 is specular

    // Current starts of as equal to diffuse.
    vec4 current = in_Color0;
    // Temp starts off as equal to vec4(0)
    vec4 temp = vec4(0.0);

    vec4 unboundTextureConst = vec4(0.0, 0.0, 0.0, 1.0);

    vec4 previousStageTextureVal = vec4(0.0);

    uint pointMode = SpecPointMode();
    bool isSprite = bitfieldExtract(pointMode, 1, 1) == 1u;

    for (uint i = 0; i < TextureStageCount; i++) {
        vec4 dst = ResultIsTemp(i) ? temp : current;

        uint colorOp = ColorOp(i);

        // This cancels all subsequent stages.
        if (colorOp == D3DTOP_DISABLE)
            break;

        uint colorArgs[TextureArgCount] = {
            ColorArg0(i),
            ColorArg1(i),
            ColorArg2(i)
        };
        uint alphaOp = AlphaOp(i);
        uint alphaArgs[TextureArgCount] = {
            AlphaArg0(i),
            AlphaArg1(i),
            AlphaArg2(i)
        };

        vec4 textureVal = vec4(0.0);
        bool usesTexture = false;
        [[unroll]]
        for (uint argI = 0; argI < TextureArgCount; argI++) {
            usesTexture = usesTexture || colorArgs[argI] == D3DTA_TEXTURE || alphaArgs[argI] == D3DTA_TEXTURE;
        }
        if (usesTexture) {
            // We need to replace TEXCOORD inputs with gl_PointCoord
            // if D3DRS_POINTSPRITEENABLE is set.
            vec4 texCoord = isSprite ? vec4(gl_PointCoord, 0.0, 0.0) : texCoords[i];
            textureVal = TextureBound(i) ? GetTexture(i, texCoord, previousStageTextureVal) : vec4(0.0, 0.0, 0.0, 1.0);
        }

        // Fast path if alpha/color path is identical.
        // D3DTOP_DOTPRODUCT3 also has special quirky behaviour here.
        bool fastPath = colorOp == alphaOp && colorArgs == alphaArgs;
        if (fastPath || colorOp == D3DTOP_DOTPRODUCT3) {
            vec4 colorArgVals[TextureArgCount] = ProcessArgs(i, colorArgs, current, temp, textureVal);
            dst = DoOp(colorOp, dst, colorArgVals, current, textureVal);
        } else {
            vec4 colorResult = dst;
            vec4 alphaResult = dst;

            vec4 colorArgVals[TextureArgCount] = ProcessArgs(i, colorArgs, current, temp, textureVal);
            colorResult = DoOp(colorOp, dst, colorArgVals, current, textureVal);

            if (alphaOp != D3DTOP_DISABLE) {
                vec4 alphaArgVals[TextureArgCount] = ProcessArgs(i, alphaArgs, current, temp, textureVal);
                alphaResult = DoOp(alphaOp, dst, alphaArgVals, current, textureVal);
            }

            // src0.x, src0.y, src0.z src1.w
            if (colorResult != dst)
                dst = vec4(colorResult.rgb, dst.a);

            // src0.x, src0.y, src0.z src1.w
            // But we flip src0, src1 to be inverse of color.
            if (alphaResult != dst)
                dst = vec4(dst.rgb, alphaResult.a);
        }

        if (ResultIsTemp(i)) {
            temp = dst;
        } else {
            current = dst;
        }
        previousStageTextureVal = textureVal;
    }

    // TODO: Should this be done per-stage?
    // The FF generator only uses stage 0
    if (GlobalSpecularEnable(0)) {
        vec4 specular = in_Color1 * vec4(1.0, 1.0, 1.0, 0.0);
        current += specular;
    }

    current = DoFixedFunctionFog(gl_FragCoord, current);

    out_Color0 = current;
    alphaTestPS();
}
