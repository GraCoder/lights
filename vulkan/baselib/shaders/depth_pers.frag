#version 450

layout(binding = 0) uniform ShadowMatrix
{
  vec4 light;
  mat4 proj;
  mat4 view;
  mat4 mvp;
  mat4 pers;
}
shadow_matrix;

layout(set = 1, binding = 0) uniform sampler2D tex;

layout(location = 0) out vec4 frag_color;
layout(location = 2) in vec2 vp_uv;

void main(void)
{
  vec4 clr = texture(tex, vp_uv);
  if(clr.a == 0.0f)
    discard;

  frag_color = vec4(0, 0, 0, 1);
}