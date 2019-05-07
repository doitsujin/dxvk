#version 450

layout(set = 0, binding = 1) uniform sampler2D s_font;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform push_data {
  vec4 color;
};

float sampleAlpha(float alpha_bias, float dist_range) {
  float value = texture(s_font, v_texcoord).r + alpha_bias - 0.5f;
  float dist  = value * dot(vec2(dist_range, dist_range), 1.0f / fwidth(v_texcoord.xy));
  return clamp(dist + 0.5f, 0.0f, 1.0f);
}

void main() {
  float r_alpha_center = sampleAlpha(0.0f, 5.0f);
  float r_alpha_shadow = sampleAlpha(0.3f, 5.0f);
  
  vec4 r_center = vec4(color.rgb, color.a * r_alpha_center);
  vec4 r_shadow = vec4(0.0f, 0.0f, 0.0f, r_alpha_shadow);
  
  o_color = mix(r_shadow, r_center, r_alpha_center);
  o_color.rgb *= o_color.a;
}