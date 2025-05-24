#version 460

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require

layout(set = 0, binding = 0) uniform sampler s_samplers[];
layout(set = 1, binding = 0) uniform texture3D s_texture;

layout(location = 0) in  vec2 i_pos;
layout(location = 0) out vec4 o_color;

layout(push_constant, scalar)
uniform push_block {
  vec3 p_src_coord0;
  vec3 p_src_coord1;
  uint p_layer_count;
  uint p_sampler;
};

void main() {
  vec3 coord = mix(p_src_coord0, p_src_coord1,
    vec3(i_pos, (float(gl_Layer) + 0.5f) / float(p_layer_count)));
  o_color = texture(sampler3D(s_texture, s_samplers[p_sampler]), coord);
}
