#version 460

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

layout(set = 0, binding = 0) uniform sampler s_samplers[];
layout(set = 1, binding = 0) uniform texture3D s_texture;

layout(location = 0) in  vec2 i_pos;
layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform push_block {
  float p_src_coord0_x, p_src_coord0_y, p_src_coord0_z;
  float p_src_coord1_x, p_src_coord1_y, p_src_coord1_z;
  uint p_layer_count;
  uint p_sampler;
};

void main() {
  vec3 coord = mix(
    vec3(p_src_coord0_x, p_src_coord0_y, p_src_coord0_z),
    vec3(p_src_coord1_x, p_src_coord1_y, p_src_coord1_z),
    vec3(i_pos, (float(gl_Layer) + 0.5f) / float(p_layer_count)));

  o_color = texture(sampler3D(s_texture, s_samplers[p_sampler]), coord);
}
