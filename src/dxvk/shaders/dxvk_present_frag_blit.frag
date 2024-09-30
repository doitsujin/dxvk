#version 450

#extension GL_GOOGLE_include_directive : enable

#include "dxvk_present_common.glsl"

layout(location = 0) in  vec2 i_coord;
layout(location = 0) out vec4 o_color;

void main() {
  vec2 coord = vec2(src_offset) + vec2(src_extent) * i_coord;
  o_color = input_to_sc_rgb(textureLod(s_image, coord, 0.0f));
  o_color = sc_rgb_to_output(o_color);
}
