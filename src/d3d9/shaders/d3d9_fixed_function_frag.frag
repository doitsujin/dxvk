#version 450

// Fixed Frog Pipeline

layout (constant_id = 13) const bool s_texture_bound = true;
layout (binding = 13) uniform sampler2D s_texture;

layout (constant_id = 1249) const bool alpha_test = false;
layout (constant_id = 1250) const uint alpha_func = 7u;

layout (push_constant) uniform render_state_t {
  float alpha_ref;
} render_state;

layout (location = 0) in vec4 in_TEXCOORD[8];
layout (location = 8) in vec4 in_COLOR;

layout (location = 0) out vec4 o_color;

void ps_main() {
  if (s_texture_bound)
    o_color *= texture(s_texture, in_TEXCOORD[0].xy);
}

void main() {
  o_color = in_COLOR;

  ps_main();

  if (alpha_test) {
    bool keep;
    switch (alpha_func)
    {
      case 0u: {
        keep = false;
        break;
      }
      case 1u: {
        keep = o_color.a < render_state.alpha_ref;
        break;
      }
      case 2u: {
        keep = o_color.a == render_state.alpha_ref;
        break;
      }
      case 3u: {
        keep = o_color.a <= render_state.alpha_ref;
        break;
      }
      case 4u: {
        keep = o_color.a > render_state.alpha_ref;
        break;
      }
      case 5u: {
        keep = o_color.a != render_state.alpha_ref;
        break;
      }
      case 6u: {
        keep = o_color.a >= render_state.alpha_ref;
        break;
      }
      case 7u:
      default: {
        keep = true;
        break;
      }
    }
    if (!keep)
      discard;
  }

}