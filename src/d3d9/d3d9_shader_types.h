// Do this instead of pragma once to make sure it works with GLSL
#ifndef D3D9_SHADER_TYPES_H
#define D3D9_SHADER_TYPES_H

#ifdef GLSL

// Basic types

#define uint32_t uint

// Math types

#define Vector4 vec4
#define Matrix4 mat4

// D3D9 header types

#define D3DLIGHTTYPE uint
#define D3DCOLORVALUE vec4

struct D3DMATERIAL9 {
  D3DCOLORVALUE   Diffuse;
  D3DCOLORVALUE   Ambient;
  D3DCOLORVALUE   Specular;
  D3DCOLORVALUE   Emissive;
  float           Power;
};

#define D3DTEXTURETRANSFORMFLAGS uint
#define D3DTTFF_DISABLE 0
#define D3DTTFF_COUNT1 1
#define D3DTTFF_COUNT2 2
#define D3DTTFF_COUNT3 3
#define D3DTTFF_COUNT4 4
#define D3DTTFF_PROJECTED 256

#define D3DMATERIALCOLORSOURCE uint
#define D3DMCS_MATERIAL 0
#define D3DMCS_COLOR0 1
#define D3DMCS_COLOR1 2

#define D3DMULTISAMPLE_TYPE uint
#define D3DLIGHT_POINT       1
#define D3DLIGHT_SPOT        2
#define D3DLIGHT_DIRECTIONAL 3

#define DXVK_TSS_TCI_PASSTHRU 0x00000000
#define DXVK_TSS_TCI_CAMERASPACENORMAL 0x00010000
#define DXVK_TSS_TCI_CAMERASPACEPOSITION 0x00020000
#define DXVK_TSS_TCI_CAMERASPACEREFLECTIONVECTOR 0x00030000
#define DXVK_TSS_TCI_SPHEREMAP 0x00040000

#define TCIOffset 16
#define TCIMask   (7 << TCIOffset)

#define D3DFOGMODE uint
#define D3DFOG_NONE 0
#define D3DFOG_EXP 1
#define D3DFOG_EXP2 2
#define D3DFOG_LINEAR 3

#else

#include "../util/util_matrix.h"
#include "d3d9_caps.h"
#include <d3d9types.h>

namespace dxvk {
#endif


#ifndef GLSL
enum D3D9FF_VertexBlendMode {
  D3D9FF_VertexBlendMode_Disabled = 0,
  D3D9FF_VertexBlendMode_Normal = 1,
  D3D9FF_VertexBlendMode_Tween = 2,
};
#else
#define D3D9FF_VertexBlendMode uint
#define D3D9FF_VertexBlendMode_Disabled 0
#define D3D9FF_VertexBlendMode_Normal 1
#define D3D9FF_VertexBlendMode_Tween 2
#endif


struct D3D9Light {
#ifndef GLSL
  D3D9Light(const D3DLIGHT9& light, Matrix4 viewMtx)
    : Diffuse      ( Vector4(light.Diffuse.r,  light.Diffuse.g,  light.Diffuse.b,  light.Diffuse.a) )
    , Specular     ( Vector4(light.Specular.r, light.Specular.g, light.Specular.b, light.Specular.a) )
    , Ambient      ( Vector4(light.Ambient.r,  light.Ambient.g,  light.Ambient.b,  light.Ambient.a) )
    , Position     ( viewMtx * Vector4(light.Position.x,  light.Position.y,  light.Position.z,  1.0f) )
    , Direction    ( normalize(viewMtx * Vector4(light.Direction.x, light.Direction.y, light.Direction.z, 0.0f)) )
    , Type         ( light.Type )
    , Range        ( light.Range )
    , Falloff      ( light.Falloff )
    , Attenuation0 ( light.Attenuation0 )
    , Attenuation1 ( light.Attenuation1 )
    , Attenuation2 ( light.Attenuation2 )
    , Theta        ( cosf(light.Theta / 2.0f) )
    , Phi          ( cosf(light.Phi / 2.0f) ) { }
#endif

  Vector4 Diffuse;
  Vector4 Specular;
  Vector4 Ambient;

  Vector4 Position;
  Vector4 Direction;

  D3DLIGHTTYPE Type;
  float Range;
  float Falloff;
  float Attenuation0;
  float Attenuation1;
  float Attenuation2;
  float Theta;
  float Phi;
};


// This is needed in fixed function for POSITION_T support.
// These are constants we need to * and add to move
// Window Coords -> Real Coords w/ respect to the viewport.
struct D3D9ViewportInfo {
  Vector4 inverseOffset;
  Vector4 inverseExtent;
};


struct D3D9FFShaderKeyVSData {
#ifndef GLSL
  union {
    struct {
      uint32_t TexcoordIndices : 24;

      uint32_t HasPositionT : 1;

      uint32_t HasColor0 : 1; // Diffuse
      uint32_t HasColor1 : 1; // Specular

      uint32_t HasPointSize : 1;

      uint32_t UseLighting : 1;

      uint32_t NormalizeNormals : 1;
      uint32_t LocalViewer : 1;
      uint32_t RangeFog : 1;

      uint32_t TexcoordFlags : 24;

      uint32_t DiffuseSource : 2;
      uint32_t AmbientSource : 2;
      uint32_t SpecularSource : 2;
      uint32_t EmissiveSource : 2;

      uint32_t TransformFlags : 24;

      uint32_t LightCount : 4;

      uint32_t TexcoordDeclMask : 24;
      uint32_t HasFog : 1;

      uint32_t VertexBlendMode    : 2;
      uint32_t VertexBlendIndexed : 1;
      uint32_t VertexBlendCount   : 3;

      uint32_t VertexClipping     : 1;

      uint32_t Projected : 8;
    } Contents;
#endif

    uint32_t Primitive[5];

#ifndef GLSL
  };
#endif
};


struct D3D9FixedFunctionVS {
  Matrix4 WorldView;
  Matrix4 NormalMatrix;
  Matrix4 InverseView;
  Matrix4 Projection;

  Matrix4 TexcoordMatrices[8];

  D3D9ViewportInfo ViewportInfo;

  Vector4 GlobalAmbient;
  D3D9Light Lights[8]; // See caps::MaxEnabledLights
  D3DMATERIAL9 Material;
  float TweenFactor;

  // TODO: Refactor once this works and we figure out what to
  //       do with the existing generated fixed function shaders
  D3D9FFShaderKeyVSData Key;
};


struct D3D9FixedFunctionVertexBlendDataHW {
  Matrix4 WorldView[8];
};

struct D3D9FixedFunctionVertexBlendDataSW {
  Matrix4 WorldView[256];
};


struct D3D9RenderStateInfo {
  float fogColor[3];
  float fogScale;
  float fogEnd;
  float fogDensity;

  uint32_t alphaRef;

  float pointSize;
  float pointSizeMin;
  float pointSizeMax;
  float pointScaleA;
  float pointScaleB;
  float pointScaleC;
};

#ifndef GLSL
}
#endif

#endif //D3D9_SHADER_TYPES_H