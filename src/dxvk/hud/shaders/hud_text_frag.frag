#version 450

#extension GL_GOOGLE_include_directive : require

#include "hud_frag_common.glsl"

layout(binding = 2) uniform sampler2D s_font;

layout(location = 0) in vec2 v_texcoord;
layout(location = 1) in vec4 v_color;

layout(location = 0) out vec4 o_color;

float sampleAlpha(float alpha_bias, float dist_range) {
  float value = textureLod(s_font, v_texcoord, 0).r + alpha_bias - 0.5f;
  float dist  = value * dot(vec2(dist_range, dist_range), 1.0f / fwidth(v_texcoord.xy));
  return clamp(dist + 0.5f, 0.0f, 1.0f);
}

void main() {
  float r_alpha_center = sampleAlpha(0.0f, 5.0f);
  float r_alpha_shadow = sampleAlpha(0.3f, 5.0f);
  
  vec3 r_center = v_color.rgb;
  vec3 r_shadow = vec3(0.0f, 0.0f, 0.0f);

  o_color.rgb = mix(r_shadow, r_center, r_alpha_center);
  o_color.a = r_alpha_shadow * v_color.a;
  o_color.rgb *= o_color.a;

  o_color.rgb = encodeOutput(o_color.rgb);
}
