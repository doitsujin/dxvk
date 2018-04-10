#version 450

const vec4 g_vpos[4] = {
  vec4(-1.0f, -1.0f, 0.0f, 1.0f),
  vec4(-1.0f,  1.0f, 0.0f, 1.0f),
  vec4( 1.0f, -1.0f, 0.0f, 1.0f),
  vec4( 1.0f,  1.0f, 0.0f, 1.0f),
};

layout(location = 0) out vec2 o_texcoord;

void main() {
  vec4 pos = g_vpos[gl_VertexIndex];
  o_texcoord  = 0.5f + 0.5f * pos.xy;
  gl_Position = pos;
}