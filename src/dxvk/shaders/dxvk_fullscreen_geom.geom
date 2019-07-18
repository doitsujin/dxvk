#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in  int  i_instance[3];
layout(location = 1) in  vec2 i_texcoord[3];
layout(location = 0) out vec2 o_texcoord;

void main() {
  for (int i = 0; i < 3; i++) {
    o_texcoord  = i_texcoord[i];
    gl_Layer    = i_instance[i];
    gl_Position = gl_in[i].gl_Position;
    EmitVertex();
  }
  
  EndPrimitive();
}
