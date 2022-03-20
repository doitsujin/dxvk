#version 450

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

layout(binding = 1) uniform usamplerBuffer text_buffer;

layout(push_constant)
uniform push_data_t {
  vec4 text_color;
  vec2 text_pos;
  uint text_offset;
  float text_size;
  vec2 hud_scale;
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
  o_color = text_color;

  // Compute character index and vertex index for the current
  // character. We'll render two triangles per character.
  uint chr_idx = gl_VertexIndex / 6;
  uint vtx_idx = gl_VertexIndex - 6 * chr_idx;

  // Load glyph info based on vertex index
  uint glyph_idx = texelFetch(text_buffer, int(text_offset + chr_idx)).x;
  glyph_info_t glyph_info = glyph_data[glyph_idx];

  // Compute texture coordinate from glyph data
  vec2 coord = vec2((coord_mask >> vtx_idx) & 0x1);

  vec2 tex_xy = unpack_u16(glyph_info.packed_xy);
  vec2 tex_wh = unpack_u16(glyph_info.packed_wh);

  o_texcoord = tex_xy + coord * tex_wh;

  // Compute vertex position. We can easily do this here since our
  // font is a monospace font, otherwise we'd need to preprocess
  // the strings to render in a compute shader.
  float size_factor = text_size / font_data.size;

  vec2 local_pos = tex_wh * coord - unpack_u16(glyph_info.packed_origin)
    + vec2(font_data.advance * float(chr_idx), 0.0f);
  vec2 pixel_pos = text_pos + size_factor * local_pos;
  vec2 scaled_pos = 2.0f * hud_scale * pixel_pos - 1.0f;

  gl_Position = vec4(scaled_pos, 0.0f, 1.0f);
}
