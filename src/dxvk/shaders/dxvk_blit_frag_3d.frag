#version 450

layout(set = 0, binding = 0)
uniform sampler3D s_texture;

layout(location = 0) in  vec2 i_pos;
layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform push_block {
  vec3 p_src_coord0;
  vec3 p_src_coord1;
  uint p_layer_count;
};

void main() {
  vec3 coord = mix(p_src_coord0, p_src_coord1,
    vec3(i_pos, (float(gl_Layer) + 0.5f) / float(p_layer_count)));
  o_color = texture(s_texture, coord);
}