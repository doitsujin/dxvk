#version 450

layout(set = 0, binding = 0)
uniform sampler1DArray s_texture;

layout(location = 0) in  vec2 i_pos;
layout(location = 0) out vec4 o_color;

layout(push_constant)
uniform push_block {
  vec3 p_src_coord0;
  vec3 p_src_coord1;
  uint p_layer_count;
};

void main() {
  float coord = mix(p_src_coord0.x, p_src_coord1.x, i_pos.x);
  o_color = texture(s_texture, vec2(coord, gl_Layer));
}