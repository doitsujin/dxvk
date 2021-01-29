#version 450

layout(constant_id = 1225) const bool srgbSwapchain = false;

layout(location = 0) in  vec4 v_color;
layout(location = 0) out vec4 o_color;

vec3 linearToSrgb(vec3 color) {
  bvec3 isLo = lessThanEqual(color, vec3(0.0031308f));

  vec3 loPart = color * 12.92f;
  vec3 hiPart = pow(color, vec3(5.0f / 12.0f)) * 1.055f - 0.055f;
  return mix(hiPart, loPart, isLo);
}

void main() {
  o_color = vec4(
    v_color.rgb * v_color.a,
    v_color.a);

  if (!srgbSwapchain)
    o_color.rgb = linearToSrgb(o_color.rgb);
}