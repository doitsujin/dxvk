#version 450

layout(set = 0, binding = 0)
uniform sampler2DArray s_texture;

layout(location = 0) in  vec3 i_pos;
layout(location = 0) out vec4 o_color;

void main() {
  o_color = texture(s_texture, i_pos);
}