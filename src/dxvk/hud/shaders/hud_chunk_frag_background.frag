#version 450

#extension GL_GOOGLE_include_directive : require

#include "hud_frag_common.glsl"

layout(push_constant)
uniform push_data_t {
  uvec2 surface_size;
  float opacity;
  float scale;
};

layout(location = 0) out vec4 o_color;

void main() {
  o_color = vec4(0.0f, 0.0f, 0.0f, 0.75f);
  o_color.a *= opacity;
  o_color = linear_to_output(o_color);
}
