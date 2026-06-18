const float FloatMaxValue = 340282346638528859811704183484516925440.0;

const uint TextureStageCount = 8;

#define D3DFOGMODE uint
const uint D3DFOG_NONE   = 0;
const uint D3DFOG_EXP    = 1;
const uint D3DFOG_EXP2   = 2;
const uint D3DFOG_LINEAR = 3;

const uint MaxSharedPushDataSize = 32;

struct D3D9VsPushData {
    uint packedReservedAndPointSize;
    uint packedPointSizeMinMax;
};

struct D3D9FfvsPushData {
    float pointScaleA;
    float pointScaleB;
    float pointScaleC;
};

struct D3D9FfpsPushData {
    uint textureFactor;
};

struct D3D9SharedPushData {
    uint packedFogColorAndAlphaRef;
    float fogDistanceScale;
    float fogDistanceEnd;
    float fogDensity;
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

// Bindings have to match D3D9ShaderResourceMapping.
// Set numbers are arbitrarily set in d3d9_fixed_function.cpp
#define SAMPLER_SET             0

#define SRV_SET                 1
#define SRV_PS_BASE             0

#define CBV_SET                 2
#define SPEC_DATA_SET           3

#define CBV_VS_CLIP_PLANES      0
#define CBV_VS_FIXED_FUNCTION   1
#define CBV_VS_VERTEX_BLEND     2

#define CBV_PS_SHARED           5

layout(set = SPEC_DATA_SET, binding = 0, scalar) uniform SpecConsts {
    uint alphaTestAndModeArgs;
    uint fogArgs;
    uint packedProjMaskAndFfArgs;
    uint unused_vsSamplerTypesAndBools;
    uint psSamplerTypes;
    uint psSamplerModes;
    uint unused_psBools;
    uint unused_spec7;
    uint packedStageOps01;
    uint packedStageOps23;
    uint packedStageOps45;
    uint packedStageOps67;
    uint packedStageArgs0;
    uint packedStageArgs1;
    uint packedStageArgs2;
    uint packedStageArgs3;
    uint packedStageArgs4;
    uint packedStageArgs5;
    uint packedStageArgs6;
    uint packedStageArgs7;
};

// Fog parameters
struct FogState {
    bool enable;
    uint pixelMode;
    uint vertexMode;
    bool useZ;
};

FogState getFogState() {
    FogState result;
    result.enable = bitfieldExtract(fogArgs, 0, 1) != 0u;
    result.vertexMode = bitfieldExtract(fogArgs, 8, 2);
    result.pixelMode = bitfieldExtract(fogArgs, 16, 2);
    result.useZ = bitfieldExtract(fogArgs, 24, 1) != 0u;
    return result;
}
// Checks whether specular lighting is enabled
bool isSpecularEnabled() {
    return bitfieldExtract(packedProjMaskAndFfArgs, 8, 1) != 0;
}

vec4 decodeD3DColor(uint color) {
    return unpackUnorm4x8(color).bgra;
}
