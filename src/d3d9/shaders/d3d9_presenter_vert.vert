#version 450

layout(location = 0) out vec2 o_texcoord;

layout(push_constant) uniform present_info_t {
  vec2 scale;
  vec2 offset;
} u_presentInfo;

void main() {
  vec2 coord = vec2(
    float(gl_VertexIndex & 2),
    float(gl_VertexIndex & 1) * 2.0f);
	
  gl_Position = vec4(-1.0f + 2.0f * coord, 0.0f, 1.0f);

  coord *= u_presentInfo.scale;
  coord += u_presentInfo.offset;

  o_texcoord  = coord;
}