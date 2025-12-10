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
    uint dynamicSpecConstDword[20];
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
layout (constant_id = 13) const uint SpecConstDword13 = 0;
layout (constant_id = 14) const uint SpecConstDword14 = 0;
layout (constant_id = 15) const uint SpecConstDword15 = 0;
layout (constant_id = 16) const uint SpecConstDword16 = 0;
layout (constant_id = 17) const uint SpecConstDword17 = 0;
layout (constant_id = 18) const uint SpecConstDword18 = 0;
layout (constant_id = 19) const uint SpecConstDword19 = 0;
layout (constant_id = 20) const uint SpecConstDword20 = 0;

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
const uint SpecFFLastActiveTextureStage = 12;
const uint SpecSamplerDrefClamp = 13;
const uint SpecClipPlaneCount = 14;
const uint SpecPointMode = 15;
const uint SpecDrefScaling = 16;
const uint SpecFFGlobalSpecularEnabled = 17;
const uint SpecFFTextureStage0ColorOp = 18;
const uint SpecFFTextureStage0ColorArg1 = 19;
const uint SpecFFTextureStage0ColorArg2 = 20;
const uint SpecFFTextureStage0AlphaOp = 21;
const uint SpecFFTextureStage0AlphaArg1 = 22;
const uint SpecFFTextureStage0AlphaArg2 = 23;
const uint SpecFFTextureStage0ResultIsTemp = 24;
const uint SpecFFTextureStage1ColorOp = 25;
const uint SpecFFTextureStage1ColorArg1 = 26;
const uint SpecFFTextureStage1ColorArg2 = 27;
const uint SpecFFTextureStage1AlphaOp = 28;
const uint SpecFFTextureStage1AlphaArg1 = 29;
const uint SpecFFTextureStage1AlphaArg2 = 30;
const uint SpecFFTextureStage1ResultIsTemp = 31;
const uint SpecFFTextureStage2ColorOp = 32;
const uint SpecFFTextureStage2ColorArg1 = 33;
const uint SpecFFTextureStage2ColorArg2 = 34;
const uint SpecFFTextureStage2AlphaOp = 35;
const uint SpecFFTextureStage2AlphaArg1 = 36;
const uint SpecFFTextureStage2AlphaArg2 = 37;
const uint SpecFFTextureStage2ResultIsTemp = 38;
const uint SpecFFTextureStage3ColorOp = 39;
const uint SpecFFTextureStage3ColorArg1 = 40;
const uint SpecFFTextureStage3ColorArg2 = 41;
const uint SpecFFTextureStage3AlphaOp = 42;
const uint SpecFFTextureStage3AlphaArg1 = 43;
const uint SpecFFTextureStage3AlphaArg2 = 44;
const uint SpecFFTextureStage3ResultIsTemp = 45;
const uint SpecFFTextureStage4ColorOp = 46;
const uint SpecFFTextureStage4ColorArg1 = 47;
const uint SpecFFTextureStage4ColorArg2 = 48;
const uint SpecFFTextureStage4AlphaOp = 49;
const uint SpecFFTextureStage4AlphaArg1 = 50;
const uint SpecFFTextureStage4AlphaArg2 = 51;
const uint SpecFFTextureStage4ResultIsTemp = 52;
const uint SpecFFTextureStage5ColorOp = 53;
const uint SpecFFTextureStage5ColorArg1 = 54;
const uint SpecFFTextureStage5ColorArg2 = 55;
const uint SpecFFTextureStage5AlphaOp = 56;
const uint SpecFFTextureStage5AlphaArg1 = 57;
const uint SpecFFTextureStage5AlphaArg2 = 58;
const uint SpecFFTextureStage5ResultIsTemp = 59;
const uint SpecFFTextureStage6ColorOp = 60;
const uint SpecFFTextureStage6ColorArg1 = 61;
const uint SpecFFTextureStage6ColorArg2 = 62;
const uint SpecFFTextureStage6AlphaOp = 63;
const uint SpecFFTextureStage6AlphaArg1 = 64;
const uint SpecFFTextureStage6AlphaArg2 = 65;
const uint SpecFFTextureStage6ResultIsTemp = 66;
const uint SpecFFTextureStage7ColorOp = 67;
const uint SpecFFTextureStage7ColorArg1 = 68;
const uint SpecFFTextureStage7ColorArg2 = 69;
const uint SpecFFTextureStage7AlphaOp = 70;
const uint SpecFFTextureStage7AlphaArg1 = 71;
const uint SpecFFTextureStage7AlphaArg2 = 72;
const uint SpecFFTextureStage7ResultIsTemp = 73;
const uint SpecFFTextureStage0ColorArg0 = 74;
const uint SpecFFTextureStage1ColorArg0 = 75;
const uint SpecFFTextureStage2ColorArg0 = 76;
const uint SpecFFTextureStage3ColorArg0 = 77;
const uint SpecFFTextureStage4ColorArg0 = 78;
const uint SpecFFTextureStage5ColorArg0 = 79;
const uint SpecFFTextureStage6ColorArg0 = 80;
const uint SpecFFTextureStage7ColorArg0 = 81;
const uint SpecFFTextureStage0AlphaArg0 = 82;
const uint SpecFFTextureStage1AlphaArg0 = 83;
const uint SpecFFTextureStage2AlphaArg0 = 84;
const uint SpecFFTextureStage3AlphaArg0 = 85;
const uint SpecFFTextureStage4AlphaArg0 = 86;
const uint SpecFFTextureStage5AlphaArg0 = 87;
const uint SpecFFTextureStage6AlphaArg0 = 88;
const uint SpecFFTextureStage7AlphaArg0 = 89;
const uint SpecConstantCount = 90;

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
    { 4, 16,  3 }, // FFLastActiveTextureStage

    { 5, 0, 21 },  // SamplerDrefClamp
    { 5, 21, 3 },  // ClipPlaneCount
    { 5, 24, 2 },  // PointMode
    { 5, 26, 5 },  // DrefScaling

    { 6, 31, 1 },  // FFGlobalSpecularEnabled.

    { 6,  0, 5 },  // FFTextureStage0ColorOp
    { 6,  5, 5 },  // FFTextureStage0ColorArg1
    { 6, 10, 5 },  // FFTextureStage0ColorArg2
    { 6, 15, 5 },  // FFTextureStage0AlphaOp
    { 6, 20, 5 },  // FFTextureStage0AlphaArg1
    { 6, 25, 5 },  // FFTextureStage0AlphaArg2
    { 6, 30, 1 },  // FFTextureStage0ResultIsTemp

    { 7,  0, 5 },  // FFTextureStage1ColorOp
    { 7,  5, 5 },  // FFTextureStage1ColorArg1
    { 7, 10, 5 },  // FFTextureStage1ColorArg2
    { 7, 15, 5 },  // FFTextureStage1AlphaOp
    { 7, 20, 5 },  // FFTextureStage1AlphaArg1
    { 7, 25, 5 },  // FFTextureStage1AlphaArg2
    { 7, 30, 1 },  // FFTextureStage1ResultIsTemp

    { 8,  0, 5 },  // FFTextureStage2ColorOp
    { 8,  5, 5 },  // FFTextureStage2ColorArg1
    { 8, 10, 5 },  // FFTextureStage2ColorArg2
    { 8, 15, 5 },  // FFTextureStage2AlphaOp
    { 8, 20, 5 },  // FFTextureStage2AlphaArg1
    { 8, 25, 5 },  // FFTextureStage2AlphaArg2
    { 8, 30, 1 },  // FFTextureStage2ResultIsTemp

    { 9,  0, 5 },  // FFTextureStage3ColorOp
    { 9,  5, 5 },  // FFTextureStage3ColorArg1
    { 9, 10, 5 },  // FFTextureStage3ColorArg2
    { 9, 15, 5 },  // FFTextureStage3AlphaOp
    { 9, 20, 5 },  // FFTextureStage3AlphaArg1
    { 9, 25, 5 },  // FFTextureStage3AlphaArg2
    { 9, 30, 1 },  // FFTextureStage3ResultIsTemp

    { 10,  0, 5 },  // FFTextureStage4ColorOp
    { 10,  5, 5 },  // FFTextureStage4ColorArg1
    { 10, 10, 5 },  // FFTextureStage4ColorArg2
    { 10, 15, 5 },  // FFTextureStage4AlphaOp
    { 10, 20, 5 },  // FFTextureStage4AlphaArg1
    { 10, 25, 5 },  // FFTextureStage4AlphaArg2
    { 10, 30, 1 },  // FFTextureStage4ResultIsTemp

    { 11,  0, 5 },  // FFTextureStage5ColorOp
    { 11,  5, 5 },  // FFTextureStage5ColorArg1
    { 11, 10, 5 },  // FFTextureStage5ColorArg2
    { 11, 15, 5 },  // FFTextureStage5AlphaOp
    { 11, 20, 5 },  // FFTextureStage5AlphaArg1
    { 11, 25, 5 },  // FFTextureStage5AlphaArg2
    { 11, 30, 1 },  // FFTextureStage5ResultIsTemp

    { 12,  0, 5 },  // FFTextureStage6ColorOp
    { 12,  5, 5 },  // FFTextureStage6ColorArg1
    { 12, 10, 5 },  // FFTextureStage6ColorArg2
    { 12, 15, 5 },  // FFTextureStage6AlphaOp
    { 12, 20, 5 },  // FFTextureStage6AlphaArg1
    { 12, 25, 5 },  // FFTextureStage6AlphaArg2
    { 12, 30, 1 },  // FFTextureStage6ResultIsTemp

    { 13,  0, 5 },  // FFTextureStage7ColorOp
    { 13,  5, 5 },  // FFTextureStage7ColorArg1
    { 13, 10, 5 },  // FFTextureStage7ColorArg2
    { 13, 15, 5 },  // FFTextureStage7AlphaOp
    { 13, 20, 5 },  // FFTextureStage7AlphaArg1
    { 13, 25, 5 },  // FFTextureStage7AlphaArg2
    { 13, 30, 1 },  // FFTextureStage7ResultIsTemp

    { 14,  0, 5 },  // FFTextureStage0ColorArg0
    { 14,  5, 5 },  // FFTextureStage1ColorArg0
    { 14, 10, 5 },  // FFTextureStage2ColorArg0
    { 14, 15, 5 },  // FFTextureStage3ColorArg0
    { 14, 20, 5 },  // FFTextureStage4ColorArg0
    { 14, 25, 5 },  // FFTextureStage5ColorArg0

    { 15,  0, 5 },  // FFTextureStage6ColorArg0
    { 15,  5, 5 },  // FFTextureStage7ColorArg0
    { 15, 10, 5 },  // FFTextureStage0AlphaArg0
    { 15, 15, 5 },  // FFTextureStage1AlphaArg0
    { 15, 20, 5 },  // FFTextureStage2AlphaArg0
    { 15, 25, 5 },  // FFTextureStage3AlphaArg0

    { 16,  0, 5 },  // FFTextureStage4AlphaArg0
    { 16,  5, 5 },  // FFTextureStage5AlphaArg0
    { 16, 10, 5 },  // FFTextureStage6AlphaArg0
    { 16, 15, 5 },  // FFTextureStage7AlphaArg0
};

bool specIsOptimized() {
    return SpecConstDword20 != 0u;
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
        case 13u:
            return SpecConstDword13;
        case 14u:
            return SpecConstDword14;
        case 15u:
            return SpecConstDword15;
        case 16u:
            return SpecConstDword16;
        case 17u:
            return SpecConstDword17;
        case 18u:
            return SpecConstDword18;
        case 19u:
            return SpecConstDword19;
        case 20u:
            return SpecConstDword20;
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
