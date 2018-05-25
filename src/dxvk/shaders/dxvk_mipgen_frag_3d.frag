#version 450

layout(set = 0, binding = 0)
uniform sampler3D s_texture;

layout(location = 0) in  vec3 i_pos;
layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform push_block {
  uint p_layer_count;
};

void main() {
  o_color = texture(s_texture, vec3(i_pos.xy,
    (i_pos.z + 0.5f) / float(p_layer_count)));
}