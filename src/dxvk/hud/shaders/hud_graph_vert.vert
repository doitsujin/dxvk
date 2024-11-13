#version 450

layout(location = 0) out vec2 o_coord;

layout(push_constant)
uniform push_data_t {
  uvec2 surface_size;
  float opacity;
  float scale;
  uint  packed_xy;
  uint  packed_wh;
  uint  frame_index;
};

vec2 unpack_u16(uint v) {
  // Inputs may be signed
  int hi = int(v);
  int lo = int(v << 16);
  return vec2(float(lo >> 16), float(hi >> 16));
}

void main() {
  vec2 coord = vec2(
    float(gl_VertexIndex  & 1),
    float(gl_VertexIndex >> 1));
  o_coord = vec2(coord.x, 1.0f - coord.y);

  vec2 surface_size_f = vec2(surface_size) / scale;

  vec2 pos = unpack_u16(packed_xy);
  pos = mix(pos, surface_size_f + pos, lessThan(pos, vec2(0.0f)));

  vec2 size = unpack_u16(packed_wh);

  vec2 pixel_pos = pos + size * coord;
  vec2 scaled_pos = 2.0f * (pixel_pos / surface_size_f) - 1.0f;
  gl_Position = vec4(scaled_pos, 0.0f, 1.0f);
}
