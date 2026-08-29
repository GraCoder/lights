#version 450

layout(set = 0, binding = 0) uniform MatrixObject
{
  vec4 eye;
  mat4 proj;
  mat4 view;
} mvp;

layout(location = 0) in vec3 attr_pos;
layout(location = 1) in vec3 attr_norm;
layout(location = 2) in vec2 attr_uv;

layout(location = 0) out vec2 vp_uv;

layout(push_constant) uniform Transform
{
  mat4 m;
} transform;

void main()
{
  gl_Position = mvp.proj * mvp.view * transform.m * vec4(attr_pos, 1.0);
  vp_uv = attr_uv;
}
