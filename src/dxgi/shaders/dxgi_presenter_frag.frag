#version 450

layout(binding = 0) uniform sampler   s_sampler;
layout(binding = 1) uniform texture2D t_texture;

layout(binding = 2) uniform sampler   s_gamma;
layout(binding = 3) uniform texture1D t_gamma;

layout(binding = 4)
uniform u_gamma_info_t {
  layout(offset =  0) vec4 in_factor;
  layout(offset = 16) vec4 in_offset;
} u_gamma_info;

layout(location = 0) in  vec2 i_texcoord;
layout(location = 0) out vec4 o_color;

void main() {
  vec4 color = texture(sampler2D(t_texture, s_sampler), i_texcoord);
  
  vec3 cp_lookup = color.rgb;
  cp_lookup *= u_gamma_info.in_factor.rgb;
  cp_lookup += u_gamma_info.in_offset.rgb;
  
  o_color = vec4(
    texture(sampler1D(t_gamma, s_gamma), cp_lookup.r).r,
    texture(sampler1D(t_gamma, s_gamma), cp_lookup.g).g,
    texture(sampler1D(t_gamma, s_gamma), cp_lookup.b).b,
    color.a);
}