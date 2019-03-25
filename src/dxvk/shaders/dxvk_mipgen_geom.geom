#version 450

layout(points) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in  int  i_instance[1];
layout(location = 0) out vec3 o_pos;

void main() {
  gl_Position = vec4(-1.0f,  3.0f, 0.0f, 1.0f);
  gl_Layer    = i_instance[0];
  o_pos       = vec3(0.0f, 2.0f, float(i_instance[0]));
  EmitVertex();
  
  gl_Position = vec4(-1.0f, -1.0f, 0.0f, 1.0f);
  gl_Layer    = i_instance[0];
  o_pos       = vec3(0.0f, 0.0f, float(i_instance[0]));
  EmitVertex();
  
  gl_Position = vec4( 3.0f, -1.0f, 0.0f, 1.0f);
  gl_Layer    = i_instance[0];
  o_pos       = vec3(2.0f, 0.0f, float(i_instance[0]));
  EmitVertex();
  
  EndPrimitive();
}