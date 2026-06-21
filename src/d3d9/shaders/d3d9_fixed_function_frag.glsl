#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_spirv_intrinsics : require
#extension GL_EXT_demote_to_helper_invocation : require
#extension GL_ARB_derivative_control : require
#extension GL_EXT_control_flow_attributes : require
#extension GL_EXT_nonuniform_qualifier : require

#include "d3d9_fixed_function_common.glsl"

// The locations need to match with RegisterLinkerSlot in dxso_util.cpp
#ifndef INTERP_MODE
#define INTERP_MODE
#endif

layout(location = 0) INTERP_MODE in vec4 in_Normal;
layout(location = 1) INTERP_MODE in vec4 in_Texcoord0;
layout(location = 2) INTERP_MODE in vec4 in_Texcoord1;
layout(location = 3) INTERP_MODE in vec4 in_Texcoord2;
layout(location = 4) INTERP_MODE in vec4 in_Texcoord3;
layout(location = 5) INTERP_MODE in vec4 in_Texcoord4;
layout(location = 6) INTERP_MODE in vec4 in_Texcoord5;
layout(location = 7) INTERP_MODE in vec4 in_Texcoord6;
layout(location = 8) INTERP_MODE in vec4 in_Texcoord7;
layout(location = 9) INTERP_MODE in vec4 in_Color0;
layout(location = 10) INTERP_MODE in vec4 in_Color1;
layout(location = 11) INTERP_MODE in float in_Fog;

layout(location = 0) out vec4 out_Color0;

// Alpha test parameters
const uint VK_COMPARE_OP_NEVER             = 0;
const uint VK_COMPARE_OP_LESS              = 1;
const uint VK_COMPARE_OP_EQUAL             = 2;
const uint VK_COMPARE_OP_LESS_OR_EQUAL     = 3;
const uint VK_COMPARE_OP_GREATER           = 4;
const uint VK_COMPARE_OP_NOT_EQUAL         = 5;
const uint VK_COMPARE_OP_GREATER_OR_EQUAL  = 6;
const uint VK_COMPARE_OP_ALWAYS            = 7;

struct AlphaTestState {
    uint compareOp;
    uint precisionBits;
};

AlphaTestState getAlphaTestState() {
    AlphaTestState result;
    result.compareOp = bitfieldExtract(alphaTestAndModeArgs, 0, 3);
    result.precisionBits = bitfieldExtract(alphaTestAndModeArgs, 4, 4);
    return result;
}

// Sampler state
const uint TEXTURE_TYPE_2D         = 0;
const uint TEXTURE_TYPE_3D         = 1;
const uint TEXTURE_TYPE_CUBE       = 2;
const uint TEXTURE_TYPE_NULL       = 3;

const uint SAMPLER_MODE_DEFAULT    = 0;
const uint SAMPLER_MODE_FETCH4     = 1;
const uint SAMPLER_MODE_DREF       = 2;
const uint SAMPLER_MODE_DREF_CLAMP = 3;

struct SamplerState {
    bool projected;
    uint type;
    uint mode;
    float drefScale;
};

SamplerState getSamplerState(uint idx) {
    uint drefScaleShift = bitfieldExtract(alphaTestAndModeArgs, 24, 8);

    SamplerState result;
    result.projected = bitfieldExtract(packedProjMaskAndFfArgs, int(idx), 1) != 0;
    result.type = bitfieldExtract(psSamplerTypes, int(idx + idx), 2);
    result.mode = bitfieldExtract(psSamplerModes, int(idx + idx), 2);
    result.drefScale = 1.0f / max(1.0f, float(1u << drefScaleShift) - 1.0f);
    return result;
}

// Fixed-function texture stage parameters
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

// These differ from the actual D3D definitions. DXVK will shift
// the modifiers one to the right since only 7 args are defined.
const uint D3DTA_SELECTMASK                 = 0x07;

const uint D3DTA_DIFFUSE                    = 0x00;
const uint D3DTA_CURRENT                    = 0x01;
const uint D3DTA_TEXTURE                    = 0x02;
const uint D3DTA_TFACTOR                    = 0x03;
const uint D3DTA_SPECULAR                   = 0x04;
const uint D3DTA_TEMP                       = 0x05;
const uint D3DTA_CONSTANT                   = 0x06;

const uint D3DTA_COMPLEMENT                 = 0x08;
const uint D3DTA_ALPHAREPLICATE             = 0x10;

struct TextureStageArguments {
    uint arg0;
    uint arg1;
    uint arg2;
};

struct TextureStage {
    bool storeToTemp;
    uint colorOp;
    uint alphaOp;
    TextureStageArguments colorArgs;
    TextureStageArguments alphaArgs;
};

TextureStage getTextureStage(uint idx) {
    uint ops = 0u;
    uint args = 0u;

    switch (idx) {
        case 0: ops = packedStageOps01 >>  0; args = packedStageArgs0; break;
        case 1: ops = packedStageOps01 >> 16; args = packedStageArgs1; break;
        case 2: ops = packedStageOps23 >>  0; args = packedStageArgs2; break;
        case 3: ops = packedStageOps23 >> 16; args = packedStageArgs3; break;
        case 4: ops = packedStageOps45 >>  0; args = packedStageArgs4; break;
        case 5: ops = packedStageOps45 >> 16; args = packedStageArgs5; break;
        case 6: ops = packedStageOps67 >>  0; args = packedStageArgs6; break;
        case 7: ops = packedStageOps67 >> 16; args = packedStageArgs7; break;
    }

    TextureStage result;
    result.storeToTemp = bitfieldExtract(ops, 15, 1) != 0;
    result.colorOp = bitfieldExtract(ops, 0, 5);
    result.alphaOp = bitfieldExtract(ops, 8, 5);
    result.colorArgs.arg0 = bitfieldExtract(args,  0, 5);
    result.colorArgs.arg1 = bitfieldExtract(args,  5, 5);
    result.colorArgs.arg2 = bitfieldExtract(args, 10, 5);
    result.alphaArgs.arg0 = bitfieldExtract(args, 16, 5);
    result.alphaArgs.arg1 = bitfieldExtract(args, 21, 5);
    result.alphaArgs.arg2 = bitfieldExtract(args, 26, 5);
    return result;
}


// Checks whether point sprites are enabled
bool isPointSpriteEnabled() {
    return bitfieldExtract(alphaTestAndModeArgs, 8, 1) != 0;
}

struct D3D9SharedPSStage {
    uint Constant;
    uint Padding;
    float BumpEnvMat[2][2];
    float BumpEnvLScale;
    float BumpEnvLOffset;
};

struct D3D9SharedPS {
    D3D9SharedPSStage Stages[TextureStageCount];
};


layout(set = CBV_SET, binding = CBV_PS_SHARED, scalar, row_major)
uniform SharedData {
    D3D9SharedPS sharedData;
};

layout(push_constant, scalar, row_major)
uniform RenderStates {
    D3D9SharedPushData global;

    layout(offset = MaxSharedPushDataSize)
    D3D9FfpsPushData ffps;

    uint packedSamplerIndices[TextureStageCount / 2u];
};

layout(set = SRV_SET, binding = SRV_PS_BASE) uniform texture2D t2d[TextureStageCount];
layout(set = SRV_SET, binding = SRV_PS_BASE) uniform textureCube tcube[TextureStageCount];
layout(set = SRV_SET, binding = SRV_PS_BASE) uniform texture3D t3d[TextureStageCount];

layout(set = SAMPLER_SET, binding = 0) uniform sampler sampler_heap[];

vec4 calculateFog(vec4 oColor) {
    FogState fogState = getFogState();

    vec3 fogColor = unpackUnorm4x8(global.packedFogColorAndAlphaRef).bgr;
    float fogScale = global.fogDistanceScale;
    float fogEnd = global.fogDistanceEnd;
    float fogDensity = global.fogDensity;

    if (!fogState.enable)
        return oColor;

    float depth = fogState.useZ ? gl_FragCoord.z : (1.0 / gl_FragCoord.w);
    float fogFactor;

    switch (fogState.pixelMode) {
        case D3DFOG_NONE:
            fogFactor = in_Fog;
            break;

        // (end - d) / (end - start)
        case D3DFOG_LINEAR:
            fogFactor = fogEnd - depth;
            fogFactor = fogFactor * fogScale;
            break;

        // 1 / (e^[d * density])^2
        case D3DFOG_EXP2:
        // 1 / (e^[d * density])
        case D3DFOG_EXP:
            fogFactor = depth * fogDensity;

            if (fogState.pixelMode == D3DFOG_EXP2)
                fogFactor *= fogFactor;

            // Provides the rcp.
            fogFactor = -fogFactor;
            fogFactor = exp(fogFactor);
            break;
    }

    fogFactor = spvNClamp(fogFactor, 0.0, 1.0);

    vec4 color = oColor;
    vec3 color3 = color.rgb;
    vec3 fogFact3 = vec3(fogFactor);
    vec3 lerpedFrog = mix(fogColor, color3, fogFact3);
    return vec4(lerpedFrog.r, lerpedFrog.g, lerpedFrog.b, color.a);
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


float adjustDref(float dref, SamplerState state) {
    dref *= state.drefScale;

    if (state.mode == SAMPLER_MODE_DREF_CLAMP)
        dref = clamp(dref, 0.0f, 1.0f);

    return dref;
}

vec4 sampleTexture(uint stage, vec4 texcoord, vec4 previousStageTextureVal) {
    SamplerState state = getSamplerState(stage);

    if (state.projected)
        texcoord /= texcoord.w;

    uint previousStageColorOp = 0;

    if (stage > 0) {
        uint previousStageColorOp = getTextureStage(stage - 1u).colorOp;

        if (previousStageColorOp == D3DTOP_BUMPENVMAP || previousStageColorOp == D3DTOP_BUMPENVMAPLUMINANCE)
            texcoord = calculateBumpmapCoords(stage, texcoord, previousStageTextureVal);
    }

    // The only time we should ever be able to observe a null texture is with
    // D3DTOP_PREMODULATE. It's not 100% clear what's supposed to happen in
    // that case, but make it so that the multiplication has no effect for now.
    vec4 texVal = vec4(1.0f);

    switch (state.type) {
        case TEXTURE_TYPE_2D:
            if (state.mode >= SAMPLER_MODE_DREF) {
                texcoord.z = adjustDref(texcoord.z, state);
                texVal = texture(sampler2DShadow(t2d[stage], sampler_heap[loadSamplerHeapIndex(stage)]), texcoord.xyz).xxxx;
            } else {
                texVal = texture(sampler2D(t2d[stage], sampler_heap[loadSamplerHeapIndex(stage)]), texcoord.xy);
            }
            break;
        case TEXTURE_TYPE_CUBE:
            if (state.mode >= SAMPLER_MODE_DREF) {
                texcoord.w = adjustDref(texcoord.w, state);
                texVal = texture(samplerCubeShadow(tcube[stage], sampler_heap[loadSamplerHeapIndex(stage)]), texcoord).xxxx;
            } else {
                texVal = texture(samplerCube(tcube[stage], sampler_heap[loadSamplerHeapIndex(stage)]), texcoord.xyz);
            }
            break;
        case TEXTURE_TYPE_3D:
            texVal = texture(sampler3D(t3d[stage], sampler_heap[loadSamplerHeapIndex(stage)]), texcoord.xyz);
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


vec4 readArgValue(uint stage, uint arg, vec4 current, vec4 temp, vec4 textureVal, bool premodulate) {
    vec4 reg = vec4(1.0);
    switch (arg & D3DTA_SELECTMASK) {
        case D3DTA_CONSTANT:
            reg = decodeD3DColor(sharedData.Stages[stage].Constant);
            break;
        case D3DTA_CURRENT:
            reg = mix(current, current * textureVal, premodulate.xxxx);
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
            reg = decodeD3DColor(ffps.textureFactor);
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

struct TextureStageArgumentValues {
    vec4 arg0;
    vec4 arg1;
    vec4 arg2;
};

TextureStageArgumentValues readArgValues(uint stage, const TextureStageArguments args, vec4 current, vec4 temp, vec4 textureVal, bool premodulate) {
    TextureStageArgumentValues argVals;
    argVals.arg0 = readArgValue(stage, args.arg0, current, temp, textureVal, premodulate);
    argVals.arg1 = readArgValue(stage, args.arg1, current, temp, textureVal, premodulate);
    argVals.arg2 = readArgValue(stage, args.arg2, current, temp, textureVal, premodulate);
    return argVals;
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
            return saturate(arg.arg1 * arg.arg2 * 4.0);

        case D3DTOP_MODULATE2X:
            return saturate(arg.arg1 * arg.arg2 * 2.0);

        case D3DTOP_MODULATE:
            return saturate(arg.arg1 * arg.arg2);

        case D3DTOP_ADDSIGNED2X:
            return saturate(2.0 * (arg.arg1 + (arg.arg2 - vec4(0.5))));

        case D3DTOP_ADDSIGNED:
            return saturate(arg.arg1 + (arg.arg2 - vec4(0.5)));

        case D3DTOP_ADD:
            return saturate(arg.arg1 + arg.arg2);

        case D3DTOP_SUBTRACT:
            return saturate(arg.arg1 - arg.arg2);

        case D3DTOP_ADDSMOOTH:
            return saturate(fma(complement(arg.arg1), arg.arg2, arg.arg1));

        case D3DTOP_BLENDDIFFUSEALPHA:
            return mix(arg.arg2, arg.arg1, in_Color0.aaaa);

        case D3DTOP_BLENDTEXTUREALPHA:
            return mix(arg.arg2, arg.arg1, textureVal.aaaa);

        case D3DTOP_BLENDFACTORALPHA:
            return mix(arg.arg2, arg.arg1, decodeD3DColor(ffps.textureFactor).aaaa);

        case D3DTOP_BLENDTEXTUREALPHAPM:
            return saturate(fma(arg.arg2, complement(textureVal.aaaa), arg.arg1));

        case D3DTOP_BLENDCURRENTALPHA:
            return mix(arg.arg2, arg.arg1, current.aaaa);

        case D3DTOP_PREMODULATE:
            return arg.arg1;

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


bool alphaTest(float alpha) {
    AlphaTestState alphaTest = getAlphaTestState();

    uint alphaRefInitial = bitfieldExtract(global.packedFogColorAndAlphaRef, 24, 8);
    float alphaRef;

    if (alphaTest.compareOp < VK_COMPARE_OP_ALWAYS) {
        // Check if the given bit precision is supported
        bool useIntPrecision = alphaTest.precisionBits <= 8;

        if (useIntPrecision) {
            // Adjust alpha ref to the given range
            uint alphaRefInt = (alphaRefInitial << alphaTest.precisionBits) | (alphaRefInitial >> (8u - alphaTest.precisionBits));

            // Convert alpha ref to float since we'll do the comparison based on that
            alphaRef = float(alphaRefInt);

            // Adjust alpha to the given range and round
            float alphaFactor = float((256u << alphaTest.precisionBits) - 1u);

            alpha = roundEven(alpha * alphaFactor);
        } else {
            alphaRef = float(alphaRefInitial) / 255.0;
        }

        switch (alphaTest.compareOp) {
            case VK_COMPARE_OP_LESS:                return alpha <  alphaRef;
            case VK_COMPARE_OP_EQUAL:               return alpha == alphaRef;
            case VK_COMPARE_OP_LESS_OR_EQUAL:       return alpha <= alphaRef;
            case VK_COMPARE_OP_GREATER:             return alpha >  alphaRef;
            case VK_COMPARE_OP_NOT_EQUAL:           return alpha != alphaRef;
            case VK_COMPARE_OP_GREATER_OR_EQUAL:    return alpha >= alphaRef;
            default:                                return false;  // NEVER
        }
    }

    return true;
}

struct TextureStageState {
    vec4 current;
    vec4 temp;
    vec4 previousStageTextureVal;
    bool premodulateColor;
    bool premodulateAlpha;
};

vec4 getTexCoord(uint stage) {
    // If point sprites are enabled, we need to replace the
    // input texture coordinate with the point coordinate
    if (isPointSpriteEnabled())
        return vec4(gl_PointCoord, 0.0f, 0.0f);

    switch (stage) {
        case 0: return in_Texcoord0;
        case 1: return in_Texcoord1;
        case 2: return in_Texcoord2;
        case 3: return in_Texcoord3;
        case 4: return in_Texcoord4;
        case 5: return in_Texcoord5;
        case 6: return in_Texcoord6;
        case 7: return in_Texcoord7;
    }

    return vec4(0.0f);
}

TextureStageState runTextureStage(uint stage, TextureStageState state) {
    TextureStage info = getTextureStage(stage);

    // This cancels all subsequent stages.
    if (info.colorOp == D3DTOP_DISABLE)
        return state;

    vec4 prev = info.storeToTemp ? state.temp : state.current;
    vec4 next = prev;

    vec4 textureVal = sampleTexture(stage, getTexCoord(stage), state.previousStageTextureVal);

    // Fast path if alpha/color path is identical.
    // D3DTOP_DOTPRODUCT3 also has special quirky behaviour here.
    bool fastPath = info.colorOp == info.alphaOp &&
        info.colorArgs.arg0 == info.alphaArgs.arg0 &&
        info.colorArgs.arg1 == info.alphaArgs.arg1 &&
        info.colorArgs.arg2 == info.alphaArgs.arg2 &&
        state.premodulateColor == state.premodulateAlpha;

    if (fastPath || info.colorOp == D3DTOP_DOTPRODUCT3) {
        TextureStageArgumentValues colorArgVals = readArgValues(stage, info.colorArgs, state.current, state.temp, textureVal, state.premodulateColor);
        next = calculateTextureStage(info.colorOp, prev, colorArgVals, state.current, textureVal);
    } else {
        TextureStageArgumentValues colorArgVals = readArgValues(stage, info.colorArgs, state.current, state.temp, textureVal, state.premodulateColor);
        next.rgb = calculateTextureStage(info.colorOp, prev, colorArgVals, state.current, textureVal).rgb;

        if (info.alphaOp != D3DTOP_DISABLE) {
            TextureStageArgumentValues alphaArgVals = readArgValues(stage, info.alphaArgs, state.current, state.temp, textureVal, state.premodulateAlpha);
            next.a = calculateTextureStage(info.alphaOp, prev, alphaArgVals, state.current, textureVal).a;
        }
    }

    state.temp = info.storeToTemp ? next : state.temp;
    state.current = info.storeToTemp ? state.current : next;
    state.previousStageTextureVal = textureVal;
    state.premodulateColor = info.colorOp == D3DTOP_PREMODULATE;
    state.premodulateAlpha = info.alphaOp == D3DTOP_PREMODULATE;
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
    state.premodulateColor = false;
    state.premodulateAlpha = false;

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

    if (isSpecularEnabled())
        state.current.xyz += in_Color1.xyz;

    state.current = calculateFog(state.current);

    out_Color0 = state.current;

    if (!alphaTest(state.current.a))
        demote;
}
