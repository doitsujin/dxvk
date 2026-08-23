#version 450

#extension GL_EXT_samplerless_texture_functions : require

layout(set = 0, binding = 0)
uniform texture2D s_texture;

layout(location = 0) in vec2 v_coord;
layout(location = 0) out vec4 o_color;

void main() {
  o_color = texelFetch(s_texture, ivec2(v_coord), 0);
}
