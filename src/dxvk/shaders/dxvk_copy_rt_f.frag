#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout : enable

#include "dxvk_color_space.glsl"

layout(constant_id = 0) const bool c_is_srgb = false;

layout(set = 0, binding = 0)
uniform writeonly image2DArray s_dst_image;

/* Input attachment index is patched at runtime */
layout(input_attachment_index = 8, set = 0, binding = 1)
uniform subpassInput s_src_image;

layout(push_constant, scalar)
uniform push_args_t {
  uvec2 dst_offset;
  uvec2 src_offset;
  uint dst_layer;
  uint src_layer;
} push_args;

void main() {
  uvec2 dst_coord = push_args.dst_offset + uvec2(gl_FragCoord.xy) - push_args.src_offset;
  uint  dst_layer = push_args.dst_layer + gl_Layer - push_args.src_layer;

  vec4 data = subpassLoad(s_src_image);

  if (c_is_srgb)
    data.xyz = linear_to_srgb(data.xyz);

  imageStore(s_dst_image, ivec3(dst_coord, dst_layer), data);
}
