#version 450

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 vp_uv;
layout(location = 1) in vec4 vp_color;

layout(location = 0) out vec4 frag_color;

void main()
{
  frag_color = vp_color * texture(tex, vp_uv);
  frag_color = vp_color;
}
