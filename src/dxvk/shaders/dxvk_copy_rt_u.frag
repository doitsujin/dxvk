#version 460

#extension GL_EXT_scalar_block_layout : enable

layout(set = 0, binding = 0)
uniform writeonly uimage2DArray s_dst_image;

/* Input attachment index is patched at runtime */
layout(input_attachment_index = 8, set = 0, binding = 1)
uniform usubpassInput s_src_image;

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

  imageStore(s_dst_image, ivec3(dst_coord, dst_layer), subpassLoad(s_src_image));
}
