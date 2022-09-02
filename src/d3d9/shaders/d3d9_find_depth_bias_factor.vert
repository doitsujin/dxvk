#version 450

void main() {
  vec2 coord = vec2(
    float(gl_VertexIndex & 2),
    float(gl_VertexIndex & 1) * 2.0f);

  gl_Position = vec4(
    -1.0f + coord.x,
    -1.0f + 3.0f * coord.y,
    gl_VertexIndex == 2 ? 0.9 : 0.0,
    1.0f);
}
