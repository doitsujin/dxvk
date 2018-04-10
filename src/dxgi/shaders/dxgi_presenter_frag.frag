#version 450

layout(binding = 0) uniform sampler   s_sampler;
layout(binding = 1) uniform texture2D t_texture;

layout(location = 0) in  vec2 i_texcoord;
layout(location = 0) out vec4 o_color;

void main() {
  o_color = texture(sampler2D(t_texture, s_sampler), i_texcoord);
}