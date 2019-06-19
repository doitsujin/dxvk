#version 450

// Fixed Frog Pipeline

layout(binding = 2) uniform D3D9FixedFunctionVS {
  layout(row_major) mat4 D3DTS_WORLD;
  layout(row_major) mat4 D3DTS_VIEW;
  layout(row_major) mat4 D3DTS_PROJECTION;
};

layout (constant_id = 1251) const bool ff_has_position_t = false;
layout (constant_id = 1252) const bool ff_has_color = false;

layout (location = 0) in vec4 in_POSITION;
layout (location = 1) in vec4 in_TEXCOORD[8];
layout (location = 9) in vec4 in_COLOR;

layout (location = 0) out vec4 out_TEXCOORD[8];
layout (location = 8) out vec4 out_COLOR;

void main() {
  gl_Position = vec4(in_POSITION.xyz, 1);

  if (!ff_has_position_t) {
    mat4 wvp = D3DTS_WORLD * D3DTS_VIEW * D3DTS_PROJECTION;
    gl_Position = gl_Position * wvp;
  }

  for (uint i = 0; i < 8; i++)
    out_TEXCOORD[i] = in_TEXCOORD[i];

  if (ff_has_color)
    out_COLOR = in_COLOR;
  else
    out_COLOR = vec4(1,1,1,1);
}