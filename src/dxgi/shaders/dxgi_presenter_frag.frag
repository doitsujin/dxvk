#version 450

layout(constant_id = 1249) const bool c_has_gamma = false;

layout(binding = 0) uniform sampler2D s_image;
layout(binding = 1) uniform sampler1D s_gamma;

layout(location = 0) in  vec2 i_texcoord;
layout(location = 0) out vec4 o_color;

void main() {
  o_color = texture(s_image, i_texcoord);
  
  if (c_has_gamma) {
    o_color = vec4(
      texture(s_gamma, o_color.r).r,
      texture(s_gamma, o_color.g).g,
      texture(s_gamma, o_color.b).b,
      o_color.a);
  }
}