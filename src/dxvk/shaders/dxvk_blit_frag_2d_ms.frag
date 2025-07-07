#version 460

#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_samplerless_texture_functions : enable

layout(set = 0, binding = 0) uniform sampler s_samplers[];
layout(set = 1, binding = 0) uniform texture2DMSArray s_image_ms;

layout(location = 0) sample in vec2 i_pos;
layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform push_block {
  float p_src_coord0_x, p_src_coord0_y, p_src_coord0_z;
  float p_src_coord1_x, p_src_coord1_y, p_src_coord1_z;
  uint p_layer_count;
};

layout(constant_id = 0) const int c_samples = 1;
layout(constant_id = 1) const bool c_point_filter = false;

/* Sample grid layout for each pixel */
const uvec2 sample_scale[] = {
  uvec2(2u, 1u),
  uvec2(2u, 2u),
  uvec2(4u, 2u),
  uvec2(4u, 4u),
};

/* Order of samples within the grid in row-major order */
const uint64_t sample_maps[] = {
  0x0000000000000001ul,
  0x0000000000003210ul,
  0x0000000026147035ul,
  0xe58b602c3714d9aful,
};

void main() {
  vec2 coord = vec2(p_src_coord0_x, p_src_coord0_y) + vec2(p_src_coord1_x - p_src_coord0_x - 1u, p_src_coord1_y - p_src_coord0_y - 1u) * i_pos;

  int lookup_index = max(findLSB(c_samples) - 1, 0);

  uvec2 scale = sample_scale[lookup_index];
  uint64_t map = sample_maps[lookup_index];

  coord *= vec2(scale);

  vec2 i_coord = trunc(coord);
  vec2 f_coord = c_point_filter ? vec2(0.0f) : vec2(coord - i_coord);

  o_color = vec4(0.0f);

  for (uint i = 0u; i < (c_point_filter ? 1u : 4u); i++) {
    uvec2 lookup_offset = uvec2(i & 1u, i >> 1u);

    uvec2 sample_coord = uvec2(i_coord + lookup_offset) % scale;
    uvec2 pixel_coord = uvec2(i_coord + lookup_offset) / scale;

    uint sample_index = scale.x * sample_coord.y + sample_coord.x;
    sample_index = uint(map >> (4u * sample_index)) & 0xfu;

    vec4 color = texelFetch(s_image_ms, ivec3(pixel_coord, gl_Layer), int(sample_index));
    vec2 factor = mix(f_coord, 1.0f - f_coord, equal(lookup_offset, 0u.xx));

    o_color += color * factor.x * factor.y;
  }

  o_color = o_color;
}
