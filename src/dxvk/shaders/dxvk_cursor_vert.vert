#version 460

layout(push_constant)
uniform present_info_t {
  ivec2 dst_extent;
  ivec2 cursor_offset;
  ivec2 cursor_extent;
};

layout(location = 0) out vec2 o_texcoord;

void main() {
  vec2 coord = vec2(
    float((gl_VertexIndex >> 1) & 1),
    float(gl_VertexIndex & 1));

  o_texcoord = coord;

  coord *= vec2(cursor_extent) / vec2(dst_extent);
  coord += vec2(cursor_offset) / vec2(dst_extent);

  gl_Position = vec4(-1.0f + 2.0f * coord, 0.0f, 1.0f);
}
