#version 450

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "dxvk_resolve_common.glsl"

layout(constant_id = 0) const int c_samples = 1;
layout(constant_id = 1) const int c_mode    = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;

layout(binding = 0) uniform texture2DMSArray s_image;

layout(location = 0) out vec4 o_color;

vec4 load_color(ivec3 coord, int s) {
  return texelFetch(s_image, coord, s);
}

resolve_fn(resolve_color, vec4, load_color)

layout(push_constant)
uniform u_info_t {
  ivec2 offset;
} u_info;

void main() {
  ivec3 coord = ivec3(gl_FragCoord.xy + u_info.offset, gl_Layer);
  o_color = resolve_color(coord, c_samples, c_mode);
}
