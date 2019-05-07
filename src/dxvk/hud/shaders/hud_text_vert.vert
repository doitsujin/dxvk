#version 450

layout(set = 0, binding = 0, std140)
uniform u_hud {
  uvec2 size;
} g_hud;

layout(location = 0) in  vec2 v_position;
layout(location = 1) in uvec2 v_texcoord;

layout(location = 0) out vec2 o_texcoord;

void main() {
  o_texcoord = vec2(v_texcoord);
  
  vec2 pos = 2.0f * (v_position / vec2(g_hud.size)) - 1.0f;
  gl_Position = vec4(pos, 0.0f, 1.0f);
}