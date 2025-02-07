#version 450

#extension GL_GOOGLE_include_directive : require

#include "hud_frag_common.glsl"

layout(binding = 4) uniform sampler2D s_font;

layout(push_constant)
uniform push_data_t {
  uvec2 surface_size;
  float opacity;
  float scale;
};

layout(location = 0) in vec2 v_texcoord;
layout(location = 1) in vec4 v_color;
layout(location = 2) in float v_bias;
layout(location = 3) in float v_range;

layout(location = 0) out vec4 o_color;

float sampleAlpha(float alpha_bias, float dist_range) {
  float value = textureLod(s_font, v_texcoord, 0).r + alpha_bias - 0.5f;
  float dist  = value * dot(vec2(dist_range, dist_range), 1.0f / fwidth(v_texcoord.xy));
  return clamp(dist + 0.5f, 0.0f, 1.0f);
}

void main() {
  float r_alpha = sampleAlpha(v_bias, v_range);
  
  o_color = v_color;
  o_color.a *= r_alpha;
  o_color.rgb *= o_color.a;

  o_color = linear_to_output(o_color);
}
