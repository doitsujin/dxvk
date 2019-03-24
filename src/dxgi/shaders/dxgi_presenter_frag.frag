#version 450

layout(constant_id = 3) const bool s_gamma_bound = false;

layout(binding = 0) uniform sampler   s_sampler;
layout(binding = 1) uniform texture2D t_texture;

layout(binding = 2) uniform sampler   s_gamma;
layout(binding = 3) uniform texture1D t_gamma;

layout(location = 0) in  vec2 i_texcoord;
layout(location = 0) out vec4 o_color;

void main() {
  o_color = texture(sampler2D(t_texture, s_sampler), i_texcoord);
  
  if (s_gamma_bound) {
    o_color = vec4(
      texture(sampler1D(t_gamma, s_gamma), o_color.r).r,
      texture(sampler1D(t_gamma, s_gamma), o_color.g).g,
      texture(sampler1D(t_gamma, s_gamma), o_color.b).b,
      o_color.a);
  }
}