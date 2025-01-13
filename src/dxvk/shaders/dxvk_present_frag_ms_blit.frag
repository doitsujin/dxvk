#version 450

#extension GL_GOOGLE_include_directive : enable

#include "dxvk_present_common.glsl"

layout(location = 0) in  vec2 i_coord;
layout(location = 0) out vec4 o_color;

const vec2 sample_positions[] = {
  /* 2 samples */
  vec2( 0.25f, 0.25f),
  vec2(-0.25f,-0.25f),
  /* 4 samples */
  vec2(-0.125f,-0.375f),
  vec2( 0.375f,-0.125f),
  vec2(-0.375f, 0.125f),
  vec2( 0.125f, 0.375f),
  /* 8 samples */
  vec2( 0.0625f,-0.1875f),
  vec2(-0.0625f, 0.1875f),
  vec2( 0.3125f, 0.0625f),
  vec2(-0.1875f,-0.3125f),
  vec2(-0.3125f, 0.3125f),
  vec2(-0.4375f,-0.0625f),
  vec2( 0.1875f, 0.4375f),
  vec2( 0.4375f,-0.4375f),
  /* 16 samples */
  vec2( 0.0625f, 0.0625f),
  vec2(-0.0625f,-0.1875f),
  vec2(-0.1875f, 0.1250f),
  vec2( 0.2500f,-0.0625f),
  vec2(-0.3125f,-0.1250f),
  vec2( 0.1250f, 0.3125f),
  vec2( 0.3125f, 0.1875f),
  vec2( 0.1875f,-0.3125f),
  vec2(-0.1250f, 0.3750f),
  vec2( 0.0000f,-0.4375f),
  vec2(-0.2500f,-0.3750f),
  vec2(-0.3750f, 0.2500f),
  vec2(-0.5000f, 0.0000f),
  vec2( 0.4375f,-0.2500f),
  vec2( 0.3750f, 0.4375f),
  vec2(-0.4375f,-0.5000f),
};

void main() {
  vec2 coord = vec2(src_offset) + vec2(src_extent) * i_coord;

  ivec2 cint = ivec2(coord);
  vec2 cfrac = fract(coord) - 0.5f;

  uint pos_index = c_samples - 2u;

  o_color = vec4(0.0f);

  for (uint i = 0; i < c_samples; i++) {
    vec2 sample_pos = sample_positions[pos_index + i];

    ivec2 coffset = ivec2(greaterThan(cfrac - sample_pos, vec2(0.5f)));
    o_color += input_to_sc_rgb(texelFetch(s_image_ms, cint + coffset, int(i)));
  }

  o_color = composite_image(o_color / float(c_samples));
  o_color = sc_rgb_to_output(o_color);
}
