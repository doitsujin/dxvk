#version 450

#define CP_COUNT 1025

layout(binding = 0) uniform sampler   s_sampler;
layout(binding = 1) uniform texture2D t_texture;

layout(binding = 2)
uniform u_gamma_ramp_t {
  layout(offset =  0) vec4 in_factor;
  layout(offset = 16) vec4 in_offset;
  layout(offset = 32) vec4 cp_values[CP_COUNT + 1];
} u_gamma_ramp;

layout(location = 0) in  vec2 i_texcoord;
layout(location = 0) out vec4 o_color;

void main() {
  o_color = texture(sampler2D(t_texture, s_sampler), i_texcoord);
  
//   vec3 cp_lookup = o_color.rgb;
//   cp_lookup *= u_gamma_ramp.in_factor.rgb;
//   cp_lookup += u_gamma_ramp.in_offset.rgb;
//   
//   cp_lookup = clamp(
//     cp_lookup * float(CP_COUNT - 1),
//     0.0f, float(CP_COUNT - 1));
//   
//   vec3  cp_fpart = fract(cp_lookup);
//   ivec3 cp_index = ivec3(cp_lookup);
//   
//   for (int i = 0; i < 3; i++) {
//     int cp_entry = cp_index[i];
//     
//     float lo = u_gamma_ramp.cp_values[cp_entry + 0][i];
//     float hi = u_gamma_ramp.cp_values[cp_entry + 1][i];
//     
//     if (cp_entry == CP_COUNT - 1)
//       hi = lo;
//     
//     o_color[i] = mix(lo, hi, cp_fpart[i]);
//   }
}