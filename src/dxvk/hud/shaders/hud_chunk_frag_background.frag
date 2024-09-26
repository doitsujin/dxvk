#version 450

#extension GL_GOOGLE_include_directive : require

#include "hud_frag_common.glsl"

layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform push_data_t {
  vec2 pos;
  vec2 size;
  vec2 scale;
  float opacity;
  uint color;
  uint maskIndex;
  uint pageCount;
};

void main() {
  vec4 rgba = unpackUnorm4x8(color);
  o_color = vec4(encodeOutput(rgba.rgb), rgba.a * opacity);
}
