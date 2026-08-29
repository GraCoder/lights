#version 450

layout(binding = 0) uniform MVP
{
  vec4 eye;
  mat4 proj;
  mat4 view;
} mvp;

layout(location = 0) in vec3 attr_pos;
layout(location = 1) in vec3 attr_norm;
layout(location = 2) in vec2 attr_uv;

layout(location = 0) out vec3 vp_pos;
layout(location = 1) out vec3 vp_norm;
layout(location = 2) out vec2 vp_uv;

layout(set = 4, binding = 0) uniform ShadowMatrix{
  vec4 light;
  mat4 proj;
  mat4 view;
  mat4 mvp;
  mat4 pers;
} shadow_matrix;


layout(push_constant) uniform Transform
{
  mat4 m;
}
transform;

void main(void)
{
  vec4 pos = transform.m * vec4(attr_pos, 1.0);
  gl_Position = mvp.proj * mvp.view * pos;

  vp_uv = attr_uv;
  vp_pos = pos.xyz / pos.w;
  
  vec4 norm = transform.m * vec4(attr_norm, 0);
  vp_norm = norm.xyz;
}
