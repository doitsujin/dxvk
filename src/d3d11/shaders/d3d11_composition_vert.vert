#version 450

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_samplerless_texture_functions : require

layout(location = 0) out vec2 o_coord;

layout(push_constant, scalar)
uniform push_t {
  ivec2 src_offset;
  ivec2 dst_offset;
  ivec2 extent;
  uvec2 resolution;
} push;

void main() {
  vec2 rect_coord = vec2(push.extent) * vec2(
    float(gl_VertexIndex & 2) * 0.5f,
    float(gl_VertexIndex & 1));

  vec2 src_location = rect_coord + vec2(push.src_offset);
  vec2 dst_location = rect_coord + vec2(push.dst_offset);

  o_coord = src_location;
  gl_Position = vec4(-1.0f + 2.0f * (dst_location / vec2(push.resolution)), 0.0f, 1.0f);
}
