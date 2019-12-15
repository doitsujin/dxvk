#version 450

layout(location = 0) in  vec2 v_position;
layout(location = 1) in uvec2 v_texcoord;
layout(location = 2) in  vec4 v_color;

layout(location = 0) out vec2 o_texcoord;
layout(location = 1) out vec4 o_color;

void main() {
  o_texcoord = vec2(v_texcoord);
  o_color    = v_color;
  
  vec2 pos = 2.0f * v_position - 1.0f;
  gl_Position = vec4(pos, 0.0f, 1.0f);
}