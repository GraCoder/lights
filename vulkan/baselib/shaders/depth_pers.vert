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

layout(location = 0) in vec3 attr_pos;
layout(location = 2) in vec2 attr_uv;

layout(location = 0) out vec3 vp_pos;
layout(location = 2) out vec2 vp_uv;

layout(push_constant) uniform Transform
{
  mat4 m;
}
transform;

void main(void)
{
  vec4 pos = shadow_matrix.pers * transform.m * vec4(attr_pos, 1.0); 
  pos.xyz = pos.xyz / pos.w;
  pos.w = 1.0;
  gl_Position = shadow_matrix.mvp * pos;

  vp_uv = attr_uv;
}