const float FloatMaxValue = 340282346638528859811704183484516925440.0;

const uint TextureStageCount = 8;

#define D3DFOGMODE uint
const uint D3DFOG_NONE   = 0;
const uint D3DFOG_EXP    = 1;
const uint D3DFOG_EXP2   = 2;
const uint D3DFOG_LINEAR = 3;

struct D3D9RenderStateInfo {
    float fogColor[3];
    float fogScale;
    float fogEnd;
    float fogDensity;

    uint alphaRef;

    float pointSize;
    float pointSizeMin;
    float pointSizeMax;
    float pointScaleA;
    float pointScaleB;
    float pointScaleC;
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


// Dynamic "spec constants"
// Binding has to match with getSpecConstantBufferSlot in dxso_util.h
layout(set = 0, binding = 31, scalar) uniform SpecConsts {
    uint dynamicSpecConstDword[13];
};

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

const uint SpecSamplerType = 0;
const uint SpecSamplerDepthMode = 1;
const uint SpecAlphaCompareOp = 2;
const uint SpecSamplerProjected = 3;
const uint SpecSamplerNull = 4;
const uint SpecAlphaPrecisionBits = 5;
const uint SpecFogEnabled = 6;
const uint SpecVertexFogMode = 7;
const uint SpecPixelFogMode = 8;
const uint SpecVertexShaderBools = 9;
const uint SpecPixelShaderBools = 10;
const uint SpecSamplerFetch4 = 11;
const uint SpecSamplerDrefClamp = 12;
const uint SpecClipPlaneCount = 13;
const uint SpecPointMode = 14;
const uint SpecDrefScaling = 15;
const uint SpecConstantCount = 16;

struct BitfieldPosition {
    uint dwordOffset;
    uint bitOffset;
    uint sizeInBits;
};

// Needs to match d3d9_spec_constants.h
BitfieldPosition SpecConstLayout[SpecConstantCount] = {
    { 0, 0, 32 },  // SamplerType

    { 1, 0,  21 }, // SamplerDepthMode
    { 1, 21, 3 },  // AlphaCompareOp
    { 1, 24, 8 },  // SamplerProjected

    { 2, 0,  21 }, // SamplerNull
    { 2, 21, 4 },  // AlphaPrecisionBits
    { 2, 25, 1 },  // FogEnabled
    { 2, 26, 2 },  // VertexFogMode
    { 2, 28, 2 },  // PixelFogMode

    { 3, 0,  16 }, // VertexShaderBools
    { 3, 16, 16 }, // PixelShaderBools

    { 4, 0,  16 }, // SamplerFetch4

    { 5, 0, 21 },  // SamplerDrefClamp
    { 5, 21, 3 },  // ClipPlaneCount
    { 5, 24, 2 },  // PointMode
    { 5, 26, 5 },  // DrefScaling
};

bool specIsOptimized() {
    return SpecConstDword12 != 0u;
}

uint specDword(uint index) {
    if (!specIsOptimized()) {
        return dynamicSpecConstDword[index];
    }

    switch (index) {
        case 0u:
            return SpecConstDword0;
        case 1u:
            return SpecConstDword1;
        case 2u:
            return SpecConstDword2;
        case 3u:
            return SpecConstDword3;
        case 4u:
            return SpecConstDword4;
        case 5u:
            return SpecConstDword5;
        case 6u:
            return SpecConstDword6;
        case 7u:
            return SpecConstDword7;
        case 8u:
            return SpecConstDword8;
        case 9u:
            return SpecConstDword9;
        case 10u:
            return SpecConstDword10;
        case 11u:
            return SpecConstDword11;
        case 12u:
            return SpecConstDword12;
        default:
            return 0u;
    }
}

uint specUint(uint specConstIdx, uint bitOffset, uint bits) {
    BitfieldPosition pos = SpecConstLayout[specConstIdx];
    uint dword = specDword(pos.dwordOffset);
    return bitfieldExtract(dword, int(pos.bitOffset + bitOffset), int(bits));
}

uint specUint(uint specConstIdx) {
    BitfieldPosition pos = SpecConstLayout[specConstIdx];
    uint dword = specDword(pos.dwordOffset);
    return bitfieldExtract(dword, int(pos.bitOffset), int(pos.sizeInBits));
}

bool specBool(uint specConstIdx, uint bitOffset) {
    return specUint(specConstIdx, bitOffset, 1u) != 0u;
}

bool specBool(uint specConstIdx) {
    return specUint(specConstIdx) != 0u;
}
