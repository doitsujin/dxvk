#version 450

layout(location = 0) out int o_instance;

void main() {
  o_instance = gl_InstanceIndex;
  gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);
}