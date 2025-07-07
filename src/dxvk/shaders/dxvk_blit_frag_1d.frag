#version 460

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

layout(set = 0, binding = 0) uniform sampler s_samplers[];
layout(set = 1, binding = 0) uniform texture1DArray s_texture;

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
  float coord = mix(p_src_coord0_x, p_src_coord1_x, i_pos.x);
  o_color = texture(sampler1DArray(s_texture, s_samplers[p_sampler]), vec2(coord, gl_Layer));
}
