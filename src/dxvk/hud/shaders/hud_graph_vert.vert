#version 450

layout(location = 0) out vec2 o_coord;

layout(push_constant)
uniform push_data_t {
  uint offset;
  uint count;
  vec2 pos;
  vec2 size;
  vec2 scale;
  float opacity;
};

void main() {
  vec2 coord = vec2(
    float(gl_VertexIndex  & 1),
    float(gl_VertexIndex >> 1));
  o_coord = coord;

  vec2 pixel_pos = pos + size * coord;
  vec2 scaled_pos = 2.0f * scale * pixel_pos - 1.0f;
  gl_Position = vec4(scaled_pos, 0.0f, 1.0f);
}
