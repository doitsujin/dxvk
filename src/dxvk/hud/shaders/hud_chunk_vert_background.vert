#version 450

struct draw_info_t {
  uint packed_xy;
  uint packed_wh;
  uint packed_range;
  uint color;
};

layout(binding = 0, std430)
readonly buffer draw_data_t {
  draw_info_t draw_infos[];
};

layout(push_constant)
uniform push_data_t {
  uvec2 surface_size;
  float opacity;
  float scale;
};

vec2 unpack_u16(uint v) {
  // Inputs may be signed
  int hi = int(v);
  int lo = int(v << 16);
  return vec2(float(lo >> 16), float(hi >> 16));
}

layout(location = 0) out uint o_active;

void main() {
  draw_info_t draw = draw_infos[gl_InstanceIndex];

  vec2 coord = vec2(
    float(gl_VertexIndex  & 1),
    float(gl_VertexIndex >> 1));

  vec2 surface_size_f = vec2(surface_size) / scale;

  vec2 pos = unpack_u16(draw.packed_xy);
  vec2 size = unpack_u16(draw.packed_wh);

  pos = mix(pos, surface_size_f + pos, lessThan(pos, vec2(0.0f)));

  vec2 pixel_pos = pos + size * coord + (2.0f * coord - 1.0f);
  vec2 scaled_pos = 2.0f * (pixel_pos / surface_size_f) - 1.0f;

  o_active = bitfieldExtract(draw.packed_range, 31, 1);
  gl_Position = vec4(scaled_pos, 0.0f, 1.0f);
}
