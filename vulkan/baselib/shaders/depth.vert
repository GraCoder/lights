#version 450

layout(binding = 0) uniform MVP
{
  vec4 eye;
  mat4 proj;
  mat4 view;
}
mvp;

layout(location = 0) in vec3 attr_pos;

layout(push_constant) uniform Transform
{
  mat4 m;
}
transform;

void main(void)
{
  vec4 pos = transform.m * vec4(attr_pos, 1.0);
  gl_Position = mvp.proj * mvp.view * pos;
}