#version 450

layout(binding = 0) uniform MVP
{
  mat4 model;
  mat4 proj;
  mat4 view;
  vec4 cam;
}
mvp;

layout(push_constant) uniform PrimitiveData
{
  mat4 model;
} primitive;

layout(location = 0) in vec3 attr_pos;
layout(location = 1) in vec3 attr_norm;
layout(location = 2) in vec2 attr_uv;

layout(location = 0) out vec3 vp_pos;
layout(location = 1) out vec3 vp_norm;
layout(location = 2) out vec2 vp_uv;

void main(void)
{
  gl_Position = mvp.proj * mvp.view * mvp.model * primitive.model * vec4(attr_pos, 1.0);

  vp_uv = attr_uv;
  vp_pos = attr_pos;
  vp_norm = attr_norm;
}
