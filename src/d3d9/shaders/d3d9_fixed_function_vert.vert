#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_spirv_intrinsics : require

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
const uint MaxClipPlaneCount = 6;
out float gl_ClipDistance[MaxClipPlaneCount];
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


#include "d3d9_fixed_function_common.glsl"

const uint MaxEnabledLights = 8;

struct D3D9ViewportInfo {
    vec4 inverseOffset;
    vec4 inverseExtent;
};

#define D3DLIGHTTYPE uint
const uint D3DLIGHT_POINT       = 1;
const uint D3DLIGHT_SPOT        = 2;
const uint D3DLIGHT_DIRECTIONAL = 3;

struct D3D9Light {
    vec4 Diffuse;
    vec4 Specular;
    vec4 Ambient;

    vec4 Position;
    vec4 Direction;

    D3DLIGHTTYPE Type;
    float Range;
    float Falloff;
    float Attenuation0;
    float Attenuation1;
    float Attenuation2;
    float Theta;
    float Phi;
};

#define D3DCOLORVALUE vec4

struct D3DMATERIAL9 {
    D3DCOLORVALUE   Diffuse;
    D3DCOLORVALUE   Ambient;
    D3DCOLORVALUE   Specular;
    D3DCOLORVALUE   Emissive;
    float           Power;
};

struct D3D9FixedFunctionVS {
    mat4 WorldView;
    mat4 NormalMatrix;
    mat4 InverseView;
    mat4 Projection;

    mat4 TexcoordMatrices[TextureStageCount];

    D3D9ViewportInfo ViewportInfo;

    vec4 GlobalAmbient;
    D3D9Light Lights[MaxEnabledLights];
    D3DMATERIAL9 Material;
    float TweenFactor;

    uint KeyPrimitives[4];
};

#define D3D9FF_VertexBlendMode uint
const uint D3D9FF_VertexBlendMode_Disabled = 0;
const uint D3D9FF_VertexBlendMode_Normal   = 1;
const uint D3D9FF_VertexBlendMode_Tween    = 2;

#define D3DMATERIALCOLORSOURCE uint
const uint D3DMCS_MATERIAL = 0;
const uint D3DMCS_COLOR1   = 1;
const uint D3DMCS_COLOR2   = 2;

#define D3DTEXTURETRANSFORMFLAGS uint
const uint D3DTTFF_DISABLE   = 0;
const uint D3DTTFF_COUNT1    = 1;
const uint D3DTTFF_COUNT2    = 2;
const uint D3DTTFF_COUNT3    = 3;
const uint D3DTTFF_COUNT4    = 4;
const uint D3DTTFF_PROJECTED = 256;

const uint DXVK_TSS_TCI_PASSTHRU                    = 0x00000000;
const uint DXVK_TSS_TCI_CAMERASPACENORMAL           = 0x00010000;
const uint DXVK_TSS_TCI_CAMERASPACEPOSITION         = 0x00020000;
const uint DXVK_TSS_TCI_CAMERASPACEREFLECTIONVECTOR = 0x00030000;
const uint DXVK_TSS_TCI_SPHEREMAP                   = 0x00040000;

const uint TCIOffset = 16;
const uint TCIMask = (7 << TCIOffset);


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


// Functions to extract information from the packed VS key
// See D3D9FFShaderKeyVSData in d3d9_shader_types.h
// Please, dearest compiler, inline all of this.
uint texcoordIndices() {
    return bitfieldExtract(data.KeyPrimitives[0], 0, 24);
}
bool vertexHasPositionT() {
    return bitfieldExtract(data.KeyPrimitives[0], 24, 1) != 0;
}
bool vertexHasColor0() {
    return bitfieldExtract(data.KeyPrimitives[0], 25, 1) != 0;
}
bool vertexHasColor1() {
    return bitfieldExtract(data.KeyPrimitives[0], 26, 1) != 0;
}
bool vertexHasPointSize() {
    return bitfieldExtract(data.KeyPrimitives[0], 27, 1) != 0;
}
bool useLighting() {
    return bitfieldExtract(data.KeyPrimitives[0], 28, 1) != 0;
}
bool normalizeNormals() {
    return bitfieldExtract(data.KeyPrimitives[0], 29, 1) != 0;
}
bool localViewer() {
    return bitfieldExtract(data.KeyPrimitives[0], 30, 1) != 0;
}
bool rangeFog() {
    return bitfieldExtract(data.KeyPrimitives[0], 31, 1) != 0;
}

uint texcoordFlags() {
    return bitfieldExtract(data.KeyPrimitives[1], 0, 24);
}
uint diffuseSource() {
    return bitfieldExtract(data.KeyPrimitives[1], 24, 2);
}
uint ambientSource() {
    return bitfieldExtract(data.KeyPrimitives[1], 26, 2);
}
uint specularSource() {
    return bitfieldExtract(data.KeyPrimitives[1], 28, 2);
}
uint emissiveSource() {
    return bitfieldExtract(data.KeyPrimitives[1], 30, 2);
}

uint transformFlags() {
    return bitfieldExtract(data.KeyPrimitives[2], 0, 24);
}
uint lightCount() {
    return bitfieldExtract(data.KeyPrimitives[2], 24, 4);
}
bool specularEnabled() {
    return bitfieldExtract(data.KeyPrimitives[2], 28, 1) != 0;
}

uint vertexTexcoordDeclMask() {
    return bitfieldExtract(data.KeyPrimitives[3], 0, 24);
}
bool vertexHasFog() {
    return bitfieldExtract(data.KeyPrimitives[3], 24, 1) != 0;
}
D3D9FF_VertexBlendMode blendMode() {
    return bitfieldExtract(data.KeyPrimitives[3], 25, 2);
}
bool vertexBlendIndexed() {
    return bitfieldExtract(data.KeyPrimitives[3], 27, 1) != 0;
}
uint vertexBlendCount() {
    return bitfieldExtract(data.KeyPrimitives[3], 28, 2);
}
bool vertexClipping() {
    return bitfieldExtract(data.KeyPrimitives[3], 30, 1) != 0;
}


float calculateFog(vec4 vPos, vec4 oColor) {
    vec4 color1 = vertexHasColor1() ? in_Color1 : vec4(0.0, 0.0, 0.0, 1.0);

    vec4 specular = color1;
    bool hasSpecular = vertexHasColor1();

    vec3 fogColor = vec3(rs.fogColor[0], rs.fogColor[1], rs.fogColor[2]);
    float fogScale = rs.fogScale;
    float fogEnd = rs.fogEnd;
    float fogDensity = rs.fogDensity;
    D3DFOGMODE fogMode = specUint(SpecVertexFogMode);
    bool fogEnabled = specBool(SpecFogEnabled);
    if (!fogEnabled) {
        return 0.0;
    }

    float w = vPos.w;
    float z = vPos.z;
    float depth;
    if (rangeFog()) {
        vec3 pos3 = vPos.xyz;
        depth = length(pos3);
    } else {
        depth = vertexHasFog() ? in_Fog : abs(z);
    }
    float fogFactor;
    if (vertexHasPositionT()) {
        fogFactor = hasSpecular ? specular.w : 1.0;
    } else {
        switch (fogMode) {
            case D3DFOG_NONE:
                if (hasSpecular)
                    fogFactor = specular.w;
                else
                    fogFactor = 1.0;
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
    }

    return fogFactor;
}


float calculatePointSize(vec4 vtx) {
    float value = vertexHasPointSize() ? in_PointSize : rs.pointSize;
    uint pointMode = specUint(SpecPointMode);
    bool isScale = bitfieldExtract(pointMode, 0, 1) != 0;
    float scaleC = rs.pointScaleC;
    float scaleB = rs.pointScaleB;
    float scaleA = rs.pointScaleA;

    vec3 vtx3 = vtx.xyz;

    float DeSqr = dot(vtx3, vtx3);
    float De    = sqrt(DeSqr);
    float scaleValue = scaleC * DeSqr;
    scaleValue = fma(scaleB, De, scaleValue);
    scaleValue += scaleA;
    scaleValue = sqrt(scaleValue);
    scaleValue = value / scaleValue;

    value = isScale ? scaleValue : value;

    float pointSizeMin = rs.pointSizeMin;
    float pointSizeMax = rs.pointSizeMax;

    return clamp(value, pointSizeMin, pointSizeMax);
}


void emitVsClipping(vec4 vtx) {
    vec4 worldPos = data.InverseView * vtx;

    // Always consider clip planes enabled when doing GPL by forcing 6 for the quick value.
    uint clipPlaneCount = specUint(SpecClipPlaneCount);

    // Compute clip distances
    for (uint i = 0; i < MaxClipPlaneCount; i++) {
        vec4 clipPlane = clipPlanes[i];
        float dist = dot(worldPos, clipPlane);
        bool clipPlaneEnabled = i < clipPlaneCount;
        float value = clipPlaneEnabled ? dist : 0.0;
        gl_ClipDistance[i] = value;
    }
}


vec4 pickMaterialSource(uint source, vec4 material) {
    if (source == D3DMCS_COLOR1 && vertexHasColor0())
        return in_Color0;
    else if (source == D3DMCS_COLOR2 && vertexHasColor1())
        return in_Color1;
    else
        return material;
}


void main() {
    vec4 vtx = in_Position0;
    gl_Position = in_Position0;
    vec3 normal = in_Normal0.xyz;

    if (blendMode() == D3D9FF_VertexBlendMode_Tween) {
        vec4 vtx1 = in_Position1;
        vec3 normal1 = in_Normal1.xyz;
        vtx = mix(vtx, vtx1, data.TweenFactor);
        normal = mix(normal, normal1, data.TweenFactor);
    }

    if (!vertexHasPositionT()) {
        if (blendMode() == D3D9FF_VertexBlendMode_Normal) {
            float blendWeightRemaining = 1.0;
            vec4 vtxSum = vec4(0.0);
            vec3 nrmSum = vec3(0.0);

            for (uint i = 0; i <= vertexBlendCount(); i++) {
                uint arrayIndex;
                if (vertexBlendIndexed()) {
                    arrayIndex = uint(round(in_BlendIndices[i]));
                } else {
                    arrayIndex = i;
                }
                mat4 worldView = WorldViewArray[arrayIndex];

                mat3 nrmMtx;
                for (uint j = 0; j < 3; j++) {
                    nrmMtx[j] = worldView[j].xyz;
                }

                vec4 vtxResult = vtx * worldView;
                vec3 nrmResult = normal * nrmMtx;

                float weight;
                if (i != vertexBlendCount()) {
                    weight = in_BlendWeight[i];
                    blendWeightRemaining -= weight;
                } else {
                    weight = blendWeightRemaining;
                }

                vec4 weightVec4 = vec4(weight, weight, weight, weight);

                vtxSum = fma(vtxResult, weightVec4, vtxSum);
                nrmSum = fma(nrmResult, weightVec4.xyz, nrmSum);
            }

            vtx = vtxSum;
            normal = nrmSum;
        } else {
            vtx = vtx * data.WorldView;

            mat3 nrmMtx = mat3(data.NormalMatrix);

            normal = nrmMtx * normal;
        }

        // Some games rely on normals not being normal.
        if (normalizeNormals()) {
            bool isZeroNormal = all(equal(normal, vec3(0.0, 0.0, 0.0)));
            normal = isZeroNormal ? normal : normalize(normal);
        }

        gl_Position = vtx * data.Projection;
    } else {
        gl_Position *= data.ViewportInfo.inverseExtent;
        gl_Position += data.ViewportInfo.inverseOffset;

        // We still need to account for perspective correction here...

        float w = gl_Position.w;
        float rhw = w == 0.0 ? 1.0 : 1.0 / w;
        gl_Position.xyz *= rhw;
        gl_Position.w = rhw;
    }

    vec4 outNrm = vec4(normal, 1.0);
    out_Normal = outNrm;

    vec4 texCoords[TextureStageCount];
    texCoords[0] = in_Texcoord0;
    texCoords[1] = in_Texcoord1;
    texCoords[2] = in_Texcoord2;
    texCoords[3] = in_Texcoord3;
    texCoords[4] = in_Texcoord4;
    texCoords[5] = in_Texcoord5;
    texCoords[6] = in_Texcoord6;
    texCoords[7] = in_Texcoord7;

    vec4 transformedTexCoords[TextureStageCount];

    for (uint i = 0; i < TextureStageCount; i++) {
        // 0b111 = 7
        uint inputIndex = (texcoordIndices() >> (i * 3)) & 7;
        uint inputFlags = (texcoordFlags() >> (i * 3)) & 7;
        uint texcoordCount = (vertexTexcoordDeclMask() >> (inputIndex * 3)) & 7;

        vec4 transformed;

        uint flags = (transformFlags() >> (i * 3)) & 7;

        // Passing 0xffffffff results in it getting clamped to the dimensions of the texture coords and getting treated as PROJECTED
        // but D3D9 does not apply the transformation matrix.
        bool applyTransform = flags > D3DTTFF_COUNT1 && flags <= D3DTTFF_COUNT4;

        uint count = min(flags, 4u);

        // A projection component index of 4 means we won't do projection
        uint projIndex = count != 0 ? count - 1 : 4;

        switch (inputFlags) {
            default:
            case (DXVK_TSS_TCI_PASSTHRU >> TCIOffset):
                transformed = texCoords[inputIndex & 0xFF];

                if (texcoordCount < 4) {
                    // Vulkan sets the w component to 1.0 if that's not provided by the vertex buffer, D3D9 expects 0 here
                    transformed.w = 0.0;
                }

                if (applyTransform && !vertexHasPositionT()) {
                    /*This doesn't happen every time and I cannot figure out the difference between when it does and doesn't.
                    Keep it disabled for now, it's more likely that games rely on the zero texcoord than the weird 1 here.
                    if (texcoordCount <= 1) {
                      // y gets padded to 1 for some reason
                      transformed.y = 1.0;
                    }*/

                    if (texcoordCount >= 1 && texcoordCount < 4) {
                        // The first component after the last one thats backed by a vertex buffer gets padded to 1 for some reason.
                        uint idx = texcoordCount;
                        transformed[idx] = 1.0;
                    }
                } else if (texcoordCount != 0 && !applyTransform) {
                    // COUNT0, COUNT1, COUNT > 4 => take count from vertex decl if that's not zero
                    count = texcoordCount;
                }

                projIndex = count != 0 ? count - 1 : 4;
                break;

            case (DXVK_TSS_TCI_CAMERASPACENORMAL >> TCIOffset):
                transformed = outNrm;
                if (!applyTransform) {
                    count = 3;
                    projIndex = 4;
                }
                break;

            case (DXVK_TSS_TCI_CAMERASPACEPOSITION >> TCIOffset):
                transformed = vtx;
                if (!applyTransform) {
                    count = 3;
                    projIndex = 4;
                }
                break;

            case (DXVK_TSS_TCI_CAMERASPACEREFLECTIONVECTOR >> TCIOffset): {
                vec3 vtx3 = vtx.xyz;
                vtx3 = normalize(vtx3);

                vec3 reflection = reflect(vtx3, normal);
                transformed = vec4(reflection, 1.0);
                if (!applyTransform) {
                    count = 3;
                    projIndex = 4;
                }
                break;
            }

            case (DXVK_TSS_TCI_SPHEREMAP >> TCIOffset): {
                vec3 vtx3 = vtx.xyz;
                vtx3 = normalize(vtx3);

                vec3 reflection = reflect(vtx3, normal);
                float m = length(reflection + vec3(0.0, 0.0, 1.0)) * 2.0;

                transformed = vec4(
                    reflection.x / m + 0.5,
                    reflection.y / m + 0.5,
                    0.0,
                    1.0
                );
                break;
            }
        }

        if (applyTransform && !vertexHasPositionT()) {
            transformed = transformed * data.TexcoordMatrices[i];
        }

        // TODO: Shouldn't projected be checked per texture stage?
        if (specUint(SpecSamplerProjected) != 0u && projIndex < 4) {
            // The projection idx is always based on the flags, even when the input mode is not DXVK_TSS_TCI_PASSTHRU.
            float projValue = transformed[projIndex];

            // The w component is only used for projection or unused, so always insert the component that's supposed to be divided by there.
            // The fragment shader will then decide whether to project or not.
            transformed.w = projValue;
        }

        // TODO: Shouldn't projected be checked per texture stage?
        uint totalComponents = (specUint(SpecSamplerProjected) != 0u && projIndex < 4) ? 3 : 4;
        for (uint j = count; j < totalComponents; j++) {
            // Discard the components that exceed the specified D3DTTFF_COUNT
            transformed[j] = 0.0;
        }

        transformedTexCoords[i] = transformed;
    }

    out_Texcoord0 = transformedTexCoords[0];
    out_Texcoord1 = transformedTexCoords[1];
    out_Texcoord2 = transformedTexCoords[2];
    out_Texcoord3 = transformedTexCoords[3];
    out_Texcoord4 = transformedTexCoords[4];
    out_Texcoord5 = transformedTexCoords[5];
    out_Texcoord6 = transformedTexCoords[6];
    out_Texcoord7 = transformedTexCoords[7];

    if (useLighting()) {
        vec4 diffuseValue = vec4(0.0);
        vec4 specularValue = vec4(0.0);
        vec4 ambientValue = vec4(0.0);

        for (uint i = 0; i < lightCount(); i++) {
            D3D9Light light = data.Lights[i];

            vec4 diffuse = light.Diffuse;
            vec4 specular = light.Specular;
            vec4 ambient = light.Ambient;
            vec3 position = light.Position.xyz;
            vec3 direction = light.Direction.xyz;
            uint type = light.Type;
            float range = light.Range;
            float falloff = light.Falloff;
            float atten0 = light.Attenuation0;
            float atten1 = light.Attenuation1;
            float atten2 = light.Attenuation2;
            float theta = light.Theta;
            float phi = light.Phi;

            bool isSpot = type == D3DLIGHT_SPOT;
            bool isDirectional = type == D3DLIGHT_DIRECTIONAL;

            bvec3 isDirectional3 = bvec3(isDirectional);

            vec3 vtx3 = vtx.xyz;

            vec3 delta = position - vtx3;
            float d = length(delta);
            vec3 hitDir = -direction;
                 hitDir = mix(delta, hitDir, isDirectional3);
                 hitDir = normalize(hitDir);

            float atten = fma(d, atten2, atten1);
                  atten = fma(d, atten, atten0);
                  atten = 1.0 / atten;
                  atten = spvNMin(atten, FloatMaxValue);

                  atten = d > range ? 0.0 : atten;
                  atten = isDirectional ? 1.0 : atten;

            // Spot Lighting
            {
                float rho = dot(-hitDir, direction);
                float spotAtten = rho - phi;
                      spotAtten = spotAtten / (theta - phi);
                      spotAtten = pow(spotAtten, falloff);

                bool insideThetaAndPhi = rho <= theta;
                bool insidePhi = rho > phi;
                     spotAtten = insidePhi ? spotAtten : 0.0;
                     spotAtten = insideThetaAndPhi ? spotAtten : 1.0;
                     spotAtten = clamp(spotAtten, 0.0, 1.0);

                     spotAtten = atten * spotAtten;
                     atten     = isSpot ? spotAtten : atten;
            }

            float hitDot = dot(normal, hitDir);
                  hitDot = clamp(hitDot, 0.0, 1.0);

            float diffuseness = hitDot * atten;

            vec3 mid;
            if (localViewer()) {
                mid = normalize(vtx3);
                mid = hitDir - mid;
            } else {
                mid = hitDir - vec3(0.0, 0.0, 1.0);
            }

            mid = normalize(mid);

            float midDot = dot(normal, mid);
                  midDot = clamp(midDot, 0.0, 1.0);
            bool doSpec = midDot > 0.0;
                 doSpec = doSpec && hitDot > 0.0;

            float specularness = pow(midDot, data.Material.Power);
                  specularness *= atten;
                  specularness = doSpec ? specularness : 0.0;

            vec4 lightAmbient  = ambient * atten;
            vec4 lightDiffuse  = diffuse * diffuseness;
            vec4 lightSpecular = specular * specularness;

            ambientValue  += lightAmbient;
            diffuseValue  += lightDiffuse;
            specularValue += lightSpecular;
        }

        vec4 matDiffuse  = pickMaterialSource(diffuseSource(), data.Material.Diffuse);
        vec4 matAmbient  = pickMaterialSource(ambientSource(), data.Material.Ambient);
        vec4 matEmissive = pickMaterialSource(emissiveSource(), data.Material.Emissive);
        vec4 matSpecular = pickMaterialSource(specularSource(), data.Material.Specular);

        vec4 finalColor0 = fma(matAmbient, data.GlobalAmbient, matEmissive);
             finalColor0 = fma(matAmbient, ambientValue, finalColor0);
             finalColor0 = fma(matDiffuse, diffuseValue, finalColor0);
             finalColor0.a = matDiffuse.a;

        vec4 finalColor1 = matSpecular * specularValue;

        // Saturate
        finalColor0 = clamp(finalColor0, vec4(0.0), vec4(1.0));

        finalColor1 = clamp(finalColor1, vec4(0.0), vec4(1.0));

        out_Color0 = finalColor0;
        if (specularEnabled()) {
            out_Color1 = finalColor1;
        } else {
            out_Color1 = vertexHasColor1() ? in_Color1 : vec4(0.0, 0.0, 0.0, 1.0);
        }
    } else {
        out_Color0 = vertexHasColor0() ? in_Color0 : vec4(0.0, 0.0, 0.0, 1.0);
        out_Color1 = vertexHasColor1() ? in_Color1 : vec4(0.0, 0.0, 0.0, 1.0);
    }

    out_Fog = calculateFog(vtx, vec4(0.0));

    gl_PointSize = calculatePointSize(vtx);

    // We statically declare 6 clip planes, so we always need to write values.
    emitVsClipping(vtx);
}
