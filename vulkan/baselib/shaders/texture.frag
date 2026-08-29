#version 450

layout(location = 0) in vec2 vp_uv;
layout(location = 0) out vec4 frag_color;

layout(set = 1, binding = 0) uniform sampler2D tex;

void main()
{
  frag_color = texture(tex, vp_uv);
  if (frag_color.a == 0.0)
    discard;
}
