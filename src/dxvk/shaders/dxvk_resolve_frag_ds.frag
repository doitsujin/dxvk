#version 450

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_stencil_export : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "dxvk_resolve_common.glsl"

layout(constant_id = 0) const int c_samples = 1;
layout(constant_id = 1) const int c_mode_d  = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
layout(constant_id = 2) const int c_mode_s  = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;

layout(binding = 0) uniform  texture2DMSArray s_depth;
layout(binding = 1) uniform utexture2DMSArray s_stencil;

float load_depth(ivec3 coord, int s) {
  return texelFetch(s_depth, coord, s).r;
}

uint load_stencil(ivec3 coord, int s) {
  return texelFetch(s_stencil, coord, s).r;
}

resolve_fn(resolve_depth, float, load_depth)
resolve_fn(resolve_stencil, uint, load_stencil)

layout(push_constant)
uniform u_info_t {
  ivec2 offset;
} u_info;

void main() {
  ivec3 coord = ivec3(gl_FragCoord.xy + u_info.offset, gl_Layer);

  gl_FragDepth = resolve_depth(coord, c_samples, c_mode_d);
  gl_FragStencilRefARB = int(resolve_stencil(coord, c_samples, c_mode_s));
}
