#version 450

layout(constant_id = 1225) const bool srgbSwapchain = false;

layout(set = 0, binding = 0) uniform sampler2D s_font;

layout(location = 0) in vec2 v_texcoord;
layout(location = 1) in vec4 v_color;

layout(location = 0) out vec4 o_color;

vec3 linearToSrgb(vec3 color) {
  bvec3 isLo = lessThanEqual(color, vec3(0.0031308f));

  vec3 loPart = color * 12.92f;
  vec3 hiPart = pow(color, vec3(5.0f / 12.0f)) * 1.055f - 0.055f;
  return mix(hiPart, loPart, isLo);
}

float sampleAlpha(float alpha_bias, float dist_range) {
  float value = textureLod(s_font, v_texcoord, 0).r + alpha_bias - 0.5f;
  float dist  = value * dot(vec2(dist_range, dist_range), 1.0f / fwidth(v_texcoord.xy));
  return clamp(dist + 0.5f, 0.0f, 1.0f);
}

void main() {
  float r_alpha_center = sampleAlpha(0.0f, 5.0f);
  float r_alpha_shadow = sampleAlpha(0.3f, 5.0f);
  
  vec4 r_center = vec4(v_color.rgb, v_color.a * r_alpha_center);
  vec4 r_shadow = vec4(0.0f, 0.0f, 0.0f, r_alpha_shadow);
  
  o_color = mix(r_shadow, r_center, r_alpha_center);
  o_color.rgb *= o_color.a;

  if (!srgbSwapchain)
    o_color.rgb = linearToSrgb(o_color.rgb);
}
