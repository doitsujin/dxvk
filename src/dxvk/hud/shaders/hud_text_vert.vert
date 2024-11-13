#version 460

struct font_info_t {
  float size;
  float advance;
  uvec2 padding;
};

struct glyph_info_t {
  uint packed_xy;
  uint packed_wh;
  uint packed_origin;
};

layout(binding = 0, std430)
readonly buffer font_buffer_t {
  font_info_t font_data;
  glyph_info_t glyph_data[];
};

struct draw_info_t {
  uint text_offset;
  uint text_length_and_size;
  uint packed_xy;
  uint color;
};

layout(binding = 1, std430)
readonly buffer draw_buffer_t {
  draw_info_t draw_infos[];
};

layout(binding = 2) uniform usamplerBuffer text_buffer;

layout(push_constant)
uniform push_data_t {
  uvec2 surface_size;
  float opacity;
  float scale;
};

layout(location = 0) out vec2 o_texcoord;
layout(location = 1) out vec4 o_color;

const uvec2 coord_mask = uvec2(0x2a, 0x1c);

vec2 unpack_u16(uint v) {
  // Inputs may be signed
  int hi = int(v);
  int lo = int(v << 16);
  return vec2(float(lo >> 16), float(hi >> 16));
}

void main() {
  draw_info_t draw_info = draw_infos[gl_DrawID];
  o_color = unpackUnorm4x8(draw_info.color);

  // Compute character index and vertex index for the current
  // character. We'll render two triangles per character.
  uint chr_idx = gl_VertexIndex / 6;
  uint vtx_idx = gl_VertexIndex - 6 * chr_idx;

  // Load glyph info based on vertex index
  uint glyph_idx = texelFetch(text_buffer, int(draw_info.text_offset + chr_idx)).x;
  glyph_info_t glyph_info = glyph_data[glyph_idx];

  // Compute texture coordinate from glyph data
  vec2 coord = vec2((coord_mask >> vtx_idx) & 0x1);

  vec2 tex_xy = unpack_u16(glyph_info.packed_xy);
  vec2 tex_wh = unpack_u16(glyph_info.packed_wh);

  o_texcoord = tex_xy + coord * tex_wh;

  // Compute vertex position. We can easily do this here since our
  // font is a monospace font, otherwise we'd need to preprocess
  // the strings to render in a compute shader.
  uint text_size = bitfieldExtract(draw_info.text_length_and_size, 16, 16);
  float size_factor = float(text_size) / font_data.size;

  vec2 surface_size_f = vec2(surface_size) / scale;

  vec2 text_pos = unpack_u16(draw_info.packed_xy);
  text_pos = mix(text_pos, surface_size_f + text_pos, lessThan(text_pos, vec2(0.0f)));

  vec2 local_pos = tex_wh * coord - unpack_u16(glyph_info.packed_origin)
    + vec2(font_data.advance * float(chr_idx), 0.0f);
  vec2 pixel_pos = text_pos + size_factor * local_pos;
  vec2 scaled_pos = 2.0f * (pixel_pos / surface_size_f) - 1.0f;

  gl_Position = vec4(scaled_pos, 0.0f, 1.0f);
}
