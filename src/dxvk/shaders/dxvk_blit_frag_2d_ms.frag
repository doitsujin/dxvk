#version 460

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_samplerless_texture_functions : enable

layout(set = 0, binding = 0) uniform sampler s_samplers[];
layout(set = 1, binding = 0) uniform texture2DMSArray s_image_ms;

layout(location = 0) in  vec2 i_pos;
layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform push_block {
  float p_src_coord0_x, p_src_coord0_y, p_src_coord0_z;
  float p_src_coord1_x, p_src_coord1_y, p_src_coord1_z;
  uint p_layer_count;
};

layout(constant_id = 0) const int c_samples = 1;

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
  vec2 coord = vec2(p_src_coord0_x, p_src_coord0_y) + vec2(p_src_coord1_x - p_src_coord0_x, p_src_coord1_y - p_src_coord0_y) * i_pos;

  ivec2 cint = ivec2(coord);
  vec2 cfrac = fract(coord) - 0.5f;

  uint pos_index = c_samples - 2u;

  o_color = vec4(0.0f);

  for (uint i = 0; i < c_samples; i++) {
    vec2 sample_pos = sample_positions[pos_index + i];

    ivec2 coffset = ivec2(greaterThan(cfrac - sample_pos, vec2(0.5f)));
    o_color += texelFetch(s_image_ms, ivec3(cint + coffset, gl_Layer), int(i));
  }
  o_color = o_color / float(c_samples);
}
