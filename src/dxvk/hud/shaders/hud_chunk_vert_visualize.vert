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

layout(location = 0) out vec2 o_coord;

layout(location = 1, component = 0) out uint o_color;
layout(location = 1, component = 1) out uint o_mask_index;
layout(location = 1, component = 2) out uint o_page_count;
layout(location = 1, component = 3) out uint o_active;

vec2 unpack_u16(uint v) {
  // Inputs may be signed
  int hi = int(v);
  int lo = int(v << 16);
  return vec2(float(lo >> 16), float(hi >> 16));
}

void main() {
  draw_info_t draw = draw_infos[gl_InstanceIndex];

  vec2 coord = vec2(
    float(gl_VertexIndex  & 1),
    float(gl_VertexIndex >> 1));

  o_coord = coord;
  o_color = draw.color;
  o_mask_index = bitfieldExtract(draw.packed_range,  0, 16);
  o_page_count = bitfieldExtract(draw.packed_range, 16, 15);
  o_active = bitfieldExtract(draw.packed_range, 31, 1);

  vec2 surface_size_f = vec2(surface_size) / scale;

  vec2 pos = unpack_u16(draw.packed_xy);
  vec2 size = unpack_u16(draw.packed_wh);

  pos = mix(pos, surface_size_f + pos, lessThan(pos, vec2(0.0f)));

  vec2 pixel_pos = pos + size * coord;
  vec2 scaled_pos = 2.0f * (pixel_pos / surface_size_f) - 1.0f;

  gl_Position = vec4(scaled_pos, 0.0f, 1.0f);
}
