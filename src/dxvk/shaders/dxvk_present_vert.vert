#version 450

layout(location = 0) out vec2 o_coord;

void main() {
  vec2 coord = vec2(
    float(gl_VertexIndex & 2),
    float(gl_VertexIndex & 1) * 2.0f);

  o_coord = coord;
  gl_Position = vec4(-1.0f + 2.0f * coord, 0.0f, 1.0f);
}
