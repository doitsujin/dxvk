#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_spirv_intrinsics : require
#extension GL_EXT_demote_to_helper_invocation : require
#extension GL_ARB_derivative_control : require
#extension GL_EXT_control_flow_attributes : require
#extension GL_EXT_nonuniform_qualifier : require


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


const uint SpecConstOptimizedTextureStageCount = 4;
const uint TextureArgCount = 3;
const uint MaxSharedPushDataSize = 64;

#include "d3d9_fixed_function_common.glsl"

struct D3D9FFTextureStage {
    uint Primitive[2];
};

struct D3D9FixedFunctionPS {
    vec4 textureFactor;
    D3D9FFTextureStage Stages[8];
};

struct D3D9SharedPSStage {
    float Constant[4];
    float BumpEnvMat[2][2];
    float BumpEnvLScale;
    float BumpEnvLOffset;
    float Padding[2];
};

struct D3D9SharedPS {
    D3D9SharedPSStage Stages[TextureStageCount];
};

const uint D3DTOP_DISABLE                   = 1;
const uint D3DTOP_SELECTARG1                = 2;
const uint D3DTOP_SELECTARG2                = 3;
const uint D3DTOP_MODULATE                  = 4;
const uint D3DTOP_MODULATE2X                = 5;
const uint D3DTOP_MODULATE4X                = 6;
const uint D3DTOP_ADD                       = 7;
const uint D3DTOP_ADDSIGNED                 = 8;
const uint D3DTOP_ADDSIGNED2X               = 9;
const uint D3DTOP_SUBTRACT                  = 10;
const uint D3DTOP_ADDSMOOTH                 = 11;
const uint D3DTOP_BLENDDIFFUSEALPHA         = 12;
const uint D3DTOP_BLENDTEXTUREALPHA         = 13;
const uint D3DTOP_BLENDFACTORALPHA          = 14;
const uint D3DTOP_BLENDTEXTUREALPHAPM       = 15;
const uint D3DTOP_BLENDCURRENTALPHA         = 16;
const uint D3DTOP_PREMODULATE               = 17;
const uint D3DTOP_MODULATEALPHA_ADDCOLOR    = 18;
const uint D3DTOP_MODULATECOLOR_ADDALPHA    = 19;
const uint D3DTOP_MODULATEINVALPHA_ADDCOLOR = 20;
const uint D3DTOP_MODULATEINVCOLOR_ADDALPHA = 21;
const uint D3DTOP_BUMPENVMAP                = 22;
const uint D3DTOP_BUMPENVMAPLUMINANCE       = 23;
const uint D3DTOP_DOTPRODUCT3               = 24;
const uint D3DTOP_MULTIPLYADD               = 25;
const uint D3DTOP_LERP                      = 26;

const uint D3DTA_SELECTMASK     = 0x0000000f;
const uint D3DTA_DIFFUSE        = 0x00000000;
const uint D3DTA_CURRENT        = 0x00000001;
const uint D3DTA_TEXTURE        = 0x00000002;
const uint D3DTA_TFACTOR        = 0x00000003;
const uint D3DTA_SPECULAR       = 0x00000004;
const uint D3DTA_TEMP           = 0x00000005;
const uint D3DTA_CONSTANT       = 0x00000006;
const uint D3DTA_COMPLEMENT     = 0x00000010;
const uint D3DTA_ALPHAREPLICATE = 0x00000020;

const uint D3DRTYPE_SURFACE       = 1;
const uint D3DRTYPE_VOLUME        = 2;
const uint D3DRTYPE_TEXTURE       = 3;
const uint D3DRTYPE_VOLUMETEXTURE = 4;
const uint D3DRTYPE_CUBETEXTURE   = 5;
const uint D3DRTYPE_VERTEXBUFFER  = 6;
const uint D3DRTYPE_INDEXBUFFER   = 7;

const uint VK_COMPARE_OP_NEVER            = 0;
const uint VK_COMPARE_OP_LESS             = 1;
const uint VK_COMPARE_OP_EQUAL            = 2;
const uint VK_COMPARE_OP_LESS_OR_EQUAL    = 3;
const uint VK_COMPARE_OP_GREATER          = 4;
const uint VK_COMPARE_OP_NOT_EQUAL        = 5;
const uint VK_COMPARE_OP_GREATER_OR_EQUAL = 6;
const uint VK_COMPARE_OP_ALWAYS           = 7;

const uint PerTextureStageSpecConsts = SpecFFTextureStage1ColorOp - SpecFFTextureStage0ColorOp;


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

    layout(offset = MaxSharedPushDataSize) uint packedSamplerIndices[TextureStageCount / 2];
};

layout(set = 0, binding = 13) uniform texture2D t2d[TextureStageCount];
layout(set = 0, binding = 13) uniform textureCube tcube[TextureStageCount];
layout(set = 0, binding = 13) uniform texture3D t3d[TextureStageCount];

layout(set = 1, binding = 0) uniform sampler sampler_heap[];


// Functions to extract information from the packed texture stages
uint colorOp(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[0], 0, 5);
}
uint colorArg0(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[0], 5, 6);
}
uint colorArg1(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[0], 11, 6);
}
uint colorArg2(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[0], 17, 6);
}

uint alphaOp(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[0], 23, 5);
}
uint alphaArg0(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[1], 0, 6);
}
uint alphaArg1(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[1], 6, 6);
}
uint alphaArg2(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[1], 12, 6);
}

bool resultIsTemp(uint stageIndex) {
    return bitfieldExtract(data.Stages[stageIndex].Primitive[1], 18, 1) != 0;
}


vec4 calculateFog(vec4 vPos, vec4 oColor) {
    vec3 fogColor = vec3(rs.fogColor[0], rs.fogColor[1], rs.fogColor[2]);
    float fogScale = rs.fogScale;
    float fogEnd = rs.fogEnd;
    float fogDensity = rs.fogDensity;
    D3DFOGMODE fogMode = specUint(SpecPixelFogMode);
    bool fogEnabled = specBool(SpecFogEnabled);
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
float adjustDref(float reference, uint samplerIndex) {
    uint drefScaleFactor = specUint(SpecDrefScaling);
    if (drefScaleFactor != 0) {
        float maxDref = 1.0 / (float(1 << drefScaleFactor) - 1.0);
        reference *= maxDref;
    }
    if (specBool(SpecSamplerDrefClamp, samplerIndex)) {
        reference = clamp(reference, 0.0, 1.0);
    }
    return reference;
}


vec4 calculateBumpmapCoords(uint stage, vec4 baseCoords, vec4 previousStageTextureVal) {
    uint previousStage = stage - 1;

    vec4 coords = baseCoords;
    [[unroll]]
    for (uint i = 0; i < 2; i++) {
        float tc_m_n = coords[i];
        vec2 bm = vec2(sharedData.Stages[previousStage].BumpEnvMat[i][0], sharedData.Stages[previousStage].BumpEnvMat[i][1]);
        vec2 t = previousStageTextureVal.xy;
        float result = tc_m_n + dot(bm, t);
        coords[i] = result;
    }
    return coords;
}


uint loadSamplerHeapIndex(uint samplerBindingIndex) {
    uint packedSamplerIndex = packedSamplerIndices[samplerBindingIndex / 2u];
    return bitfieldExtract(packedSamplerIndex, 16 * (int(samplerBindingIndex) & 1), 16);
}


vec4 sampleTexture(uint stage, vec4 texcoord, vec4 previousStageTextureVal) {
    if (specBool(SpecSamplerProjected, stage)) {
        texcoord /= texcoord.w;
    }

    uint previousStageColorOp = 0;
    if (stage > 0) {
        bool isPreviousStageOptimized = specIsOptimized() && stage - 1 < SpecConstOptimizedTextureStageCount;
        previousStageColorOp = isPreviousStageOptimized ? specUint(SpecFFTextureStage0ColorOp + PerTextureStageSpecConsts * (stage - 1)) : colorOp(stage - 1);
    }

    if (stage != 0 && (
        previousStageColorOp == D3DTOP_BUMPENVMAP
        || previousStageColorOp == D3DTOP_BUMPENVMAPLUMINANCE)) {
        texcoord = calculateBumpmapCoords(stage, texcoord, previousStageTextureVal);
    }

    vec4 texVal;
    uint textureType = D3DRTYPE_TEXTURE + specUint(SpecSamplerType, 2u * stage, 2u);
    switch (textureType) {
        case D3DRTYPE_TEXTURE:
            if (specBool(SpecSamplerDepthMode, stage)) {
                texcoord.z = adjustDref(texcoord.z, stage);
                texVal = texture(sampler2DShadow(t2d[stage], sampler_heap[loadSamplerHeapIndex(stage)]), texcoord.xyz).xxxx;
            } else {
                texVal = texture(sampler2D(t2d[stage], sampler_heap[loadSamplerHeapIndex(stage)]), texcoord.xy);
            }
            break;
        case D3DRTYPE_CUBETEXTURE:
            if (specBool(SpecSamplerDepthMode, stage)) {
                texcoord.w = adjustDref(texcoord.w, stage);
                texVal = texture(samplerCubeShadow(tcube[stage], sampler_heap[loadSamplerHeapIndex(stage)]), texcoord).xxxx;
            } else {
                texVal = texture(samplerCube(tcube[stage], sampler_heap[loadSamplerHeapIndex(stage)]), texcoord.xyz);
            }
            break;
        case D3DRTYPE_VOLUMETEXTURE:
            texVal = texture(sampler3D(t3d[stage], sampler_heap[loadSamplerHeapIndex(stage)]), texcoord.xyz);
            break;
        default:
            // This should never happen unless there's a major bug in the API implementation.
            // Produce a value that's obviously wrong to make it obvious when it somehow does happen.
            texVal = vec4(999.9);
            break;
    }

    if (stage != 0 && previousStageColorOp == D3DTOP_BUMPENVMAPLUMINANCE) {
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


vec4 readArgValue(uint stage, uint arg, vec4 current, vec4 temp, vec4 textureVal) {
    vec4 reg = vec4(1.0);
    switch (arg & D3DTA_SELECTMASK) {
        case D3DTA_CONSTANT:
            reg = vec4(
                sharedData.Stages[stage].Constant[0],
                sharedData.Stages[stage].Constant[1],
                sharedData.Stages[stage].Constant[2],
                sharedData.Stages[stage].Constant[3]
            );
            break;
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
            break;
    }

    // reg = 1 - reg
    if ((arg & D3DTA_COMPLEMENT) != 0)
        reg = vec4(1.0) - reg;

    // reg = reg.wwww
    if ((arg & D3DTA_ALPHAREPLICATE) != 0)
        reg = reg.aaaa;

    return reg;
}

struct TextureStageArguments {
    uint arg0;
    uint arg1;
    uint arg2;
};

struct TextureStageArgumentValues {
    vec4 arg0;
    vec4 arg1;
    vec4 arg2;
};

TextureStageArgumentValues readArgValues(uint stage, const TextureStageArguments args, vec4 current, vec4 temp, vec4 textureVal) {
    TextureStageArgumentValues argVals;
    argVals.arg0 = readArgValue(stage, args.arg0, current, temp, textureVal);
    argVals.arg1 = readArgValue(stage, args.arg1, current, temp, textureVal);
    argVals.arg2 = readArgValue(stage, args.arg2, current, temp, textureVal);
    return argVals;
}

uint repackArg(uint arg) {
    // Move the flags by 1 bit. 0x18 = 0b11000
    return (arg & ~0x18) | ((arg & 0x18) << 1u);
}

vec4 complement(vec4 val) {
    return vec4(1.0) - val;
}

vec4 saturate(vec4 val) {
    return clamp(val, vec4(0.0), vec4(1.0));
}

vec4 calculateTextureStage(uint op, vec4 dst, const TextureStageArgumentValues arg, vec4 current, vec4 textureVal) {
    switch (op) {
        case D3DTOP_SELECTARG1:
            return arg.arg1;

        case D3DTOP_SELECTARG2:
            return arg.arg2;

        case D3DTOP_MODULATE4X:
            return arg.arg1 * arg.arg2 * 4.0;

        case D3DTOP_MODULATE2X:
            return arg.arg1 * arg.arg2 * 2.0;

        case D3DTOP_MODULATE:
            return arg.arg1 * arg.arg2;

        case D3DTOP_ADDSIGNED2X:
            return saturate(2.0 * (arg.arg1 + (arg.arg2 - vec4(0.5))));

        case D3DTOP_ADDSIGNED:
            return saturate(arg.arg1 + (arg.arg2 - vec4(0.5)));

        case D3DTOP_ADD:
            return saturate(arg.arg1 + arg.arg2);

        case D3DTOP_SUBTRACT:
            return saturate(arg.arg1 - arg.arg2);

        case D3DTOP_ADDSMOOTH:
            return fma(complement(arg.arg1), arg.arg2, arg.arg1);

        case D3DTOP_BLENDDIFFUSEALPHA:
            return mix(arg.arg2, arg.arg1, in_Color0.aaaa);

        case D3DTOP_BLENDTEXTUREALPHA:
            return mix(arg.arg2, arg.arg1, textureVal.aaaa);

        case D3DTOP_BLENDFACTORALPHA:
            return mix(arg.arg2, arg.arg1, data.textureFactor.aaaa);

        case D3DTOP_BLENDTEXTUREALPHAPM:
            return saturate(fma(arg.arg2, complement(textureVal.aaaa), arg.arg1));

        case D3DTOP_BLENDCURRENTALPHA:
            return mix(arg.arg2, arg.arg1, current.aaaa);

        case D3DTOP_PREMODULATE:
            return dst; // Not implemented

        case D3DTOP_MODULATEALPHA_ADDCOLOR:
            return saturate(fma(arg.arg1.aaaa, arg.arg2, arg.arg1));

        case D3DTOP_MODULATECOLOR_ADDALPHA:
            return saturate(fma(arg.arg1, arg.arg2, arg.arg1.aaaa));

        case D3DTOP_MODULATEINVALPHA_ADDCOLOR:
            return saturate(fma(complement(arg.arg1.aaaa), arg.arg2, arg.arg1));

        case D3DTOP_MODULATEINVCOLOR_ADDALPHA:
            return saturate(fma(complement(arg.arg1), arg.arg2, arg.arg1.aaaa));

        case D3DTOP_BUMPENVMAPLUMINANCE:
        case D3DTOP_BUMPENVMAP:
            // Load texture for the next stage...
            return dst;

        case D3DTOP_DOTPRODUCT3:
            return saturate(vec4(dot(arg.arg1.rgb - vec3(0.5), arg.arg2.rgb - vec3(0.5)) * 4.0));

        case D3DTOP_MULTIPLYADD:
            return saturate(fma(arg.arg1, arg.arg2, arg.arg0));

        case D3DTOP_LERP:
            return mix(arg.arg2, arg.arg1, arg.arg0);

        default:
            // Unhandled texture op!
            return dst;

    }

    return vec4(0.0);
}


void alphaTest() {
    uint alphaFunc = specUint(SpecAlphaCompareOp);
    uint alphaPrecision = specUint(SpecAlphaPrecisionBits);
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
            break;

        case VK_COMPARE_OP_LESS:
            atestResult = alpha < alphaRef;
            break;

        case VK_COMPARE_OP_EQUAL:
            atestResult = alpha == alphaRef;
            break;

        case VK_COMPARE_OP_LESS_OR_EQUAL:
            atestResult = alpha <= alphaRef;
            break;

        case VK_COMPARE_OP_GREATER:
            atestResult = alpha > alphaRef;
            break;

        case VK_COMPARE_OP_NOT_EQUAL:
            atestResult = alpha != alphaRef;
            break;

        case VK_COMPARE_OP_GREATER_OR_EQUAL:
            atestResult = alpha >= alphaRef;
            break;

        default:
        case VK_COMPARE_OP_ALWAYS:
            atestResult = true;
            break;
    }

    bool atestDiscard = !atestResult;
    if (atestDiscard) {
        demote;
    }
}

struct TextureStageState {
    vec4 current;
    vec4 temp;
    vec4 previousStageTextureVal;
};

TextureStageState runTextureStage(uint stage, TextureStageState state) {
    if (stage > specUint(SpecFFLastActiveTextureStage)) {
        return state;
    }

    const bool isStageOptimized = specIsOptimized() && stage < SpecConstOptimizedTextureStageCount;

    const uint colorOp = isStageOptimized ? specUint(SpecFFTextureStage0ColorOp + PerTextureStageSpecConsts * stage) : colorOp(stage);

    // This cancels all subsequent stages.
    if (colorOp == D3DTOP_DISABLE)
        return state;

    const bool resultIsTemp = isStageOptimized ? specBool(SpecFFTextureStage0ResultIsTemp + PerTextureStageSpecConsts * stage) : resultIsTemp(stage);
    vec4 dst = resultIsTemp ? state.temp : state.current;

    const uint alphaOp = isStageOptimized ? specUint(SpecFFTextureStage0AlphaOp + PerTextureStageSpecConsts * stage) : alphaOp(stage);

    const bool usesArg0 = !isStageOptimized
        || colorOp == D3DTOP_LERP
        || colorOp == D3DTOP_MULTIPLYADD
        || alphaOp == D3DTOP_LERP
        || alphaOp == D3DTOP_MULTIPLYADD;

    const TextureStageArguments colorArgs = {
        usesArg0 ? colorArg0(stage) : D3DTA_CONSTANT,
        isStageOptimized ? repackArg(specUint(SpecFFTextureStage0ColorArg1 + PerTextureStageSpecConsts * stage)) : colorArg1(stage),
        isStageOptimized ? repackArg(specUint(SpecFFTextureStage0ColorArg2 + PerTextureStageSpecConsts * stage)) : colorArg2(stage)
    };
    const TextureStageArguments alphaArgs = {
        usesArg0 ? alphaArg0(stage) : D3DTA_CONSTANT,
        isStageOptimized ? repackArg(specUint(SpecFFTextureStage0AlphaArg1 + PerTextureStageSpecConsts * stage)) : alphaArg1(stage),
        isStageOptimized ? repackArg(specUint(SpecFFTextureStage0AlphaArg2 + PerTextureStageSpecConsts * stage)) : alphaArg2(stage)
    };

    vec4 textureVal = vec4(0.0);
    bool usesTexture = (colorArgs.arg0 & D3DTA_SELECTMASK) == D3DTA_TEXTURE
        || (colorArgs.arg1 & D3DTA_SELECTMASK) == D3DTA_TEXTURE
        || (colorArgs.arg2 & D3DTA_SELECTMASK) == D3DTA_TEXTURE
        || (alphaArgs.arg0 & D3DTA_SELECTMASK) == D3DTA_TEXTURE
        || (alphaArgs.arg1 & D3DTA_SELECTMASK) == D3DTA_TEXTURE
        || (alphaArgs.arg2 & D3DTA_SELECTMASK) == D3DTA_TEXTURE;

    if (usesTexture) {
        // We need to replace TEXCOORD inputs with gl_PointCoord
        // if D3DRS_POINTSPRITEENABLE is set.
        const uint pointMode = specUint(SpecPointMode);
        const bool isSprite = bitfieldExtract(pointMode, 1, 1) == 1u;

        vec4 texCoord;
        if (isSprite) {
            texCoord = vec4(gl_PointCoord, 0.0, 0.0);
        } else {
            switch (stage) {
                case 0: texCoord = in_Texcoord0; break;
                case 1: texCoord = in_Texcoord1; break;
                case 2: texCoord = in_Texcoord2; break;
                case 3: texCoord = in_Texcoord3; break;
                case 4: texCoord = in_Texcoord4; break;
                case 5: texCoord = in_Texcoord5; break;
                case 6: texCoord = in_Texcoord6; break;
                case 7: texCoord = in_Texcoord7; break;
            }
        }
        const vec4 unboundTextureConst = vec4(0.0, 0.0, 0.0, 1.0);
        textureVal = !specBool(SpecSamplerNull, stage) ? sampleTexture(stage, texCoord, state.previousStageTextureVal) : unboundTextureConst;
    }

    // Fast path if alpha/color path is identical.
    // D3DTOP_DOTPRODUCT3 also has special quirky behaviour here.
    const bool fastPath = colorOp == alphaOp && colorArgs == alphaArgs;
    if (fastPath || colorOp == D3DTOP_DOTPRODUCT3) {
        TextureStageArgumentValues colorArgVals = readArgValues(stage, colorArgs, state.current, state.temp, textureVal);
        dst = calculateTextureStage(colorOp, dst, colorArgVals, state.current, textureVal);
    } else {
        vec4 colorResult = dst;
        vec4 alphaResult = dst;

        TextureStageArgumentValues colorArgVals = readArgValues(stage, colorArgs, state.current, state.temp, textureVal);
        colorResult = calculateTextureStage(colorOp, dst, colorArgVals, state.current, textureVal);

        if (alphaOp != D3DTOP_DISABLE) {
            TextureStageArgumentValues alphaArgVals = readArgValues(stage, alphaArgs, state.current, state.temp, textureVal);
            alphaResult = calculateTextureStage(alphaOp, dst, alphaArgVals, state.current, textureVal);
        }

        dst.xyz = colorResult.xyz;

        // src0.x, src0.y, src0.z src1.w
        if (alphaOp != D3DTOP_DISABLE) {
            dst.a = alphaResult.a;
        }
    }

    if (resultIsTemp) {
        state.temp = dst;
    } else {
        state.current = dst;
    }
    state.previousStageTextureVal = textureVal;

    return state;
}

void main() {
    // in_Color0 is diffuse
    // in_Color1 is specular

    TextureStageState state;
    // Current starts of as equal to diffuse.
    state.current = in_Color0;
    // Temp starts off as equal to vec4(0)
    state.temp = vec4(0.0);
    state.previousStageTextureVal = vec4(0.0);

    // If we turn this into a loop, performance becomes very poor on the proprietary Nvidia driver
    // because it fails to unroll it.
    state = runTextureStage(0, state);
    state = runTextureStage(1, state);
    state = runTextureStage(2, state);
    state = runTextureStage(3, state);
    state = runTextureStage(4, state);
    state = runTextureStage(5, state);
    state = runTextureStage(6, state);
    state = runTextureStage(7, state);

    if (specBool(SpecFFGlobalSpecularEnabled)) {
        state.current.xyz += in_Color1.xyz;
    }

    state.current = calculateFog(gl_FragCoord, state.current);

    out_Color0 = state.current;

    alphaTest();
}
